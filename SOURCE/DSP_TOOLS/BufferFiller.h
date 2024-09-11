#pragma once
#include "Util/Juce_Header.h"

/**
 * @brief An assortment of functions for filling buffers with amplitude values in various patters
 * 
 */

class BufferFiller
{
public:


    //////////////////////////
    // All indices set to 1.f
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



    //====================
    //
    static void generateHanning(juce::AudioBuffer<float>& bufferToFill)
    {
        bufferToFill.clear();

        auto writePtr = bufferToFill.getArrayOfWritePointers();
        int numSamples = bufferToFill.getNumSamples() - 1;  // why -1 I am not sure yet
        
        for(int sampleIndex = 0; sampleIndex <= numSamples; sampleIndex++)
        {
            writePtr[0][sampleIndex] = 0.5f * (1.0f - std::cos(2.0f * M_PI * sampleIndex / (numSamples)));
        }
    }


};