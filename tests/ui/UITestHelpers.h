#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * UITestHelpers - Helper functions for JUCE UI testing
 * 
 * Based on JUCE forum discussions about GUI testing challenges.
 * Provides utility functions for simulating user interactions.
 */
namespace UITestHelpers {

/**
 * Simulate a button click
 */
inline void triggerButtonClick(juce::Button* button) {
    if (button && button->isEnabled()) {
        button->triggerClick();
    }
}

/**
 * Simulate combo box selection
 */
inline void selectComboBoxItem(juce::ComboBox* comboBox, int itemId) {
    if (comboBox && itemId > 0 && itemId <= comboBox->getNumItems()) {
        comboBox->setSelectedId(itemId);
    }
}

/**
 * Find a component by its button text
 */
template<typename ComponentType>
inline ComponentType* findComponentByText(juce::Component* parent, const juce::String& text) {
    if (!parent) return nullptr;
    
    for (int i = 0; i < parent->getNumChildComponents(); ++i) {
        auto* child = parent->getChildComponent(i);
        
        if (auto* component = dynamic_cast<ComponentType*>(child)) {
            if (auto* button = dynamic_cast<juce::Button*>(component)) {
                if (button->getButtonText() == text) {
                    return component;
                }
            }
        }
        
        // Recursive search
        if (auto* found = findComponentByText<ComponentType>(child, text)) {
            return found;
        }
    }
    
    return nullptr;
}

/**
 * Find a combo box containing specific item text
 */
inline juce::ComboBox* findComboBoxWithItem(juce::Component* parent, const juce::String& itemText) {
    if (!parent) return nullptr;
    
    for (int i = 0; i < parent->getNumChildComponents(); ++i) {
        auto* child = parent->getChildComponent(i);
        
        if (auto* comboBox = dynamic_cast<juce::ComboBox*>(child)) {
            for (int j = 0; j < comboBox->getNumItems(); ++j) {
                if (comboBox->getItemText(j) == itemText) {
                    return comboBox;
                }
            }
        }
        
        // Recursive search
        if (auto* found = findComboBoxWithItem(child, itemText)) {
            return found;
        }
    }
    
    return nullptr;
}

/**
 * Count components of a specific type
 */
template<typename ComponentType>
inline int countComponentsOfType(juce::Component* parent) {
    if (!parent) return 0;
    
    int count = 0;
    
    for (int i = 0; i < parent->getNumChildComponents(); ++i) {
        auto* child = parent->getChildComponent(i);
        
        if (dynamic_cast<ComponentType*>(child) != nullptr) {
            count++;
        }
        
        // Recursive count
        count += countComponentsOfType<ComponentType>(child);
    }
    
    return count;
}

/**
 * Simulate parameter change with gesture
 */
inline void changeParameterWithGesture(juce::AudioProcessorParameter* param, float newValue) {
    if (param) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(newValue);
        param->endChangeGesture();
    }
}

} // namespace UITestHelpers