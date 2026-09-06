// Renders a standard MIDI file through one plugin instance per track and writes
// the mix as a WAV, so a demo song can be checked without a host.
//
//   YMulatorSynthAU_SongRender --midi song.mid --out song.wav
//       --program 1=10 --program 2=39 ...      bundled program (preset index) per MIDI track (1-based, tempo track excluded)
//       [--opm voices.opm --voice 1=0 ...]      or voices from an .opm file, by index within that file
//       [--motion 3=2]                          MotionPanel preset index per track
//       [--param 2=macro_brightness:15]         any parameter, plain value
//       [--bpm 172]                             transport tempo reported to the plugins (default: from the file)
//       [--gain 1=0.8] [--master 0.6] [--rate 48000] [--tail 1.5]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ui/MotionPanel.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {

struct Options {
    juce::String midiPath, outputPath = "song.wav";
    std::map<int, int> programs, motions, voices;
    std::map<int, float> gains;
    std::vector<std::pair<int, std::pair<juce::String, float>>> params;
    juce::String opmPath;
    double bpm = 0.0;
    float master = 0.6f;
    double sampleRate = 48000.0;
    double tailSeconds = 1.5;
};

Options parseArgs(int argc, char** argv)
{
    Options o;
    auto pair = [](const char* s, int& key, double& value) { key = std::atoi(s); const char* eq = std::strchr(s, '='); value = eq ? std::atof(eq + 1) : 0.0; };
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : ""; };
        int k; double v;
        if (std::strcmp(argv[i], "--midi") == 0) o.midiPath = next();
        else if (std::strcmp(argv[i], "--out") == 0) o.outputPath = next();
        else if (std::strcmp(argv[i], "--program") == 0) { pair(next(), k, v); o.programs[k] = static_cast<int>(v); }
        else if (std::strcmp(argv[i], "--motion") == 0) { pair(next(), k, v); o.motions[k] = static_cast<int>(v); }
        else if (std::strcmp(argv[i], "--gain") == 0) { pair(next(), k, v); o.gains[k] = static_cast<float>(v); }
        else if (std::strcmp(argv[i], "--opm") == 0) o.opmPath = next();
        else if (std::strcmp(argv[i], "--voice") == 0) { pair(next(), k, v); o.voices[k] = static_cast<int>(v); }
        else if (std::strcmp(argv[i], "--bpm") == 0) o.bpm = std::atof(next());
        else if (std::strcmp(argv[i], "--param") == 0) {
            const juce::String spec(next());
            const int track = spec.upToFirstOccurrenceOf("=", false, false).getIntValue();
            const juce::String rest = spec.fromFirstOccurrenceOf("=", false, false);
            o.params.push_back({ track, { rest.upToFirstOccurrenceOf(":", false, false), rest.fromFirstOccurrenceOf(":", false, false).getFloatValue() } });
        }
        else if (std::strcmp(argv[i], "--master") == 0) o.master = static_cast<float>(std::atof(next()));
        else if (std::strcmp(argv[i], "--rate") == 0) o.sampleRate = std::atof(next());
        else if (std::strcmp(argv[i], "--tail") == 0) o.tailSeconds = std::atof(next());
    }
    return o;
}

// Lets the motion engine follow a tempo, as a DAW transport would
class FixedTransport : public juce::AudioPlayHead {
public:
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setBpm(bpm);
        info.setPpqPosition(ppq);
        info.setIsPlaying(true);
        return info;
    }
    double bpm = 120.0, ppq = 0.0;
};

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const Options options = parseArgs(argc, argv);
    
    juce::File midiFile = juce::File::getCurrentWorkingDirectory().getChildFile(options.midiPath);
    juce::FileInputStream stream(midiFile);
    juce::MidiFile midi;
    if (!stream.openedOk() || !midi.readFrom(stream)) {
        std::fprintf(stderr, "could not read %s\n", midiFile.getFullPathName().toRawUTF8());
        return 1;
    }
    double fileBpm = 120.0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (int e = 0; e < midi.getTrack(t)->getNumEvents(); ++e)
            if (const auto& m = midi.getTrack(t)->getEventPointer(e)->message; m.isTempoMetaEvent())
                fileBpm = 60.0 / m.getTempoSecondsPerQuarterNote();
    midi.convertTimestampTicksToSeconds();
    FixedTransport transport;
    transport.bpm = options.bpm > 0.0 ? options.bpm : fileBpm;
    
    constexpr int kBlock = 512;
    struct Track { std::unique_ptr<YMulatorSynthAudioProcessor> processor; juce::MidiMessageSequence events; float gain = 1.0f; juce::String name; };
    std::vector<Track> tracks;
    double lastEvent = 0.0;
    int musicalIndex = 0;
    for (int t = 0; t < midi.getNumTracks(); ++t) {
        const auto* seq = midi.getTrack(t);
        bool hasNotes = false;
        for (int e = 0; e < seq->getNumEvents(); ++e) if (seq->getEventPointer(e)->message.isNoteOn()) { hasNotes = true; break; }
        if (!hasNotes) continue;
        ++musicalIndex;
        Track track;
        track.events = *seq;
        track.processor = std::make_unique<YMulatorSynthAudioProcessor>();
        track.processor->setPlayConfigDetails(0, 2, options.sampleRate, kBlock);
        track.processor->prepareToPlay(options.sampleRate, kBlock);
        track.processor->setPlayHead(&transport);
        const auto program = options.programs.find(musicalIndex);
        track.processor->setCurrentProgram(program != options.programs.end() ? program->second : 7);
        if (auto voice = options.voices.find(musicalIndex); voice != options.voices.end() && options.opmPath.isNotEmpty()) {
            // The plugin keeps imported files in its preset folder, so a bank may already be there
            const juce::File opm = juce::File::getCurrentWorkingDirectory().getChildFile(options.opmPath);
            auto bankIndex = [&]() { return track.processor->getBankNames().indexOf(opm.getFileNameWithoutExtension()); };
            if (bankIndex() < 0) track.processor->loadOpmFile(opm);
            if (bankIndex() < 0) { std::fprintf(stderr, "could not load %s\n", opm.getFullPathName().toRawUTF8()); return 1; }
            track.processor->setCurrentPresetInBank(bankIndex(), voice->second);
        }
        track.name = track.processor->getProgramName(track.processor->getCurrentProgram());
        for (int e = 0; e < seq->getNumEvents(); ++e) {
            const auto& m = seq->getEventPointer(e)->message;
            if (m.isTrackNameEvent()) track.name = m.getTextFromTextMetaEvent() + " -> " + track.name;
            lastEvent = juce::jmax(lastEvent, m.getTimeStamp());
        }
        if (auto motion = options.motions.find(musicalIndex); motion != options.motions.end())
            for (const auto& [id, value] : MotionPanel::presets()[static_cast<size_t>(juce::jlimit(0, static_cast<int>(MotionPanel::presets().size()) - 1, motion->second))].values)
                if (auto* p = track.processor->getParameters().getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
        for (const auto& [trackIndex, idValue] : options.params)
            if (trackIndex == musicalIndex)
                if (auto* p = track.processor->getParameters().getParameter(idValue.first)) p->setValueNotifyingHost(p->convertTo0to1(idValue.second));
        if (auto gain = options.gains.find(musicalIndex); gain != options.gains.end()) track.gain = gain->second;
        std::printf("track %d: %s (gain %.2f)\n", musicalIndex, track.name.toRawUTF8(), track.gain);
        tracks.push_back(std::move(track));
    }
    if (tracks.empty()) { std::fprintf(stderr, "no note tracks\n"); return 1; }
    
    const int totalSamples = static_cast<int>((lastEvent + options.tailSeconds) * options.sampleRate);
    juce::AudioBuffer<float> mix(2, totalSamples);
    mix.clear();
    juce::AudioBuffer<float> block(2, kBlock);
    std::vector<int> cursor(tracks.size(), 0);
    float peak = 0.0f;
    for (int start = 0; start < totalSamples; start += kBlock) {
        const int n = juce::jmin(kBlock, totalSamples - start);
        const double blockStart = start / options.sampleRate, blockEnd = (start + n) / options.sampleRate;
        transport.ppq = blockStart * transport.bpm / 60.0;
        for (size_t t = 0; t < tracks.size(); ++t) {
            juce::MidiBuffer midiBlock;
            auto& seq = tracks[t].events;
            while (cursor[t] < seq.getNumEvents() && seq.getEventPointer(cursor[t])->message.getTimeStamp() < blockEnd) {
                const auto& m = seq.getEventPointer(cursor[t])->message;
                if (m.isNoteOnOrOff())
                    midiBlock.addEvent(m, juce::jlimit(0, n - 1, static_cast<int>((m.getTimeStamp() - blockStart) * options.sampleRate)));
                ++cursor[t];
            }
            block.clear();
            tracks[t].processor->processBlock(block, midiBlock);
            for (int ch = 0; ch < 2; ++ch) mix.addFrom(ch, start, block, ch, 0, n, tracks[t].gain * options.master);
        }
    }
    for (int ch = 0; ch < 2; ++ch) peak = juce::jmax(peak, mix.getMagnitude(ch, 0, totalSamples));
    
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(options.outputPath);
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(out), options.sampleRate, 2, 16, {}, 0));
    if (writer == nullptr || !writer->writeFromAudioSampleBuffer(mix, 0, totalSamples)) { std::fprintf(stderr, "could not write %s\n", out.getFullPathName().toRawUTF8()); return 1; }
    writer.reset();
    std::printf("wrote %s: %.1f s, peak %.3f\n", out.getFullPathName().toRawUTF8(), totalSamples / options.sampleRate, peak);
    for (auto& t : tracks) t.processor->releaseResources();
    return 0;
}
