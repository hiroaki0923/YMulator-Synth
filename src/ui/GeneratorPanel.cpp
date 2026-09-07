#include "GeneratorPanel.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"

namespace {
const juce::Identifier kSettingsNode("generator");
const juce::Identifier kCategoryProperty("category");
constexpr int kChipRadioGroup = 4101;
const juce::String kArrow = juce::String(juce::CharPointer_UTF8(" \xe2\x86\x94 "));
}

GeneratorPanel::GeneratorPanel(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    using ymulatorsynth::GeneratorCategory;
    auto saved = settings();
    const int savedCategory = saved.getProperty(kCategoryProperty, static_cast<int>(GeneratorCategory::Any));
    
    for (size_t i = 0; i < chips.size(); ++i) {
        auto& chip = chips[i];
        chip = std::make_unique<juce::TextButton>(ymulatorsynth::PatchGenerator::categoryName(static_cast<GeneratorCategory>(i)));
        chip->setClickingTogglesState(true);
        chip->setRadioGroupId(kChipRadioGroup);
        chip->getProperties().set("chip", true);
        chip->setToggleState(static_cast<int>(i) == savedCategory, juce::dontSendNotification);
        chip->onClick = [this, i]() { storeCategory(static_cast<GeneratorCategory>(i)); };
        addAndMakeVisible(*chip);
    }
    
    const std::array<std::pair<const char*, const char*>, 6> specs = {{
        { "bright", "Dark" }, { "complex", "Simple" }, { "hardAttack", "Soft" },
        { "length", "Short" }, { "moving", "Still" }, { "metallic", "Harmonic" } }};
    const char* rightWords[6] = { "Bright", "Complex", "Hard attack", "Long", "Moving", "Metallic" };
    const float defaults[6] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.2f, 0.2f };
    
    for (size_t i = 0; i < directions.size(); ++i) {
        auto& d = directions[i];
        d.property = specs[i].first;
        d.label = specs[i].second;
        d.slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        d.slider->setRange(0.0, 100.0, 1.0);
        d.slider->setValue(static_cast<double>(saved.getProperty(d.property, defaults[i] * 100.0f)), juce::dontSendNotification);
        addAndMakeVisible(*d.slider);
        d.name = std::make_unique<juce::Label>("", juce::String(specs[i].second) + kArrow + rightWords[i]);
        d.name->setFont(UiTheme::mono(10.0f));
        d.name->setColour(juce::Label::textColourId, UiTheme::muted);
        addAndMakeVisible(*d.name);
        d.value = std::make_unique<juce::Label>("", juce::String(juce::roundToInt(d.slider->getValue())));
        d.value->setFont(UiTheme::mono(10.0f));
        d.value->setColour(juce::Label::textColourId, UiTheme::muted);
        d.value->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*d.value);
        auto* slider = d.slider.get();
        auto* valueLabel = d.value.get();
        const char* property = d.property;
        d.slider->onValueChange = [this, slider, valueLabel, property]() {
            valueLabel->setText(juce::String(juce::roundToInt(slider->getValue())), juce::dontSendNotification);
            settings().setProperty(property, static_cast<float>(slider->getValue()), nullptr);
        };
    }
}

juce::ValueTree GeneratorPanel::settings() const
{
    return audioProcessor.getParameters().state.getOrCreateChildWithName(kSettingsNode, nullptr);
}

void GeneratorPanel::storeCategory(ymulatorsynth::GeneratorCategory category)
{
    settings().setProperty(kCategoryProperty, static_cast<int>(category), nullptr);
}

ymulatorsynth::GeneratorInput GeneratorPanel::currentInput() const
{
    ymulatorsynth::GeneratorInput input;
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i]->getToggleState()) input.category = static_cast<ymulatorsynth::GeneratorCategory>(i);
    auto value = [this](size_t i) { return static_cast<float>(directions[i].slider->getValue() / 100.0); };
    input.bright = value(0);
    input.complex = value(1);
    input.hardAttack = value(2);
    input.length = value(3);
    input.moving = value(4);
    input.metallic = value(5);
    return input;
}

void GeneratorPanel::resized()
{
    auto bounds = getLocalBounds();
    auto chipRow = bounds.removeFromTop(24);
    for (auto& chip : chips) {
        const int width = juce::roundToInt(juce::GlyphArrangement::getStringWidth(UiTheme::sans(12.0f), chip->getButtonText())) + 26;
        chip->setBounds(chipRow.removeFromLeft(width));
        chipRow.removeFromLeft(6);
    }
    bounds.removeFromTop(14);
    
    const int columns = 2, rows = 3, columnGap = 28, rowGap = 18;
    const int cellWidth = (bounds.getWidth() - columnGap) / columns;
    const int cellHeight = juce::jmin(48, (bounds.getHeight() - rowGap * (rows - 1)) / rows);
    for (size_t i = 0; i < directions.size(); ++i) {
        auto& d = directions[i];
        const int col = static_cast<int>(i) % columns, row = static_cast<int>(i) / columns;
        auto cell = juce::Rectangle<int>(bounds.getX() + col * (cellWidth + columnGap), bounds.getY() + row * (cellHeight + rowGap), cellWidth, cellHeight);
        auto labelRow = cell.removeFromTop(16);
        d.value->setBounds(labelRow.removeFromRight(40));
        d.name->setBounds(labelRow);
        d.slider->setBounds(cell.withTrimmedTop(2));
    }
}
