#include "Granulator.h"




//
Granulator::Granulator()
{

}

//
Granulator::~Granulator()
{

}

//
void Granulator::prepare(double sampleRate)
{
    mSampleRate = sampleRate;
    _updateEmissionPeriod();
}

//
const double Granulator::getCurrentSampleRate()
{
    return mSampleRate;
}

//
void Granulator::setGrainLengthInMs(double lengthInMs)
{
    mGrainLengthInSamples = (lengthInMs * 0.001) * mSampleRate;
}

//
const int Granulator::getGrainLengthInSamples()
{
    return mGrainLengthInSamples;
}

//
void Granulator::setEmissionRateInHz(double rateInHz)
{
    if(rateInHz <= 0)
        mEmissionRateInHz = 0;
    else
        mEmissionRateInHz = rateInHz;

    _updateEmissionPeriod();
}

//
const int Granulator::getEmissionPeriodInSamples()
{
    return mEmissionPeriodInSamples;
}

//
void Granulator::_updateEmissionPeriod()
{
    if(mSampleRate == 0 || mEmissionRateInHz <= 0)
        mEmissionPeriodInSamples = 0;
    else
        mEmissionPeriodInSamples = mSampleRate / mEmissionRateInHz;
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
        
        for(int ch = 0; ch < numChannels; ch++)
        {
            auto sample = buffer.getSample(ch, sampleIndex) * windowGain;
            buffer.setSample(ch, sampleIndex, sample);
        }

        mCurrentPhaseIndex++;
        if(mCurrentPhaseIndex >= mEmissionPeriodInSamples)
            mCurrentPhaseIndex = 0;
    }
}