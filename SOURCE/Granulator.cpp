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
	//  RD::BufferRange spilloverRange(0, mSpilloverLength);
	// 	// reminder that we wrote already windowed grains to this spillover buffer so don't window again
	// 	juce::dsp::AudioBlock<float> spilloverBlock = BufferHelper::getRangeAsBlock(mSpilloverBuffer, spilloverRange);
	// 	BufferHelper::writeBlockToBuffer(outputBuffer, spilloverBlock, spilloverRange);
	// }

	// RD::BufferRange spilloverRange(0, mSpilloverBuffer.getNumSamples() - 1);
	// juce::dsp::AudioBlock<float> spilloverBlock = BufferHelper::getRangeAsBlock(mSpilloverBuffer, spilloverRange);
	// BufferHelper::writeBlockToBuffer(outputBuffer, spilloverBlock, spilloverRange);
	// clear after writing

	for(int sampleIndex = 0; sampleIndex < mSpilloverLength; sampleIndex++)
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


	int numWholeGrains = (int) mCurrentGrainData.mNumGrainsToOutput; // round up to make sure we get enough to fill outputBuffer, clip range later
	RD::BufferRange sourceRangeForGranulating;
	_getSourceRangeNeededForNumGrains(numWholeGrains, detectedPeriod, mCurrentGrainData.mSourceRange, sourceRangeForGranulating);

	juce::int64 shiftOffset = _calculatePitchShiftOffset(detectedPeriod, shiftRatio);

	RD::BufferRange readRange;
	RD::BufferRange writeRange;

	float periodAfterShifting = detectedPeriod * (1.f / shiftRatio);

	juce::dsp::AudioBlock<float> lookaheadBlock(lookaheadBuffer);

	// store spillover for each grain (most will be zero, some may be more)
	// which ever is largest will get assigned to mSpilloverLength after we loop grains
	juce::int64 maxSpillover = 0;

	for(juce::int64 grainNumber = 0; grainNumber < numWholeGrains; grainNumber++)
	{
		_updateGrainReadRange(readRange, sourceRangeForGranulating, grainNumber, detectedPeriod);
		_updateGrainWriteRange(writeRange, mCurrentGrainData.mOutputRange, grainNumber, detectedPeriod, periodAfterShifting);

		// this is the whole grain, part of it might get written to spillover
		// juce::dsp::AudioBlock<float> grainReadBlock = BufferHelper::getRangeAsBlock(lookaheadBuffer, readRange);
		juce::dsp::AudioBlock<float> grainReadBlock = lookaheadBlock.getSubBlock(readRange.getStartIndex(), readRange.getLengthInSamples());
		_applyWindowToFullGrain(grainReadBlock);
	

		// figure out which part of the grain writes to output and to spillover
		RD::BufferRange grainRangeForOutput(0,0); // this one writes to outputBuffer
		RD::BufferRange grainRangeForSpillover(0, 0); // this one goes to spilloverBuffer
		RD::BufferRange writeRangeInOutputBuffer(0,0); // this one writes to outputBuffer
		RD::BufferRange spilloverBufferWriteRange(0, 0); // this one goes to spilloverBuffer
		_getWriteRangeInOutputAndSpillover(writeRange, mCurrentGrainData.mOutputRange, grainRangeForOutput, grainRangeForSpillover, writeRangeInOutputBuffer, spilloverBufferWriteRange);

		// get block/range for  outputBuffer (processBlock) and write to it here
		juce::dsp::AudioBlock<float> outputBlock = grainReadBlock.getSubBlock(grainRangeForOutput.getStartIndex(), grainRangeForOutput.getLengthInSamples());
		BufferHelper::writeBlockToBuffer(outputBuffer, outputBlock, writeRangeInOutputBuffer);

		// update this so we know how far to write next block
		if(!spilloverBufferWriteRange.isEmpty())
		{

			// writeRangeInOutputBuffer.length should correspond with the start of the section of the grain to "spillover", not end (that is relative to output buffer not grain itself)
			juce::dsp::AudioBlock<float> spilloverBlock = grainReadBlock.getSubBlock(grainRangeForSpillover.getStartIndex(), grainRangeForSpillover.getLengthInSamples());
			BufferHelper::writeBlockToBuffer(mSpilloverBuffer, spilloverBlock, spilloverBufferWriteRange);


			if(spilloverBufferWriteRange.getEndIndex() > maxSpillover)
			{
				maxSpillover = spilloverBufferWriteRange.getEndIndex();
			}
		}




		// take block range for 

	}

	mSpilloverLength = maxSpillover;
	mPreviousPeriod = detectedPeriod;



} 





//====================
juce::int64 Granulator::_calculateSpilloverOffsetAfterShifting(float prevPeriod, juce::int64 spilloverNumSamples, float shiftRatio)
{
	// start of last grain from previous block, + prevShiftedPeriod = start of first grain
	return juce::int64(spilloverNumSamples);
}






//==============================================================================
void Granulator::_getSourceRangeNeededForNumGrains(int numGrains, float detectedPeriod
											, const RD::BufferRange& sourceRange
											, RD::BufferRange& sourceRangeForShifting)
{
	juce::int64 numSourceSamplesNeeded = juce::int64(numGrains * (double)detectedPeriod);
	juce::int64 startInSource = sourceRange.getLengthInSamples() - numSourceSamplesNeeded;
	jassert(startInSource >= 0);

	sourceRangeForShifting.setStartIndex(startInSource);
	sourceRangeForShifting.setEndIndex(sourceRange.getEndIndex());
} 


//==============================================================================
void Granulator::_getWriteRangeInOutputAndSpillover(const RD::BufferRange& wholeGrainWriteRange,
											const RD::BufferRange& outputRange,
											RD::BufferRange& grainRangeForOutputBuffer,
											RD::BufferRange& grainRangeForSpilloverBuffer,
											RD::BufferRange& writeRangeInOutputBuffer,
											RD::BufferRange& spilloverBufferWriteRange )
{
	// wholeGrainWriteRange may extend past the end of the current process block and will then write to the spillover buffer


	// by default the output buffer write range is the whole grain range, but some grains extend past the end of the output buffer
	writeRangeInOutputBuffer.setStartIndex(wholeGrainWriteRange.getStartIndex());
	writeRangeInOutputBuffer.setEndIndex(wholeGrainWriteRange.getEndIndex());

	grainRangeForOutputBuffer.setStartIndex(0);
	grainRangeForOutputBuffer.setEndIndex(wholeGrainWriteRange.getLengthInSamples() - 1);

	// by default none
	grainRangeForSpilloverBuffer.setIsEmpty(true);
	
	// by default no spillover
	spilloverBufferWriteRange.setIsEmpty(true);

	// if it extends past end
	if(wholeGrainWriteRange.getEndIndex() > outputRange.getEndIndex() && wholeGrainWriteRange.getStartIndex() < outputRange.getEndIndex())
	{
		writeRangeInOutputBuffer.setStartIndex(wholeGrainWriteRange.getStartIndex());
		writeRangeInOutputBuffer.setEndIndex(outputRange.getEndIndex());

		// ramge of grain to read and then write to outputBuffer
		grainRangeForOutputBuffer.setStartIndex(0);
		grainRangeForOutputBuffer.setEndIndex(writeRangeInOutputBuffer.getLengthInSamples() - 1);


		juce::int64 spilloverSamples = (wholeGrainWriteRange.getEndIndex() - outputRange.getEndIndex());
		spilloverBufferWriteRange.setStartIndex(0); // always write to start of spill over
		spilloverBufferWriteRange.setEndIndex(spilloverSamples - 1);


		juce::int64 grainRangeForSpilloverStart = grainRangeForOutputBuffer.getEndIndex() + 1;

		grainRangeForSpilloverBuffer.setStartIndex(grainRangeForSpilloverStart); // start one index after the end index of range for output buffer 
		grainRangeForSpilloverBuffer.setEndIndex(grainRangeForSpilloverStart + spilloverSamples - 1);
	}
}

//==============================================================================
void Granulator::_updateGrainReadRange(RD::BufferRange& readRange, const RD::BufferRange& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod)
{
	juce::int64 startOfSourceRange = sourceRangeNeededForShifting.getStartIndex();
	juce::int64 offsetDueToGrainNumber = (juce::int64)(grainNumber * detectedPeriod);
	juce::int64 length = (juce::int64)detectedPeriod;

	readRange.setStartIndex(startOfSourceRange + offsetDueToGrainNumber);
	readRange.setEndIndex(readRange.getStartIndex() + (juce::int64)detectedPeriod - 1);

	if(readRange.getEndIndex() >= sourceRangeNeededForShifting.getEndIndex())
		readRange.setEndIndex(sourceRangeNeededForShifting.getEndIndex());

}

//==============================================================================
void Granulator::_updateGrainWriteRange(RD::BufferRange& writeRange, const RD::BufferRange& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting)
{
	juce::int64 startOfOutputRange = mSpilloverLength; // is this ever not 0? This refers to first index of processBlock(outputRange)
	juce::int64 offsetDueToGrainNumber = (juce::int64)(grainNumber * periodAfterShifting);

	writeRange.setStartIndex(startOfOutputRange + offsetDueToGrainNumber);
	writeRange.setLengthInSamples( (juce::int64)detectedPeriod );

	// if(writeRange.getEndIndex() >= outputRange.getEndIndex())
	// 	writeRange.setEndIndex(outputRange.getEndIndex());
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
	mCurrentGrainData.mSourceRange.setStartIndex( 0 );
	mCurrentGrainData.mSourceRange.setEndIndex( (juce::int64) (lookaheadBuffer.getNumSamples() - 1) );
}

//==============================================================================
void Granulator::_updateOutputRange(const juce::AudioBuffer<float>& outputBuffer)
{
	mCurrentGrainData.mOutputRange.setStartIndex( 0 );
	mCurrentGrainData.mOutputRange.setEndIndex( (juce::int64) (outputBuffer.getNumSamples() - 1) );
}

//==============================================================================
//------------------------------------------------------------------------------
void Granulator::_updateNumGrainsToOutput(float detectedPeriod, float shiftRatio)
{

	double noShiftNumGrains = (double)(mCurrentGrainData.mOutputRange.getLengthInSamples()) / (double) detectedPeriod;
	mCurrentGrainData.mNumGrainsToOutput = noShiftNumGrains * (double)shiftRatio;
}
//==============================================================================





//==============================================================================
void Granulator::_updateFullGrainRange(float startIndex, float detectedPeriod)
{
	mCurrentGrainData.mFullGrainRange.setStartIndex( (juce::int64)startIndex );
	mCurrentGrainData.mFullGrainRange.setEndIndex( (juce::int64)detectedPeriod + (juce::int64)startIndex );
}

//==============================================================================
void Granulator::_updateClippedGrainRange()
{
	//mCurrentGrainData.mClippedGrainRange = mCurrentGrainData.mFullGrainRange.getIntersectionWith(mCurrentGrainData.mSourceRange);
}

//==============================================================================
void Granulator::_updateShiftedRange(float detectedPeriod, float shiftRatio)
{
	// juce::int64 shiftedOffset = _calculatePitchShiftOffset(detectedPeriod, shiftRatio);
	// juce::int64 shiftedStart = mCurrentGrainData.mClippedGrainRange.getStartIndex() + shiftedOffset;
	// mCurrentGrainData.mShiftedRange = mCurrentGrainData.mClippedGrainRange.movedToStartAt(shiftedStart);
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

