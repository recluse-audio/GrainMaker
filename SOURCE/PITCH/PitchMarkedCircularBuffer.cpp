#include "PitchMarkedCircularBuffer.h"


//=========================
//
PitchMarkedCircularBuffer::PitchMarkedCircularBuffer()
{

}


//=========================
//
PitchMarkedCircularBuffer::~PitchMarkedCircularBuffer()
{

}

//==========================
//
void PitchMarkedCircularBuffer::setSize(int numChannels, int numSamples)
{
    mAudioBuffer.setSize(numChannels, numSamples);
    mPitchBuffer.setSize(numChannels, numSamples);
}

//==========================
//
bool PitchMarkedCircularBuffer::pushBufferAndPitch(juce::AudioBuffer<float>& buffer, float pitchInHz)
{   
    mAudioBuffer.pushBuffer(buffer);
    mPitchBuffer.pushValue(buffer.getNumSamples(), pitchInHz);
}


//=========================
//
float PitchMarkedCircularBuffer::popBufferAndPitch(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& pitchBuffer)
{
    mAudioBuffer.popBuffer(buffer);
    mPitchBuffer.popBuffer(pitchBuffer);
}