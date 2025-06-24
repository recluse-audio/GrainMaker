#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"


//
Granulator::Granulator()
{
    mWindow.setShape(Window::Shape::kNone);
	mWindow.setLooping(true);
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

	if(detectedPeriod <= 0)
		return; // noise, no pitch, no shifting

	
	_updateSourceRange(lookaheadBuffer);
	_updateOutputRange(outputBuffer);
	_updateNumGrainsToOutput(detectedPeriod, shiftRatio);
	_updateOutputRangeInSource();
	_updateSourceRangeNeededForShifting(shiftRatio);

	int numWholeGrains = (int) mCurrentGrainData.mNumGrainsToOutput;
	float numPartialGrains = (float)mCurrentGrainData.mNumGrainsToOutput - (float)numWholeGrains;


	juce::int64 shiftOffset = _calculatePitchShiftOffset(detectedPeriod, shiftRatio);

	juce::Range<juce::int64> readRange;
	juce::Range<juce::int64> writeRange;
	float periodAfterShifting = detectedPeriod * (1.f / shiftRatio);

	for(juce::int64 grainNumber = 0; grainNumber < numWholeGrains; grainNumber++)
	{
		_updateGrainReadRange(readRange, mCurrentGrainData.mSourceRangeNeededForShifting, grainNumber, detectedPeriod);
		_updateGrainWriteRange(writeRange, mCurrentGrainData.mOutputRange, grainNumber, detectedPeriod, periodAfterShifting);

		juce::dsp::AudioBlock<float> readBlock = BufferHelper::getRangeAsBlock(lookaheadBuffer, readRange);
		_applyWindowToFullGrain(readBlock);
		BufferHelper::writeBlockToBuffer(outputBuffer, readBlock, writeRange);

	}

		_updateGrainReadRange(readRange, mCurrentGrainData.mSourceRangeNeededForShifting, mCurrentGrainData.mNumGrainsToOutput - 1.f, detectedPeriod);
		_updateGrainWriteRange(writeRange, mCurrentGrainData.mOutputRange, mCurrentGrainData.mNumGrainsToOutput - 1.f, detectedPeriod, periodAfterShifting);
		juce::dsp::AudioBlock<float> readBlock = BufferHelper::getRangeAsBlock(lookaheadBuffer, readRange);

		_applyWindowToPartialGrain(readBlock);
		BufferHelper::writeBlockToBuffer(outputBuffer, readBlock, writeRange);

		// juce::dsp::AudioBlock<float> outputBlock = BufferHelper::getRangeAsBlock(outputBuffer, mCurrentGrainData.mOutputRange);
		// _applyWindowToFullGrain(outputBlock);

} 



//==============================================================================
void Granulator::_updateGrainReadRange(juce::Range<juce::int64>& readRange, const juce::Range<juce::int64>& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod)
{
	juce::int64 startOfSourceRange = sourceRangeNeededForShifting.getStart();
	juce::int64 offsetDueToGrainNumber = (juce::int64)(grainNumber * detectedPeriod);
	juce::int64 length = (juce::int64)detectedPeriod;

	readRange.setStart(startOfSourceRange + offsetDueToGrainNumber);
	readRange.setLength(length);

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
	writeRange.setLength(length);

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
void Granulator::_updateSourceRangeNeededForShifting(float shiftRatio)
{
	juce::int64 startInSource = mCurrentGrainData.mOutputRangeInSource.getStart();
	juce::int64 outputLength = mCurrentGrainData.mOutputRangeInSource.getLength();
	float floatStartInSource = (float)startInSource;
	float floatOutputLength = (float)outputLength;

	// If shifting up we will take excess source audio data and create extra grains out of it to produce the shift up effect
	juce::int64 sourceLengthNeededForShifting = (juce::int64)(floatOutputLength * shiftRatio);

	juce::int64 shiftOffset = sourceLengthNeededForShifting - outputLength;
	juce::int64 shiftedStart = startInSource - shiftOffset;

	mCurrentGrainData.mSourceRangeNeededForShifting.setStart(shiftedStart);
	mCurrentGrainData.mSourceRangeNeededForShifting.setLength(sourceLengthNeededForShifting);
	
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