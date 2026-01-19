/**
 * Created by Ryan Devens 2025-10-13
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/GRAIN/Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

/**
 * @brief Test helper class to access private members of Granulator
 */
class GranulatorTester
{
public:
	// Buffer state index accessors
	static int getReadingBufferIndex(Granulator& granulator)
	{
		return granulator.mReadingBufferIndex;
	}

	static int getWritingBufferIndex(Granulator& granulator)
	{
		return granulator.mWritingBufferIndex;
	}

	static int getSpilloverBufferIndex(Granulator& granulator)
	{
		return granulator.mSpilloverBufferIndex;
	}

	static bool getNeedToFillReading(Granulator& granulator)
	{
		return granulator.mNeedToFillReading;
	}

	// Buffer access by state
	static juce::AudioBuffer<float>& getReadingBuffer(Granulator& granulator)
	{
		return granulator._getReadingBuffer();
	}

	static juce::AudioBuffer<float>& getWritingBuffer(Granulator& granulator)
	{
		return granulator._getWritingBuffer();
	}

	static juce::AudioBuffer<float>& getSpilloverBuffer(Granulator& granulator)
	{
		return granulator._getSpilloverBuffer();
	}

	// Get the specific buffer in the array of buffers by index regardless of state
	static juce::AudioBuffer<float>& getBufferByIndex(Granulator& granulator, int index)
	{
		// keep in bounds
		if(index < 0 )
			index = 0;
		if((size_t)index >= granulator.mGrainBuffers.size())
			index = granulator.mGrainBuffers.size() - 1;

		return granulator.mGrainBuffers[index];
	}

	//
	static int getNumGrainBuffers(Granulator& granulator)
	{
		return static_cast<int>(granulator.mGrainBuffers.size());
	}
	//
	static int getProcessBlockSize(Granulator& granulator)
	{
		return granulator.mProcessBlockSize;
	}

	static Window& getWindow(Granulator& granulator)
	{
		return *granulator.mWindow.get();
	}

	static juce::int64 getGrainReadPos(Granulator& granulator)
	{
		return granulator.mGrainReadPos;
	}

	static juce::int64 getProcessBlockWritePos(Granulator& granulator)
	{
		return granulator.mProcessBlockWritePos;
	}

	static void granulateToGrainBuffer(Granulator& granulator, juce::AudioBuffer<float>& lookahead,
									   juce::AudioBuffer<float>& grainBuffer, juce::AudioBuffer<float>& spilloverBuffer,
									   float detectedPeriod, float shiftedPeriod)
	{
		granulator._granulateToGrainBuffer(lookahead, grainBuffer, spilloverBuffer, detectedPeriod, shiftedPeriod);
	}
};

/**
 * @brief Helper struct to encapsulate common test input parameters for Granulator
 * Provides configurable mock processor input with sensible defaults
 */
struct MockProcessorInput
{
	// Configuration parameters
	double testSampleRate;
	int testBlockSize;
	int testLookaheadSize;

	// Audio buffers
	juce::AudioBuffer<float> lookaheadBuffer;
	juce::AudioBuffer<float> processBuffer;

	// Period parameters
	float detectedPeriod;
	float shiftedPeriod;

	/**
	 * @brief Construct mock input with default values
	 * @param sampleRate Sample rate for processing (default: 44100.0)
	 * @param blockSize Process block size (default: 512)
	 * @param lookaheadSize Lookahead buffer size (default: 2048)
	 * @param period Detected period in samples (default: 128.0f)
	 * @param shiftedPer Shifted period in samples (default: same as period)
	 */
	MockProcessorInput(
		double sampleRate = 48000.0,
		int blockSize = 256,
		int lookaheadSize = 1024,
		float period = 128.0f,
		float shiftedPer = 128.0f
	)
		: testSampleRate(sampleRate)
		, testBlockSize(blockSize)
		, testLookaheadSize(lookaheadSize)
		, lookaheadBuffer(2, lookaheadSize)
		, processBuffer(2, blockSize)
		, detectedPeriod(period)
		, shiftedPeriod(shiftedPer)
	{
		lookaheadBuffer.clear();
		processBuffer.clear();
		BufferFiller::fillWithAllOnes(lookaheadBuffer);
		BufferFiller::fillWithAllOnes(processBuffer);
	}

	/**
	 * @brief Fill buffers with custom values
	 */
	void fillBuffers(float lookaheadValue, float processValue)
	{
		BufferFiller::fillWithValue(lookaheadBuffer, lookaheadValue);
		BufferFiller::fillWithValue(processBuffer, processValue);
	}

	/**
	 * @brief Clear both buffers
	 */
	void clearBuffers()
	{
		lookaheadBuffer.clear();
		processBuffer.clear();
	}
};


//==============================================================================
//==============================================================================
//=============================== prepare() ====================================
//==============================================================================
//==============================================================================

TEST_CASE("Granulator prepare() initializes correctly", "[Granulator][prepare]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;

	const double testSampleRate = 48000.0;
	const int testBlockSize = 256;
	const int testLookaheadSize = 1024;

	// Call prepare
	granulator.prepare(testSampleRate, testBlockSize, testLookaheadSize);

	SECTION("Grain buffers are sized correctly")
	{
		// Check that all three grain buffers have the correct size
		CHECK(GranulatorTester::getReadingBuffer(granulator).getNumSamples() == testLookaheadSize);
		CHECK(GranulatorTester::getWritingBuffer(granulator).getNumSamples() == testLookaheadSize);
		CHECK(GranulatorTester::getSpilloverBuffer(granulator).getNumSamples() == testLookaheadSize);

		// Check they are stereo (2 channels)
		CHECK(GranulatorTester::getReadingBuffer(granulator).getNumChannels() == 2);
		CHECK(GranulatorTester::getWritingBuffer(granulator).getNumChannels() == 2);
		CHECK(GranulatorTester::getSpilloverBuffer(granulator).getNumChannels() == 2);
	}

	SECTION("Buffer state indices are initialized correctly")
	{
		CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
		CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
		CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);
	}

	SECTION("mNeedToFillReading is set to true")
	{
		CHECK(GranulatorTester::getNeedToFillReading(granulator) == true);
	}

	SECTION("Window is configured correctly")
	{
		Window& window = GranulatorTester::getWindow(granulator);

		// Check window is set to looping
		CHECK(window.getIsLooping() == true);

		// Check window shape is Hanning
		CHECK(window.getShape() == Window::Shape::kHanning);

		// Check window period is set to blockSize
		CHECK(window.getPeriod() == testBlockSize);

		// Check window's underlying size is set to equivalent of 1 second at samplerate
		CHECK(window.getSize() == testSampleRate);
	}

	SECTION("Process block size is stored correctly")
	{
		CHECK(GranulatorTester::getProcessBlockSize(granulator) == testBlockSize);
	}
}

//********************************************************************************************/


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================

TEST_CASE("Granulator first call to granulate() flips mNeedToFillReading as expected", "[Granulator][granulate]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;
	MockProcessorInput input;

	// Call prepare
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);

	// Verify mNeedToFillReading is true before first granulate call
	CHECK(GranulatorTester::getNeedToFillReading(granulator) == true);

	// First call to granulate should flip mNeedToFillReading to false
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);

	// Verify mNeedToFillReading is now false after first granulate call
	CHECK(GranulatorTester::getNeedToFillReading(granulator) == false);

	// Call prepare again
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);

	// Verify mNeedToFillReading is true before first granulate call
	CHECK(GranulatorTester::getNeedToFillReading(granulator) == true);
}


//********************************************************************************************/


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
TEST_CASE("Granulator fills GrainBuffers appropriately", "[Granulator][grainBuffer]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;
	MockProcessorInput input;

	// Call prepare
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);


	// Only want to test that all 1's buffer's are read from lookahead to grainBuffer in this test.
	// Need to remove windowing for high contrast. Window shape is set in prepare() by default.
	Window& window = GranulatorTester::getWindow(granulator);
	window.setShape(Window::Shape::kNone);

	// Check that buffers are silent where they should be and 
	// flags for grainBuffer indices are in expected starting config
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getSpilloverBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANULATE #1 [0-255]
	// Reading buffer is filled with entire input.lookaheadBuffer
	// Writing and Spillover buffers are still all 0's
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	
	// Check that flags for grainBuffer indices are still in expected starting config
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// double check that we are starting with index [0] as expected. mGrainBuffer[0] should be first read buffer
	REQUIRE(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);

	// GRANULATE #2 [256-512]
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	REQUIRE(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANULATE #3 [512-767]
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	REQUIRE(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANLULATE #4 [768-1023]
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	REQUIRE(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANULATE #5 --TRANSITION-- [1024-1280]
	// at this point we should have switched to index 1 buffer and cleared index 0
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	REQUIRE(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 1), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 0)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 1)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 2)) == true);
	// Indices
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 2);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 0);

}

//********************************************************************************************/


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
TEST_CASE("Read/write pos related to lookahead/grainBuffers/processBlock all add up", "[Granulator][grainBuffer]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;
	MockProcessorInput input;

	// Call prepare first so buffers exist
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);

	juce::AudioBuffer<float>& readingBuffer = GranulatorTester::getReadingBuffer(granulator);
	juce::AudioBuffer<float>& lookaheadBuffer = input.lookaheadBuffer;
	lookaheadBuffer.clear();
	BufferFiller::fillIncremental(lookaheadBuffer);

	// Only want to test that incremental buffer values are read from lookahead to grainBuffer in this test.
	// Need to remove windowing for high contrast. Window shape is set in prepare() by default.
	Window& window = GranulatorTester::getWindow(granulator);
	window.setShape(Window::Shape::kNone);

	// Call private function in Granulator
	juce::AudioBuffer<float>& spilloverBuffer = GranulatorTester::getSpilloverBuffer(granulator);
	GranulatorTester::granulateToGrainBuffer(granulator, input.lookaheadBuffer, readingBuffer, spilloverBuffer, input.detectedPeriod, input.shiftedPeriod);

	int numSamples  = GranulatorTester::getReadingBuffer(granulator).getNumSamples();
	int numChannels = GranulatorTester::getReadingBuffer(granulator).getNumChannels();

	for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
	{
		for (int ch = 0; ch < numChannels; ++ch)
		{
			float grainSample = readingBuffer.getSample(ch, sampleIndex);
			CHECK(grainSample == (float)sampleIndex);
		}
	}
}
//********************************************************************************************/


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
TEST_CASE("mGrainReadPos is incremented properly when writing to processBlock()", "[Granulator][grainBuffer]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;
	MockProcessorInput input;

	juce::AudioBuffer<float>& lookaheadBuffer = input.lookaheadBuffer;
	lookaheadBuffer.clear();
	BufferFiller::fillIncremental(lookaheadBuffer);

	// Call prepare
	// Only want to test that incremental values are read from lookahead to grainBuffer in this test.
	// Need to remove windowing for high contrast. Window shape is set in prepare() by default.
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);
	Window& window = GranulatorTester::getWindow(granulator);
	window.setShape(Window::Shape::kNone);

	// Call granulate which fills reading buffer and writes to processBuffer
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);

	int numSamples  = input.processBuffer.getNumSamples();
	int numChannels = input.processBuffer.getNumChannels();

	// make sure incremental lookaheadBuffer data made it to processBuffer
	for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
	{
		for (int ch = 0; ch < numChannels; ++ch)
		{
			float processBufferSample = input.processBuffer.getSample(ch, sampleIndex);
			CHECK(processBufferSample == (float)sampleIndex);
		}
	}

	// at this point, we've should have taken 256 samples (process block size) of reading buffer data [0-255]
	// and written it to sample indices [0-255] of the processBuffer
	CHECK(input.processBuffer.getNumSamples() == 256);
}

//********************************************************************************************/


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
TEST_CASE("Testing '' - how grainBuffers are filled from lookaheadBuffer", "[Granulator][grainBuffer]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	Granulator granulator;
	MockProcessorInput input;

	// Fill lookaheadBuffer with grains who's sample values correspond with their grain number
	// Grain size = 2 * detectedPeriod (2 cycles per grain)
	// With detectedPeriod = 128, each grain is 256 samples
	// grain 1: [0-255] -> set to 1.f
	// grain 2: [256-511] -> set to 2.f
	// grain 3: [512-767] -> set to 3.f
	// grain 4: [768-1023] -> set to 4.f

	juce::AudioBuffer<float>& lookaheadBuffer = input.lookaheadBuffer;
	lookaheadBuffer.clear();
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 0, 255, 1.f);
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 256, 511, 2.f);
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 512, 767, 3.f);
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 768, 1023, 4.f);


	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);
	Window& window = GranulatorTester::getWindow(granulator);
	window.setShape(Window::Shape::kNone);

	juce::AudioBuffer<float>& readingBuffer = GranulatorTester::getReadingBuffer(granulator);
	int numChannels = readingBuffer.getNumChannels();


}
//********************************************************************************************/


//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
//==============================================================================
TEST_CASE("Testing '_granulateToGrainBuffer()' - how grainBuffers are filled from lookaheadBuffer", "[Granulator][grainBuffer]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	Granulator granulator;
	MockProcessorInput input;

	// Fill lookaheadBuffer with grains who's sample values correspond with their grain number
	// Grain size = 2 * detectedPeriod (2 cycles per grain)
	// With detectedPeriod = 128, each grain is 256 samples
	// grain 1: [0-255] -> set to 1.f
	// grain 2: [256-511] -> set to 2.f
	// grain 3: [512-767] -> set to 3.f
	// grain 4: [768-1023] -> set to 4.f

	juce::AudioBuffer<float>& lookaheadBuffer = input.lookaheadBuffer;
	lookaheadBuffer.clear();
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 0, 255, 1.f);
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 256, 511, 2.f);
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 512, 767, 3.f);
	BufferFiller::fillRangeWithValue(lookaheadBuffer, 768, 1023, 4.f);


	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);
	Window& window = GranulatorTester::getWindow(granulator);
	window.setShape(Window::Shape::kNone);

	juce::AudioBuffer<float>& readingBuffer = GranulatorTester::getReadingBuffer(granulator);
	juce::AudioBuffer<float>& spilloverBuffer = GranulatorTester::getSpilloverBuffer(granulator);
	int numChannels = readingBuffer.getNumChannels();

	SECTION("No shifting, grains of size 256 (2x period of 128) are positioned accordingly.")
	{
		input.detectedPeriod = 128.f;
		input.shiftedPeriod = 128.f; // no shifting

		GranulatorTester::granulateToGrainBuffer(granulator, input.lookaheadBuffer, readingBuffer, spilloverBuffer,
												input.detectedPeriod, input.shiftedPeriod);

		// GRAIN 1: [0-255] -> 1.f
		for (int i = 0; i <= 255; ++i)
			for (int ch = 0; ch < numChannels; ++ch)
				CHECK(readingBuffer.getSample(ch, i) == 1.f);

		// GRAIN 2: [256-511] -> 2.f
		for (int i = 256; i <= 511; ++i)
			for (int ch = 0; ch < numChannels; ++ch)
				CHECK(readingBuffer.getSample(ch, i) == 2.f);

		// GRAIN 3: [512-767] -> 3.f
		for (int i = 512; i <= 767; ++i)
			for (int ch = 0; ch < numChannels; ++ch)
				CHECK(readingBuffer.getSample(ch, i) == 3.f);

		// GRAIN 4: [768-1023] -> 4.f
		for (int i = 768; i <= 1023; ++i)
			for (int ch = 0; ch < numChannels; ++ch)
				CHECK(readingBuffer.getSample(ch, i) == 4.f);
	}

	SECTION("Shifting up, overlap expected at overlapping indices")
	{
		// input.detectedPeriod = 128.f;
		// input.shiftedPeriod = 96.f; // shifting up (grains placed closer together)

		// GranulatorTester::granulateToGrainBuffer(granulator, input.lookaheadBuffer, grainBuffer1,
		// 										input.detectedPeriod, input.shiftedPeriod);

		// // With shiftedPeriod = 192, grains (256 samples each) are written at intervals of 192 samples
		// // Grain 1: written at [0-255]
		// // Grain 2: written at [192-447] (overlaps with grain 1 at [192-255])
		// // Grain 3: written at [384-639] (overlaps with grain 2 at [384-447])
		// // Grain 4: written at [576-831] (overlaps with grain 3 at [576-639])

		// // GRAIN 1 non-overlap: [0-191] -> 1.f
		// for (int i = 0; i <= 191; ++i)
		// 	for (int ch = 0; ch < numChannels; ++ch)
		// 		CHECK(grainBuffer1.getSample(ch, i) == 1.f);

		// // GRAIN 2 non-overlap: [256-383] -> 2.f
		// for (int i = 256; i <= 383; ++i)
		// 	for (int ch = 0; ch < numChannels; ++ch)
		// 		CHECK(grainBuffer1.getSample(ch, i) == 2.f);

		// // GRAIN 3 non-overlap: [448-575] -> 3.f
		// for (int i = 448; i <= 575; ++i)
		// 	for (int ch = 0; ch < numChannels; ++ch)
		// 		CHECK(grainBuffer1.getSample(ch, i) == 3.f);

		// // GRAIN 4 non-overlap: [640-831] -> 4.f
		// for (int i = 640; i <= 831; ++i)
		// 	for (int ch = 0; ch < numChannels; ++ch)
		// 		CHECK(grainBuffer1.getSample(ch, i) == 4.f);
	}

}

