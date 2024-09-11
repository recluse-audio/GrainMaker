#include "DSP_TOOLS/CircularBuffer.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "TEST_UTILS/TestUtils.h"
#include "TEST_UTILS/BufferGenerator.h"

// README: While I normally don't use really short abbreviations, I may use "cb" to mean CircularBuffer in this file

// friend class that retrieves private variables for testing purposes.
class CircularBufferTest
{
public:

    CircularBufferTest(CircularBuffer& cb) : mCircularBuffer(cb)
    {

    }

    // these are real safety net test more so to make sure I don't niss a juce function call
    int getNumSamples() { return mCircularBuffer.mCircularBuffer.getNumSamples(); }
    int getNumChannels() { return mCircularBuffer.mCircularBuffer.getNumChannels(); }
    int getWritePos() { return mCircularBuffer.mWritePos; }
    int getReadPos() { return mCircularBuffer.mReadPos; }
    float getSample(int ch, int sampleIndex) { return mCircularBuffer.mCircularBuffer.getSample(ch, sampleIndex); }

private:
    CircularBuffer& mCircularBuffer;

};

TEST_CASE("Can set size of CircularBuffer")
{
    CircularBuffer cb;
    CircularBufferTest cbTest(cb); 

    int numChan = 1;
    int numSamples = 96000;

    cb.setSize(numChan, numSamples);

    CHECK(cbTest.getNumSamples() == numSamples);
    CHECK(cbTest.getNumChannels() ==  numChan);
    CHECK(cbTest.getWritePos() == 0); // writePos should reset to 0 after resizing, which clears the whole buffer
    CHECK(cbTest.getReadPos() == 0); // readPos should reset to 0 after resizing, which clears the whole buffer
}


//==============================
TEST_CASE("Can write/read properly")
{
    CircularBuffer cb;
    CircularBufferTest cbTest(cb); 

    int numChan = 1;
    int numSamples = 96000;

    cb.setSize(numChan, numSamples);
}

//============================
TEST_CASE("Test writing and wrapping in ring buffer ")
{

	SECTION("fail to write if channel config mismatch")
	{
		CircularBuffer circularBuffer;
		
		juce::AudioBuffer<float> inputBuffer(1, 2048);
		circularBuffer.setSize(2, 240000);
		
		bool writeSuccess = circularBuffer.pushBuffer(inputBuffer);
		CHECK(writeSuccess == false);

	}
	
	SECTION("Write position is correct after wrapping ")
	{
        CircularBuffer cb;
        CircularBufferTest cbTest(cb); 
		cb.setSize(1, 3);

		juce::AudioBuffer<float> wrapBuffer(1, 5);
		cb.pushBuffer(wrapBuffer);
		CHECK(cbTest.getWritePos() == 2);

	}

	
}

//===============================
//
TEST_CASE("test writing chunks of ring buffer")
{
    CircularBuffer cb;
    CircularBufferTest cbTest(cb); 
	cb.setSize(1, 5);

	float data[] = {1.f, 2.f, 3.f, 4.f, 5.f };
	juce::AudioBuffer<float> sourceBuffer(1, 5);
	for(auto i = 0; i < 5; i++)
		sourceBuffer.setSample(0, i, data[i]);

	cb.pushBuffer(sourceBuffer);
	
	for(int j = 0; j < 5; j++)
		CHECK(cbTest.getSample(0, j) == sourceBuffer.getSample(0, j));
}




//===============================
//
TEST_CASE("Can read from buffer.")
{
    CircularBuffer cb;
    CircularBufferTest cbTest(cb); 

    int testBufferSize = 1024;
    juce::AudioBuffer<float> testBuffer(2, testBufferSize);
    BufferGenerator::fillWithAllOnes(testBuffer);

    // make sure testBuffer filled properly
    for(int sampleIndex = 0; sampleIndex < testBuffer.getNumSamples(); sampleIndex++)
    {
        CHECK(testBuffer.getSample(0, sampleIndex) == 1.f);
    }
	
    // pushes or "writes" the all one buffer into the circular buffer
    cb.pushBuffer(testBuffer);

    // going to read and pop from the circular buffer into this
    juce::AudioBuffer<float> readBuffer(2, testBufferSize);
    readBuffer.clear();

    // read a buffer starting at 0 and reading the length of the input buffer
    cb.readRange(readBuffer, 0);
    for(int sampleIndex = 0; sampleIndex < readBuffer.getNumSamples(); sampleIndex++)
    {
        CHECK(readBuffer.getSample(0, sampleIndex) == 1.f);
        CHECK(cbTest.getReadPos() == 0); // read range should not update internal mReadPos, unlike popBuffer();
    }
    readBuffer.clear();

    // pop a buffer starting at 0 and reading the length of the input buffer.  should update readPos as well
    cb.popBuffer(readBuffer);
    for(int sampleIndex = 0; sampleIndex < readBuffer.getNumSamples(); sampleIndex++)
    {
        CHECK(readBuffer.getSample(0, sampleIndex) == 1.f);
        CHECK(cbTest.getReadPos() == testBufferSize);
    }

    
}



