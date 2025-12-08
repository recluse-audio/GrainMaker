#include "GrainShifter.h"
#include "../../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "Granulator.h"

GrainShifter::GrainShifter()
{
	mWindow.setShape(Window::Shape::kHanning);
	mGrainProcessingBuffer.clear();

	// Initialize both GrainBuffers to 2 channels
	for (int i = 0; i < 2; ++i)
	{
		mGrainBuffers[i].getBufferReference().setSize(2, 0);
	}
}

GrainShifter::~GrainShifter()
{
}

void GrainShifter::prepare(double sampleRate, int lookaheadBufferNumSamples)
{
	mSampleRate = sampleRate;
	mGrainProcessingBuffer.setSize(2, lookaheadBufferNumSamples);
	mWindow.setSize(lookaheadBufferNumSamples);

	// Set both GrainBuffers to 2 channels and lookaheadBufferNumSamples length
	for (int i = 0; i < 2; ++i)
	{
		mGrainBuffers[i].getBufferReference().setSize(2, lookaheadBufferNumSamples);
		mGrainBuffers[i].setLengthInSamples(lookaheadBufferNumSamples);
	}
}

void GrainShifter::setGrainShape(Window::Shape newShape)
{
	mWindow.setShape(newShape);
}

Window& GrainShifter::getGrainWindow()
{
	return mWindow;
}

void GrainShifter::processShifting(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& outputBuffer, float detectedPeriod, float shiftRatio)
{
	if(detectedPeriod < 50 || shiftRatio < 0.5 || shiftRatio > 1.5)
		return;
		
	float periodAfterShifting = detectedPeriod * shiftRatio;
	Granulator::granulateBuffer(lookaheadBuffer, outputBuffer, detectedPeriod, periodAfterShifting, mWindow, true);
}




//=======================================================
//================ PRIVATE ==============================
//=======================================================

//===============
juce::int64 GrainShifter::_calculateGrainSamplesRemainingForGivenPhase(double phaseInRadians, float detectedPeriod)
{
	// Normalize phase to [0, 2π) range
	double normalizedPhase = std::fmod(phaseInRadians, juce::MathConstants<double>::twoPi);
	if (normalizedPhase < 0.0)
		normalizedPhase += juce::MathConstants<double>::twoPi;

	// Calculate fraction of period completed (0.0 = start, 1.0 = end)
	double fractionCompleted = normalizedPhase / juce::MathConstants<double>::twoPi;

	// Calculate samples remaining from current phase position to end of period
	double samplesRemaining = detectedPeriod * (1.0 - fractionCompleted);

	// Round to nearest integer and return
	juce::int64 grainSamplesRemaining = static_cast<juce::int64>(std::round(samplesRemaining));

	return grainSamplesRemaining;
}

//========================
float GrainShifter::_readNextGrainSample(int channel)
{
	// Get the currently active grain buffer
	GrainBuffer& activeBuffer = mGrainBuffers[mActiveGrainBufferIndex];
	juce::int64 bufferLength = activeBuffer.getLengthInSamples();

	// Read the sample from the active buffer at the current index
	float sample = 0.0f;
	if (bufferLength > 0 && mGrainReadIndex < bufferLength)
	{
		sample = activeBuffer.getBufferReference().getSample(channel, static_cast<int>(mGrainReadIndex));
	}

	// Note: The index increment and wrapping should be done once per sample,
	// not per channel. This should be handled by the calling function after
	// all channels have been read for the current sample position.

	return sample;
}

//========================
void GrainShifter::_incrementGrainReadIndex()
{
	// Get the currently active grain buffer's length
	juce::int64 bufferLength = mGrainBuffers[mActiveGrainBufferIndex].getLengthInSamples();

	// Increment the read index
	mGrainReadIndex++;

	// Check if we need to wrap around and switch buffers
	if (mGrainReadIndex >= bufferLength)
	{
		// Reset index to 0
		mGrainReadIndex = 0;

		// Switch to the other buffer
		mActiveGrainBufferIndex = (mActiveGrainBufferIndex == 0) ? 1 : 0;
	}
}

//========================
juce::int64 GrainShifter::_calculateFirstGrainStartingPos(juce::int64 prevShiftedPeriod, const RD::BufferRange& prevOutputRange, const RD::BufferRange& prevGrainWriteRange )
{

	juce::int64 startPos = 0;

	// adjust for proper incrementing in buffer ranges
	juce::int64 adjustedPeriod = prevShiftedPeriod - (juce::int64)1;

	juce::int64 startInPrevOutput = prevGrainWriteRange.getStartIndex();
	juce::int64 endInPrevOutput = prevGrainWriteRange.getEndIndex();

	juce::int64 prevOutputLength = prevOutputRange.getLengthInSamples();

	// if last end on the very last sample (unlikely)
	if(startInPrevOutput + adjustedPeriod == prevOutputRange.getEndIndex() + 1)
	{
		startPos = 0;
	}
	else
	{
		startPos = (startInPrevOutput + adjustedPeriod) - prevOutputRange.getLengthInSamples();
	}

	return startPos;

}

int GrainShifter::_calculateNumGrainsToOutput(float detectedPeriod, float shiftRatio, const RD::BufferRange& outputRange, const juce::int64 firstGrainStartPos)
{
	float outputRangeAfterSpillover = (float)outputRange.getLengthInSamples() - (float)firstGrainStartPos;
	float noShiftNumGrains = outputRangeAfterSpillover / detectedPeriod;
	float numGrainsAfterShifting = noShiftNumGrains * shiftRatio;

	// round up, we have spill over
	if(numGrainsAfterShifting > (int)numGrainsAfterShifting)
		numGrainsAfterShifting += 1.f; 
		
	return (int)numGrainsAfterShifting;
}

void GrainShifter::_updateSourceRangeNeededForNumGrains(int numGrains, float detectedPeriod, const RD::BufferRange& sourceRange, RD::BufferRange& rangeNeeded)
{
	juce::int64 numSamplesNeeded = (float)numGrains * detectedPeriod;
	juce::int64 indexLengthOfSamples = numSamplesNeeded - 1; // if you need one sample, technically. you need 0 length of index meaning just one index
	juce::int64 sampleStart = sourceRange.getEndIndex() - indexLengthOfSamples;
	
	if(sampleStart < sourceRange.getStartIndex())
		sampleStart = sourceRange.getStartIndex();
	
	rangeNeeded.setStartIndex(sampleStart);
	rangeNeeded.setEndIndex(sourceRange.getEndIndex());
}

juce::int64 GrainShifter::_calculatePeriodAfterShifting(float detectedPeriod, float shiftRatio)
{
    return detectedPeriod;
}

void GrainShifter::_updateGrainReadRange(RD::BufferRange& readRange, const RD::BufferRange& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod)
{
}

void GrainShifter::_updateGrainWriteRange(RD::BufferRange& writeRange, const RD::BufferRange& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting)
{
}

//=================
void GrainShifter::_applyWindowToFullGrain(juce::dsp::AudioBlock<float>& block)
{
}

//=================
float GrainShifter::_getWindowSampleAtIndexInPeriod(int indexInPeriod, float period)
{

}

//=================
void GrainShifter::_writeGrainBufferToOutput(const juce::AudioBuffer<float>& fullGrainBuffer, juce::AudioBuffer<float>& outputBuffer, PreviousBlockData& previousBlockData)
{

}

