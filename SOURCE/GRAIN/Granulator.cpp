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

	// Configure the shared window
	mWindow.setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kHanning, maxGrainSize);

	// Prepare each grain's buffer and reset
	for (auto& grain : mGrains)
	{
		grain.prepare(maxGrainSize, 2);
		grain.reset();
	}

	mSynthMark = -1;
}


//=======================================
void Granulator::processDetecting(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer,
									std::tuple<int, int> dryBlockRange, std::tuple<int, int> processCounterRange)
{
	// Read dry audio from circular buffer and write to processBlock
	int dryStart = std::get<0>(dryBlockRange);
	int dryEnd = std::get<1>(dryBlockRange);
	int numChannels = processBlock.getNumChannels();

	for (int sampleIndex = dryStart; sampleIndex < dryEnd; ++sampleIndex)
	{
		int blockIndex = sampleIndex - dryStart;
		int wrappedIndex = circularBuffer.getWrappedIndex(sampleIndex);

		for (int ch = 0; ch < numChannels; ++ch)
		{
			float sample = circularBuffer.getBuffer().getSample(ch, wrappedIndex);
			processBlock.setSample(ch, blockIndex, sample);
		}
	}

	// Process any active grains (overlap-add on top of dry signal)
	processActiveGrains(processBlock, processCounterRange);
}

//=======================================
void Granulator::processTracking(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer,
				 		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRangeInSampleCount,
						std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRangeInSampleCount,
						std::tuple<juce::int64, juce::int64> processCounterRange,
				  		float detectedPeriod,  float shiftedPeriod)
{
	juce::int64 currentAnalysisWriteMark = std::get<1>(analysisWriteRangeInSampleCount);
	juce::int64 nextAnalysisWriteMark = currentAnalysisWriteMark + (juce::int64)(detectedPeriod);

	// If mSynthMark has drifted too far ahead of currentAnalysisWriteMark (due to detection variance),
	// resync it to prevent grain emission from stalling. This can happen when the detected peak
	// is found slightly earlier than predicted, causing analysisWriteMark to be behind mSynthMark.
	if(mSynthMark > nextAnalysisWriteMark)
	{
		mSynthMark = currentAnalysisWriteMark;
	}

	// This while loop is intended to make a grain for every synth mark
	// between grainRange.start -> grainRange.mid. it is not "start -> end" because of overlap
	// mSynthMark matches analysisWriteMark when tracking starts, then increments by shifted period.
	// If shifting up we'd need 2 synth marks before we have the next analysis marker
	// CURRENT STRATEGY:
	// 	Use same analysisRange for grains who's synth marks are within same analysisWriteRange
	// 	[ currentGrainWriteStart ]-----------------[ currentGrainWriteMark(mid) ]------------------[ currentGrainWriteEnd ]
	while(mSynthMark < nextAnalysisWriteMark)
	{
		// no previous marks
		if(mSynthMark < 0)
		{
			mSynthMark = currentAnalysisWriteMark;
		}

		juce::int64 synthStart = mSynthMark - (juce::int64)detectedPeriod;
		juce::int64 synthEnd = mSynthMark + (juce::int64)detectedPeriod - 1;
		std::tuple<juce::int64, juce::int64, juce::int64> synthRangeInSampleCount = {synthStart, mSynthMark, synthEnd};

		makeGrain(circularBuffer, analysisReadRangeInSampleCount, synthRangeInSampleCount, detectedPeriod);

		mSynthMark = mSynthMark + (juce::int64)shiftedPeriod; // IMPORTANT TO USE SHIFTED HERE
	}



	// Process all active grains
	processActiveGrains(processBlock, processCounterRange);
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
void Granulator::makeGrain(CircularBuffer& circularBuffer,
				 		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange,
						std::tuple<juce::int64, juce::int64, juce::int64> synthRange,
						float detectedPeriod)
{
	int grainIndex = _findInactiveGrainIndex();
	if (grainIndex < 0)
		return; // No available slot

	Grain& grain = mGrains[grainIndex];

	// Configure window period for this grain size
	// Use same truncation as PluginProcessor::getAnalysisReadRange to avoid mismatch
	int grainSize = static_cast<int>(detectedPeriod) * 2;
	mWindow.setPeriod(grainSize);
	mWindow.resetReadPos();

	// Set up the grain
	grain.isActive = true;
	grain.mAnalysisRange = analysisReadRange;
	grain.mSynthRange = synthRange;

	int numChannels = circularBuffer.getNumChannels();
	juce::int64 readStart = std::get<0>(analysisReadRange);
	juce::int64 readEnd = std::get<2>(analysisReadRange);

	grain.mBuffer.clear();
	// Read from circular buffer, apply window, store in grain's buffer
	for (juce::int64 sampleCount = readStart; sampleCount <= readEnd; ++sampleCount)
	{
		int grainBufferIndex = static_cast<int>(sampleCount - readStart);
		int wrappedIndex = circularBuffer.getWrappedIndex(sampleCount);
		float windowValue = mWindow.getNextSample();

		for (int ch = 0; ch < numChannels; ++ch)
		{
			float sample = circularBuffer.getBuffer().getSample(ch, wrappedIndex);
			float windowedSample = sample * windowValue;
			grain.mBuffer.setSample(ch, grainBufferIndex, windowedSample);
		}
	}
}

//=======================================
void Granulator::processActiveGrains(juce::AudioBuffer<float>& processBlock,
								std::tuple<juce::int64, juce::int64> processCounterRange)
{
	int numChannels = processBlock.getNumChannels();
	juce::int64 blockStart = std::get<0>(processCounterRange);
	juce::int64 blockEnd = std::get<1>(processCounterRange);

	for (auto& grain : mGrains)
	{
		if (!grain.isActive)
			continue;

		juce::int64 synthStart = std::get<0>(grain.mSynthRange);
		juce::int64 synthEnd = std::get<2>(grain.mSynthRange);

		// Check if grain overlaps with current block
		if (synthEnd < blockStart || synthStart > blockEnd)
		{
			// Grain doesn't overlap with this block
			// If grain is completely in the past, deactivate it
			if (synthEnd < blockStart)
			{
				grain.isActive = false;
			}
			continue;
		}

		// Calculate overlap region
		juce::int64 overlapStart = std::max(synthStart, blockStart);
		juce::int64 overlapEnd = std::min(synthEnd, blockEnd);

		// Process each sample in the overlap region
		for (juce::int64 sampleCount = overlapStart; sampleCount <= overlapEnd; ++sampleCount)
		{
			// Index within this process block
			int blockIndex = static_cast<int>(sampleCount - blockStart);

			// Index within the grain's buffer
			int grainBufferIndex = static_cast<int>(sampleCount - synthStart);

			// Read from grain's pre-windowed buffer and add to output (overlap-add)
			for (int ch = 0; ch < numChannels; ++ch)
			{
				float grainSample = grain.mBuffer.getSample(ch, grainBufferIndex);
				float currentSample = processBlock.getSample(ch, blockIndex);
				processBlock.setSample(ch, blockIndex, currentSample + grainSample);
			}
		}

		// Deactivate grain if it's completely processed
		if (synthEnd <= blockEnd)
		{
			grain.isActive = false;
		}
	}
}

//=======================================
void Granulator::_updateNextSynthStartIndex(float shiftedPeriod)
{
	mSynthMark += static_cast<juce::int64>(shiftedPeriod);
}
