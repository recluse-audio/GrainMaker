#include "GrainShifter.h"
#include "../../SUBMODULES/RD/SOURCE/BufferHelper.h"

GrainShifter::GrainShifter()
{
    mWindow.setShape(Window::Shape::kNone);
	mGrainProcessingBuffer.clear();

}

GrainShifter::~GrainShifter()
{
}

void GrainShifter::setGrainShape(Window::Shape newShape)
{

}

void GrainShifter::prepare(double sampleRate, int blockSize)
{
	mWindow.setSizeShapePeriod(sampleRate, Window::Shape::kHanning, sampleRate);
    mSampleRate = sampleRate;
	mGrainProcessingBuffer.setSize(2, mSampleRate); // much bigger than we need




}

Window& GrainShifter::getGrainWindow()
{
	return mWindow;
}

void GrainShifter::processShifting(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& outputBuffer, float detectedPeriod, float shiftRatio)
{
	// write the spillover samples from previous processBlock to output (the final grain will almost always extend beyond end of prev buffer)
	_writeGrainBufferSpilloverToOutput(mGrainProcessingBuffer, outputBuffer, mPreviousBlockData);
	mGrainProcessingBuffer.clear(); // clean now, write new data/spillover
	
	// Make ranges out of the buffer we are granulating and the buffer we are writing to
	RD::BufferRange sourceRange; RD::BufferRange outputRange;
	sourceRange.setRangeAccordingToBuffer(lookaheadBuffer);
	outputRange.setRangeAccordingToBuffer(outputBuffer);

	// This will determine the whole number of grains needed.
	// Using whole number grains so the final grain always "spills over" into the next block.
	// This is affected by spillover length
	int numGrains = _calculateNumGrainsToOutput(detectedPeriod, shiftRatio, outputRange, mSpilloverLength);
	juce::int64 periodAfterShifting = _calculatePeriodAfterShifting(detectedPeriod, shiftRatio);

	RD::BufferRange sourceRangeNeeded;
	_updateSourceRangeNeededForNumGrains(numGrains, detectedPeriod, sourceRange, sourceRangeNeeded);

	juce::dsp::AudioBlock<float> lookaheadBlock(lookaheadBuffer);

	for(int grainIndex = 0; grainIndex < numGrains; grainIndex++)
	{
		RD::BufferRange grainReadRangeInSource;
		_updateGrainReadRange(grainReadRangeInSource, sourceRangeNeeded, grainIndex, detectedPeriod);

		// this is the whole grain, part of it might get written to spillover
		// juce::dsp::AudioBlock<float> grainReadBlock = BufferHelper::getRangeAsBlock(lookaheadBuffer, readRange);
		juce::dsp::AudioBlock<float> grainReadBlock = lookaheadBlock.getSubBlock(grainReadRangeInSource.getStartIndex(), grainReadRangeInSource.getLengthInSamples());
		_applyWindowToFullGrain(grainReadBlock);

		// range in mGrainProcessingBuffer which accounts for the final grain which will extend beyond final index of output buffer
		RD::BufferRange grainWriteRangeInFullGrainBuffer; 
		_updateGrainWriteRange(grainWriteRangeInFullGrainBuffer, outputRange, grainIndex, detectedPeriod, periodAfterShifting);

		BufferHelper::writeBlockToBuffer(mGrainProcessingBuffer, grainReadBlock, grainWriteRangeInFullGrainBuffer);

	}

	_writeGrainBufferToOutput(mGrainProcessingBuffer, outputBuffer, mPreviousBlockData);

	
}

//=======================================================
//================ PRIVATE ==============================
//=======================================================

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

