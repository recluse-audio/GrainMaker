#include "CircularBuffer.h"

CircularBuffer::CircularBuffer()
{
    mCircularBuffer.clear();
    mCircularBuffer.setSize(2, 48000);
}

//=======================
CircularBuffer::~CircularBuffer()
{

}

//=======================
void CircularBuffer::setSize(int numChannels, int numSamples)
{
    mCircularBuffer.clear();
    mCircularBuffer.setSize(numChannels, numSamples);
    mWritePos = 0;
    mReadPos = 0;
}

//=======================
bool CircularBuffer::pushBuffer(juce::AudioBuffer<float>& buffer)
{
    bool success = false;
    success = _writeToCircularBuffer(buffer);
    return success;
}

//=======================
bool CircularBuffer::popBuffer(juce::AudioBuffer<float>& buffer)
{
    bool success = false;
    success = _readFromCircularBuffer(buffer, mReadPos);
    _incrementReadPosition(buffer.getNumSamples());
    return success;
}

//=======================
//
bool CircularBuffer::readRange(juce::AudioBuffer<float>& buffer, int startingReadIndex)
{
    bool success = false;
    success = _readFromCircularBuffer(buffer, startingReadIndex);
    return success;
}


//=======================
//
bool CircularBuffer::_writeToCircularBuffer(juce::AudioBuffer<float>& buffer)
{

    if(buffer.getNumChannels() != mCircularBuffer.getNumChannels())
        return false;

    for(int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); sampleIndex++)
    {
        for(int ch = 0; ch < buffer.getNumChannels(); ch++)
        {
            auto sample = buffer.getSample(ch, sampleIndex);
            mCircularBuffer.setSample(ch, mWritePos, sample);

        }

        mWritePos++;
        if(mWritePos >= mCircularBuffer.getNumSamples())
            mWritePos = mWritePos - mCircularBuffer.getNumSamples();
    }

    return true;

}


//=======================
//
bool CircularBuffer::_readFromCircularBuffer(juce::AudioBuffer<float>& buffer, int startingReadIndex)
{
    if(buffer.getNumChannels() != mCircularBuffer.getNumChannels())
        return false;

    auto readPos = startingReadIndex;

    for(int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); sampleIndex++)
    {
        for(int ch = 0; ch < buffer.getNumChannels(); ch++)
        {
            auto sample = mCircularBuffer.getSample(ch, readPos);
            buffer.setSample(ch, sampleIndex, sample);

        }

        readPos++; // note this is not mReadPos, don't call _incrementReadPosition()
        if(readPos >= mCircularBuffer.getNumSamples())
            readPos = readPos - mCircularBuffer.getNumSamples();
    }

    return true;
}


//======================
//
void CircularBuffer::_incrementReadPosition(int numSamples)
{
    int newReadPos = mReadPos + numSamples;

    if(newReadPos >= mCircularBuffer.getNumSamples())
        newReadPos = newReadPos - mCircularBuffer.getNumSamples();

    // now that we are wrapped and valid, assign to mReadPos
    mReadPos = newReadPos;
}

