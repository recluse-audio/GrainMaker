/**
 * Granulator.cpp
 * Created by Ryan Devens
 */

#include "Granulator.h"

Granulator::Granulator()
{
}

Granulator::~Granulator()
{
}

//=======================================
void Granulator::prepare(double sampleRate, int blockSize, int maxGrainSize)
{
	mSampleRate = sampleRate;
	mBlockSize = blockSize;
	mMaxGrainSize = maxGrainSize;

	// Pre-allocate grain read buffer (2 * period for full grain)
	mGrainReadBuffer.setSize(2, maxGrainSize);
	mGrainReadBuffer.clear();

	// Prepare each grain's window and reset
	for (auto& grain : mGrains)
	{
		grain.prepare(static_cast<int>(sampleRate));
		grain.reset();
	}
}


//=======================================
void Granulator::processDetecting(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer,  
									std::tuple<int, int> dryBlockRange)
{
	_processActiveGrains(processBlock, circularBuffer, )
}

//=======================================
void Granulator::processTracking(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer,
				 		std::tuple<juce::int64, juce::int64> analysisReadRangeInSampleCount, 
						std::tuple<juce::int64, juce::int64> analysisWriteRangeInSampleCount,
						std::tuple<juce::int64, juce::int64> processCounterRange,
				  		float detectedPeriod,  float shiftedPeriod)
{
	// center of would be grain.
	juce::int64 analysisWriteMark = std::get<0>(analysisWriteRangeInSampleCount) + (juce::int64)(detectedPeriod);

	while(mSynthMark < analysisWriteMark)
	{
		// no previous marks 
		if(mSynthMark < 0)
		{
			mSynthMark = analysisWriteMark;
		}
		
		_makeGrain(circularBuffer, analysisReadRangeInSampleCount, analysisWriteRangeInSampleCount, detectedPeriod);

		mSynthMark = mSynthMark + (juce::int64)shiftedPeriod; // IMPORTANT TO USE SHIFTED HERE
	}



	// 
	// Process all active grains
	_processActiveGrains(processBlock, circularBuffer, processCounterRange);
}

//=======================================
int Granulator::_findInactiveGrainIndex()
{
	for (int i = 0; i < kNumGrains; ++i)
	{
		if (!mGrains[i].isActive)
			return i;
	}
	return -1; // No inactive grain available
}

//=======================================
void Granulator::_makeGrain(CircularBuffer& circularBuffer,
				 		std::tuple<juce::int64, juce::int64> analysisReadRangeInSampleCount, 
						std::tuple<juce::int64, juce::int64> analysisWriteRangeInSampleCount,
						float detectedPeriod)
{
	int grainIndex = _findInactiveGrainIndex();
	if (grainIndex < 0)
		return; // No available slot

	Grain& grain = mGrains[grainIndex];

	// Set up the grain
	grain.isActive = true;

	// Configure this grain's window
	int grainSize = static_cast<int>(detectedPeriod * 2.0f);
	grain.setWindowPeriod(grainSize);
}

//=======================================
void Granulator::_processActiveGrains(juce::AudioBuffer<float>& processBlock,  CircularBuffer& circularBuffer,
								std::tuple<juce::int64, juce::int64> processCounterRange)
{
	int numChannels = processBlock.getNumChannels();
	int blockSize = processBlock.getNumSamples();

	for (auto& grain : mGrains)
	{
		if (!grain.isActive)
			continue;

		juce::int64 grainSynthStart = std::get<0>(grain.mSynthRange);
		juce::int64 grainSynthEnd = std::get<1>(grain.mSynthRange);

		// Check if this grain overlaps with current block
		juce::int64 blockStart = std::get<0>(processCounterRange);
		juce::int64 blockEnd = std::get<1>(processCounterRange);

		if (grainSynthEnd < blockStart || grainSynthStart > blockEnd)
		{
			// Grain doesn't overlap with this block
			// If grain is completely in the past, deactivate it
			if (grainSynthEnd < blockStart)
			{
				grain.isActive = false;
			}
			continue;
		}

		// Calculate overlap region
		juce::int64 overlapStart = std::max(grainSynthStart, blockStart);
		juce::int64 overlapEnd = std::min(grainSynthEnd, blockEnd);

		// Read grain data from circular buffer
		juce::int64 grainAnalysisStart = std::get<0>(grain.mAnalysisRange);

		// Calculate window phase offset if grain started before this block
		int windowStartOffset = 0;
		if (grainSynthStart < blockStart)
		{
			windowStartOffset = static_cast<int>(blockStart - grainSynthStart);
		}

		// Set grain's window read position
		grain.setWindowPhaseOffset(windowStartOffset);

		// Process each sample in the overlap region
		for (juce::int64 absSample = overlapStart; absSample <= overlapEnd; ++absSample)
		{
			// Index within this process block
			int blockIndex = static_cast<int>(absSample - blockStart);

			// Index within the grain
			int grainIndex = static_cast<int>(absSample - grainSynthStart);

			// Corresponding analysis index (absolute)
			juce::int64 absAnalysisIndex = grainAnalysisStart + grainIndex;

			// Convert to circular buffer index
			int circIndex = circularBuffer.getWrappedIndex(absAnalysisIndex);

			// Get window value from this grain's window
			float windowValue = grain.getNextWindowSample();

			// Read from circular buffer and apply window
			for (int ch = 0; ch < numChannels; ++ch)
			{
				float sample = circularBuffer.getBuffer().getSample(ch, circIndex);
				float windowedSample = sample * windowValue;

				// Add to output (overlap-add)
				float currentSample = processBlock.getSample(ch, blockIndex);
				processBlock.setSample(ch, blockIndex, currentSample + windowedSample);
			}
		}

		// Deactivate grain if it's completely processed
		if (grainSynthEnd <= blockEnd)
		{
			grain.isActive = false;
		}
	}
}

//=======================================
void Granulator::_updateNextSynthStartIndex(float shiftedPeriod)
{
	mNextAbsSynthStartIndex += static_cast<juce::int64>(shiftedPeriod);
}
