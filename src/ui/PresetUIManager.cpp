#include "PresetUIManager.h"
#include "../PluginProcessor.h"
#include "../utils/Debug.h"
#include "../utils/ParameterIDs.h"
#include <set>

PresetUIManager::PresetUIManager(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupComponents();
    
    // Listen for parameter state changes relevant to presets
    audioProcessor.getParameters().state.addListener(this);
    
    // Initialize bank and preset displays
    juce::MessageManager::callAsync([this]() {
        updateBankComboBox();
        updatePresetComboBox();
    });
    
    CS_DBG("PresetUIManager created");
}

PresetUIManager::~PresetUIManager()
{
    // Remove listener to avoid dangling pointer - check if state is still valid
    try {
        audioProcessor.getParameters().state.removeListener(this);
    } catch (...) {
        // Ignore exceptions during destruction
    }
    CS_DBG("PresetUIManager destroyed");
}

void PresetUIManager::paint(juce::Graphics& g)
{
    // No specific painting needed - child components handle their own painting
    juce::ignoreUnused(g);
}

void PresetUIManager::resized()
{
    auto bounds = getLocalBounds();
    
    // Save button on the right first
    auto saveButtonArea = bounds.removeFromRight(50);
    if (savePresetButton) {
        auto centeredButtonArea = saveButtonArea.withHeight(25).withCentre(saveButtonArea.getCentre());
        savePresetButton->setBounds(centeredButtonArea);
    }
    
    // Bank label and ComboBox
    auto bankLabelArea = bounds.removeFromLeft(40);
    if (bankLabel) {
        bankLabel->setBounds(bankLabelArea);
    }
    
    auto bankComboArea = bounds.removeFromLeft(120).reduced(5, 0);
    if (bankComboBox) {
        auto centeredBankArea = bankComboArea.withHeight(30).withCentre(bankComboArea.getCentre());
        bankComboBox->setBounds(centeredBankArea);
    }
    
    // Preset label and ComboBox
    auto presetLabelArea = bounds.removeFromLeft(45);
    if (presetLabel) {
        presetLabel->setBounds(presetLabelArea);
    }
    
    // Remaining space for preset ComboBox
    if (presetComboBox) {
        auto centeredPresetArea = bounds.withHeight(30).withCentre(bounds.getCentre()).reduced(5, 0);
        presetComboBox->setBounds(centeredPresetArea);
    }
}

void PresetUIManager::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                             const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged);
    
    const auto propertyName = property.toString();
    
    // Special handling for preset index changes from DAW
    if (propertyName == "presetIndexChanged") {
        juce::MessageManager::callAsync([this]() {
            updateBankComboBox();
            updatePresetComboBox();
        });
        return;
    }
    
    // Special handling for bank list changes (after DAW project load)
    if (propertyName == "bankListUpdated") {
        CS_DBG("PresetUIManager received bankListUpdated notification");
        juce::MessageManager::callAsync([this]() {
            updateBankComboBox();
            updatePresetComboBox();
        });
        return;
    }
    
    // Define preset-relevant properties that should trigger UI updates
    static const std::set<std::string> presetRelevantProperties = {
        "presetIndex",
        "isCustomMode",
        "currentBankIndex",
        "currentPresetInBank",
        "presetListUpdated",
        "bankListUpdated"
    };
    
    // Filter out properties that don't affect preset display
    if (presetRelevantProperties.find(propertyName.toStdString()) == presetRelevantProperties.end()) {
        return;
    }
    
    // This is a preset-relevant property change - update the preset UI
    juce::MessageManager::callAsync([this]() {
        updateBankComboBox();
        updatePresetComboBox();
    });
}

void PresetUIManager::updateBankComboBox()
{
    if (!bankComboBox) return;
    
    // Get current bank names
    auto bankNames = audioProcessor.getBankNames();
    
    // Check if we need to update (including the Import item)
    int expectedItems = bankNames.size() + 1; // +1 for "Import OPM File..."
    bool needsUpdate = (bankComboBox->getNumItems() != expectedItems);
    if (!needsUpdate) {
        for (int i = 0; i < bankNames.size(); ++i) {
            if (bankComboBox->getItemText(i) != bankNames[i]) {
                needsUpdate = true;
                break;
            }
        }
        // Check if last item is still the import option
        if (bankComboBox->getItemText(bankComboBox->getNumItems() - 1) != "Import OPM File...") {
            needsUpdate = true;
        }
    }
    
    if (!needsUpdate) {
        return; // Already up to date
    }
    
    bankComboBox->clear();
    
    // Add all banks
    for (int i = 0; i < bankNames.size(); ++i) {
        bankComboBox->addItem(bankNames[i], i + 1);
    }
    
    // Add separator and import option
    bankComboBox->addSeparator();
    bankComboBox->addItem("Import OPM File...", 9999); // Use high ID to distinguish
    
    // Select bank from ValueTreeState (for DAW persistence)
    int savedBankIndex = 0; // Default to Factory bank
    
    // Try state property first
    auto& state = audioProcessor.getParameters().state;
    if (state.hasProperty(ParamID::Global::CurrentBankIndex)) {
        savedBankIndex = state.getProperty(ParamID::Global::CurrentBankIndex, 0);
        CS_FILE_DBG("PresetUIManager restored bank index from state property: " + juce::String(savedBankIndex));
    } else {
        // Fallback to parameter approach
        auto bankParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentBankIndex);
        if (bankParam) {
            savedBankIndex = static_cast<int>(bankParam->getValue() * (bankParam->getNumSteps() - 1));
            CS_DBG("PresetUIManager restored bank index from parameter: " + juce::String(savedBankIndex));
        }
    }
    
    // Ensure the bank index is valid
    isUpdatingFromState = true;
    if (savedBankIndex >= 0 && savedBankIndex < bankNames.size()) {
        CS_FILE_DBG("PresetUIManager setting bank combo to ID: " + juce::String(savedBankIndex + 1));
        bankComboBox->setSelectedId(savedBankIndex + 1, juce::dontSendNotification);
    } else {
        CS_FILE_DBG("PresetUIManager bank index invalid, defaulting to Factory");
        bankComboBox->setSelectedId(1, juce::dontSendNotification); // Default to Factory
    }
    isUpdatingFromState = false;
}

void PresetUIManager::updatePresetComboBox()
{
    if (!presetComboBox) return;
    
    // Get presets for currently selected bank
    int selectedBankId = bankComboBox ? bankComboBox->getSelectedId() : 1;
    int bankIndex = selectedBankId - 1; // Convert to 0-based index
    
    // Get preset names for the selected bank
    auto presetNames = audioProcessor.getPresetsForBank(bankIndex);
    
    // Check if we need to update
    bool needsUpdate = (presetComboBox->getNumItems() != presetNames.size());
    if (!needsUpdate) {
        for (int i = 0; i < presetNames.size(); ++i) {
            if (presetComboBox->getItemText(i) != presetNames[i]) {
                needsUpdate = true;
                break;
            }
        }
    }
    
    if (!needsUpdate && !audioProcessor.isInCustomMode()) {
        return; // Already up to date
    }
    
    // Rebuild preset list
    presetComboBox->clear();
    
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetComboBox->addItem(presetNames[i], i + 1);
    }
    
    // Set current selection (if not in custom mode)
    if (!audioProcessor.isInCustomMode())
    {
        // Get saved preset index from ValueTreeState (for DAW persistence)
        int savedPresetIndex = 7; // Default to Init preset
        
        // Try state property first
        auto& state = audioProcessor.getParameters().state;
        if (state.hasProperty(ParamID::Global::CurrentPresetInBank)) {
            savedPresetIndex = state.getProperty(ParamID::Global::CurrentPresetInBank, 7);
            CS_FILE_DBG("PresetUIManager restored preset index from state property: " + juce::String(savedPresetIndex));
        } else {
            // Fallback to parameter approach
            auto presetParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentPresetInBank);
            if (presetParam) {
                savedPresetIndex = static_cast<int>(presetParam->getValue() * (presetParam->getNumSteps() - 1));
                CS_DBG("PresetUIManager restored preset index from parameter: " + juce::String(savedPresetIndex));
            }
        }
        
        // Ensure the preset index is valid for this bank
        isUpdatingFromState = true;
        if (savedPresetIndex >= 0 && savedPresetIndex < presetNames.size()) {
            CS_FILE_DBG("PresetUIManager setting preset combo to ID: " + juce::String(savedPresetIndex + 1));
            presetComboBox->setSelectedId(savedPresetIndex + 1, juce::dontSendNotification);
        } else {
            CS_DBG("PresetUIManager preset index invalid, using fallback search");
            // Fallback: Find which preset in the current bank matches the global current preset
            int currentGlobalIndex = audioProcessor.getCurrentProgram();
            
            // Find the preset index within the current bank
            for (int i = 0; i < presetNames.size(); ++i) {
                int globalIndex = audioProcessor.getPresetManager().getGlobalPresetIndex(bankIndex, i);
                if (globalIndex == currentGlobalIndex) {
                    presetComboBox->setSelectedId(i + 1, juce::dontSendNotification);
                    break;
                }
            }
        }
        isUpdatingFromState = false;
    }
    
    // Enable/disable Save button based on custom mode
    if (savePresetButton) {
        bool hasChanges = audioProcessor.isInCustomMode();
        savePresetButton->setEnabled(hasChanges);
        
        // Update visual appearance based on state
        if (hasChanges) {
            savePresetButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a5568));
            savePresetButton->setTooltip("Save modified settings as new preset");
        } else {
            savePresetButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
            savePresetButton->setTooltip("Save as new preset (modify parameters to enable)");
        }
    }
}

void PresetUIManager::refreshPresetDisplay()
{
    updateBankComboBox();
    updatePresetComboBox();
}

void PresetUIManager::setupComponents()
{
    // Bank selector
    bankComboBox = std::make_unique<juce::ComboBox>();
    bankComboBox->addItem("Factory", 1);
    bankComboBox->onChange = [this]() { onBankChanged(); };
    addAndMakeVisible(*bankComboBox);
    
    bankLabel = std::make_unique<juce::Label>("", "Bank");
    bankLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    bankLabel->setJustificationType(juce::Justification::centredRight);
    bankLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*bankLabel);
    
    // Preset selector
    presetComboBox = std::make_unique<juce::ComboBox>();
    presetComboBox->onChange = [this]() { onPresetChanged(); };
    addAndMakeVisible(*presetComboBox);
    
    presetLabel = std::make_unique<juce::Label>("", "Preset");
    presetLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel->setJustificationType(juce::Justification::centredRight);
    presetLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*presetLabel);
    
    savePresetButton = std::make_unique<juce::TextButton>("Save");
    savePresetButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a5568));
    savePresetButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    savePresetButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    savePresetButton->setTooltip("Save current settings as new preset");
    savePresetButton->onClick = [this]() { savePresetDialog(); };
    // Initially disabled - will be enabled when in custom mode
    savePresetButton->setEnabled(false);
    addAndMakeVisible(*savePresetButton);
}

void PresetUIManager::onBankChanged()
{
    CS_FILE_DBG("PresetUIManager onBankChanged called, isUpdatingFromState=" + juce::String(isUpdatingFromState ? "true" : "false"));
    if (!bankComboBox || isUpdatingFromState) return;
    
    int selectedId = bankComboBox->getSelectedId();
    CS_FILE_DBG("PresetUIManager onBankChanged: selectedId = " + juce::String(selectedId));
    
    // Check if "Import OPM File..." was selected
    if (selectedId == 9999) {
        CS_DBG("PresetUIManager Import OPM File option selected");
        
        // Reset to previous bank selection (Factory by default)
        bankComboBox->setSelectedId(1, juce::dontSendNotification);
        
        // Open import dialog
        loadOpmFileDialog();
        return;
    }
    
    // Save bank selection to ValueTreeState for DAW persistence
    int bankIndex = selectedId - 1; // Convert to 0-based index
    
    // Save to state property (more reliable for persistence)
    auto& state = audioProcessor.getParameters().state;
    state.setProperty(ParamID::Global::CurrentBankIndex, bankIndex, nullptr);
    CS_DBG("PresetUIManager saved bank index to state property: " + juce::String(bankIndex));
    
    // Also save to parameter for host automation
    auto bankParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentBankIndex);
    if (bankParam) {
        float normalizedValue = bankParam->convertTo0to1(static_cast<float>(bankIndex));
        bankParam->setValueNotifyingHost(normalizedValue);
    }
    
    // Normal bank selection - defer update to avoid blocking the dropdown
    juce::MessageManager::callAsync([this]() {
        updatePresetComboBox();
    });
}

void PresetUIManager::onPresetChanged()
{
    CS_FILE_DBG("PresetUIManager onPresetChanged called, isUpdatingFromState=" + juce::String(isUpdatingFromState ? "true" : "false"));
    if (!presetComboBox || !bankComboBox || isUpdatingFromState) return;
    
    int selectedPresetId = presetComboBox->getSelectedId();
    int selectedBankId = bankComboBox->getSelectedId();
    CS_FILE_DBG("PresetUIManager onPresetChanged: bankId=" + juce::String(selectedBankId) + ", presetId=" + juce::String(selectedPresetId));
    
    if (selectedPresetId > 0 && selectedBankId > 0)
    {
        // Convert to 0-based indices
        int bankIndex = selectedBankId - 1;
        int presetIndex = selectedPresetId - 1;
        
        // Save preset selection to ValueTreeState for DAW persistence
        auto& state = audioProcessor.getParameters().state;
        state.setProperty(ParamID::Global::CurrentPresetInBank, presetIndex, nullptr);
        CS_DBG("PresetUIManager saved preset index to state property: " + juce::String(presetIndex));
        
        // Also save to parameter for host automation
        auto presetParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentPresetInBank);
        if (presetParam) {
            float normalizedValue = presetParam->convertTo0to1(static_cast<float>(presetIndex));
            presetParam->setValueNotifyingHost(normalizedValue);
        }
        
        // Defer the actual change to avoid blocking the dropdown
        juce::MessageManager::callAsync([this, bankIndex, presetIndex]() {
            audioProcessor.setCurrentPresetInBank(bankIndex, presetIndex);
        });
    }
}

void PresetUIManager::loadOpmFileDialog()
{
    CS_DBG("PresetUIManager loadOpmFileDialog() called");
    
    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Select a VOPM preset file",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.opm"
    );
    
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    
    fileChooser->launchAsync(chooserFlags, [this, fileChooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        
        if (file.existsAsFile())
        {
            // Load the OPM file through the audio processor
            int numLoaded = audioProcessor.loadOpmFile(file);
            
            if (numLoaded > 0)
            {
                // Update the bank list to include the new bank
                updateBankComboBox();
                
                // Select the newly created bank (it will be the last bank before the Import option)
                auto bankNames = audioProcessor.getBankNames();
                int newBankIndex = bankNames.size(); // Last bank
                if (newBankIndex > 0) {
                    bankComboBox->setSelectedId(newBankIndex, juce::dontSendNotification);
                }
                
                // Update presets for the newly selected bank
                updatePresetComboBox();
                
                // Notify that preset list has been updated
                audioProcessor.getParameters().state.setProperty("presetListUpdated", juce::var(juce::Random::getSystemRandom().nextInt()), nullptr);
                
                // Show success message
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Load Successful",
                    "Loaded " + juce::String(numLoaded) + " preset(s) from " + file.getFileName()
                );
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Load Error",
                    "Failed to load any presets from: " + file.getFileName()
                );
            }
        }
    });
}

void PresetUIManager::savePresetDialog()
{
    // Get default preset name
    juce::String defaultName = "My Preset";
    if (audioProcessor.isInCustomMode()) {
        defaultName = audioProcessor.getCustomPresetName();
    }
    
    // Create dialog
    auto* dialog = new juce::AlertWindow("Save Preset", 
                                        "Enter a name for the new preset:", 
                                        juce::MessageBoxIconType::QuestionIcon);
    
    dialog->addTextEditor("presetName", defaultName, "Preset Name:");
    dialog->addButton("Save", 1);
    dialog->addButton("Cancel", 0);
    
    // Use a lambda that captures the dialog pointer
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog](int result)
    {
        if (result == 1)
        {
            // Get the preset name from the text editor
            auto* textEditor = dialog->getTextEditor("presetName");
            if (textEditor)
            {
                juce::String presetName = textEditor->getText().trim();
                
                if (presetName.isNotEmpty())
                {
                    // Save directly to User bank (dummy file parameter - not used anymore)
                    savePresetToFile(juce::File{}, presetName);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Invalid Name",
                        "Please enter a valid preset name."
                    );
                }
            }
        }
        
        delete dialog;
    }));
}

void PresetUIManager::savePresetToFile(const juce::File& file, const juce::String& presetName)
{
    juce::ignoreUnused(file); // File parameter not used - saving to User bank instead
    
    // Save the preset to the User bank instead of a file
    if (audioProcessor.saveCurrentPresetToUserBank(presetName))
    {
        // Update UI to show the new preset
        updateBankComboBox();
        updatePresetComboBox();
        
        // Select User bank
        auto bankNames = audioProcessor.getBankNames();
        for (int i = 0; i < bankNames.size(); ++i) {
            if (bankNames[i] == "User") {
                bankComboBox->setSelectedId(i + 1, juce::dontSendNotification);
                updatePresetComboBox();
                break;
            }
        }
        
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Save Successful",
            "Preset '" + presetName + "' has been saved to the User bank.\n\n" +
            "Your preset will persist across application restarts."
        );
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Save Error",
            "Failed to save preset '" + presetName + "' to User bank."
        );
    }
}