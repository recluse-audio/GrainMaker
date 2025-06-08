/**
 * @file Granulator.h
 * @author Ryan Devens
 * @brief 
 * @version 0.1
 * @date 2025-03-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */


 #pragma once
 #include "Util/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"

 /**
  * @brief You pump in audio to this class, it chops it up into grains.
  * 
  * It does not store the audio data, but keeps track of grain position and windows accordingly.
  * 
  * 
  * 
  */
 class Granulator
 {
public:
    Granulator();
    ~Granulator();


    void setGrainShape(Window::Shape newShape);

    void prepare(double sampleRate);
    const double getCurrentSampleRate();

    void setGrainLengthInMs(double lengthInMs);
    void setGrainLengthInSamples(int numSamples);
    const int getGrainLengthInSamples();

    void setEmissionRateInHz(double rateInHz);
    void setEmissionPeriodInSamples(int numSamples);
    const int getEmissionPeriodInSamples();

	void setPhaseOffset(float phaseOffset);
	
    void process(juce::AudioBuffer<float>& buffer);

	void processShifting(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& outputBuffer, float detectedPeriod, float shiftRatio);


private:
	friend class GranulatorTester;
    double mSampleRate = -1;
    Window mWindow;

	/** 
	 * @brief These Ranges get used ever processBlock so it made sense to make them member variables
	 */
	struct GrainData
	{
		// Range of lookaheadBuffer
		juce::Range<juce::int64> mSourceRange	 { 0, 0 };
		// Range in lookaheadBuffer that is considered the current "grain" of audio data that will be shifted if necessary
		juce::Range<juce::int64> mFullGrainRange { 0, 0 };
		// Range for portion of mFullGrainRange that is within lookahead
		juce::Range<juce::int64> mClippedGrainRange { 0, 0 };
		// Where the grain would be after shift offset (lookahead offset not applied yet)
		juce::Range<juce::int64> mShiftedRange   { 0, 0 };
		// Range of mGrainRange that will be read to write to outputBuffer (might not read/write entire grain)
		juce::Range<juce::int64> mReadRange      { 0, 0 };
		// Would mShiftedRange be written to outputBuffer after considering the lookahead num samples
		juce::Range<juce::int64> mWriteRange     { 0, 0 };
	};
	GrainData mCurrentGrainData;

	
    int mGrainLengthInSamples = -1; // length of audio segment being windowed
    /**
     * @brief Length between one grain emission and the next.
     * Can be longer, shorter that grain length itself.
     * 
     * If this is longer that the grain it will result in a tremolo like effect, adding silence between audible sounds
     * 
     */
    int mEmissionPeriodInSamples = -1; // length between one grain emission and the next.  

    int mCurrentPhaseIndex = 0; // Index within grain emission
    void _incrementPhase();

	// accounts for current mPhaseIncrement and finds the value from the internal buffer
	float _getWindowSampleAtIndexInPeriod(int indexInPeriod, float period);

	/** Returns portion of a grain's range that will end up being written to outputBuffer (may be partial), this doesn't write, but tells us what to write and where  */
	juce::Range<juce::int64> _getGrainRangeOverlappingOutput(juce::Range<juce::int64> rangeInLookahead, juce::Range<juce::int64> totalOutputRange, float shiftOffset);

	// Number of samples to shift grains based on its period and shift ratio.
	// rounds float values down and returns juce::int64
	juce::int64 _calculatePitchShiftOffset(float detectedPeriod, float shiftRatio);

	// Float indices get converted to juce::int64 in these functions
	// fix or optimize them here and don't worry about chasing them down all over the repo
	void _updateSourceRange(const juce::AudioBuffer<float>& lookaheadBuffer);
	void _updateFullGrainRange(float startIndex, float detectedPeriod);
	void _updateClippedGrainRange();
	void _updateShiftRange(float detectedPeriod, float shiftRatio);
	void _updateReadRange();
	// Would mShiftedRange be written to outputBuffer after considering the lookahead num samples
	void _updateWriteRange();
};