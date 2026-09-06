// Renders the plugin editor off-screen to a PNG so UI changes can be reviewed
// without opening a host. Built as YMulatorSynthAU_UISnapshot (see tests/CMakeLists.txt).
//
//   YMulatorSynthAU_UISnapshot --out ui.png [--preset N | --bank B --preset P] [--then-preset N]
//                              [--scale 2] [--settle 300] [--dump] [--focus-macro N] [--note N]
//   YMulatorSynthAU_UISnapshot --list-presets
//
// --preset alone selects a global program index the way a host program change
// does; with --bank it goes through the UI's bank/preset path instead.
// --then-preset changes the program again after the editor exists, which is how
// a host program change reaches an open editor. --dump prints the state
// properties and every combo box text so a render can be checked in a terminal.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ui/MainComponent.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Options {
    juce::String outputPath = "ui_snapshot.png";
    int preset = -1;
    int bank = -1;
    int thenPreset = -1;
    float scale = 2.0f;
    int settleMs = 300;
    bool listPresets = false;
    bool dump = false;
    int focusMacro = -1;   // index into ymulatorsynth::Macro, highlights its targets
    int note = -1;         // MIDI note to hold while capturing (fills the output scope)
};

Options parseArgs(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        if (std::strcmp(argv[i], "--out") == 0) o.outputPath = next();
        else if (std::strcmp(argv[i], "--preset") == 0) o.preset = std::atoi(next());
        else if (std::strcmp(argv[i], "--bank") == 0) o.bank = std::atoi(next());
        else if (std::strcmp(argv[i], "--then-preset") == 0) o.thenPreset = std::atoi(next());
        else if (std::strcmp(argv[i], "--scale") == 0) o.scale = static_cast<float>(std::atof(next()));
        else if (std::strcmp(argv[i], "--settle") == 0) o.settleMs = std::atoi(next());
        else if (std::strcmp(argv[i], "--list-presets") == 0) o.listPresets = true;
        else if (std::strcmp(argv[i], "--dump") == 0) o.dump = true;
        else if (std::strcmp(argv[i], "--focus-macro") == 0) o.focusMacro = std::atoi(next());
        else if (std::strcmp(argv[i], "--note") == 0) o.note = std::atoi(next());
    }
    return o;
}

void pumpMessages(int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}

// Prints every combo box with its current text, plus the top-level state
// properties, so a render can be checked from the terminal as well as by eye.
void dumpControls(juce::Component& root, int depth = 0)
{
    for (auto* child : root.getChildren()) {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(child))
            std::printf("%*scombo \"%s\" = \"%s\" (id %d)\n", depth * 2, "",
                        combo->getName().toRawUTF8(), combo->getText().toRawUTF8(), combo->getSelectedId());
        dumpControls(*child, depth + 1);
    }
}

void dumpState(juce::AudioProcessorValueTreeState& parameters)
{
    const auto& state = parameters.state;
    for (int i = 0; i < state.getNumProperties(); ++i) {
        const auto name = state.getPropertyName(i);
        std::printf("state.%s = %s\n", name.toString().toRawUTF8(), state.getProperty(name).toString().toRawUTF8());
    }
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

    if (options.thenPreset >= 0) {
        processor.setCurrentProgram(options.thenPreset);
        pumpMessages(options.settleMs);
    }

    if (options.note >= 0) {
        juce::AudioBuffer<float> audio(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, options.note, static_cast<juce::uint8>(100)), 0);
        for (int block = 0; block < 40; ++block) {
            processor.processBlock(audio, midi);
            midi.clear();
        }
        pumpMessages(options.settleMs);
    }

    if (options.focusMacro >= 0) {
        for (int i = 0; i < editor->getNumChildComponents(); ++i)
            if (auto* main = dynamic_cast<MainComponent*>(editor->getChildComponent(i)))
                main->setMacroFocus(static_cast<ymulatorsynth::Macro>(options.focusMacro));
        pumpMessages(options.settleMs);
    }

    if (options.dump) {
        dumpState(processor.getParameters());
        dumpControls(*editor);
    }

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
