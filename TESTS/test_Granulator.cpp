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

	// Analysis/Synthesis mark accessors
	static int getNextAnalysisMarkIndex(Granulator& granulator, juce::AudioBuffer<float>& buffer,
										float detectedPeriod, int currentIndex)
	{
		return granulator._getNextAnalysisMarkIndex(buffer, detectedPeriod, currentIndex);
	}

	static int getWindowCenterIndex(Granulator& granulator, juce::AudioBuffer<float>& buffer,
									int analysisMarkIndex, float detectedPeriod)
	{
		return granulator._getWindowCenterIndex(buffer, analysisMarkIndex, detectedPeriod);
	}

	static std::tuple<int, int, int> getWindowBounds(Granulator& granulator, juce::AudioBuffer<float>& buffer,
													 int analysisMarkIndex, float detectedPeriod)
	{
		return granulator._getWindowBounds(buffer, analysisMarkIndex, detectedPeriod);
	}

	static int getNextSynthMarkIndex(Granulator& granulator, float detectedPeriod, float shiftedPeriod,
									 int currentSynthMarkIndex, int currentAnalysisMarkIndex)
	{
		return granulator._getNextSynthMarkIndex(detectedPeriod, shiftedPeriod, currentSynthMarkIndex, currentAnalysisMarkIndex);
	}
};
//=======================================================================================
//=======================================================================================

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
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);

	// GRANULATE #2 [256-511]
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANULATE #3 [512-767]
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANLULATE #4 [768-1023]
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 0), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getReadingBuffer(granulator)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getWritingBuffer(granulator)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 2);

	// GRANULATE #5 [0-255]
	// at this point we should have switched to index 1 buffer and cleared index 0
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 1), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 0)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 1)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 2)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 2);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 0);

	// GRANULATE #6 [256-511]
	// at this point we should have switched to index 1 buffer and cleared index 0
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 1), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 0)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 1)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 2)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 2);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 0);

	// GRANULATE #7 [512-767]
	// at this point we should have switched to index 1 buffer and cleared index 0
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 1), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 0)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 1)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 2)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 2);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 0);

	// GRANULATE #8 [768-1023]
	// at this point we should have switched to index 1 buffer and cleared index 0
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 1), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 0)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 1)) == false);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 2)) == true);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 1);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 2);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 0);

	// GRANULATE #9 --TRANSITION-- [0-255]
	// at this point we should have switched to index 1 buffer and cleared index 0
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getBufferByIndex(granulator, 2), GranulatorTester::getReadingBuffer(granulator)));
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 0)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 1)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getBufferByIndex(granulator, 2)) == false);
	CHECK(GranulatorTester::getReadingBufferIndex(granulator) == 2);
	CHECK(GranulatorTester::getWritingBufferIndex(granulator) == 0);
	CHECK(GranulatorTester::getSpilloverBufferIndex(granulator) == 1);

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
TEST_CASE("Analysis mark index calculations", "[Granulator][Marks]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	Granulator granulator;
	MockProcessorInput input;

	// Unrelated to other test sections in this test case
	// just making sure it handles first lookahead properly (negative current index)
	SECTION("When current index is negative, we create first mark based on peak within first period")
	{
		juce::AudioBuffer<float> firstPeriodBuffer(2, 128);
		firstPeriodBuffer.clear();
		BufferFiller::fillWithAllOnes(firstPeriodBuffer);
		BufferFiller::fillRangeWithValue(firstPeriodBuffer, 64, 64, 1.1); // index 64 will be mark location;
		int result = GranulatorTester::getNextAnalysisMarkIndex(granulator, firstPeriodBuffer, 128, -1);
		CHECK(result == 64);
	}

	juce::AudioBuffer<float>& lookaheadBuffer = input.lookaheadBuffer;
	lookaheadBuffer.clear();

	SECTION("getNextAnalysisMarkIndex returns expected index")
	{
		int result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, -1);
		CHECK(result == 0);
		auto [start1, center1, end1] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start1 == -128);
		CHECK(center1 == 0);
		CHECK(end1 == 127);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 128);
		auto [start2, center2, end2] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start2 == 0);
		CHECK(center2 == 128);
		CHECK(end2 == 255);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 256);
		auto [start3, center3, end3] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start3 == 128);
		CHECK(center3 == 256);
		CHECK(end3 == 383);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 384);
		auto [start4, center4, end4] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start4 == 256);
		CHECK(center4 == 384);
		CHECK(end4 == 511);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 512);
		auto [start5, center5, end5] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start5 == 384);
		CHECK(center5 == 512);
		CHECK(end5 == 639);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 640);
		auto [start6, center6, end6] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start6 == 512);
		CHECK(center6 == 640);
		CHECK(end6 == 767);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 768);
		auto [start7, center7, end7] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start7 == 640);
		CHECK(center7 == 768);
		CHECK(end7 == 895);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 896);
		auto [start8, center8, end8] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start8 == 768);
		CHECK(center8 == 896);
		CHECK(end8 == 1023);

		result = GranulatorTester::getNextAnalysisMarkIndex(granulator, lookaheadBuffer, input.detectedPeriod, result);
		CHECK(result == 0);
		auto [start9, center9, end9] = GranulatorTester::getWindowBounds(granulator, lookaheadBuffer, result, input.detectedPeriod);
		CHECK(start9 == -128);
		CHECK(center9 == 0);
		CHECK(end9 == 127);
	}

	SECTION("getWindowCenterIndex returns expected index")
	{
		// TODO: Set up test conditions
		// int analysisMarkIndex = ...;
		// int result = GranulatorTester::getWindowCenterIndex(granulator, lookaheadBuffer, analysisMarkIndex, input.detectedPeriod);
		// CHECK(result == expectedValue);
	}

	SECTION("getNextSynthMarkIndex returns expected index")
	{
		// TODO: Set up test conditions
		// int currentSynthMarkIndex = ...;
		// int currentAnalysisMarkIndex = ...;
		// int result = GranulatorTester::getNextSynthMarkIndex(granulator, input.detectedPeriod, input.shiftedPeriod, currentSynthMarkIndex, currentAnalysisMarkIndex);
		// CHECK(result == expectedValue);
	}
}

