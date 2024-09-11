#pragma once
#include "Util/Juce_Header.h"

// DEPRECETED::: Use DSP_TOOLS/BufferFiller.h
class BufferGenerator
{
public:
    static void fillWithAllOnes(juce::AudioBuffer<float>& bufferToFill)
    {
        bufferToFill.clear();
        auto writePtr = bufferToFill.getArrayOfWritePointers();

        for(int sampleIndex = 0; sampleIndex < bufferToFill.getNumSamples(); sampleIndex++)
        {
            for(int ch = 0; ch < bufferToFill.getNumChannels(); ch++)
            {
                writePtr[ch][sampleIndex] = 1.f;
            }

        }
    }

};