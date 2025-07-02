#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"


//
Granulator::Granulator()
{
    mWindow.setShape(Window::Shape::kNone);
	mWindow.setLooping(true);

	mSpilloverBuffer.clear();
}

//
Granulator::~Granulator()
{

}

//
void Granulator::prepare(double sampleRate)
{
	mWindow.setSizeShapePeriod(sampleRate, Window::Shape::kHanning, sampleRate);
    mSampleRate = sampleRate;
}

void Granulator::prepare(double sampleRate, int blockSize)
{
	mWindow.setSizeShapePeriod(sampleRate, Window::Shape::kHanning, sampleRate);
    mSampleRate = sampleRate;
	mSpilloverBuffer.setSize(2, blockSize); // much bigger than we need
}
//
const double Granulator::getCurrentSampleRate()
{
    return mSampleRate;
}

//
void Granulator::setGrainLengthInMs(double lengthInMs)
{
    int numSamples = (lengthInMs * 0.001) * mSampleRate;
    this->setGrainLengthInSamples(numSamples);
}

//
void Granulator::setGrainLengthInSamples(int numSamples)
{
    mGrainLengthInSamples = numSamples;
    mWindow.setPeriod(mGrainLengthInSamples);
}

//
const int Granulator::getGrainLengthInSamples()
{
    return mGrainLengthInSamples;
}

//
void Granulator::setEmissionRateInHz(double rateInHz)
{
    int newEmissionPeriod = 0;
    if(rateInHz > 0 && mSampleRate > 0)
        newEmissionPeriod = (int)mSampleRate / rateInHz;

    this->setEmissionPeriodInSamples(newEmissionPeriod);
    // TODO: This is temp, shouldn't auto update grain with new emission rate unless using a percentage
    this->setGrainLengthInSamples(newEmissionPeriod);
}

//
void Granulator::setEmissionPeriodInSamples(int numSamples)
{
    mEmissionPeriodInSamples = numSamples;
	// TODO: This is temp, shouldn't auto update grain with new emission rate unless using a percentage
	this->setGrainLengthInSamples(mEmissionPeriodInSamples);
}

//
const int Granulator::getEmissionPeriodInSamples()
{
    return mEmissionPeriodInSamples;
}

//===============
void Granulator::setPhaseOffset(float phaseOffset)
{

}


//=====================
void Granulator::process(juce::AudioBuffer<float>& buffer)
{
	int numChannels = buffer.getNumChannels();
	int numSamples = buffer.getNumSamples();

    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
    {   
        auto windowGain = 1.f;
        

        if(mCurrentPhaseIndex >= mGrainLengthInSamples)
            windowGain = 0.f;
        else
            windowGain = mWindow.getNextSample();

        
        for(int ch = 0; ch < numChannels; ch++)
        {
            auto sample = buffer.getSample(ch, sampleIndex) * windowGain;
            buffer.setSample(ch, sampleIndex, sample);
        }

        mCurrentPhaseIndex++;
        if(mCurrentPhaseIndex >= mEmissionPeriodInSamples)
        {
            mCurrentPhaseIndex = 0;
            mWindow.setReadPos(0);

        }
    }
}

//=======================
void Granulator::processShifting(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& outputBuffer, float detectedPeriod, float shiftRatio)
{	
	// first, write spillover grains from last processBlock
	// if(mSpilloverLength > 0)
	// {
	// 	juce::Range<juce::int64> spilloverRange(0, mSpilloverLength);
	// 	// reminder that we wrote already windowed grains to this spillover buffer so don't window again
	// 	juce::dsp::AudioBlock<float> spilloverBlock = BufferHelper::getRangeAsBlock(mSpilloverBuffer, spilloverRange);
	// 	BufferHelper::writeBlockToBuffer(outputBuffer, spilloverBlock, spilloverRange);
	// }

	// juce::Range<juce::int64> spilloverRange(0, mSpilloverBuffer.getNumSamples() - 1);
	// juce::dsp::AudioBlock<float> spilloverBlock = BufferHelper::getRangeAsBlock(mSpilloverBuffer, spilloverRange);
	// BufferHelper::writeBlockToBuffer(outputBuffer, spilloverBlock, spilloverRange);
	// clear after writing

	for(int sampleIndex = 0; sampleIndex < outputBuffer.getNumSamples(); sampleIndex++)
	{
		
		for(int ch = 0; ch < outputBuffer.getNumChannels(); ch++)
		{
			float spilloverSample = mSpilloverBuffer.getSample(ch, sampleIndex);
			float outputBufferSample = outputBuffer.getSample(ch, sampleIndex);
			float finalSample = spilloverSample + outputBufferSample;
			outputBuffer.setSample(ch, sampleIndex, finalSample);
		}
	}

	mSpilloverBuffer.clear();

	if(detectedPeriod <= 0)
		return; // noise, no pitch, no shifting

	
	_updateSourceRange(lookaheadBuffer);
	_updateOutputRange(outputBuffer);
	_updateNumGrainsToOutput(detectedPeriod, shiftRatio);
	// _updateOutputRangeInSource(); 
	// _updateSourceRangeNeededForShifting(shiftRatio);

	int numWholeGrains = (int) mCurrentGrainData.mNumGrainsToOutput + 1;
	juce::Range<juce::int64> sourceRangeForGranulating;
	_getSourceRangeNeededForNumGrains(numWholeGrains, detectedPeriod, mCurrentGrainData.mSourceRange, sourceRangeForGranulating);
	// float numPartialGrains = (float)mCurrentGrainData.mNumGrainsToOutput - (float)numWholeGrains;


	juce::int64 shiftOffset = _calculatePitchShiftOffset(detectedPeriod, shiftRatio);

	juce::Range<juce::int64> readRange;
	juce::Range<juce::int64> writeRange;

	float periodAfterShifting = detectedPeriod * (1.f / shiftRatio);

	juce::dsp::AudioBlock<float> lookaheadBlock(lookaheadBuffer);

	for(juce::int64 grainNumber = 0; grainNumber < numWholeGrains; grainNumber++)
	{
		_updateGrainReadRange(readRange, sourceRangeForGranulating, grainNumber, detectedPeriod);
		_updateGrainWriteRange(writeRange, mCurrentGrainData.mOutputRange, grainNumber, detectedPeriod, periodAfterShifting);

		// this is the whole grain, part of it might get written to spillover
		// juce::dsp::AudioBlock<float> grainReadBlock = BufferHelper::getRangeAsBlock(lookaheadBuffer, readRange);
		juce::dsp::AudioBlock<float> grainReadBlock = lookaheadBlock.getSubBlock(readRange.getStart(), readRange.getLength());
		_applyWindowToFullGrain(grainReadBlock);
	

		// figure out which part of the grain writes to output and to spillover
		juce::Range<juce::int64> grainRangeForOutput(0,0); // this one writes to outputBuffer
		juce::Range<juce::int64> grainRangeForSpillover(0, 0); // this one goes to spilloverBuffer
		juce::Range<juce::int64> writeRangeInOutputBuffer(0,0); // this one writes to outputBuffer
		juce::Range<juce::int64> spilloverBufferWriteRange(0, 0); // this one goes to spilloverBuffer
		_getWriteRangeInOutputAndSpillover(writeRange, mCurrentGrainData.mOutputRange, grainRangeForOutput, grainRangeForSpillover, writeRangeInOutputBuffer, spilloverBufferWriteRange);

		// get block/range for  outputBuffer (processBlock) and write to it here
		juce::dsp::AudioBlock<float> outputBlock = grainReadBlock.getSubBlock(grainRangeForOutput.getStart(), grainRangeForOutput.getLength() + 1);
		BufferHelper::writeBlockToBuffer(outputBuffer, outputBlock, writeRangeInOutputBuffer);

		// update this so we know how far to write next block
		if(spilloverBufferWriteRange.getLength() > 0)
		{

			// writeRangeInOutputBuffer.length should correspond with the start of the section of the grain to "spillover", not end (that is relative to output buffer not grain itself)
			juce::dsp::AudioBlock<float> spilloverBlock = grainReadBlock.getSubBlock(grainRangeForSpillover.getStart(), grainRangeForSpillover.getLength() + 1 );
			BufferHelper::writeBlockToBuffer(mSpilloverBuffer, spilloverBlock, spilloverBufferWriteRange);
		}

		



		// take block range for 

	}




} 

//==============================================================================
void Granulator::_getSourceRangeNeededForNumGrains(int numGrains, float detectedPeriod
											, const juce::Range<juce::int64>& sourceRange
											, juce::Range<juce::int64>& sourceRangeForShifting)
{
	juce::int64 numSourceSamplesNeeded = juce::int64(numGrains * (double)detectedPeriod);
	juce::int64 startInSource = sourceRange.getEnd() - numSourceSamplesNeeded + 1;
	jassert(startInSource >= 0);

	sourceRangeForShifting.setStart(startInSource);
	sourceRangeForShifting.setEnd(sourceRange.getEnd());


} 


//==============================================================================
void Granulator::_getWriteRangeInOutputAndSpillover(const juce::Range<juce::int64>& wholeGrainRange,
											const juce::Range<juce::int64>& outputRange,
											juce::Range<juce::int64>& grainRangeForOutputBuffer,
											juce::Range<juce::int64>& grainRangeForSpilloverBuffer,
											juce::Range<juce::int64>& writeRangeInOutputBuffer,
											juce::Range<juce::int64>& spilloverBufferWriteRange )
{
	// by default the output buffer write range is the whole grain range, but some grains extend past the end of the output buffer
	writeRangeInOutputBuffer.setStart(wholeGrainRange.getStart());
	writeRangeInOutputBuffer.setEnd(wholeGrainRange.getEnd());

	grainRangeForOutputBuffer.setStart(0);
	grainRangeForOutputBuffer.setLength(wholeGrainRange.getLength());

	// by default none
	grainRangeForSpilloverBuffer.setStart(0);
	grainRangeForSpilloverBuffer.setEnd(0);
	
	// by default no spillover
	spilloverBufferWriteRange.setStart(0);
	spilloverBufferWriteRange.setEnd(0);

	// if it extends past end
	if(wholeGrainRange.getEnd() > outputRange.getEnd())
	{
		writeRangeInOutputBuffer.setStart(wholeGrainRange.getStart());
		writeRangeInOutputBuffer.setEnd(outputRange.getEnd());

		// ramge of grain to read and then write to outputBuffer
		grainRangeForOutputBuffer.setStart(0);
		grainRangeForOutputBuffer.setEnd(writeRangeInOutputBuffer.getLength());


		juce::int64 spilloverSamples = wholeGrainRange.getEnd() - outputRange.getEnd();
		spilloverBufferWriteRange.setEnd(spilloverSamples - 1);

		juce::int64 grainRangeForSpilloverStart = grainRangeForOutputBuffer.getEnd() + 1;
		juce::int64 grainRangeForSpilloverEnd = grainRangeForSpilloverStart + spilloverSamples - 1;

		grainRangeForSpilloverBuffer.setStart(grainRangeForSpilloverStart);
		grainRangeForSpilloverBuffer.setEnd(grainRangeForSpilloverEnd);
	}
}

//==============================================================================
void Granulator::_updateGrainReadRange(juce::Range<juce::int64>& readRange, const juce::Range<juce::int64>& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod)
{
	juce::int64 startOfSourceRange = sourceRangeNeededForShifting.getStart();
	juce::int64 offsetDueToGrainNumber = (juce::int64)(grainNumber * detectedPeriod);
	juce::int64 length = (juce::int64)detectedPeriod;

	readRange.setStart(startOfSourceRange + offsetDueToGrainNumber);
	readRange.setEnd(readRange.getStart() + (juce::int64)detectedPeriod - 1);

	if(readRange.getEnd() >= sourceRangeNeededForShifting.getEnd())
		readRange.setEnd(sourceRangeNeededForShifting.getEnd());

}

//==============================================================================
void Granulator::_updateGrainWriteRange(juce::Range<juce::int64>& writeRange, const juce::Range<juce::int64>& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting)
{
	juce::int64 startOfOutputRange = 0; // is this ever not 0? This refers to first index of processBlock(outputRange)
	juce::int64 offsetDueToGrainNumber = (juce::int64)(grainNumber * periodAfterShifting);
	juce::int64 length = (juce::int64)detectedPeriod;

	writeRange.setStart(startOfOutputRange + offsetDueToGrainNumber);
	writeRange.setLength(length - 1); // -1 to make juce::Range behave the way I want for a buffer

	if(writeRange.getEnd() >= outputRange.getEnd())
		writeRange.setEnd(outputRange.getEnd());
}





//=======================
void Granulator::setGrainShape(Window::Shape newShape)
{
    mWindow.setShape(newShape);
}






//=================
// TODO: return a float here and get an interpolated sample for that last partial value
juce::int64 Granulator::_calculatePitchShiftOffset(float period, float shiftRatio)
{
	// a grain that is shifted up will result in a shiftedPeriod that is smaller than the detectedPeriod
	float shiftedPeriod = period * (1.f / shiftRatio);

	// shifting up produces a positive shift offset, which is used in writePos calculation (is actually subtracted from writePos, so there is sign flipping here)
	// shiftRatio's of over 1.f will result in a positive shiftOffset value, and below will result in each grain "happening sooner in time" - i.e. pitch cycles happening more frequently,
	// shiftRatio's of under 1.f will result in negative shiftOffset, below this negative value would be subtracted from readPos, meaning it is technically added.
	// shifting down is essentially saying each grain occurs later in time relative to the previous grain
	juce::int64 shiftOffset = (juce::int64)(period - shiftedPeriod);
	return shiftOffset;
}



//==============================================================================
void Granulator::_updateSourceRange(const juce::AudioBuffer<float>& lookaheadBuffer)
{
	mCurrentGrainData.mSourceRange.setStart( 0 );
	mCurrentGrainData.mSourceRange.setEnd( (juce::int64) (lookaheadBuffer.getNumSamples() - 1) );
}

//==============================================================================
void Granulator::_updateOutputRange(const juce::AudioBuffer<float>& outputBuffer)
{
	mCurrentGrainData.mOutputRange.setStart( 0 );
	mCurrentGrainData.mOutputRange.setEnd( (juce::int64) (outputBuffer.getNumSamples() - 1) );
}

//==============================================================================
//------------------------------------------------------------------------------
void Granulator::_updateNumGrainsToOutput(float detectedPeriod, float shiftRatio)
{

	double noShiftNumGrains = (double)(mCurrentGrainData.mOutputRange.getLength() + 1) / (double) detectedPeriod;
	mCurrentGrainData.mNumGrainsToOutput = noShiftNumGrains * (double)shiftRatio;
}
//==============================================================================


//==============================================================================
void Granulator::_updateOutputRangeInSource()
{
	juce::int64 startInSource = mCurrentGrainData.mSourceRange.getLength() - mCurrentGrainData.mOutputRange.getLength();
	juce::int64 endInSource = mCurrentGrainData.mOutputRange.getLength() + startInSource;
	mCurrentGrainData.mOutputRangeInSource.setStart(startInSource);
	mCurrentGrainData.mOutputRangeInSource.setEnd(endInSource);
}

//=============================================================================
// void Granulator::_updateSourceRangeNeededForShifting(float shiftRatio)
// {

	
// 	juce::int64 startInSource = mCurrentGrainData.mOutputRangeInSource.getStart();
// 	juce::int64 outputLength = mCurrentGrainData.mOutputRangeInSource.getLength();
// 	float floatStartInSource = (float)startInSource;
// 	float floatOutputLength = (float)outputLength;

// 	// If shifting up we will take excess source audio data and create extra grains out of it to produce the shift up effect
// 	juce::int64 sourceLengthNeededForShifting = (juce::int64)(floatOutputLength * shiftRatio);

// 	juce::int64 shiftOffset = sourceLengthNeededForShifting - outputLength;
// 	juce::int64 shiftedStart = startInSource - shiftOffset;

// 	mCurrentGrainData.mSourceRangeNeededForShifting.setStart(shiftedStart);
// 	mCurrentGrainData.mSourceRangeNeededForShifting.setLength(sourceLengthNeededForShifting);
	
// }

//=============================================================================
void Granulator::_updateSourceRangeNeededForShifting(float shiftRatio)
{
	juce::int64 startInSourceBuffer = mCurrentGrainData.mOutputRangeInSource.getStart();
	juce::int64 endInSourceBuffer = mCurrentGrainData.mOutputRangeInSource.getEnd();

	// only need more data if we are shifting up
	if(shiftRatio > 1.f)
	{
		juce::int64 startInSource = mCurrentGrainData.mOutputRangeInSource.getStart();
		juce::int64 samplesNeeded = mCurrentGrainData.mOutputRangeInSource.getLength() + 1; // + 1 to account for juce::Range::length vs juce::Buffer::numSamples discrepancy
		float floatStartInSource = (float)startInSource;
		float floatSamplesNeeded = (float)samplesNeeded;

		// If shifting up we will take excess source audio data and create extra grains out of it to produce the shift up effect
		juce::int64 sourceSamplesNeededForShifting = (juce::int64)(floatSamplesNeeded * shiftRatio);

		juce::int64 shiftOffset = sourceSamplesNeededForShifting - samplesNeeded;
		
		startInSourceBuffer = startInSource - shiftOffset;
		endInSourceBuffer = startInSourceBuffer + sourceSamplesNeededForShifting - 1; // -1 to get back in buffer sample index, not total sample numbers
	}

	mCurrentGrainData.mSourceRangeNeededForShifting.setStart(startInSourceBuffer);
	mCurrentGrainData.mSourceRangeNeededForShifting.setEnd(endInSourceBuffer);
	
}


//==============================================================================
void Granulator::_updateFullGrainRange(float startIndex, float detectedPeriod)
{
	mCurrentGrainData.mFullGrainRange.setStart( (juce::int64)startIndex );
	mCurrentGrainData.mFullGrainRange.setEnd( (juce::int64)detectedPeriod + (juce::int64)startIndex );
}

//==============================================================================
void Granulator::_updateClippedGrainRange()
{
	mCurrentGrainData.mClippedGrainRange = mCurrentGrainData.mFullGrainRange.getIntersectionWith(mCurrentGrainData.mSourceRange);
}

//==============================================================================
void Granulator::_updateShiftedRange(float detectedPeriod, float shiftRatio)
{
	juce::int64 shiftedOffset = _calculatePitchShiftOffset(detectedPeriod, shiftRatio);
	juce::int64 shiftedStart = mCurrentGrainData.mClippedGrainRange.getStart() + shiftedOffset;
	mCurrentGrainData.mShiftedRange = mCurrentGrainData.mClippedGrainRange.movedToStartAt(shiftedStart);
}


























//=================
float Granulator::_getWindowSampleAtIndexInPeriod(int indexInPeriod, float period)
{
	double phaseIncrement = (double)mWindow.getSize() / (double)period;
	double readPos = (double)indexInPeriod * phaseIncrement;
	return mWindow.getAtReadPos(readPos);
}

//===============================================================================
void Granulator::_applyWindowToFullGrain(juce::dsp::AudioBlock<float>& block)
{
	// handle no-window situation here so we don't even ask for its values
	if(mWindow.getShape() == Window::Shape::kNone)
		return;

	for(int sampleIndex = 0; sampleIndex < block.getNumSamples(); sampleIndex++)
	{
		float windowVal = _getWindowSampleAtIndexInPeriod(sampleIndex, block.getNumSamples());

		for(int ch = 0; ch < block.getNumChannels(); ch++)
		{
			float currentSample = block.getSample(ch, sampleIndex);
			float windowedSample = currentSample * windowVal;
			block.setSample(ch, sampleIndex, windowedSample);
		}
	}

}

//===============================================================================
void Granulator::_applyWindowToPartialGrain(juce::dsp::AudioBlock<float>& block)
{
	int fadeOutSamples = 100;
	float fadeOutValue = 1.f;
	float fadeOutIncrement = 0.01f;

	for(int sampleIndex = 0; sampleIndex < block.getNumSamples(); sampleIndex++)
	{
		float windowVal = _getWindowSampleAtIndexInPeriod(sampleIndex, block.getNumSamples());

		if(sampleIndex >= block.getNumSamples() - fadeOutSamples)
			fadeOutValue -= fadeOutIncrement;

		for(int ch = 0; ch < block.getNumChannels(); ch++)
		{
			float currentSample = block.getSample(ch, sampleIndex);
			float windowedSample = currentSample * windowVal * fadeOutValue;
			block.setSample(ch, sampleIndex, windowedSample);
		}
	}


}