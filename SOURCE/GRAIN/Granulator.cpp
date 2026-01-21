/**
 * Granulator.cpp
 * Created by Ryan Devens
 */

#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"

Granulator::Granulator()
{
	mWindow = std::make_unique<Window>();
}

Granulator::~Granulator()
{
	mWindow.reset();
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

	// Reset all grains
	for (auto& grain : mGrains)
	{
		grain.reset();
	}

	mNextAbsSynthStartIndex = 0;

	// Configure window
	mWindow->setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kHanning, maxGrainSize);
	mWindow->setLooping(true);
}

//=======================================
void Granulator::process(juce::AudioBuffer<float>& processBlock,
						 CircularBuffer& circularBuffer,
						 AnalysisMarker& analysisMarker,
						 juce::int64 absSampleIndex,
						 float detectedPeriod,
						 float shiftedPeriod)
{
	// Check if we need to create a new grain
	// if the absSampleIndex is far enough along that we will need the next
	// analysis window to make the next expected grain.
	// However, don't make any new ones if no period was detected (less than 2)
	if (absSampleIndex >= mNextAbsSynthStartIndex && detectedPeriod >= 2.f)
	{
		_makeGrain(circularBuffer, analysisMarker, absSampleIndex, detectedPeriod, shiftedPeriod);
		_updateNextSynthStartIndex(shiftedPeriod);
	}

	// Process all active grains
	_processActiveGrains(processBlock, circularBuffer, absSampleIndex);
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
							juce::int64 absSampleIndex,
							float detectedPeriod,
							float shiftedPeriod)
{
	int grainIndex = _findInactiveGrainIndex();
	if (grainIndex < 0)
		return; // No available slot

	Grain& grain = mGrains[grainIndex];

	// Get analysis mark from AnalysisMarker
	juce::int64 absAnalysisMark = analysisMarker.getNextAnalysisMarkIndex(circularBuffer, detectedPeriod, absSampleIndex);

	// Set up the grain
	grain.isActive = true;
	grain.mAbsAnalysisMarkIndex = absAnalysisMark;
	grain.mAbsSynthesisMarkIndex = mNextAbsSynthStartIndex;
	grain.detectedPeriod = detectedPeriod;

	// Update window period for this grain
	int grainSize = static_cast<int>(detectedPeriod * 2.0f);
	mWindow->setPeriod(grainSize);
}

//=======================================
void Granulator::_processActiveGrains(juce::AudioBuffer<float>& processBlock,
									  CircularBuffer& circularBuffer,
									  juce::int64 absSampleIndex)
{
	int numChannels = processBlock.getNumChannels();
	int blockSize = processBlock.getNumSamples();

	for (auto& grain : mGrains)
	{
		if (!grain.isActive)
			continue;

		int grainSize = static_cast<int>(grain.detectedPeriod * 2.0f);
		juce::int64 grainSynthStart = grain.mAbsSynthesisMarkIndex - static_cast<juce::int64>(grain.detectedPeriod);
		juce::int64 grainSynthEnd = grainSynthStart + grainSize - 1;

		// Check if this grain overlaps with current block
		juce::int64 blockStart = absSampleIndex;
		juce::int64 blockEnd = absSampleIndex + blockSize - 1;

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
		juce::int64 grainAnalysisStart = grain.mAbsAnalysisMarkIndex - static_cast<juce::int64>(grain.detectedPeriod);
		int circBufferSize = circularBuffer.getSize();

		// Calculate window phase offset if grain started before this block
		int windowStartOffset = 0;
		if (grainSynthStart < blockStart)
		{
			windowStartOffset = static_cast<int>(blockStart - grainSynthStart);
		}

		// Set window read position
		double phaseIncrement = static_cast<double>(mWindow->getSize()) / static_cast<double>(mWindow->getPeriod());
		mWindow->setReadPos(windowStartOffset * phaseIncrement);

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
			int circIndex = static_cast<int>(absAnalysisIndex % circBufferSize);
			if (circIndex < 0) circIndex += circBufferSize;

			// Get window value
			float windowValue = mWindow->getNextSample();

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
