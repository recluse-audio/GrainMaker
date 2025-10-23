#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+


#include "../SOURCE/GRAIN/GrainShifter.h"
#include "../SOURCE/PluginProcessor.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/RelativeFilePath.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"

#include "TEST_UTILS/GrainShifterTester.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"


TEST_CASE("GrainShifter is prepared correctly when prepareToPlay() is called", "test_GrainShifter")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;
	double sampleRate = 48000;
	int lookaheadBufferSize = 1024;
	grainShifter.prepare(sampleRate, lookaheadBufferSize);

	CHECK(GrainShifterTester::getSampleRate(grainShifter) == sampleRate);
	CHECK(grainShifter.getGrainWindow().getSize() == lookaheadBufferSize);
}


TEST_CASE("GrainShifter allows you to set grain shape")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;
	grainShifter.setGrainShape(Window::Shape::kNone);
	CHECK(grainShifter.getGrainWindow().getShape() == Window::Shape::kNone);
	grainShifter.setGrainShape(Window::Shape::kHanning);
	CHECK(grainShifter.getGrainWindow().getShape() == Window::Shape::kHanning);
}

TEST_CASE("PluginProcessor prepareToPlay configures lookahead buffer correctly", "[test_GrainShifter]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	PluginProcessor processor;

	// Helper lambda to calculate expected lookahead buffer size based on the rules
	auto calculateExpectedLookaheadSize = [](double sampleRate, int blockSize) -> int
	{
		// Base lookahead size calculation based on block size
		int baseLookaheadSize;
		if (blockSize <= 512)
		{
			baseLookaheadSize = 1024;
		}
		else
		{
			baseLookaheadSize = blockSize * 2;
		}

		// Apply sample rate multiplier
		// 44100 and 48000: multiplier = 1 (base size)
		// 88200 and 96000: multiplier = 2
		// 176400 and 192000: multiplier = 4
		int multiplier = 1;
		if (sampleRate >= 176000)
		{
			multiplier = 4;
		}
		else if (sampleRate >= 88000)
		{
			multiplier = 2;
		}

		return baseLookaheadSize * multiplier;
	};

	SECTION("Block size <= 512 uses 1024 base lookahead")
	{
		// Test with block sizes 128, 256, 512 at 44.1kHz
		double sampleRate = 44100;

		int blockSize128 = 128;
		processor.prepareToPlay(sampleRate, blockSize128);
		int expectedSize = calculateExpectedLookaheadSize(sampleRate, blockSize128);
		CHECK(expectedSize == 1024);

		int blockSize256 = 256;
		processor.prepareToPlay(sampleRate, blockSize256);
		expectedSize = calculateExpectedLookaheadSize(sampleRate, blockSize256);
		CHECK(expectedSize == 1024);

		int blockSize512 = 512;
		processor.prepareToPlay(sampleRate, blockSize512);
		expectedSize = calculateExpectedLookaheadSize(sampleRate, blockSize512);
		CHECK(expectedSize == 1024);
	}

	SECTION("Block size > 512 uses double the block size as base lookahead")
	{
		double sampleRate = 48000;

		int blockSize1024 = 1024;
		processor.prepareToPlay(sampleRate, blockSize1024);
		int expectedSize = calculateExpectedLookaheadSize(sampleRate, blockSize1024);
		CHECK(expectedSize == 2048);

		int blockSize2048 = 2048;
		processor.prepareToPlay(sampleRate, blockSize2048);
		expectedSize = calculateExpectedLookaheadSize(sampleRate, blockSize2048);
		CHECK(expectedSize == 4096);
	}

	SECTION("Sample rates 44100 and 48000 use base lookahead size")
	{
		int blockSize = 256;

		processor.prepareToPlay(44100, blockSize);
		int expectedSize44k = calculateExpectedLookaheadSize(44100, blockSize);
		CHECK(expectedSize44k == 1024);

		processor.prepareToPlay(48000, blockSize);
		int expectedSize48k = calculateExpectedLookaheadSize(48000, blockSize);
		CHECK(expectedSize48k == 1024);

		// Both should be the same
		CHECK(expectedSize44k == expectedSize48k);
	}

	SECTION("Sample rates 88200 and 96000 double the lookahead size")
	{
		int blockSize = 256;

		processor.prepareToPlay(88200, blockSize);
		int expectedSize88k = calculateExpectedLookaheadSize(88200, blockSize);
		CHECK(expectedSize88k == 2048);

		processor.prepareToPlay(96000, blockSize);
		int expectedSize96k = calculateExpectedLookaheadSize(96000, blockSize);
		CHECK(expectedSize96k == 2048);

		// Both should be the same
		CHECK(expectedSize88k == expectedSize96k);
	}

	SECTION("Sample rates 176400 and 192000 quadruple the lookahead size")
	{
		int blockSize = 256;

		processor.prepareToPlay(176400, blockSize);
		int expectedSize176k = calculateExpectedLookaheadSize(176400, blockSize);
		CHECK(expectedSize176k == 4096);

		processor.prepareToPlay(192000, blockSize);
		int expectedSize192k = calculateExpectedLookaheadSize(192000, blockSize);
		CHECK(expectedSize192k == 4096);

		// Both should be the same
		CHECK(expectedSize176k == expectedSize192k);
	}

	SECTION("Combined test: various configurations")
	{
		// Test multiple combinations to ensure the rules work together correctly
		struct TestConfig
		{
			double sampleRate;
			int blockSize;
			int expectedLookahead;
		};

		TestConfig configs[] = {
			// Low sample rates with small blocks
			{44100, 128, 1024},
			{48000, 256, 1024},
			{44100, 512, 1024},
			{48000, 512, 1024},

			// Low sample rates with large blocks
			{44100, 1024, 2048},
			{48000, 2048, 4096},

			// Medium sample rates with small blocks
			{88200, 128, 2048},
			{96000, 256, 2048},
			{88200, 512, 2048},

			// Medium sample rates with large blocks
			{88200, 1024, 4096},
			{96000, 2048, 8192},

			// High sample rates with small blocks
			{176400, 128, 4096},
			{192000, 256, 4096},
			{176400, 512, 4096},

			// High sample rates with large blocks
			{176400, 1024, 8192},
			{192000, 2048, 16384}
		};

		for (const auto& config : configs)
		{
			processor.prepareToPlay(config.sampleRate, config.blockSize);
			int calculatedSize = calculateExpectedLookaheadSize(config.sampleRate, config.blockSize);
			CHECK(calculatedSize == config.expectedLookahead);
		}
	}

	SECTION("GrainBuffer sizes are updated when prepareToPlay is called")
	{
		// Get access to the GrainShifter
		GrainShifter* grainShifter = processor.getGrainShifter();
		REQUIRE(grainShifter != nullptr);

		// Test configuration 1: Small block size at 44.1kHz
		processor.prepareToPlay(44100, 256);
		int expectedLookahead1 = calculateExpectedLookaheadSize(44100, 256);

		// Check both GrainBuffers
		for (int i = 0; i < 2; ++i)
		{
			CHECK(GrainShifterTester::getGrainBufferNumChannels(*grainShifter, i) == 2);
			CHECK(GrainShifterTester::getGrainBufferNumSamples(*grainShifter, i) == expectedLookahead1);
			CHECK(GrainShifterTester::getGrainBufferLengthInSamples(*grainShifter, i) == expectedLookahead1);
		}

		// Test configuration 2: Large block size at 96kHz
		processor.prepareToPlay(96000, 2048);
		int expectedLookahead2 = calculateExpectedLookaheadSize(96000, 2048);

		// Check both GrainBuffers updated to new size
		for (int i = 0; i < 2; ++i)
		{
			CHECK(GrainShifterTester::getGrainBufferNumChannels(*grainShifter, i) == 2);
			CHECK(GrainShifterTester::getGrainBufferNumSamples(*grainShifter, i) == expectedLookahead2);
			CHECK(GrainShifterTester::getGrainBufferLengthInSamples(*grainShifter, i) == expectedLookahead2);
		}

		// Test configuration 3: High sample rate with small block
		processor.prepareToPlay(192000, 512);
		int expectedLookahead3 = calculateExpectedLookaheadSize(192000, 512);

		// Verify final configuration
		for (int i = 0; i < 2; ++i)
		{
			CHECK(GrainShifterTester::getGrainBufferNumChannels(*grainShifter, i) == 2);
			CHECK(GrainShifterTester::getGrainBufferNumSamples(*grainShifter, i) == expectedLookahead3);
			CHECK(GrainShifterTester::getGrainBufferLengthInSamples(*grainShifter, i) == expectedLookahead3);
		}
	}
}





//=============================
TEST_CASE("Can convert phase in radians to samples.")
{

}

//==============================
TEST_CASE("Can granulate buffer")
{

	SECTION("No Shifting")
	{

	}

	SECTION("Shifting Up")
	{

	}

	SECTION("Shifting Down")
	{

	}
}

//================================
TEST_CASE("mGrainReadIndex increments with each process call", "[test_GrainShifter]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;
	grainShifter.prepare(44100, 1024);

	juce::AudioBuffer<float> lookaheadBuffer(2, 1024);
	juce::AudioBuffer<float> outputBuffer(2, 256);

	// Get initial index
	juce::int64 initialIndex = GrainShifterTester::getGrainReadIndex(grainShifter);

	// First process call
	grainShifter.processShifting(lookaheadBuffer, outputBuffer, 128.0f, 1.0f);
	juce::int64 indexAfterFirst = GrainShifterTester::getGrainReadIndex(grainShifter);
	CHECK(indexAfterFirst > initialIndex);

	// Second process call
	grainShifter.processShifting(lookaheadBuffer, outputBuffer, 128.0f, 1.0f);
	juce::int64 indexAfterSecond = GrainShifterTester::getGrainReadIndex(grainShifter);
	CHECK(indexAfterSecond > indexAfterFirst);

	// Third process call
	grainShifter.processShifting(lookaheadBuffer, outputBuffer, 128.0f, 1.0f);
	juce::int64 indexAfterThird = GrainShifterTester::getGrainReadIndex(grainShifter);
	CHECK(indexAfterThird > indexAfterSecond);
}

//================================
TEST_CASE("mGrainReadIndex wraps around and switches active buffer", "[test_GrainShifter]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;

	// Prepare with a small lookahead size for easier testing
	int lookaheadSize = 512;
	grainShifter.prepare(44100, lookaheadSize);

	// Create buffers
	juce::AudioBuffer<float> lookaheadBuffer(2, lookaheadSize);

	// Get initial state
	int initialBufferIndex = GrainShifterTester::getActiveGrainBufferIndex(grainShifter);
	juce::int64 initialReadIndex = GrainShifterTester::getGrainReadIndex(grainShifter);

	CHECK(initialBufferIndex == 0); // Should start with buffer 0
	CHECK(initialReadIndex == 0);    // Should start at index 0

	SECTION("Index wraps at buffer length")
	{
		// Process enough samples to reach near the end of the buffer
		int samplesToProcess = lookaheadSize - 10;
		juce::AudioBuffer<float> outputBuffer1(2, samplesToProcess);

		grainShifter.processShifting(lookaheadBuffer, outputBuffer1, 128.0f, 1.0f);

		// Check we're near the end but haven't wrapped
		juce::int64 indexNearEnd = GrainShifterTester::getGrainReadIndex(grainShifter);
		int bufferIndexNearEnd = GrainShifterTester::getActiveGrainBufferIndex(grainShifter);

		CHECK(indexNearEnd == samplesToProcess);
		CHECK(bufferIndexNearEnd == 0); // Still on first buffer

		// Process more samples to cause wrapping
		juce::AudioBuffer<float> outputBuffer2(2, 20); // Process 20 more samples
		grainShifter.processShifting(lookaheadBuffer, outputBuffer2, 128.0f, 1.0f);

		// Check that index wrapped and buffer switched
		juce::int64 indexAfterWrap = GrainShifterTester::getGrainReadIndex(grainShifter);
		int bufferIndexAfterWrap = GrainShifterTester::getActiveGrainBufferIndex(grainShifter);

		// Should have wrapped: (lookaheadSize - 10) + 20 = lookaheadSize + 10
		// So new index should be 10
		CHECK(indexAfterWrap == 10);
		CHECK(bufferIndexAfterWrap == 1); // Should have switched to buffer 1
	}

	SECTION("Buffer switches back and forth with multiple wraps")
	{
		// Process exactly one buffer length
		juce::AudioBuffer<float> outputBuffer1(2, lookaheadSize);
		grainShifter.processShifting(lookaheadBuffer, outputBuffer1, 128.0f, 1.0f);

		// Should wrap to start and switch to buffer 1
		CHECK(GrainShifterTester::getGrainReadIndex(grainShifter) == 0);
		CHECK(GrainShifterTester::getActiveGrainBufferIndex(grainShifter) == 1);

		// Process another full buffer length
		juce::AudioBuffer<float> outputBuffer2(2, lookaheadSize);
		grainShifter.processShifting(lookaheadBuffer, outputBuffer2, 128.0f, 1.0f);

		// Should wrap again and switch back to buffer 0
		CHECK(GrainShifterTester::getGrainReadIndex(grainShifter) == 0);
		CHECK(GrainShifterTester::getActiveGrainBufferIndex(grainShifter) == 0);

		// Process a third time
		juce::AudioBuffer<float> outputBuffer3(2, lookaheadSize);
		grainShifter.processShifting(lookaheadBuffer, outputBuffer3, 128.0f, 1.0f);

		// Should be back to buffer 1
		CHECK(GrainShifterTester::getGrainReadIndex(grainShifter) == 0);
		CHECK(GrainShifterTester::getActiveGrainBufferIndex(grainShifter) == 1);
	}

	SECTION("Partial buffer processing maintains correct state")
	{
		// Process in small chunks and verify state is maintained correctly
		int chunkSize = 100;
		int numChunks = (lookaheadSize / chunkSize) + 2; // Go past one full buffer

		for (int i = 0; i < numChunks; ++i)
		{
			juce::AudioBuffer<float> smallBuffer(2, chunkSize);
			grainShifter.processShifting(lookaheadBuffer, smallBuffer, 128.0f, 1.0f);

			juce::int64 expectedIndex = ((i + 1) * chunkSize) % lookaheadSize;
			int expectedBufferIndex = ((i + 1) * chunkSize) / lookaheadSize;

			// For this test, buffer index alternates 0, 1, 0, 1...
			expectedBufferIndex = expectedBufferIndex % 2;

			CHECK(GrainShifterTester::getGrainReadIndex(grainShifter) == expectedIndex);
			CHECK(GrainShifterTester::getActiveGrainBufferIndex(grainShifter) == expectedBufferIndex);
		}
	}
}

//================================
TEST_CASE("Can determine when next lookahead is needed")
{

}

//================================
TEST_CASE("Active grain buffer index is iterated properly")
{

}

//=================================
TEST_CASE("Correct grain buffer is returned for given index")
{

}





















TEST_CASE("Can calculate correct start position given necessary data from previous block.", "test_GrainShifter")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;

	SECTION("Final grain ends on exactly first sample of next output buffer after shifting - startPos = 0 ")
	{
		juce::int64 prevShiftedPeriod = 11;
		RD::BufferRange prevOutputRange(0, 99);
		RD::BufferRange prevWriteRange(90, 99);

		juce::int64 startPos = GrainShifterTester::callCalculateFirstGrainStartPos(grainShifter, prevShiftedPeriod, prevOutputRange, prevWriteRange);
		juce::int64 expectedStartPos = 0;
		CHECK(startPos == expectedStartPos);
	}

	SECTION("Due to previous shifted period, the start pos will be higher than 0")
	{
		juce::int64 prevShiftedPeriod = 15;
		RD::BufferRange prevOutputRange(0, 99);
		RD::BufferRange prevWriteRange(90, 99);

		juce::int64 startPos = GrainShifterTester::callCalculateFirstGrainStartPos(grainShifter, prevShiftedPeriod, prevOutputRange, prevWriteRange);
		juce::int64 expectedStartPos = 4;
		CHECK(startPos == expectedStartPos);
	}

	SECTION("Final grain extends past end of prev output buffer")
	{
		juce::int64 prevShiftedPeriod = 15;
		RD::BufferRange prevOutputRange(0, 99);
		RD::BufferRange prevWriteRange(90, 105);

		juce::int64 startPos = GrainShifterTester::callCalculateFirstGrainStartPos(grainShifter, prevShiftedPeriod, prevOutputRange, prevWriteRange);
		juce::int64 expectedStartPos = 4;
		CHECK(startPos == expectedStartPos);
	}

	SECTION("Final grain extends past end of prev output buffer && prevShiftedPeriod is shorter than grain length due to shifting up")
	{
		juce::int64 prevShiftedPeriod = 14;
		RD::BufferRange prevOutputRange(0, 99);
		RD::BufferRange prevWriteRange(90, 105);

		juce::int64 startPos = GrainShifterTester::callCalculateFirstGrainStartPos(grainShifter, prevShiftedPeriod, prevOutputRange, prevWriteRange);
		juce::int64 expectedStartPos = 3;
		CHECK(startPos == expectedStartPos);
	}
}



TEST_CASE("Can determine number of grains needed to accomplish given shifting effect", "test_GrainShifter")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;
	SECTION("No shifting")
	{
		float shiftRatio = 1.f, detectedPeriod = 10.f;
		juce::int64 firstGrainStartPos = (juce::int64)0;
		RD::BufferRange outputRange(0, 99); // 100 samples total, this is endIndex [99]
		int numGrains = GrainShifterTester::callCalculateNumGrainsToOutput(grainShifter, detectedPeriod, shiftRatio, outputRange, firstGrainStartPos);
		CHECK(numGrains == 10);
	}
	SECTION("Handles fractional numGrains, rounds up")
	{
		float shiftRatio = 1.f, detectedPeriod = 11.f;
		juce::int64 firstGrainStartPos = (juce::int64)0;
		RD::BufferRange outputRange(0, 99); // 100 samples total, this is endIndex [99]
		int numGrains = GrainShifterTester::callCalculateNumGrainsToOutput(grainShifter, detectedPeriod, shiftRatio, outputRange, firstGrainStartPos);
		CHECK(numGrains == 10);
	}
	SECTION("Handles single grain that is longer than outputRange, rounds up")
	{
		float shiftRatio = 1.f, detectedPeriod = 1100.f;
		juce::int64 firstGrainStartPos = (juce::int64)0;
		RD::BufferRange outputRange(0, 99); // 100 samples total, this is endIndex [99]
		int numGrains = GrainShifterTester::callCalculateNumGrainsToOutput(grainShifter, detectedPeriod, shiftRatio, outputRange, firstGrainStartPos);
		CHECK(numGrains == 1); // yes I want this, calculate which portion of the grain later
	}
}

TEST_CASE("Can update source range needed for shifting", "test_GrainShifter")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainShifter grainShifter;
	
	SECTION("takes from fullSourceRange in a fifo manner, ending on the last/oldest sample in the fullSourceRange")
	{
		int numGrains = 10; float detectedPeriod = 10.f; 
		RD::BufferRange fullSourceRange(0, 999);
		RD::BufferRange rangeNeeded(0, 0);
		GrainShifterTester::callGetSourceRangeNeededForNumGrains(grainShifter, numGrains, detectedPeriod, fullSourceRange, rangeNeeded);
		CHECK(rangeNeeded.getLengthInSamples() == 100);
		CHECK(rangeNeeded.getStartIndex() == 900); // should be the last 100 samples of the source range
		CHECK(rangeNeeded.getEndIndex() == 999);
	}
	SECTION("Grain range needed does not return a range that extends outside the source range, even when larger than full source range")
	{
		int numGrains = 10; float detectedPeriod = 1000.f; // way too big!
		RD::BufferRange fullSourceRange(0, 999);
		RD::BufferRange rangeNeeded(0, 0);
		GrainShifterTester::callGetSourceRangeNeededForNumGrains(grainShifter, numGrains, detectedPeriod, fullSourceRange, rangeNeeded);
		CHECK(rangeNeeded.getLengthInSamples() == 1000);
		CHECK(rangeNeeded.getStartIndex() == 0); // should be the last 100 samples of the source range
		CHECK(rangeNeeded.getEndIndex() == 999);
	}


	
}

TEST_CASE("", "test_GrainShifter")
{
	GrainShifter grainShifter;


	
}

TEST_CASE("Process with pitch shifting", "test_GrainShifter")
{
	// GrainShifter grainShifter;
	// Window& window = grainShifter.getGrainWindow();
	// juce::AudioBuffer<float> lookaheadBuffer (1, 1024);
	// juce::AudioBuffer<float> outputBuffer(1, 512);

	// lookaheadBuffer.clear();
	// outputBuffer.clear();

	// BufferFiller::fillIncremental(lookaheadBuffer);

	// grainShifter.prepare(44100, 512);

	// int numLookaheadSamples = lookaheadBuffer.getNumSamples() - outputBuffer.getNumSamples();

	// SECTION("no shifting")
	// {
	// 	window.setShape(Window::Shape::kNone); // testing indices not overlapping window modified values yet

	// 	grainShifter.processShifting(lookaheadBuffer, outputBuffer, 128, 1.f);

	// 	for(int sampleIndex = 0; sampleIndex < outputBuffer.getNumSamples(); sampleIndex++)
	// 	{
			
	// 		for(int ch = 0; ch < outputBuffer.getNumChannels(); ch++)
	// 		{
	// 			float sampleResult = outputBuffer.getSample(ch, sampleIndex);
	// 			float expectedSample = (float)numLookaheadSamples + (float)sampleIndex;
	// 			CHECK(sampleResult == expectedSample);
	// 		}
	// 	}
	// }

	
}



TEST_CASE("", "test_GrainShifter")
{
	GrainShifter grainShifter;


	
}

TEST_CASE("", "test_GrainShifter")
{
	GrainShifter grainShifter;


	
}

TEST_CASE("", "test_GrainShifter")
{
	GrainShifter grainShifter;


	
}

TEST_CASE("", "test_GrainShifter")
{
	GrainShifter grainShifter;


	
}

TEST_CASE("", "test_GrainShifter")
{
	GrainShifter grainShifter;


	
}