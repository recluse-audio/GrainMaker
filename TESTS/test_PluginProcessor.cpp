/**
 * test_PluginProcessor.cpp
 * Created by Ryan Devens
 *
 * Tests for PluginProcessor's getRange functions with explicit sample indices
 * for a clear mental model of the timing relationships.
 *
 * Setup:
 * - Sine buffer: 2048 samples, 2 channels, period of 256 samples
 * - Sample rate: 48000
 * - Block size: 128
 * - Process 128 samples at a time, 13 calls total
 * - Test sections occur on the 13th processBlock call
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/PluginProcessor.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

//==============================================================================
// Test Constants - Explicit values for mental model
//==============================================================================
namespace TestConfig
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int sineBufferSize = 2048;
	constexpr int sinePeriod = 256;
	constexpr int numChannels = 2;
	constexpr int numProcessCalls = 13;
}

//==============================================================================
//==============================================================================
//========================= getProcessCounterRange() ===========================
//==============================================================================
//==============================================================================

TEST_CASE("PluginProcessor getProcessCounterRange() returns correct sample indices", "[PluginProcessor][getProcessCounterRange]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create processor and prepare
	PluginProcessor processor;
	processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

	// Create sine buffer: 2048 samples, 2 channels, period of 256
	juce::AudioBuffer<float> sineBuffer(TestConfig::numChannels, TestConfig::sineBufferSize);
	sineBuffer.clear();
	BufferFiller::generateSineCycles(sineBuffer, TestConfig::sinePeriod);

	// Create process buffer sized for one block
	juce::AudioBuffer<float> processBuffer(TestConfig::numChannels, TestConfig::blockSize);
	juce::MidiBuffer midiBuffer;

	// Process 13 blocks of 128 samples each
	// After each call, mSamplesProcessed increments by blockSize
	//
	// Call  | Before call mSamplesProcessed | Range returned by getProcessCounterRange()
	// ------|-------------------------------|--------------------------------------------
	//   1   |            0                  | (0, 127)
	//   2   |          128                  | (128, 255)
	//   3   |          256                  | (256, 383)
	//   4   |          384                  | (384, 511)
	//   5   |          512                  | (512, 639)
	//   6   |          640                  | (640, 767)
	//   7   |          768                  | (768, 895)
	//   8   |          896                  | (896, 1023)
	//   9   |         1024                  | (1024, 1151)
	//  10   |         1152                  | (1152, 1279)
	//  11   |         1280                  | (1280, 1407)
	//  12   |         1408                  | (1408, 1535)
	//  13   |         1536                  | (1536, 1663)

	// All setup, we are testing the 13th call
	for (int callIndex = 1; callIndex <= TestConfig::numProcessCalls - 1; ++callIndex)
	{
		// Calculate which portion of sineBuffer to copy into processBuffer
		int sourceStartSample = ((callIndex - 1) * TestConfig::blockSize) % TestConfig::sineBufferSize;

		// Copy 128 samples from sineBuffer into processBuffer
		for (int ch = 0; ch < TestConfig::numChannels; ++ch)
		{
			for (int sample = 0; sample < TestConfig::blockSize; ++sample)
			{
				int sourceIndex = (sourceStartSample + sample) % TestConfig::sineBufferSize;
				processBuffer.setSample(ch, sample, sineBuffer.getSample(ch, sourceIndex));
			}
		}
		// Process the block
		processor.processBlock(processBuffer, midiBuffer);
	}


	SECTION("After 12th processBlock call, range is (1536, 1663)")
	{
		auto [start, end] = processor.getProcessCounterRange();

		// mSamplesProcessed = 12 * 128 = 1536 before 13th call
		// start = mSamplesProcessed = 1536
		// end = mSamplesProcessed + blockSize - 1 = 1536 + 128 - 1 = 1663
		CHECK(start == 1536);
		CHECK(end == 1663);
	}
	
}
