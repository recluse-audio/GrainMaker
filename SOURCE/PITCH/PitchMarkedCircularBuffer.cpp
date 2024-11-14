#include "PitchMarkedCircularBuffer.h"


//=========================
//
PitchMarkedCircularBuffer::PitchMarkedCircularBuffer()
{
    setSize(2, 88200);
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
bool PitchMarkedCircularBuffer::pushBufferAndPeriod(juce::AudioBuffer<float>& buffer, float periodInSamples)
{   
    // Audio Data
    mAudioBuffer.pushBuffer(buffer);
    
    // moving through buffer by increments of the detected period.  Mark highest sample index with period.
    for(int i = 0; i <= buffer.getNumSamples(); i += periodInSamples)
    {
        int maxInPeriodIndex = _findMaxSampleIndex(buffer, i, periodInSamples); 

        // 
        for(int indexInPeriod = 0; indexInPeriod <= periodInSamples; indexInPeriod++)
        {
            // write periodInSamples at this index
            if(indexInPeriod == maxInPeriodIndex)
            {
                // store period in mPitchBuffer ch[0] and store index in period of marker at ch[1]
                mPitchBuffer.pushValue(1, periodInSamples, 0, false);
                mPitchBuffer.pushValue(1, maxInPeriodIndex, 1, true); // increment now
            }
            else // write zeros before and after maxSampleIndex
            {
                mPitchBuffer.pushValue(1, 0, 0, false);
                mPitchBuffer.pushValue(1, 0, 1, true);
            }

        }

    }

}

//=========================
//
int PitchMarkedCircularBuffer::_findMaxSampleIndex(juce::AudioBuffer<float>& buffer, int startIndex, int length)
{
    auto readPtr = buffer.getArrayOfReadPointers();
    int maxSampleIndex = 0;
    float currentMax = 0.f;

    // Get max index in this length of samples.  (Index within buffer is handled elsewhere, keep this relative to period) 
    for(int sampleIndex = 0; sampleIndex < length; sampleIndex++)
    {
        auto sample = readPtr[0][sampleIndex + startIndex];
        
        if(std::abs(sample) > currentMax)
        {
            currentMax = sample;
            maxSampleIndex = sampleIndex;
        }

    }

    return maxSampleIndex;

}

//=========================
//
float PitchMarkedCircularBuffer::popBufferAndPitch(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& pitchBuffer)
{
    mAudioBuffer.popBuffer(buffer);
    mPitchBuffer.popBuffer(pitchBuffer);
}

//=========================
//
void PitchMarkedCircularBuffer::popAudioBuffer(juce::AudioBuffer<float>& buffer)
{
    mAudioBuffer.popBuffer(buffer);

}