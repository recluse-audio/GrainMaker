#pragma once

#include "Util/Juce_Header.h"

class PitchDetector;
class PitchMarkedCircularBuffer;
class GrainCorrector;
class CircularBuffer;
class Granulator;

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
                      , public juce::AudioProcessorValueTreeState::Listener
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
	
	//===================================
    const float getLastDetectedPitch();

    juce::AudioProcessorValueTreeState& getAPVTS();

   // AudioProcessorValueTreeState::Listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
	float mShiftRatio = 1.f;
    std::unique_ptr<PitchDetector> mPitchDetector;
    std::unique_ptr<PitchMarkedCircularBuffer> mPMCBuffer;
    std::unique_ptr<GrainCorrector> mGrainCorrector;
    std::unique_ptr<CircularBuffer> mCircularBuffer;
    std::unique_ptr<Granulator> mGranulator;

	juce::AudioBuffer<float> mLookaheadBuffer;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout _createParameterLayout();

    void _initParameterListeners();
    // cleanup ugly code in PluginProcessor's constructor
    juce::AudioProcessor::BusesProperties _getBusesProperties();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
