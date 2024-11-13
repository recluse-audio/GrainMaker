#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferWriter.h"
#include "../SOURCE/PITCH/PitchMarkedCircularBuffer.h"



// class PitchBufferTest
// {
// public:

//     PitchBufferTest(PitchBufferTest& cb) : mCircularBuffer(cb)
//     {

//     }

//     // these are real safety net test more so to make sure I don't niss a juce function call
//     int getNumSamples() { return mCircularBuffer.mCircularBuffer.getNumSamples(); }
//     int getNumChannels() { return mCircularBuffer.mCircularBuffer.getNumChannels(); }
//     int getWritePos() { return mCircularBuffer.mWritePos; }
//     int getReadPos() { return mCircularBuffer.mReadPos; }
//     float getSample(int ch, int sampleIndex) { return mCircularBuffer.mCircularBuffer.getSample(ch, sampleIndex); }

// private:
//     CircularPitchBufferTestBuffer& mCircularBuffer;

// };

//===============
TEST_CASE("Can push and pop pitch and buffer.  This should create pitch marks")
{
    /**
     * @brief Set up: This simulates passing in a buffer
     * that is 1024 samples in length.  
     * 
     * In that there are 8 cycles of a perfect sine wave
     * that is 1/8 the size of it.  Meaning the period length is 128.
     * 
     * The goal is we can be sure that the PitchMarkedCircularBuffer
     * can read/write audio data as well as pitch markings.
     * 
     * In this case we would expect the pitch marking to be at sample index that is 
     * 75% through each cycle.  96 samples in a sine wave of 128 samples e.g.
     * The rest should be 0.f
     * 
     * 
     */
    int numSamples = 1024;
    int numChannels = 1;
    juce::AudioBuffer<float> ioBuffer(numChannels, numSamples);
    juce::AudioBuffer<float> pitchBuffer(numChannels, numSamples);

    ioBuffer.clear();
    pitchBuffer.clear();

    int numCycles = 8; // we will expect this many pitch marks later
    int periodLength = 1024 / numCycles; // 
    // this just to make sure we passed audio data successfully
    BufferFiller::generateSineCycles(ioBuffer, periodLength);

    //
    // End Setup
    //

    // Now we can declare 
    PitchMarkedCircularBuffer circularBuffer;

    circularBuffer.pushBufferAndPeriod(ioBuffer, periodLength);
    circularBuffer.popBufferAndPitch(ioBuffer, pitchBuffer);

    auto ioRead = ioBuffer.getArrayOfReadPointers();
    auto pitchRead = pitchBuffer.getArrayOfReadPointers();
    

    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
    {
        auto pitchSample = pitchRead[0][sampleIndex];

        // TODO: remove this temp func
        // if(pitchSample == periodLength)
        //     CHECK(sampleIndex == 32);

        if(sampleIndex == 32)
            CHECK(pitchSample == periodLength);
        else if(sampleIndex == 161)
            CHECK(pitchSample == periodLength);
        else if(sampleIndex == 290)
            CHECK(pitchSample == periodLength); 
        else if(sampleIndex == 419)
            CHECK(pitchSample == periodLength);
        else if(sampleIndex == 548)
            CHECK(pitchSample == periodLength);
        else if(sampleIndex == 677)
            CHECK(pitchSample == periodLength); 
        else if(sampleIndex == 806)
            CHECK(pitchSample == periodLength); 
        else if(sampleIndex == 935)
            CHECK(pitchSample == periodLength); 
        else
            CHECK(pitchSample == 0.f);

    }

    auto outputPath = BufferWriter::getTestOutputPath("PitchMarkBuffer.json");
    juce::File pitchJson(outputPath);
    BufferWriter::writeToJson(pitchBuffer, pitchJson);

}


//==================
// TEST_CASE("")
// {

// }