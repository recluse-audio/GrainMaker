/**
 * Granulator.h
 * Created by Ryan Devens
 *
 * Performs granular synthesis for TD-PSOLA pitch shifting.
 * Uses a CircularBuffer reference and manages an array of active Grains.
 */

#pragma once
#include "../Util/Juce_Header.h"
#include "Grain.h"
#include "AnalysisMarker.h"
#include "../SUBMODULES/RD/SOURCE/CircularBuffer.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include <array>

static constexpr int kNumGrains = 4;

class Granulator
{
public:
	Granulator();
	~Granulator();

	void prepare(double sampleRate, int blockSize, int maxGrainSize);

	// no pitch being tracked, so we pop the dry block and write it. We also write current active grains.
	// don't make any new grains though
	void processDetecting(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer, 
							std::tuple<int, int> delayedDryBlockRange, std::tuple<int, int> processCounterRange );

	// This is called when a pitch was detected and we are technically "tracking"
	// this has the ability to make new grains unlike processDetecting()
	// `analysisRange`: Two cycles of detected pitch, delayed by lookahead time, to be read from circular buffer
	// `synthMark`: Center of 2cycle grain taken from circular buffer (NOT DELAYED)
	void processTracking(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer,
				 		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRangeInSampleCount, 
						std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRangeInSampleCount,
						std::tuple<juce::int64, juce::int64> processCounterRange,
				  		float detectedPeriod,  float shiftedPeriod);

	std::array<Grain, kNumGrains>& getGrains() { return mGrains; }
	juce::int64 getSynthMark() const { return mSynthMark; }
	void resetSynthMark() { mSynthMark = -1; }
	Window& getWindow() { return mWindow; }

	// Create and activate a new grain
	void makeGrain(CircularBuffer& circularBuffer,
				   std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange,
				   std::tuple<juce::int64, juce::int64, juce::int64> synthRange,
				   float detectedPeriod);

	// Process all active grains, writing to processBlock
	void processActiveGrains(juce::AudioBuffer<float>& processBlock,
							 std::tuple<juce::int64, juce::int64> processCounterRange);

private:
	friend class GranulatorTester;
	Window mWindow;

	std::array<Grain, kNumGrains> mGrains;

	// Tracks when to create the next grain
	juce::int64 mSynthMark = -1;

	// Find an inactive grain slot, returns -1 if none available
	int _findInactiveGrainIndex();
	
	// Calculate next synthesis start index
	void _updateNextSynthStartIndex(float shiftedPeriod);
};
