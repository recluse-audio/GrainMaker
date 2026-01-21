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

	// Main processing function - called each block
	void process(juce::AudioBuffer<float>& processBlock,
				 CircularBuffer& circularBuffer,
				 juce::int64 absSampleIndex,
				 juce::int64 delayedSampleIndex,
				 float detectedPeriod,
				 float shiftedPeriod);

private:
	friend class GranulatorTester;

	int mBlockSize = 0;
	int mMaxGrainSize = 0;
	double mSampleRate = 44100.0;

	std::array<Grain, kNumGrains> mGrains;

	// Tracks when to create the next grain
	juce::int64 mNextAbsSynthStartIndex = 0;

	// Pre-allocated buffer for reading grain data from circular buffer
	juce::AudioBuffer<float> mGrainReadBuffer;

	std::unique_ptr<Window> mWindow;

	// Find an inactive grain slot, returns -1 if none available
	int _findInactiveGrainIndex();

	// Create and activate a new grain
	void _makeGrain(CircularBuffer& circularBuffer,
					juce::int64 absSampleIndex,
					float detectedPeriod,
					float shiftedPeriod);

	// Process all active grains, writing to processBlock
	void _processActiveGrains(juce::AudioBuffer<float>& processBlock,
							  CircularBuffer& circularBuffer,
							  juce::int64 absSampleIndex);

	// Calculate next synthesis start index
	void _updateNextSynthStartIndex(float shiftedPeriod);
};
