#include "AlgorithmDisplay.h"
#include "../utils/Debug.h"
#include "BinaryData.h"

AlgorithmDisplay::AlgorithmDisplay()
{
    const void* data[8] = { BinaryData::algorithm0_svg, BinaryData::algorithm1_svg, BinaryData::algorithm2_svg,
                            BinaryData::algorithm3_svg, BinaryData::algorithm4_svg, BinaryData::algorithm5_svg,
                            BinaryData::algorithm6_svg, BinaryData::algorithm7_svg };
    const int sizes[8] = { BinaryData::algorithm0_svgSize, BinaryData::algorithm1_svgSize, BinaryData::algorithm2_svgSize,
                           BinaryData::algorithm3_svgSize, BinaryData::algorithm4_svgSize, BinaryData::algorithm5_svgSize,
                           BinaryData::algorithm6_svgSize, BinaryData::algorithm7_svgSize };
    const void* fbData[8] = { BinaryData::algorithm0_fb_svg, BinaryData::algorithm1_fb_svg, BinaryData::algorithm2_fb_svg,
                              BinaryData::algorithm3_fb_svg, BinaryData::algorithm4_fb_svg, BinaryData::algorithm5_fb_svg,
                              BinaryData::algorithm6_fb_svg, BinaryData::algorithm7_fb_svg };
    const int fbSizes[8] = { BinaryData::algorithm0_fb_svgSize, BinaryData::algorithm1_fb_svgSize, BinaryData::algorithm2_fb_svgSize,
                             BinaryData::algorithm3_fb_svgSize, BinaryData::algorithm4_fb_svgSize, BinaryData::algorithm5_fb_svgSize,
                             BinaryData::algorithm6_fb_svgSize, BinaryData::algorithm7_fb_svgSize };
    for (size_t i = 0; i < diagrams.size(); ++i) {
        diagrams[i] = juce::Drawable::createFromImageData(data[i], static_cast<size_t>(sizes[i]));
        diagramsWithFb[i] = juce::Drawable::createFromImageData(fbData[i], static_cast<size_t>(fbSizes[i]));
    }
}

void AlgorithmDisplay::setAlgorithm(int algorithmNumber)
{
    CS_ASSERT_ALGORITHM(algorithmNumber);
    const int clamped = juce::jlimit(0, 7, algorithmNumber);
    if (clamped != currentAlgorithm) {
        currentAlgorithm = clamped;
        repaint();
    }
}

void AlgorithmDisplay::setFeedbackLevel(int feedbackLevel)
{
    CS_ASSERT_FEEDBACK(feedbackLevel);
    const int clamped = juce::jlimit(0, 7, feedbackLevel);
    if ((clamped > 0) != (currentFeedback > 0)) repaint();
    currentFeedback = clamped;
}

void AlgorithmDisplay::paint(juce::Graphics& g)
{
    auto& set = currentFeedback > 0 ? diagramsWithFb : diagrams;
    if (auto* drawable = set[static_cast<size_t>(currentAlgorithm)].get())
        drawable->drawWithin(g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
}
