#include <gtest/gtest.h>
#include "../../src/ui/RotaryKnob.h"

// Typing a value: the box over the dial applies the raw number on Return, snapped to the range
class RotaryKnobTest : public ::testing::Test {
protected:
    void SetUp() override {
        knob.setRange(0.0, 127.0, 1.0);
        knob.setValue(20.0, juce::dontSendNotification);
        knob.setSize(60, 60);
        knob.onValueChange = [this](double v) { received.push_back(v); };
        knob.onGestureStart = [this] { ++gestureStarts; };
        knob.onGestureEnd = [this] { ++gestureEnds; };
    }
    RotaryKnob knob { "TL", RotaryKnob::Style::Small };
    std::vector<double> received;
    int gestureStarts = 0, gestureEnds = 0;
};

TEST_F(RotaryKnobTest, TypedValueIsAppliedInsideOneGesture)
{
    knob.applyTypedValue("64");
    EXPECT_DOUBLE_EQ(knob.getValue(), 64.0);
    EXPECT_EQ(received, std::vector<double>{ 64.0 });
    EXPECT_EQ(gestureStarts, 1);
    EXPECT_EQ(gestureEnds, 1);
}

TEST_F(RotaryKnobTest, TypedValueSnapsToTheRangeAndStep)
{
    knob.applyTypedValue("300");
    EXPECT_DOUBLE_EQ(knob.getValue(), 127.0);
    knob.applyTypedValue("-5");
    EXPECT_DOUBLE_EQ(knob.getValue(), 0.0);
    knob.setRange(0.5, 12.0, 0.1);
    knob.applyTypedValue("4.26");
    EXPECT_NEAR(knob.getValue(), 4.3, 1e-9);
}

TEST_F(RotaryKnobTest, EmptyOrUnchangedTextTouchesNothing)
{
    knob.applyTypedValue("");
    knob.applyTypedValue("-");
    knob.applyTypedValue("20");
    EXPECT_DOUBLE_EQ(knob.getValue(), 20.0);
    EXPECT_TRUE(received.empty());
    EXPECT_EQ(gestureStarts, 0);
}

TEST_F(RotaryKnobTest, TextEntryOpensOverTheDialAndReturnCommits)
{
    knob.beginTextEntry();
    ASSERT_TRUE(knob.isTextEntryOpen());
    auto* editor = dynamic_cast<juce::TextEditor*>(knob.getChildComponent(0));
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->getText(), "20");
    EXPECT_TRUE(knob.getLocalBounds().contains(editor->getBounds()));
    editor->setText("99", false);
    editor->onReturnKey();
    EXPECT_DOUBLE_EQ(knob.getValue(), 99.0);
    EXPECT_FALSE(knob.isTextEntryOpen());
}
