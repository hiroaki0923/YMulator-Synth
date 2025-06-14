#include "PresetManager.h"
#include "Debug.h"
#include "VOPMParser.h"
#include "BinaryData.h"

namespace ymulatorsynth {

// Factory preset definitions
static const VOPMVoice FACTORY_VOICES[] = {
    // Electric Piano
    {
        0, "Electric Piano",
        {0, 0, 0, 0, 0}, // LFO
        {3, 6, 4, 0, 0, 15, 0}, // Channel (pan=3=center, slotMask=15=all)
        {
            {22, 5, 0, 3, 5, 30, 0, 2, 3, 0, 0}, // M1
            {16, 8, 8, 7, 2, 0, 1, 2, 3, 0, 0},  // C1
            {20, 5, 0, 3, 5, 34, 0, 4, 7, 0, 0}, // M2
            {17, 8, 7, 7, 2, 0, 1, 2, 7, 0, 0}   // C2
        }
    },
    
    // Synth Bass
    {
        1, "Synth Bass",
        {0, 0, 0, 0, 0}, // LFO
        {3, 7, 6, 0, 0, 15, 0}, // Channel
        {
            {31, 0, 0, 12, 1, 18, 1, 1, 0, 0, 0}, // M1
            {31, 0, 1, 12, 1, 4, 1, 2, 0, 0, 0},  // C1
            {31, 0, 0, 9, 0, 3, 0, 1, 7, 0, 0},  // M2
            {31, 0, 0, 9, 0, 3, 0, 1, 3, 0, 0}   // C2
        }
    },
    
    // Brass Section
    {
        2, "Brass Section",
        {0, 0, 0, 0, 0}, // LFO
        {3, 7, 2, 0, 0, 15, 0}, // Channel
        {
            {13, 6, 0, 8, 1, 25, 2, 2, 3, 0, 0}, // M1
            {15, 8, 0, 8, 1, 32, 1, 6, 7, 0, 0}, // C1
            {21, 7, 0, 8, 2, 42, 0, 2, 3, 0, 0}, // M2
            {18, 4, 0, 8, 2, 0, 1, 2, 0, 0, 0}   // C2
        }
    },
    
    // String Pad
    {
        3, "String Pad",
        {0, 0, 0, 0, 0}, // LFO
        {3, 7, 2, 0, 0, 15, 0}, // Channel
        {
            {25, 10, 0, 5, 1, 29, 1, 1, 1, 0, 0}, // M1
            {25, 11, 0, 8, 5, 15, 1, 5, 1, 0, 0}, // C1
            {28, 13, 0, 6, 2, 45, 1, 1, 0, 0, 0}, // M2
            {14, 4, 0, 6, 0, 0, 1, 1, 0, 0, 0}   // C2
        }
    },
    
    // Lead Synth
    {
        4, "Lead Synth",
        {0, 0, 0, 0, 0}, // LFO
        {3, 4, 7, 0, 0, 15, 0}, // Channel
        {
            {31, 0, 0, 10, 0, 24, 0, 1, 0, 0, 0}, // M1
            {31, 0, 0, 10, 0, 22, 0, 2, 0, 0, 0}, // C1
            {31, 0, 0, 10, 0, 26, 0, 3, 0, 0, 0}, // M2
            {31, 0, 0, 10, 0, 0, 0, 1, 0, 0, 0}   // C2
        }
    },
    
    // Organ
    {
        5, "Organ",
        {0, 0, 0, 0, 0}, // LFO
        {3, 0, 7, 0, 0, 15, 0}, // Channel
        {
            {31, 0, 0, 9, 0, 33, 0, 5, 7, 0, 0}, // M1
            {31, 13, 0, 9, 1, 0, 0, 3, 3, 0, 0}, // C1
            {31, 0, 0, 9, 0, 3, 0, 2, 3, 0, 0},  // M2
            {31, 0, 0, 9, 0, 0, 0, 1, 7, 0, 0}   // C2
        }
    },
    
    // Bells
    {
        6, "Bells",
        {0, 0, 0, 0, 0}, // LFO
        {3, 3, 4, 0, 0, 15, 0}, // Channel
        {
            {31, 12, 0, 10, 5, 38, 0, 6, 3, 0, 0}, // M1
            {31, 8, 4, 6, 11, 4, 0, 2, 3, 0, 0},  // C1
            {31, 12, 4, 6, 2, 40, 1, 6, 7, 0, 0}, // M2
            {31, 6, 4, 6, 11, 0, 0, 2, 7, 0, 0}   // C2
        }
    },
    
    // Init
    {
        7, "Init",
        {0, 0, 0, 0, 0}, // LFO
        {3, 2, 6, 0, 0, 15, 0}, // Channel
        {
            {31, 0, 0, 15, 0, 43, 0, 0, 0, 0, 0}, // M1
            {31, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0},  // C1
            {31, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0},  // M2
            {31, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0}   // C2
        }
    }
};

const int NUM_FACTORY_PRESETS = sizeof(FACTORY_VOICES) / sizeof(FACTORY_VOICES[0]);

// Preset conversion functions
Preset Preset::fromVOPM(const VOPMVoice& voice)
{
    Preset preset;
    preset.id = voice.number;
    preset.name = voice.name;
    preset.algorithm = voice.channel.algorithm;
    preset.feedback = voice.channel.feedback;
    
    // Set LFO parameters from VOPM voice
    preset.lfo.rate = voice.lfo.frequency;
    preset.lfo.amd = voice.lfo.amd;
    preset.lfo.pmd = voice.lfo.pmd;
    preset.lfo.waveform = voice.lfo.waveform;
    preset.lfo.noiseFreq = voice.lfo.noiseFreq;
    
    // Set channel AMS/PMS parameters (same for all channels in VOPM format)
    for (int ch = 0; ch < 8; ++ch) {
        preset.channels[ch].ams = voice.channel.ams;
        preset.channels[ch].pms = voice.channel.pms;
        preset.channels[ch].noiseEnable = voice.channel.noiseEnable;
    }
    
    for (int i = 0; i < 4; ++i)
    {
        const auto& op = voice.operators[i];
        // TL is already in 0-127 range in VOPM format, store as is
        preset.operators[i].totalLevel = static_cast<float>(op.totalLevel);
        preset.operators[i].multiple = static_cast<float>(op.multiple);
        preset.operators[i].detune1 = static_cast<float>(op.detune1);
        preset.operators[i].detune2 = static_cast<float>(op.detune2);
        preset.operators[i].keyScale = static_cast<float>(op.keyScale);
        preset.operators[i].attackRate = static_cast<float>(op.attackRate);
        preset.operators[i].decay1Rate = static_cast<float>(op.decay1Rate);
        preset.operators[i].decay2Rate = static_cast<float>(op.decay2Rate);
        preset.operators[i].releaseRate = static_cast<float>(op.releaseRate);
        preset.operators[i].sustainLevel = static_cast<float>(op.decay1Level);
        preset.operators[i].amsEnable = (op.amsEnable != 0);
        // Extract SLOT enable from channel slotMask
        preset.operators[i].slotEnable = (voice.channel.slotMask & (1 << i)) != 0;
    }
    
    return preset;
}

VOPMVoice Preset::toVOPM() const
{
    VOPMVoice voice;
    voice.number = id;
    voice.name = name;
    voice.channel.algorithm = algorithm;
    voice.channel.feedback = feedback;
    voice.channel.pan = 3; // Center (internal representation)
    
    // Build SLOT mask from individual operator SLOT enable flags
    int slotMask = 0;
    for (int i = 0; i < 4; ++i) {
        if (operators[i].slotEnable) {
            slotMask |= (1 << i);
        }
    }
    voice.channel.slotMask = slotMask;
    
    // Set LFO parameters to VOPM voice
    voice.lfo.frequency = lfo.rate;
    voice.lfo.amd = lfo.amd;
    voice.lfo.pmd = lfo.pmd;
    voice.lfo.waveform = lfo.waveform;
    voice.lfo.noiseFreq = lfo.noiseFreq;
    
    // Use channel 0's AMS/PMS settings (VOPM format has one setting per voice)
    voice.channel.ams = channels[0].ams;
    voice.channel.pms = channels[0].pms;
    voice.channel.noiseEnable = channels[0].noiseEnable;
    
    for (int i = 0; i < 4; ++i)
    {
        auto& op = voice.operators[i];
        // TL is stored as 0-127 range
        op.totalLevel = static_cast<int>(operators[i].totalLevel);
        op.multiple = static_cast<int>(operators[i].multiple);
        op.detune1 = static_cast<int>(operators[i].detune1);
        op.detune2 = static_cast<int>(operators[i].detune2);
        op.keyScale = static_cast<int>(operators[i].keyScale);
        op.attackRate = static_cast<int>(operators[i].attackRate);
        op.decay1Rate = static_cast<int>(operators[i].decay1Rate);
        op.decay2Rate = static_cast<int>(operators[i].decay2Rate);
        op.releaseRate = static_cast<int>(operators[i].releaseRate);
        op.decay1Level = static_cast<int>(operators[i].sustainLevel);
        op.amsEnable = operators[i].amsEnable ? 1 : 0;
    }
    
    return voice;
}

// PresetManager implementation
PresetManager::PresetManager()
{
}

void PresetManager::initialize()
{
    clear();
    loadFactoryPresets();
    loadBundledPresets();
    
    CS_DBG("PresetManager initialized with " + juce::String(presets.size()) + " presets");
}

int PresetManager::loadOPMFile(const juce::File& file)
{
    if (!file.exists())
    {
        CS_DBG("OPM file does not exist: " + file.getFullPathName());
        return 0;
    }
    
    auto voices = VOPMParser::parseFile(file);
    int loaded = 0;
    
    for (const auto& voice : voices)
    {
        auto preset = Preset::fromVOPM(voice);
        // Offset OPM preset IDs to avoid conflict with factory presets
        preset.id += NUM_FACTORY_PRESETS;
        validatePreset(preset);
        addPreset(preset);
        loaded++;
    }
    
    CS_DBG("Loaded " + juce::String(loaded) + " presets from " + file.getFileName());
    return loaded;
}

int PresetManager::loadBundledPresets()
{
    int totalLoaded = 0;
    
    // First try to load from bundled binary resources
    if (BinaryData::chipsynthaupresetcollection_opmSize > 0)
    {
        juce::String content(static_cast<const char*>(BinaryData::chipsynthaupresetcollection_opm), 
                           BinaryData::chipsynthaupresetcollection_opmSize);
        
        auto voices = VOPMParser::parseContent(content);
        for (const auto& voice : voices)
        {
            auto preset = Preset::fromVOPM(voice);
            // Offset OPM preset IDs to avoid conflict with factory presets
            preset.id += NUM_FACTORY_PRESETS;
            validatePreset(preset);
            addPreset(preset);
            totalLoaded++;
        }
        
        CS_DBG("Loaded " + juce::String(totalLoaded) + " presets from bundled resources");
        return totalLoaded;
    }
    
    // Fallback: Try to load from external files
    auto presetsDir = getPresetsDirectory();
    if (!presetsDir.exists())
    {
        CS_DBG("Presets directory does not exist: " + presetsDir.getFullPathName());
        return 0;
    }
    
    // Look for the main preset collection file
    auto collectionFile = presetsDir.getChildFile("chipsynth-au-preset-collection.opm");
    if (collectionFile.exists())
    {
        totalLoaded += loadOPMFile(collectionFile);
    }
    
    // Load any other .opm files in the directory
    juce::Array<juce::File> opmFiles;
    presetsDir.findChildFiles(opmFiles, juce::File::findFiles, false, "*.opm");
    
    for (const auto& file : opmFiles)
    {
        if (file != collectionFile) // Don't load the same file twice
        {
            totalLoaded += loadOPMFile(file);
        }
    }
    
    return totalLoaded;
}

const Preset* PresetManager::getPreset(int id) const
{
    // ID is the index in the presets array
    if (id >= 0 && id < static_cast<int>(presets.size()))
    {
        return &presets[id];
    }
    return nullptr;
}

const Preset* PresetManager::getPreset(const juce::String& name) const
{
    for (const auto& preset : presets)
    {
        if (preset.name == name)
            return &preset;
    }
    return nullptr;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& preset : presets)
    {
        names.add(preset.name);
    }
    return names;
}

void PresetManager::addPreset(const Preset& preset)
{
    // Check if preset with same ID already exists
    for (auto& existing : presets)
    {
        if (existing.id == preset.id)
        {
            // Replace existing preset
            existing = preset;
            return;
        }
    }
    
    // Add new preset
    presets.push_back(preset);
}

void PresetManager::removePreset(int id)
{
    presets.erase(
        std::remove_if(presets.begin(), presets.end(),
                      [id](const Preset& p) { return p.id == id; }),
        presets.end());
}

bool PresetManager::saveOPMFile(const juce::File& file) const
{
    juce::String content;
    content << ";==================================================\n";
    content << "; YMulator Synth Presets\n";
    content << "; Generated automatically\n";
    content << ";==================================================\n\n";
    
    for (const auto& preset : presets)
    {
        auto voice = preset.toVOPM();
        content << VOPMParser::voiceToString(voice) << "\n";
    }
    
    return file.replaceWithText(content);
}

void PresetManager::clear()
{
    presets.clear();
}

std::vector<Preset> PresetManager::getFactoryPresets()
{
    std::vector<Preset> factoryPresets;
    
    for (int i = 0; i < NUM_FACTORY_PRESETS; ++i)
    {
        factoryPresets.push_back(Preset::fromVOPM(FACTORY_VOICES[i]));
    }
    
    return factoryPresets;
}

// Private methods
void PresetManager::loadFactoryPresets()
{
    for (int i = 0; i < NUM_FACTORY_PRESETS; ++i)
    {
        auto preset = Preset::fromVOPM(FACTORY_VOICES[i]);
        validatePreset(preset);
        presets.push_back(preset);
    }
    
    CS_DBG("Loaded " + juce::String(NUM_FACTORY_PRESETS) + " factory presets");
}

juce::File PresetManager::getPresetsDirectory() const
{
    // Try to find the resources/presets directory relative to the executable
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    
    // For development, look relative to the project root
    auto projectRoot = executableFile;
    for (int i = 0; i < 10; ++i) // Go up to 10 levels
    {
        auto resourcesDir = projectRoot.getChildFile("resources").getChildFile("presets");
        if (resourcesDir.exists())
        {
            return resourcesDir;
        }
        projectRoot = projectRoot.getParentDirectory();
        if (!projectRoot.exists())
            break;
    }
    
    // For production, look in the bundle's Resources directory
    auto bundleResourcesDir = executableFile.getParentDirectory()
                                           .getParentDirectory()
                                           .getChildFile("Resources")
                                           .getChildFile("presets");
    if (bundleResourcesDir.exists())
    {
        return bundleResourcesDir;
    }
    
    // Fallback: user's Documents directory
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                     .getChildFile("YMulator Synth")
                     .getChildFile("presets");
}

void PresetManager::validatePreset(Preset& preset) const
{
    // Clamp values to valid ranges
    preset.algorithm = juce::jlimit(0, 7, preset.algorithm);
    preset.feedback = juce::jlimit(0, 7, preset.feedback);
    
    // Validate LFO parameters
    preset.lfo.rate = juce::jlimit(0, 255, preset.lfo.rate);
    preset.lfo.amd = juce::jlimit(0, 127, preset.lfo.amd);
    preset.lfo.pmd = juce::jlimit(0, 127, preset.lfo.pmd);
    preset.lfo.waveform = juce::jlimit(0, 3, preset.lfo.waveform);
    preset.lfo.noiseFreq = juce::jlimit(0, 31, preset.lfo.noiseFreq);
    
    // Validate channel AMS/PMS parameters
    for (int ch = 0; ch < 8; ++ch) {
        preset.channels[ch].ams = juce::jlimit(0, 3, preset.channels[ch].ams);
        preset.channels[ch].pms = juce::jlimit(0, 7, preset.channels[ch].pms);
        preset.channels[ch].noiseEnable = juce::jlimit(0, 1, preset.channels[ch].noiseEnable);
    }
    
    for (int i = 0; i < 4; ++i)
    {
        auto& op = preset.operators[i];
        op.totalLevel = juce::jlimit(0.0f, 127.0f, op.totalLevel);
        op.multiple = juce::jlimit(0.0f, 15.0f, op.multiple);
        op.detune1 = juce::jlimit(0.0f, 7.0f, op.detune1);
        op.detune2 = juce::jlimit(0.0f, 3.0f, op.detune2);
        op.keyScale = juce::jlimit(0.0f, 3.0f, op.keyScale);
        op.attackRate = juce::jlimit(0.0f, 31.0f, op.attackRate);
        op.decay1Rate = juce::jlimit(0.0f, 31.0f, op.decay1Rate);
        op.decay2Rate = juce::jlimit(0.0f, 31.0f, op.decay2Rate);
        op.releaseRate = juce::jlimit(0.0f, 15.0f, op.releaseRate);
        op.sustainLevel = juce::jlimit(0.0f, 15.0f, op.sustainLevel);
    }
}

} // namespace ymulatorsynth