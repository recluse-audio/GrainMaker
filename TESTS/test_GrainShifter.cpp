#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+


#include "../SOURCE/PITCH/GrainShifter.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/RelativeFilePath.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"

#include "TEST_UTILS/GrainShifterTester.h"


TEST_CASE("GrainShifter is prepared correctly when prepareToPlay() is called", "test_GrainShifter")
{
	GrainShifter grainShifter;
	double sampleRate = 48000;
	int blockSize = 512;
	grainShifter.prepare(sampleRate, blockSize);

	CHECK(GrainShifterTester::getSampleRate(grainShifter) == sampleRate);
	CHECK(grainShifter.getGrainWindow().getShape() == Window::Shape::kHanning);
	CHECK(grainShifter.getGrainWindow().getSize() == sampleRate);
}


TEST_CASE("GrainShifter allows you to set grain shape")
{
	GrainShifter grainShifter;
	grainShifter.setGrainShape(Window::Shape::kNone);
	CHECK(grainShifter.getGrainWindow().getShape() == Window::Shape::kNone);
	grainShifter.setGrainShape(Window::Shape::kHanning);
	CHECK(grainShifter.getGrainWindow().getShape() == Window::Shape::kNone);

	
}


TEST_CASE("Can calculate correct start position given necessary data from previous block.", "test_GrainShifter")
{
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