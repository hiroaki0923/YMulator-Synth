#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class AlgorithmDisplay : public juce::Component
{
public:
    AlgorithmDisplay();
    ~AlgorithmDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setAlgorithm(int algorithmNumber);
    void setFeedbackLevel(int feedbackLevel);

private:
    int currentAlgorithm = 0;  // 0-7
    int currentFeedback = 0;   // 0-7
    
    // Operator positions and connections for each algorithm
    struct OperatorInfo {
        juce::Point<float> position;
        bool isCarrier;
        juce::String name;
    };
    
    struct Connection {
        int fromOp;
        int toOp;
        bool isFeedback = false;
    };
    
    std::array<OperatorInfo, 4> operators;
    std::vector<Connection> connections;
    
    void updateAlgorithmLayout();
    void drawOperator(juce::Graphics& g, const OperatorInfo& op, const juce::Rectangle<float>& bounds);
    void drawConnection(juce::Graphics& g, const Connection& conn, const juce::Rectangle<float>& bounds);
    void drawFeedbackLoop(juce::Graphics& g, int operatorIndex, const juce::Rectangle<float>& bounds);
    
    // Algorithm definitions (YM2151's 8 algorithms)
    void setupAlgorithm0(); // M1→M2→C1→C2 (complete series)
    void setupAlgorithm1(); // M1→C1, M2→C2 (two parallel chains)
    void setupAlgorithm2(); // M1→(C1+C2), M2→C2 (branch + parallel)
    void setupAlgorithm3(); // M1→C1, M2→C1, C2 (2 input 1 output + parallel)
    void setupAlgorithm4(); // M1→C1, M2, C2 (1 chain + 2 parallel)
    void setupAlgorithm5(); // M1→(C1+C2+M2) (1 input 3 output)
    void setupAlgorithm6(); // M1→(C1+M2), C2 (1 input 2 output + parallel)
    void setupAlgorithm7(); // M1, M2, C1, C2 (4 parallel outputs)
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmDisplay)
};