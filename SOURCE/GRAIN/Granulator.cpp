/**
 * Granulator.cpp
 * Created by Ryan Devens
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include "Granulator.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
	mNormWindowBuffer.setSize(1, blockSize); mNormWindowBuffer.clear();
	mWetBuffer.setSize(2, blockSize); mWetBuffer.clear();

	// Prepare each grain's buffer and reset
	for (auto& grain : mGrains)
	{
		grain.prepare(maxGrainSize, 2);
		grain.reset();
	}

	mSynthMark = -1;
	mCumulativePhase = 0.0;
}


//=======================================
void Granulator::processDetecting(juce::AudioBuffer<float>& processBlock, CircularBuffer& circularBuffer,
									std::tuple<int, int> dryBlockRange, std::tuple<int, int> processCounterRange)
{
// {
// 	// Read dry audio from circular buffer and write to processBlock
// 	int dryStart = std::get<0>(dryBlockRange);
// 	int dryEnd = std::get<1>(dryBlockRange);
// 	int numChannels = processBlock.getNumChannels();

// 	for (int sampleIndex = dryStart; sampleIndex < dryEnd; ++sampleIndex)
// 	{
// 		int blockIndex = sampleIndex - dryStart;
// 		int wrappedIndex = circularBuffer.getWrappedIndex(sampleIndex);

// 		for (int ch = 0; ch < numChannels; ++ch)
// 		{
// 			float sample = circularBuffer.getBuffer().getSample(ch, wrappedIndex);
// 			processBlock.setSample(ch, blockIndex, sample);
// 		}
// 	}

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
		// no previous marks - initialize tracking
		if(mSynthMark < 0)
		{
			mSynthMark = currentAnalysisWriteMark;
			mCumulativePhase = 0.0; // Reset phase when tracking starts
		}

		juce::int64 synthStart = mSynthMark - (juce::int64)detectedPeriod;
		juce::int64 synthEnd = mSynthMark + (juce::int64)detectedPeriod - 1;
		std::tuple<juce::int64, juce::int64, juce::int64> synthRangeInSampleCount = {synthStart, mSynthMark, synthEnd};

		makeGrain(circularBuffer, analysisReadRangeInSampleCount, synthRangeInSampleCount, detectedPeriod, shiftedPeriod);

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

void Granulator::makeGrain(
    CircularBuffer& circularBuffer,
    std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange,
    std::tuple<juce::int64, juce::int64, juce::int64> synthRange,
    float detectedPeriod,
    float /*shiftedPeriod*/)
{
    const int grainIndex = _findInactiveGrainIndex();
    if (grainIndex < 0)
        return;

    Grain& grain = mGrains[grainIndex];

    const int period    = (int)std::llround(detectedPeriod);
    const int grainSize = period * 2;

    // Window must match the grain buffer length.
    mWindow.setPeriod(grainSize);
    mWindow.resetReadPos();

    grain.isActive = true;
    grain.mAnalysisRange = analysisReadRange;
    grain.mSynthRange = synthRange;
	grain.mGrainSize = grainSize;

    const int numChannels = circularBuffer.getNumChannels();

    const juce::int64 readStart = std::get<0>(analysisReadRange);
    const juce::int64 readEndExpected = readStart + (juce::int64)grainSize - 1;

    // Assumption for TD-PSOLA: analysisReadRange spans exactly 2*period samples and is centered
    // on the pitch mark (analysisRange.mark). We do NOT phase-rotate reads per grain.
    (void)readEndExpected; // remove if you add an assert/log

    grain.mBuffer.clear();

    for (int i = 0; i < grainSize; ++i)
    {
        const juce::int64 readPos = readStart + (juce::int64)i;
        const int wrappedIndex = circularBuffer.getWrappedIndex(readPos);
        const float w = mWindow.getNextSample();

		grain.mWindowBuffer.setSample(0, i, w);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float s = circularBuffer.getBuffer().getSample(ch, wrappedIndex);
            grain.mBuffer.setSample(ch, i, s * w);
        }
    }

    // IMPORTANT:
    // Pitch shifting happens because synth marks advance by shiftedPeriod elsewhere (mSynthMark += shiftedPeriod),
    // while analysis marks advance by detectedPeriod. Do not add a per-grain read offset here.
}


//=======================================
void Granulator::processActiveGrains(juce::AudioBuffer<float>& processBlock, std::tuple<juce::int64, juce::int64> processCounterRange)
{
	int numChannels = processBlock.getNumChannels();
	juce::int64 blockStart = std::get<0>(processCounterRange);
	juce::int64 blockEnd = std::get<1>(processCounterRange);
	mNormWindowBuffer.clear();
	mWetBuffer.clear();

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
			float windowVal = grain.mWindowBuffer.getSample(0, grainBufferIndex);
			mNormWindowBuffer.addSample(0, blockIndex, windowVal);
			// Read from grain's pre-windowed buffer and add to output (overlap-add)
			for (int ch = 0; ch < numChannels; ++ch)
			{
				float grainSample = grain.mBuffer.getSample(ch, grainBufferIndex);
				mWetBuffer.addSample(ch, blockIndex, grainSample);
			}
		}

		// Deactivate grain if it's completely processed
		if (synthEnd <= blockEnd)
		{
			grain.isActive = false;
		}
	}

	
	for (int ch = 0; ch < numChannels; ++ch)
	{
		for (int i = 0; i < processBlock.getNumSamples(); ++i)
		{
			float normalizedWindowSample = mNormWindowBuffer.getSample(0, i);
			if (normalizedWindowSample > 1.0e-6f)
				processBlock.setSample(ch, i, mWetBuffer.getSample(ch, i) / normalizedWindowSample);
		}
	}

}

//=======================================
void Granulator::_updateNextSynthStartIndex(float shiftedPeriod)
{
	mSynthMark += static_cast<juce::int64>(shiftedPeriod);
}
