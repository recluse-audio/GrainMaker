/**
 * Created by Ryan Devens 2025-10-13
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/GRAIN/Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
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
};

TEST_CASE("Granulator prepare() initializes correctly", "[Granulator][prepare]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	Granulator granulator;

	const double testSampleRate = 44100.0;
	const int testBlockSize = 512;
	const int testLookaheadSize = 2048;

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
		Window& window = granulator.getWindow();

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

