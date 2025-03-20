#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"



//
Granulator::Granulator()
{
    mWindow.setShape(Window::Shape::kNone);
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
        // don't divide by zero
        if(mSampleRate > 0)
        {
            // re-calc the grain period based on new sampleRate
            double ratioOfChange = sampleRate / mSampleRate;
            mGrainLengthInSamples = mGrainLengthInSamples * ratioOfChange;
            mEmissionPeriodInSamples = mEmissionPeriodInSamples * ratioOfChange;
        }


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
    if(rateInHz > 0)
        mEmissionPeriodInSamples = (int)mSampleRate / rateInHz;
    else
        mEmissionPeriodInSamples = 0;
}

//
void Granulator::setEmissionPeriodInSamples(int numSamples)
{
    mEmissionPeriodInSamples = numSamples;
}

//
const int Granulator::getEmissionPeriodInSamples()
{
    return mEmissionPeriodInSamples;
}




//=====================
void Granulator::process(juce::AudioBuffer<float>& buffer)
{
	int numChannels = buffer.getNumChannels();
	int numSamples = buffer.getNumSamples();

    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
    {   
        auto windowGain = 1.f;
        
        windowGain *= mWindow.getNextSample();

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


//=======================
void Granulator::setGrainShape(Window::Shape newShape)
{
    mWindow.setShape(newShape);
}