#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"



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
    if(sampleRate != mSampleRate)
    {
		mWindow.setSizeShapePeriod(sampleRate, Window::Shape::kHanning, sampleRate);


        mSampleRate = sampleRate;

    }
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
	mWindow.setPeriod(detectedPeriod);
	
	int numLookAheadSamples = lookaheadBuffer.getNumSamples() - outputBuffer.getNumSamples();

	// a grain that is shifted up will result in a shiftedPeriod that is smaller than the detectedPeriod
	float shiftedPeriod = detectedPeriod * (1.f / shiftRatio);

	// shifting up produces a positive shift offset, which is used in writePos calculation (is actually subtracted from writePos, so there is sign flipping here)
	// shiftRatio's of over 1.f will result in a positive shiftOffset value, and below will result in each grain "happening sooner in time" - i.e. pitch cycles happening more frequently,
	// shiftRatio's of under 1.f will result in negative shiftOffset, below this negative value would be subtracted from readPos, meaning it is technically added.
	// shifting down is essentially saying each grain occurs later in time relative to the previous grain
	float shiftOffset = detectedPeriod - shiftedPeriod;

	for(int readPos = 0; readPos < lookaheadBuffer.getNumSamples(); readPos++)
	{
		int writePos = readPos - numLookAheadSamples - shiftOffset;
		float windowVal = mWindow.getNextSample();
		// this sample index won't get written to output buffer, even after shifting up potentially
		if(writePos < 0)
			continue;
		else if(writePos >= outputBuffer.getNumSamples())
			break;

		for(int ch = 0; ch < outputBuffer.getNumChannels(); ch++)
		{
			float readSample = lookaheadBuffer.getSample(ch, readPos) * windowVal;
			outputBuffer.setSample(ch, writePos, readSample);
		}
	}

}

//=======================
void Granulator::setGrainShape(Window::Shape newShape)
{
    mWindow.setShape(newShape);
}