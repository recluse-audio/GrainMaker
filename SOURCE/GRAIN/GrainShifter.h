/**
 * @file GrainShifter.h
 * @author Ryan Devens
 * @brief 
 * @version 0.1
 * @date 2025-04-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once
#include "Util/Juce_Header.h"
#include "GrainBuffer.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"

 /**
  * @brief 
  * 
  */
 class GrainShifter
 {
public:
	GrainShifter();
	~GrainShifter();

	//
    void setGrainShape(Window::Shape newShape);

	//
    void prepare(double sampleRate, int lookaheadBufferNumSamples);

	// won't die if buffers are stupidly sized, but will simply write 0's
	void processShifting(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& outputBuffer, float detectedPeriod, float shiftRatio);

	Window& getGrainWindow();

	// TODO: These should probably be private functions but for now I'm making them public for temp dev work.
	// once solidifed I will use a script / AI to convert this to private and add to GrainShifterTester / test_GrainShifter.cpp
private:
	friend class GrainShifterTester;
    double mSampleRate = 44100;
    Window mWindow;

	// Array of 2 GrainBuffers for double-buffering grain processing
	GrainBuffer mGrainBuffers[2];

	// This will be the read index for both grain buffers, when it is larger than the current grain buffers length in samples (after shifting), we know to switch to the other buffer
	juce::int64 mGrainReadIndex = 0;

	// Index of the currently active grain buffer (0 or 1)
	int mActiveGrainBufferIndex = 0;

	// partial grains extending beyond the bounds of the outputBuffer get written here, in the next block they are written at the start
	juce::AudioBuffer<float> mGrainProcessingBuffer;
	double mGrainPhaseIndexInRadians = 0.0;
	bool mShouldGranulateLookaheadBuffer = true;
	// spillover is larger than even the longest grain, so track how far the spillover grains actually go into the next outputBuffer.
	// This is the end position of the last grain that spilled over from the previous processBlock
	juce::int64 mSpilloverLength = 0; 

	juce::int64 _calculateGrainSamplesRemainingForGivenPhase(double phaseInRadians, float detectedPeriod);

	// Reads the next grain sample from the current position for the given channel
	float _readNextGrainSample(int channel);

	// Increments the grain read index and switches buffers if needed
	void _incrementGrainReadIndex();

	// The start sample index of the first grain is not always [0]. If the final whole grain from the prev processBlock would
	// extend past the end of the prev processBlock then it needs to "spillover" into the next buffer. 
	// That means the audio data for those indices written to in the spillover at the start of process().
	// Therefore, we sometimes offset into the outputBuffer for the first index to write to. 
	juce::int64 _calculateFirstGrainStartingPos(juce::int64 prevShiftedPeriod, const RD::BufferRange& prevOutputRange, const RD::BufferRange& prevGrainWriteRange );

	/* returns the number of grains that would write at least 1 sample to the outputBuffer, the last one probably partial/spillover */
	// at very small block sizes this may be less than 1, in which case we round up to 1 and figure out which portion to write later.
	int _calculateNumGrainsToOutput(float detectedPeriod, float shiftRatio, const RD::BufferRange& outputRange, const juce::int64 firstGrainStartPos);

	// take the fractional num grains needed for pitch shifting and add one to be sure we have enough data
	void _updateSourceRangeNeededForNumGrains(int numGrains, float detectedPeriod, const RD::BufferRange& sourceRange, RD::BufferRange& rangeNeeded);

	juce::int64 _calculatePeriodAfterShifting(float detectedPeriod, float shiftRatio);
	void _updateGrainReadRange(RD::BufferRange& readRange, const RD::BufferRange& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod);
	void _updateGrainWriteRange(RD::BufferRange& writeRange, const RD::BufferRange& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting);
	void _applyWindowToFullGrain(juce::dsp::AudioBlock<float>& block);
	// accounts for current mPhaseIncrement and finds the value from the internal buffer
	float _getWindowSampleAtIndexInPeriod(int indexInPeriod, float period);

	// keep track of the range of the mBuffer from previous block that should be written to current block as part of "spillover"
	struct PreviousBlockData
	{
		RD::BufferRange spilloverRangeInFullGrainBuffer = { 0, 0 };
		float shiftedRatio = 1.f;		
		juce::int64 finalGrainStartIndex = 0;

		void reset()
		{
			spilloverRangeInFullGrainBuffer.setStartIndex(0);
			spilloverRangeInFullGrainBuffer.setEndIndex(0);
			float shiftRatio = 1.f;
			float finalGrainStartIndex = 0;
		}
	};

	PreviousBlockData mPreviousBlockData;
	// writes fullGrainBuffer to outputBuffer and returns the range that would "spill over"
	void _writeGrainBufferToOutput(const juce::AudioBuffer<float>& fullGrainBuffer, juce::AudioBuffer<float>& outputBuffer, PreviousBlockData& prevData);

	// 
	void _writeGrainBufferSpilloverToOutput(const juce::AudioBuffer<float>& fullGrainBuffer, juce::AudioBuffer<float>& outputBuffer, const PreviousBlockData& previousBlockData);


 };