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
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"


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
    void prepare(double sampleRate, int blockSize);
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

	// partial grains extending beyond the bounds of the outputBuffer get written here, in the next block they are written at the start
	juce::AudioBuffer<float> mSpilloverBuffer;

	// spillover is larger than even the longest grain, so track how far the spillover grains actually go into the next outputBuffer.
	// This is the end position of the last grain that spilled over from the previous processBlock
	juce::int64 mSpilloverLength = 0; 
	/** 
	 * @brief These Ranges get used ever processBlock so it made sense to make them member variables
	 */
	struct GrainData
	{
		// Range of lookaheadBuffer
		RD::BufferRange mSourceRange	 { 0, 0 };
		// Range of outputBuffer, 0 -> outputBuffer.getNumSamples()
		RD::BufferRange mOutputRange 	{ 0, 0 };
		//
		RD::BufferRange mOutputRangeInSource { 0, 0 };
		// Range of outputBuffer relative to lookaheadBuffer, with no shifiting it should start at mLookaheadSamples and end at mSourceRange.getEnd()
		RD::BufferRange mSourceRangeNeededForShifting 	{ 0, 0 };
		// Range in lookaheadBuffer that is considered the current "grain" of audio data that will be shifted if necessary
		RD::BufferRange mFullGrainRange { 0, 0 };
		// Range for portion of mFullGrainRange that is within lookahead
		RD::BufferRange mClippedGrainRange { 0, 0 };
		// Where the grain would be after shift offset (lookahead offset not applied yet)
		RD::BufferRange mShiftedRange   { 0, 0 };
		// Would mShiftedRange be written to outputBuffer after considering the lookahead num samples
		RD::BufferRange mGrainWriteRange     { 0, 0 };
		// Range of mGrainRange that will be read to write to outputBuffer (might not read/write entire grain)
		RD::BufferRange mGrainReadRange      { 0, 0 };


		double mNumGrainsToOutput = 0.0;
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
	RD::BufferRange _getGrainRangeOverlappingOutput(RD::BufferRange rangeInLookahead, RD::BufferRange totalOutputRange, float shiftOffset);

	// Number of samples to shift grains based on its period and shift ratio.
	// rounds float values down and returns juce::int64
	juce::int64 _calculatePitchShiftOffset(float detectedPeriod, float shiftRatio);

	juce::int64 _calculateFirstIndexToOutput(float shiftRatio);

	// the first two range args are used to calculate then modify the second two
	void _getWriteRangeInOutputAndSpillover(const RD::BufferRange& wholeGrainRange,
											const RD::BufferRange& outputRange,
											RD::BufferRange& grainRangeForOutputBuffer,
											RD::BufferRange& grainRangeForSpilloverBuffer,
											RD::BufferRange& outputBufferWriteRange,
											RD::BufferRange& spilloverBufferWriteRange );

	// Float indices get converted to juce::int64 in these functions
	// fix or optimize them here and don't worry about chasing them down all over the repo

	
	/* These update every processBlock() */
	void _updateSourceRange(const juce::AudioBuffer<float>& lookaheadBuffer);
	void _updateOutputRange(const juce::AudioBuffer<float>& outputBuffer);
	void _updateNumGrainsToOutput(float detectedPeriod, float shiftRatio);

	// take the fractional num grains needed for pitch shifting and add one to be sure we have enough data
	void _getSourceRangeNeededForNumGrains(int numGrains, float detectedPeriod
											, const RD::BufferRange& sourceRange
											, RD::BufferRange& sourceRangeForShifting);
	/*----- end of functions that are per-block -----------*/

	/*---------- These are done once per grain ------------*/
	void _updateFullGrainRange(float startIndex, float detectedPeriod);
	void _updateClippedGrainRange();
	void _updateShiftedRange(float detectedPeriod, float shiftRatio);
	void _updateGrainReadRange(RD::BufferRange& readRange, const RD::BufferRange& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod);
	void _updateGrainWriteRange(RD::BufferRange& writeRange, const RD::BufferRange& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting);

	/*-------- end of functions that are per-grain -----------*/

	void _applyWindowToFullGrain(juce::dsp::AudioBlock<float>& block);
	// this is for the last partial grain that most likely won't finish the window resulting in sharp dropout.
	// instead fade out quickly
	void _applyWindowToPartialGrain(juce::dsp::AudioBlock<float>& block);

	juce::int64 mOffsetFromSpillover = 0;  // in samples; carries over from block to block
	float mPreviousPeriod = 0.f;
};