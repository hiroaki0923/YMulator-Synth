// Renders the plugin editor off-screen to a PNG so UI changes can be reviewed
// without opening a host. Built as YMulatorSynthAU_UISnapshot (see tests/CMakeLists.txt).
//
//   YMulatorSynthAU_UISnapshot --out ui.png [--preset N | --bank B --preset P] [--scale 2] [--settle 300]
//   YMulatorSynthAU_UISnapshot --list-presets
//
// --preset alone selects a global program index the way a host program change
// does; with --bank it goes through the UI's bank/preset path instead.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Options {
    juce::String outputPath = "ui_snapshot.png";
    int preset = -1;
    int bank = -1;
    float scale = 2.0f;
    int settleMs = 300;
    bool listPresets = false;
};

Options parseArgs(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        if (std::strcmp(argv[i], "--out") == 0) o.outputPath = next();
        else if (std::strcmp(argv[i], "--preset") == 0) o.preset = std::atoi(next());
        else if (std::strcmp(argv[i], "--bank") == 0) o.bank = std::atoi(next());
        else if (std::strcmp(argv[i], "--scale") == 0) o.scale = static_cast<float>(std::atof(next()));
        else if (std::strcmp(argv[i], "--settle") == 0) o.settleMs = std::atoi(next());
        else if (std::strcmp(argv[i], "--list-presets") == 0) o.listPresets = true;
    }
    return o;
}

void pumpMessages(int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const Options options = parseArgs(argc, argv);

    YMulatorSynthAudioProcessor processor;
    processor.setPlayConfigDetails(0, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    if (options.listPresets) {
        for (int i = 0; i < processor.getNumPrograms(); ++i)
            std::printf("%3d  %s\n", i, processor.getProgramName(i).toRawUTF8());
        return 0;
    }

    if (options.preset >= 0) {
        if (options.bank >= 0)
            processor.setCurrentPresetInBank(options.bank, options.preset);
        else
            processor.setCurrentProgram(options.preset);
        pumpMessages(options.settleMs);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (editor == nullptr) {
        std::fprintf(stderr, "createEditor() returned null\n");
        return 1;
    }
    pumpMessages(options.settleMs);

    auto image = editor->createComponentSnapshot(editor->getLocalBounds(), false, options.scale);
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(options.outputPath);
    file.deleteFile();
    juce::FileOutputStream stream(file);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream(image, stream)) {
        std::fprintf(stderr, "could not write %s\n", file.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf("wrote %s (%dx%d px, editor %dx%d, preset %d \"%s\")\n",
                file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight(),
                editor->getWidth(), editor->getHeight(), processor.getCurrentProgram(),
                processor.getProgramName(processor.getCurrentProgram()).toRawUTF8());
    editor.reset();
    return 0;
}
