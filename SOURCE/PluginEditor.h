#pragma once
#include "PluginProcessor.h"


//==============================================================================
class PluginEditor  : public juce::AudioProcessorEditor
                    , public juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // 
	void timerCallback() override;

private:
	PluginProcessor& mProcessor;

    std::unique_ptr<juce::Label> mPitchDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
