#include "PitchMarkedCircularBuffer.h"
#include "../SUBMODULES/RD/SOURCE/BufferMath.h"

//=========================
//
PitchMarkedCircularBuffer::PitchMarkedCircularBuffer()
{
    setSize(2, 88200);
    mMarkingRule = PitchMarkedCircularBuffer::MarkingRule::kNone;
}


//=========================
//
PitchMarkedCircularBuffer::~PitchMarkedCircularBuffer()
{

}

//===========================
//
void PitchMarkedCircularBuffer::prepare(double sampleRate, int maxBlockSize)
{
    mMarkingBuffer.setSize(2, maxBlockSize);
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

    // mark this up according to buffer's audio data
    mMarkingBuffer.clear();

    auto audioBlock = juce::dsp::AudioBlock<float>(buffer);
    auto markingBlock = juce::dsp::AudioBlock<float>(mMarkingBuffer);

    _pitchMarkBlock(audioBlock, markingBlock, periodInSamples);
    
    mPitchBuffer.pushBuffer(mMarkingBuffer);
    // moving through buffer by increments of the detected period.  Mark highest sample index with period.
    // for(int i = 0; i < buffer.getNumSamples(); i += periodInSamples)
    // {
    //     int indexToMark = 0;
    //     if(mMarkingRule == MarkingRule::kMagnitude)
    //         indexToMark = _findMaxSampleIndex(buffer, i, periodInSamples); 

    //     // 
    //     for(int indexInPeriod = 0; indexInPeriod <= periodInSamples; indexInPeriod++)
    //     {
    //         // write periodInSamples at this index
    //         if(indexInPeriod == indexToMark)
    //         {
    //             // store period in mPitchBuffer ch[0] and store index in period of marker at ch[1]
    //             mPitchBuffer.pushValue(1, periodInSamples, 0, false);
    //             mPitchBuffer.pushValue(1, indexToMark, 1, true); // increment now
    //         }
    //         else // write zeros before and after maxSampleIndex
    //         {
    //             mPitchBuffer.pushValue(1, 0, 0, false);
    //             mPitchBuffer.pushValue(1, 0, 1, true);
    //         }

    //     }

    // }

}

//=========================
//
void PitchMarkedCircularBuffer::_pitchMarkBlock(juce::dsp::AudioBlock<float> audioBlock, juce::dsp::AudioBlock<float> markingBlock, int period)
{
    int currentIndex = 0;
    int length = period;

    // there is some 
    while( currentIndex < audioBlock.getNumSamples() )
    {
        if( (currentIndex + length) > audioBlock.getNumSamples() )
        {
            break;
            // TODO: how to mark the final cycle which may be incomplete?  or will it?
        }

        auto audioSubBlock = audioBlock.getSubBlock(currentIndex, length);
        auto markingSubBlock = markingBlock.getSubBlock(currentIndex, length);

        int startInPeriod = 0;

        if(mMarkingRule == MarkingRule::kMagnitude)
        {
            int offsetInPeriod = BufferMath::getMaxSampleIndex(audioSubBlock, 0);
            startInPeriod += offsetInPeriod;
        }    

        markingSubBlock.setSample(0, startInPeriod, (float)period);
        markingSubBlock.setSample(1, startInPeriod, (float)startInPeriod);

        currentIndex += length;

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

//=========================
//
void PitchMarkedCircularBuffer::_loadDataIntoBuffer(juce::AudioBuffer<float>& audioBuffer, juce::AudioBuffer<float>& pitchBuffer)
{
    audioBuffer.clear();
    audioBuffer.setSize(mAudioBuffer.getNumChannels(), mAudioBuffer.getNumSamples());

    pitchBuffer.clear();
    pitchBuffer.setSize(mPitchBuffer.getNumChannels(), mPitchBuffer.getNumSamples());

    mAudioBuffer.readRange(audioBuffer, 0);
    mPitchBuffer.readRange(pitchBuffer, 0);
}










//=======================
//
void PitchMarkedCircularBuffer::setMarkingRule(PitchMarkedCircularBuffer::MarkingRule newRule)
{
    mMarkingRule = newRule;
}


//=======================
//
PitchMarkedCircularBuffer::MarkingRule PitchMarkedCircularBuffer::getMarkingRule()
{
    return mMarkingRule;
}

//======================
//
void PitchMarkedCircularBuffer::checkWritePos(int& audioWritePos, int& pitchMarkWritePos)
{
    audioWritePos = mAudioBuffer.getWritePos();
    pitchMarkWritePos = mPitchBuffer.getWritePos();
}