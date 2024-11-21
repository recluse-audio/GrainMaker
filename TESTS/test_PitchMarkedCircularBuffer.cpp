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
TEST_CASE("Can push and pop pitch and buffer")
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

        // Now we can declare 
    PitchMarkedCircularBuffer circularBuffer;
    int circularBufferSize = 1024;
    circularBuffer.setSize(2, circularBufferSize);

    auto expectedRule = PitchMarkedCircularBuffer::MarkingRule::kMagnitude;
    circularBuffer.setMarkingRule(PitchMarkedCircularBuffer::MarkingRule::kMagnitude);
    auto actualRule = circularBuffer.getMarkingRule();
    CHECK(actualRule == expectedRule);


    int numSamples = 128;
    int numChannels = 2;
    juce::AudioBuffer<float> sineBuffer(numChannels, numSamples);
    juce::AudioBuffer<float> pitchBuffer(numChannels, numSamples);
    juce::AudioBuffer<float> analysisBuffer(numChannels, numSamples);


    sineBuffer.clear();
    pitchBuffer.clear();

    int numCycles = 10; // we will expect this many pitch marks later
    int periodLength = 128; // 
    // this just to make sure we passed audio data successfully
    BufferFiller::generateSineCycles(sineBuffer, periodLength);

    circularBuffer.prepare(44100, periodLength);

    for(int i = 0; i < numCycles; i++)
    {
        circularBuffer.pushBufferAndPeriod(sineBuffer, periodLength);
        int audioWritePos = 0;
        int pitchMarkWritePos = 0;
        circularBuffer.checkWritePos(audioWritePos, pitchMarkWritePos);

        int expectedWritePos = periodLength * (i+1);
        expectedWritePos %= circularBufferSize;
        CHECK(audioWritePos == expectedWritePos);
        CHECK(pitchMarkWritePos == expectedWritePos);
        
    }
    //
    // End Setup
    //




    circularBuffer.popBufferAndPitch(analysisBuffer, pitchBuffer);

    auto sineRead = sineBuffer.getArrayOfReadPointers();
    auto pitchRead = pitchBuffer.getArrayOfReadPointers();
    auto analysisRead = analysisBuffer.getArrayOfReadPointers();


    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
    {
        auto sineReadSample = sineRead[0][sampleIndex];

        auto markedPeriodLength = pitchRead[0][sampleIndex];
        auto indexInPeriod = pitchRead[1][sampleIndex];

        // inverse of tests below, we can expect that sample index 32 will be the peak of a period
        // therefore the value stored there should be the periodLength
        if(markedPeriodLength == periodLength)
            CHECK(indexInPeriod == 32);

        // Known indices of peak sample values in test scenario
        if(sampleIndex == 32)
            CHECK(markedPeriodLength == periodLength);
        else if(sampleIndex == 161)
            CHECK(markedPeriodLength == periodLength);
        else if(sampleIndex == 290)
            CHECK(markedPeriodLength == periodLength); 
        else if(sampleIndex == 419)
            CHECK(markedPeriodLength == periodLength);
        else if(sampleIndex == 548)
            CHECK(markedPeriodLength == periodLength);
        else if(sampleIndex == 677)
            CHECK(markedPeriodLength == periodLength); 
        else if(sampleIndex == 806)
            CHECK(markedPeriodLength == periodLength); 
        else if(sampleIndex == 935)
            CHECK(markedPeriodLength == periodLength); 
        else
            CHECK(markedPeriodLength == 0.f);

    }

    // auto outputPath = BufferWriter::getTestOutputPath("PitchMarkBuffer.json");
    // juce::File pitchJson(outputPath);
    // BufferWriter::writeToJson(pitchBuffer, pitchJson);

}


//==================
// TEST_CASE("")
// {

// }