// Renders the plugin editor off-screen to a PNG so UI changes can be reviewed
// without opening a host. Built as YMulatorSynthAU_UISnapshot (see tests/CMakeLists.txt).
//
//   YMulatorSynthAU_UISnapshot --out ui.png [--preset N | --bank B --preset P] [--then-preset N]
//                              [--scale 2] [--settle 300] [--dump] [--focus-macro N] [--note N] [--view quick|detail] [--scope-time S] [--param id=value]
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
#include "ui/OutputScope.h"
#include "core/PatchPreview.h"
#include "ui/ArpSettingsPanel.h"
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
    double scopeTime = -1.0; // --scope-time: seconds into the OUTPUT render (its timer needs a desktop window)
    std::vector<std::pair<juce::String, float>> params; // --param id=value (plain value, e.g. motion_sync=1)
    bool previewDebug = false; // --preview-debug: print the extracted preset and the preview's peak
    bool arpPanel = false;     // --arp-panel: render the arpeggio settings callout instead of the editor
    juce::String stateFile;  // --state-file: raw plugin state blob (as saved by a host) restored before the editor opens
    int note = -1;         // MIDI note to hold while capturing (fills the output scope)
    juce::String view;     // "quick" or "detail"
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
        else if (std::strcmp(argv[i], "--scope-time") == 0) o.scopeTime = std::atof(next());
        else if (std::strcmp(argv[i], "--state-file") == 0) o.stateFile = next();
        else if (std::strcmp(argv[i], "--preview-debug") == 0) o.previewDebug = true;
        else if (std::strcmp(argv[i], "--arp-panel") == 0) o.arpPanel = true;
        else if (std::strcmp(argv[i], "--param") == 0) {
            juce::String spec(next());
            o.params.emplace_back(spec.upToFirstOccurrenceOf("=", false, false), spec.fromFirstOccurrenceOf("=", false, false).getFloatValue());
        }
        else if (std::strcmp(argv[i], "--list-presets") == 0) o.listPresets = true;
        else if (std::strcmp(argv[i], "--dump") == 0) o.dump = true;
        else if (std::strcmp(argv[i], "--focus-macro") == 0) o.focusMacro = std::atoi(next());
        else if (std::strcmp(argv[i], "--note") == 0) o.note = std::atoi(next());
        else if (std::strcmp(argv[i], "--view") == 0) o.view = next();
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

    if (options.stateFile.isNotEmpty()) {
        juce::MemoryBlock blob;
        if (!juce::File::getCurrentWorkingDirectory().getChildFile(options.stateFile).loadFileAsData(blob)) { std::fprintf(stderr, "cannot read state file\n"); return 1; }
        processor.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        pumpMessages(options.settleMs);
        std::printf("restored %d bytes of state\n", static_cast<int>(blob.getSize()));
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

    for (int i = 0; i < editor->getNumChildComponents(); ++i) {
        auto* main = dynamic_cast<MainComponent*>(editor->getChildComponent(i));
        if (main == nullptr) continue;
        if (options.view.isNotEmpty())
            main->setViewMode(options.view == "detail" ? MainComponent::ViewMode::Detail : MainComponent::ViewMode::Quick);
        if (options.focusMacro >= 0)
            main->setMacroFocus(static_cast<ymulatorsynth::Macro>(options.focusMacro));
    }
    if (options.view.isNotEmpty() || options.focusMacro >= 0) pumpMessages(options.settleMs);
    for (const auto& [id, value] : options.params) {
        if (auto* p = processor.getParameters().getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
        else std::fprintf(stderr, "no parameter \"%s\"\n", id.toRawUTF8());
    }
    if (!options.params.empty()) pumpMessages(options.settleMs);
    if (options.scopeTime >= 0.0) {
        std::function<void(juce::Component&)> visit = [&](juce::Component& c) {
            if (auto* scope = dynamic_cast<OutputScope*>(&c)) scope->setPlayhead(options.scopeTime);
            for (auto* child : c.getChildren()) visit(*child);
        };
        visit(*editor);
    }

    if (options.previewDebug) {
        ymulatorsynth::Preset preset;
        processor.extractCurrentPreset(preset);
        std::printf("alg %d fb %d noise %d nfrq %d lfo %d/%d/%d/%d ams %d pms %d\n", preset.algorithm, preset.feedback,
                    preset.channels[0].noiseEnable, preset.lfo.noiseFreq, preset.lfo.rate, preset.lfo.amd, preset.lfo.pmd, preset.lfo.waveform,
                    preset.channels[0].ams, preset.channels[0].pms);
        for (int op = 0; op < 4; ++op) {
            const auto& o = preset.operators[op];
            std::printf("op%d tl %.0f ar %.0f d1r %.0f d1l %.0f d2r %.0f rr %.0f ks %.0f mul %.0f dt1 %.0f dt2 %.0f ams %d slot %d\n", op + 1,
                        o.totalLevel, o.attackRate, o.decay1Rate, o.sustainLevel, o.decay2Rate, o.releaseRate, o.keyScale, o.multiple,
                        o.detune1, o.detune2, o.amsEnable ? 1 : 0, o.slotEnable ? 1 : 0);
        }
        for (const char* id : { "op1_tl", "op1_ar", "op2_tl", "op2_ar", "op1_slot_en", "algorithm" }) {
            auto* p = processor.getParameters().getParameter(id);
            std::printf("  %s: getValue %.4f -> %.1f, raw atomic %.1f, tree value %s\n", id, p->getValue(), p->convertFrom0to1(p->getValue()),
                        processor.getParameters().getRawParameterValue(id)->load(),
                        processor.getParameters().state.getChildWithProperty("id", id).getProperty("value").toString().toRawUTF8());
        }
        ymulatorsynth::PatchPreview preview;
        auto peakOf = [](const std::vector<float>& v) { float p = 0.0f; for (float x : v) p = std::max(p, std::abs(x)); return p; };
        std::printf("preview peak: %.4f\n", peakOf(preview.render(preset, 24000)));
        auto allOn = preset; for (auto& o : allOn.operators) o.slotEnable = true;
        std::printf("preview peak with every slot on: %.4f\n", peakOf(preview.render(allOn, 24000)));
        auto mul1 = preset; for (auto& o : mul1.operators) o.multiple = 1;
        std::printf("preview peak with MUL 1: %.4f\n", peakOf(preview.render(mul1, 24000)));
        auto ks = preset; for (auto& o : ks.operators) { o.keyScale = 0; o.detune1 = 0; o.detune2 = 0; }
        std::printf("preview peak with KS/DT cleared: %.4f\n", peakOf(preview.render(ks, 24000)));
    }
    if (options.dump) {
        dumpState(processor.getParameters());
        dumpControls(*editor);
    }

    std::unique_ptr<ArpSettingsPanel> arpPanel;
    if (options.arpPanel) { arpPanel = std::make_unique<ArpSettingsPanel>(processor); pumpMessages(options.settleMs); }
    juce::Component& subject = options.arpPanel ? static_cast<juce::Component&>(*arpPanel) : static_cast<juce::Component&>(*editor);
    auto image = subject.createComponentSnapshot(subject.getLocalBounds(), false, options.scale);
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
