#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferWriter.h"
#include "../SOURCE/PITCH/GrainCorrector.h"

/**
 * @brief For testing this creates and fills a PitchMarkedCircularBuffer.
 * 
 * Essential prep stuff for GrainCorrector Testing
 * 
 * By default, this simulates a sine wave with period of 128 samples going into 
 * a PitchMarkedCircularBuffer in process blocks of size 1024.  The PitchMarkedCircularBuffer is filled with 4 of these pseudo process blocks.
 */
class SineWavePitchBuffer
{
public:
    //
    struct Data
    {
        // make these into a struct, have a set function?
        int periodLength = 128;
        int numChannels = 2;
        int circularBufferLength = 4096;
        int numProcessBlocksToPush = 4; 
    };

    SineWavePitchBuffer()
    {

        // // These are what we will use as expected values when testing reading from pitch buffer
        // mAudioDataBuffer.setSize(mData.numChannels, mData.processBlockNumSamples);
        // mPitchMarkBuffer.setSize(mData.numChannels, mData.processBlockNumSamples);

        // mAudioDataBuffer.clear();
        // mPitchMarkBuffer.clear();
        mSineCycleBuffer.setSize(mData.numChannels, mData.periodLength);

        // this just to make sure we passed audio data successfully
        BufferFiller::generateSineCycles(mSineCycleBuffer, mData.periodLength);

        mPitchMarkedCircularBuffer.setSize(mData.numChannels, mData.circularBufferLength);

        int numCycles = mData.circularBufferLength / mData.periodLength; // 

        for(int i = 0; i < numCycles; i++)
            mPitchMarkedCircularBuffer.pushBufferAndPeriod(mSineCycleBuffer, mData.periodLength);

    }


    //
    ~SineWavePitchBuffer(){}

    //
    PitchMarkedCircularBuffer& getReference()
    {
        return mPitchMarkedCircularBuffer;
    }



    PitchMarkedCircularBuffer mPitchMarkedCircularBuffer;


    juce::AudioBuffer<float> mSineCycleBuffer;

    Data mData;

private:
    void _updateData(SineWavePitchBuffer::Data newData)
    {
        mData.periodLength = newData.periodLength;
        mData.numChannels = newData.numChannels;
        mData.circularBufferLength = newData.circularBufferLength;
        mData.numProcessBlocksToPush = newData.numProcessBlocksToPush;

        
    }



};


/**
 * @brief friend class to the GrainCorrector
 * 
 */
class GrainCorrectorTest
{
public:

    //
    GrainCorrectorTest(GrainCorrector& corrector) : mGrainCorrector(corrector)
    {

    }

    // lets you grab the underlying audio buffer used for analysis in GrainCorrector
    // This might be to test that mAnalysisAudioBuffer is properly sized after calling
    // prepare() or setOutputDelay()
    juce::AudioBuffer<float>& getAnalysisAudioBuffer()
    {
        return mGrainCorrector.mAnalysisAudioBuffer;
    }

    //
    GrainCorrector& mGrainCorrector;

};

//===========
//
TEST_CASE("Basic IO, no shift or correction.")
{
    int processBlockSize = 128;
    // this is what we will write to and test to make sure we wrote properly
    juce::AudioBuffer<float> processBlock(2, processBlockSize);
    processBlock.clear();

    SineWavePitchBuffer sineWavePitchBuffer;
    GrainCorrector grainCorrector(sineWavePitchBuffer.getReference()); 

    grainCorrector.prepare(44100, processBlockSize);
    grainCorrector.process(processBlock);

    auto processBlockReadPtr = processBlock.getArrayOfReadPointers();
    auto sineWaveReadPtr = sineWavePitchBuffer.mSineCycleBuffer.getArrayOfReadPointers();

    // no shifting / output_delay happening right now, so this tests that we pop the correct data
    for(int blockIndex = 0; blockIndex < processBlockSize; blockIndex++)
    {
        auto processBlockSample = processBlockReadPtr[0][blockIndex];
        auto sineSample = sineWaveReadPtr[0][blockIndex];

        CHECK(processBlockSample == sineSample);
    }

}

//==============
//
TEST_CASE("Basic IO with output delay.")
{
    SineWavePitchBuffer sineWavePitchBuffer;
    GrainCorrector grainCorrector(sineWavePitchBuffer.getReference()); 
    GrainCorrectorTest grainTest(grainCorrector);

    int processBlockSize = 128;
    int outputDelay = 64;

    // this is what we will write to and test to make sure we wrote properly
    juce::AudioBuffer<float> processBuffer(2, processBlockSize);
    processBuffer.clear();



    grainCorrector.prepare(44100, processBlockSize);
    grainCorrector.setOutputDelay(outputDelay);
    grainCorrector.process(processBuffer);

    auto processBufferReadPtr = processBuffer.getArrayOfReadPointers();
    auto analysisBufferReadPtr = grainTest.getAnalysisAudioBuffer().getArrayOfReadPointers();
    auto sineWaveBufferPtr = sineWavePitchBuffer.mSineCycleBuffer.getArrayOfReadPointers();

    // since the sineWaveBuffer is filled with predictable cycles, we will iterate through using this
    int periodLength = sineWavePitchBuffer.mData.periodLength;

    // This part tests that the analysis window is reading at 0 delay
    // it will match the sine wave 
    for(int blockIndex = 0; blockIndex < periodLength; blockIndex++)
    {
        auto sineSample = sineWaveBufferPtr[0][blockIndex];
        auto analysisSample = analysisBufferReadPtr[0][blockIndex];

        CHECK(analysisSample == sineSample);
    }

    // This part tests that the processBuffer is correctly delayed.
    // We expect that the sine wave buffer will begin at 180 degrees of phase
    // given the outputDelay above.  Only go to outputDelay so we don't have to wrap the sineWaveBuffer
    for(int index = 0; index < outputDelay; index++)
    {
        auto sineSample = sineWaveBufferPtr[0][index+outputDelay];
        auto processSample = processBufferReadPtr[0][index];

        CHECK(processSample == sineSample);    
    }
}

//==============
//
TEST_CASE("Test pitch shift up.")
{
    SineWavePitchBuffer sineWavePitchBuffer;
    GrainCorrector grainCorrector(sineWavePitchBuffer.getReference()); 
    GrainCorrectorTest grainTest(grainCorrector);

    // since the sineWaveBuffer is filled with predictable cycles, we will iterate through using this
    int periodLength = sineWavePitchBuffer.mData.periodLength;
    int processBlockSize = periodLength;
    int outputDelay = periodLength; // differs from above test, this 
    float shiftRatio = 1.1;

    // this is what we will write to and test to make sure we wrote properly
    juce::AudioBuffer<float> processBuffer(2, processBlockSize);
    processBuffer.clear();



    grainCorrector.prepare(44100, processBlockSize);
    grainCorrector.setOutputDelay(outputDelay);
    grainCorrector.setShiftRatio(shiftRatio);
    grainCorrector.process(processBuffer);

    auto processBufferReadPtr = processBuffer.getArrayOfReadPointers();
    auto analysisBufferReadPtr = grainTest.getAnalysisAudioBuffer().getArrayOfReadPointers();
    auto sineWaveBufferPtr = sineWavePitchBuffer.mSineCycleBuffer.getArrayOfReadPointers();



    // This part tests that the processBuffer is correctly delayed.
    // We expect that the sine wave buffer will begin at 180 degrees of phase
    // given the outputDelay above.  Only go to outputDelay so we don't have to wrap the sineWaveBuffer
    for(int index = 0; index < outputDelay; index++)
    {
        auto sineSample = sineWaveBufferPtr[0][index+outputDelay];
        auto processSample = processBufferReadPtr[0][index];

        CHECK(processSample == sineSample);    
    }
}