#pragma once

#include "Util/Juce_Header.h"

class CircularBuffer;
class PitchDetector;
class Granulator;
class AnalysisMarker;
class Window;

#if (MSVC)
#include "ipps.h"
#endif

namespace MagicNumbers
{
	constexpr int minLookaheadSize = 512; // for synthesis
    constexpr int minDetectionSize = 1024; // for detection
} // end namespace MagicNumbers
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

    // range of current process block relative to total num processed, no delay
    std::tuple<juce::int64, juce::int64> getProcessCounterRange();
    
    // starts at delayed position behind process counter range
    std::tuple<juce::int64, juce::int64> getDetectionRange();

    // when we've detected a pitch, this is the range of a complete cycle nearest the end of the detection buffer
    std::tuple<juce::int64, juce::int64> getFirstPeakRange(float detectedPeriod);

    std::tuple<juce::int64, juce::int64> getPrecisePeakRange(juce::int64 predictedAnalysisMark, float detectedPeriod);

    std::tuple<juce::int64, juce::int64, juce::int64> getAnalysisReadRange(juce::int64 analysisMark, float detectedPeriod);

    // same as analysisReadRange but offset by minLookaheadSize (undelayed write position)
    std::tuple<juce::int64, juce::int64, juce::int64> getAnalysisWriteRange(std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange);

    // happens when no pitch is detected and we want to let dry signal back through, but still delayed
    std::tuple<juce::int64, juce::int64> getDryBlockRange();



    enum class ProcessState
    {
        kDetecting = 0, // looking for a pitch (1st detection, noise b4)
        kTracking = 1 // tracking a pitch (after atleast 1 detection)
    };

    ProcessState getCurrentState() { return mProcessState; }

    private:
	ProcessState mProcessState = ProcessState::kDetecting;

    float mShiftRatio = 1.f;
    std::unique_ptr<PitchDetector> mPitchDetector;
    std::unique_ptr<Granulator> mGranulator;
    std::unique_ptr<CircularBuffer> mCircularBuffer;
	std::unique_ptr<AnalysisMarker> mAnalysisMarker;

	juce::AudioBuffer<float> mDetectionBuffer;

	juce::int64 mSamplesProcessed = 0;
	int mBlockSize = 0;
    juce::int64 mPredictedNextAnalysisMark = (juce::int64) -1;


    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout _createParameterLayout();

    void _initParameterListeners();
    // cleanup ugly code in PluginProcessor's constructor
    juce::AudioProcessor::BusesProperties _getBusesProperties();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
