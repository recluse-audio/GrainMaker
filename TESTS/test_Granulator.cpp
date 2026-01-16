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
	static int getActiveGrainBuffer(Granulator& granulator)
	{
		return granulator.mActiveGrainBuffer;
	}

	static bool getNeedToFillActive(Granulator& granulator)
	{
		return granulator.mNeedToFillActive;
	}

	static juce::AudioBuffer<float>& getGrainBuffer1(Granulator& granulator)
	{
		return granulator.mGrainBuffer1;
	}

	static juce::AudioBuffer<float>& getGrainBuffer2(Granulator& granulator)
	{
		return granulator.mGrainBuffer2;
	}

	static int getProcessBlockSize(Granulator& granulator)
	{
		return granulator.mProcessBlockSize;
	}

	static Window& getWindow(Granulator& granulator) 
	{ 
		return *granulator.mWindow.get(); 
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
		// Check that both grain buffers have the correct size
		CHECK(GranulatorTester::getGrainBuffer1(granulator).getNumSamples() == testLookaheadSize);
		CHECK(GranulatorTester::getGrainBuffer2(granulator).getNumSamples() == testLookaheadSize);

		// Check they are stereo (2 channels)
		CHECK(GranulatorTester::getGrainBuffer1(granulator).getNumChannels() == 2);
		CHECK(GranulatorTester::getGrainBuffer2(granulator).getNumChannels() == 2);
	}

	SECTION("Active grain buffer is initialized to 0")
	{
		CHECK(GranulatorTester::getActiveGrainBuffer(granulator) == 0);
	}

	SECTION("mNeedToFillActive is set to true")
	{
		CHECK(GranulatorTester::getNeedToFillActive(granulator) == true);
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

TEST_CASE("Granulator first call to granulate() flips mNeedToFillActive as expected", "[Granulator][granulate]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;
	MockProcessorInput input;

	// Call prepare
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);

	// Verify mNeedToFillActive is true before first granulate call
	CHECK(GranulatorTester::getNeedToFillActive(granulator) == true);

	// First call to granulate should flip mNeedToFillActive to false
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);

	// Verify mNeedToFillActive is now false after first granulate call
	CHECK(GranulatorTester::getNeedToFillActive(granulator) == false);

	// Call prepare again
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);

	// Verify mNeedToFillActive is true before first granulate call
	CHECK(GranulatorTester::getNeedToFillActive(granulator) == true);
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

	Window& window = GranulatorTester::getWindow(granulator);
	window.setShape(Window::Shape::kNone);

	// Call prepare
	granulator.prepare(input.testSampleRate, input.testBlockSize, input.testLookaheadSize);

	CHECK(BufferHelper::isSilent(GranulatorTester::getGrainBuffer1(granulator)) == true);
	CHECK(BufferHelper::isSilent(GranulatorTester::getGrainBuffer2(granulator)) == true);

	// granulate one processBuffer, 
	// grainBuffer1 is filled with entire input.lookaheadBuffer
	// grainBuffer2 is still all 0's
	granulator.granulate(input.lookaheadBuffer, input.processBuffer, input.detectedPeriod, input.shiftedPeriod);
	CHECK(BufferHelper::buffersAreIdentical(GranulatorTester::getGrainBuffer1(granulator), input.lookaheadBuffer)); 
	CHECK(BufferHelper::isSilent(GranulatorTester::getGrainBuffer2(granulator)) == true);

}