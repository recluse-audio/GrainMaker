#include "../SOURCE/PITCH/PitchMarkedCircularBuffer.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+




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
    int numSamples = 1024;
    int numChannels = 2;
    juce::AudioBuffer<float> ioBuffer(numChannels,numSamples);
    juce::AudioBuffer<float> pitchBuffer(numChannels, numSamples);

    ioBuffer.clear();
    pitchBuffer.clear();

    // this just to make sure we passed audio data successfully
    BufferFiller::fillWithAllOnes(ioBuffer);

    PitchMarkedCircularBuffer circularBuffer;

    float pitchInHz = 100.f;
    circularBuffer.pushBufferAndPitch(ioBuffer, pitchInHz);
    circularBuffer.popBufferAndPitch(ioBuffer, pitchBuffer);

    auto ioRead = ioBuffer.getArrayOfReadPointers();
    auto pitchRead = pitchBuffer.getArrayOfReadPointers();
    

    for(int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
    {
        for(int ch = 0; ch < numChannels; ch++)
        {
            auto ioSample = ioRead[ch][sampleIndex];
            auto pitchSample = pitchRead[ch][sampleIndex];
            CHECK(ioSample == 1.f);
            CHECK(pitchSample == 100.f);
        }
    }
}