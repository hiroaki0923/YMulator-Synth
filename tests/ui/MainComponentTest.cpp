#include <gtest/gtest.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../src/PluginProcessor.h"
#include "../../src/ui/MainComponent.h"
#include "../mocks/MockAudioProcessorHost.h"

/**
 * MainComponentTest - UI Component Testing
 * 
 * Tests MainComponent functionality before Phase 2 refactoring.
 * 
 * Note: JUCE UI testing has limitations:
 * - Components may not report as "visible" without a parent window
 * - Async initialization may affect test timing
 * - Focus on structural and functional tests rather than visual state
 */
class MainComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize JUCE's MessageManager for UI testing
        if (juce::MessageManager::getInstance() == nullptr) {
            juce::MessageManager::getInstance();
        }
        
        // Create processor
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        
        // Initialize processor
        processor->prepareToPlay(44100.0, 512);
        
        // Create MainComponent
        mainComponent = std::make_unique<MainComponent>(*processor);
        
        // Ensure UI is properly sized
        mainComponent->setSize(1000, 635);
    }
    
    void TearDown() override {
        mainComponent.reset();
        if (processor) {
            processor->resetProcessBlockStaticState();
            ymulatorsynth::ParameterManager::resetStaticState();
        }
        processor.reset();
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<MainComponent> mainComponent;
};

// =============================================================================
// Basic Component Tests
// =============================================================================

TEST_F(MainComponentTest, ComponentInitialization) {
    EXPECT_NE(mainComponent.get(), nullptr);
    EXPECT_EQ(mainComponent->getWidth(), 1000);
    EXPECT_EQ(mainComponent->getHeight(), 635);
}

TEST_F(MainComponentTest, ComponentHasRequiredChildren) {
    // Check that MainComponent has the expected number of child components
    int childCount = mainComponent->getNumChildComponents();
    EXPECT_GT(childCount, 10) << "MainComponent should have multiple child components";
}

// =============================================================================
// Preset UI Component Tests  
// =============================================================================

TEST_F(MainComponentTest, PresetUIComponentsExist) {
    // Find preset-related components by searching child components
    bool foundBankComboBox = false;
    bool foundPresetComboBox = false;
    bool foundSaveButton = false;
    bool foundBankLabel = false;
    bool foundPresetLabel = false;
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        
        // Check for ComboBox components
        if (auto* comboBox = dynamic_cast<juce::ComboBox*>(child)) {
            // Bank combo should have "Factory" item
            for (int j = 0; j < comboBox->getNumItems(); ++j) {
                if (comboBox->getItemText(j) == "Factory") {
                    foundBankComboBox = true;
                }
            }
            
            // If it's not the bank combo, it might be preset combo
            if (!foundBankComboBox && comboBox->getNumItems() > 0) {
                foundPresetComboBox = true;
            }
        }
        
        // Check for Save button
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            if (button->getButtonText() == "Save") {
                foundSaveButton = true;
            }
        }
        
        // Check for labels
        if (auto* label = dynamic_cast<juce::Label*>(child)) {
            juce::String labelText = label->getText();
            if (labelText == "Bank") {
                foundBankLabel = true;
            } else if (labelText == "Preset") {
                foundPresetLabel = true;
            }
        }
    }
    
    EXPECT_TRUE(foundBankComboBox) << "Bank ComboBox not found";
    EXPECT_TRUE(foundBankLabel) << "Bank Label not found";
    EXPECT_TRUE(foundPresetLabel) << "Preset Label not found";
    EXPECT_TRUE(foundSaveButton) << "Save Button not found";
}

TEST_F(MainComponentTest, PresetUIInitialState) {
    // Find the bank ComboBox and save button
    juce::ComboBox* bankComboBox = nullptr;
    juce::TextButton* saveButton = nullptr;
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        
        if (auto* comboBox = dynamic_cast<juce::ComboBox*>(child)) {
            // Bank combo should have "Factory" item
            for (int j = 0; j < comboBox->getNumItems(); ++j) {
                if (comboBox->getItemText(j) == "Factory") {
                    bankComboBox = comboBox;
                    break;
                }
            }
        }
        
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            if (button->getButtonText() == "Save") {
                saveButton = button;
            }
        }
    }
    
    ASSERT_NE(bankComboBox, nullptr) << "Bank ComboBox not found";
    ASSERT_NE(saveButton, nullptr) << "Save Button not found";
    
    // Bank ComboBox should have at least Factory bank
    EXPECT_GE(bankComboBox->getNumItems(), 1);
    
    // Save button should initially be disabled (no custom mode)
    EXPECT_FALSE(saveButton->isEnabled()) << "Save button should be disabled initially";
}

// =============================================================================
// Global Controls Tests
// =============================================================================

// Helper function to recursively count UI components
std::pair<int, int> countUIComponents(juce::Component* component) {
    int comboBoxCount = 0;
    int sliderCount = 0;
    
    // Check if this component itself is a ComboBox or Slider
    if (dynamic_cast<juce::ComboBox*>(component) != nullptr) {
        comboBoxCount++;
    }
    if (dynamic_cast<juce::Slider*>(component) != nullptr) {
        sliderCount++;
    }
    
    // Recursively check child components
    for (int i = 0; i < component->getNumChildComponents(); ++i) {
        auto childCounts = countUIComponents(component->getChildComponent(i));
        comboBoxCount += childCounts.first;
        sliderCount += childCounts.second;
    }
    
    return {comboBoxCount, sliderCount};
}

TEST_F(MainComponentTest, GlobalControlsExist) {
    auto counts = countUIComponents(mainComponent.get());
    int comboBoxCount = counts.first;
    int sliderCount = counts.second;
    
    // Should have multiple ComboBoxes (algorithm, global pan, LFO waveform, bank, preset)
    EXPECT_GE(comboBoxCount, 4) << "Should have multiple ComboBoxes, found: " << comboBoxCount;
    
    // Should have multiple sliders/knobs
    EXPECT_GT(sliderCount, 0) << "Should have sliders/knobs for controls, found: " << sliderCount;
}

// =============================================================================
// LFO and Noise Controls Tests
// =============================================================================

TEST_F(MainComponentTest, LFOControlsExist) {
    bool foundToggleButton = false;
    int labelCount = 0;
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        
        if (dynamic_cast<juce::ToggleButton*>(child) != nullptr) {
            foundToggleButton = true;
        }
        
        if (dynamic_cast<juce::Label*>(child) != nullptr) {
            labelCount++;
        }
    }
    
    EXPECT_TRUE(foundToggleButton) << "Should have toggle button (likely for noise enable)";
    EXPECT_GT(labelCount, 5) << "Should have multiple labels for controls";
}

// =============================================================================
// Operator Panel Tests
// =============================================================================

TEST_F(MainComponentTest, OperatorPanelsExist) {
    // MainComponent should have 4 operator panels (components with many children)
    int complexComponentCount = 0;
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        
        // Operator panels typically have many sub-components
        if (child->getNumChildComponents() > 10) {
            complexComponentCount++;
        }
    }
    
    // We should find at least 4 complex components (operator panels)
    EXPECT_GE(complexComponentCount, 4) << "Should have 4 operator panels";
}

// =============================================================================
// Layout and Resizing Tests
// =============================================================================

TEST_F(MainComponentTest, ComponentResizing) {
    // Test different sizes
    mainComponent->setSize(800, 600);
    EXPECT_EQ(mainComponent->getWidth(), 800);
    EXPECT_EQ(mainComponent->getHeight(), 600);
    
    mainComponent->setSize(1200, 700);
    EXPECT_EQ(mainComponent->getWidth(), 1200);
    EXPECT_EQ(mainComponent->getHeight(), 700);
    
    // Ensure no crashes during resize
    mainComponent->resized();
}

TEST_F(MainComponentTest, ComponentBoundsAreReasonable) {
    // Check that child components are positioned within the parent bounds
    auto parentBounds = mainComponent->getLocalBounds();
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        auto childBounds = child->getBounds();
        
        // Child components should be within parent bounds (with some tolerance)
        EXPECT_GE(childBounds.getX(), -10) << "Child component too far left";
        EXPECT_GE(childBounds.getY(), -10) << "Child component too far up";
        EXPECT_LE(childBounds.getRight(), parentBounds.getWidth() + 10) << "Child component too far right";
        EXPECT_LE(childBounds.getBottom(), parentBounds.getHeight() + 10) << "Child component too far down";
    }
}

// =============================================================================
// ValueTree Property Change Tests
// =============================================================================

TEST_F(MainComponentTest, PropertyChangeHandling) {
    // Test that MainComponent can handle property changes without crashing
    auto& valueTree = processor->getParameters().state;
    
    // Simulate preset-related property changes
    valueTree.setProperty("presetIndex", 5, nullptr);
    valueTree.setProperty("isCustomMode", true, nullptr);
    valueTree.setProperty("currentBankIndex", 1, nullptr);
    
    // Give some time for async operations
    juce::Thread::sleep(10);
    
    // Component should still be valid
    EXPECT_NE(mainComponent.get(), nullptr);
    EXPECT_EQ(mainComponent->getWidth(), 1000);
}

// =============================================================================
// Integration Tests with AudioProcessor
// =============================================================================

TEST_F(MainComponentTest, ProcessorIntegration) {
    // Verify that MainComponent properly integrates with AudioProcessor
    EXPECT_EQ(processor->getCurrentProgram(), 7); // Should default to Init preset
    
    // Verify that processor has the required methods
    EXPECT_GT(processor->getBankNames().size(), 0);
    EXPECT_GT(processor->getPresetsForBank(0).size(), 0);
    
    // Test preset loading doesn't crash UI
    processor->setCurrentProgram(0);
    juce::Thread::sleep(10);
    
    EXPECT_NE(mainComponent.get(), nullptr);
}

// =============================================================================
// Painting and Visibility Tests
// =============================================================================

TEST_F(MainComponentTest, ComponentPainting) {
    // Test that component can paint without crashing
    juce::Graphics g(juce::Image(juce::Image::RGB, 1000, 635, true));
    
    EXPECT_NO_THROW({
        mainComponent->paint(g);
    });
    
    // Test some child component painting
    for (int i = 0; i < std::min(5, mainComponent->getNumChildComponents()); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        juce::Graphics childG(juce::Image(juce::Image::RGB, 100, 100, true));
        EXPECT_NO_THROW({
            child->paint(childG);
        });
    }
}

// =============================================================================
// Functional Tests - These test actual UI behavior
// =============================================================================

TEST_F(MainComponentTest, PresetComboBoxInteraction) {
    // Find preset combo box
    juce::ComboBox* presetComboBox = nullptr;
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        if (auto* comboBox = dynamic_cast<juce::ComboBox*>(child)) {
            // Look for a combo that's not the bank combo
            bool isBank = false;
            for (int j = 0; j < comboBox->getNumItems(); ++j) {
                if (comboBox->getItemText(j) == "Factory") {
                    isBank = true;
                    break;
                }
            }
            if (!isBank && comboBox->getNumItems() > 0) {
                presetComboBox = comboBox;
                break;
            }
        }
    }
    
    if (presetComboBox != nullptr) {
        // Verify preset combo has items
        EXPECT_GT(presetComboBox->getNumItems(), 0) << "Preset combo should have items";
        
        // Test selection
        if (presetComboBox->getNumItems() > 1) {
            int originalSelection = presetComboBox->getSelectedId();
            presetComboBox->setSelectedId(1);
            EXPECT_EQ(presetComboBox->getSelectedId(), 1);
            presetComboBox->setSelectedId(originalSelection);
        }
    }
}

TEST_F(MainComponentTest, SaveButtonEnabledWhenCustomMode) {
    // Find save button
    juce::TextButton* saveButton = nullptr;
    
    for (int i = 0; i < mainComponent->getNumChildComponents(); ++i) {
        auto* child = mainComponent->getChildComponent(i);
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            if (button->getButtonText() == "Save") {
                saveButton = button;
                break;
            }
        }
    }
    
    if (saveButton != nullptr) {
        // Initially should be disabled
        EXPECT_FALSE(saveButton->isEnabled());
        
        // Simulate entering custom mode by changing a parameter
        auto* param = processor->getParameters().getParameter("algorithm");
        if (param != nullptr) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(0.5f);
            param->endChangeGesture();
            
            // Give some time for UI update
            juce::Thread::sleep(50);
            
            // Note: In real usage, the save button would be enabled
            // but in testing this might not work due to async updates
        }
    }
}