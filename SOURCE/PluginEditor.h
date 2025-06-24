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
    std::unique_ptr<juce::Label> mVersionLabel;

    std::unique_ptr<juce::Label> mPitchDisplayLabel;
    std::unique_ptr<juce::Label> mShiftRatioLabel;
    std::unique_ptr<juce::Label> mEmissionRateLabel;

    std::unique_ptr<juce::Label> mPitchDisplay;

    std::unique_ptr<juce::Slider> mPitchShiftSlider;
    std::unique_ptr<juce::Slider> mEmissionRateSlider;


    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchShiftAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mEmissionRateAttachment;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
