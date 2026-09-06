#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RotaryKnob.h"

/**
 * Connects a RotaryKnob to a parameter through a hidden slider so the
 * attachment handles host automation, undo gestures and value conversion.
 */
struct KnobBinding
{
    std::unique_ptr<juce::Slider> hiddenSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    
    static KnobBinding attach(juce::AudioProcessorValueTreeState& parameters, const juce::String& parameterId,
                              RotaryKnob& knob, juce::Component& parent, std::function<void()> onChanged = {})
    {
        KnobBinding binding;
        auto* param = parameters.getParameter(parameterId);
        jassert(param != nullptr);
        
        binding.hiddenSlider = std::make_unique<juce::Slider>();
        binding.hiddenSlider->setVisible(false);
        parent.addChildComponent(*binding.hiddenSlider);
        
        if (param != nullptr) {
            const auto& range = param->getNormalisableRange();
            knob.setRange(range.start, range.end, range.interval > 0.0f ? range.interval : 1.0f);
            knob.setTooltip(param->getName(64) + ": drag up or down, hold Shift for fine steps, or scroll");
        }
        
        auto* slider = binding.hiddenSlider.get();
        auto* knobPtr = &knob;
        slider->onValueChange = [knobPtr, slider, onChanged]() {
            knobPtr->setValue(slider->getValue(), juce::dontSendNotification);
            if (onChanged) onChanged();
        };
        knob.onValueChange = [slider, onChanged](double v) {
            slider->setValue(v, juce::sendNotificationSync);
            if (onChanged) onChanged();
        };
        knob.onGestureStart = [param]() { if (param) param->beginChangeGesture(); };
        knob.onGestureEnd = [param]() { if (param) param->endChangeGesture(); };
        
        binding.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, parameterId, *binding.hiddenSlider);
        knob.setValue(slider->getValue(), juce::dontSendNotification);
        return binding;
    }
};
