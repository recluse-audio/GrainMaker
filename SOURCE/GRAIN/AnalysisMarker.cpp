/**
 * AnalysisMarker.cpp
 * Created by Ryan Devens
 */

#include "AnalysisMarker.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"

AnalysisMarker::AnalysisMarker()
{
}

//=======================================
void AnalysisMarker::prepare(double sampleRate, int blockSize)
{
	juce::ignoreUnused(sampleRate, blockSize);
	reset();
}

//=======================================
void AnalysisMarker::reset()
{
	mCurrentAbsAnalysisMarkIndex = -1;
	mIsFirstMark = true;
}

//=======================================
juce::int64 AnalysisMarker::getNextAnalysisMarkIndex(CircularBuffer& circularBuffer, float detectedPeriod, juce::int64 absSampleIndex)
{
	if (mIsFirstMark)
	{
		// For first mark, find peak within first period from current position
		// Convert to circular buffer relative position
		int circBufferSize = circularBuffer.getSize();
		int startPos = static_cast<int>(absSampleIndex % circBufferSize);
		int endPos = startPos + static_cast<int>(detectedPeriod);

		// Find peak in circular buffer
		juce::AudioBuffer<float>& buffer = circularBuffer.getBuffer();
		int peakOffset = BufferHelper::getPeakIndex(buffer, startPos, std::min(endPos, circBufferSize - 1));

		mCurrentAbsAnalysisMarkIndex = absSampleIndex + (peakOffset - startPos);
		mIsFirstMark = false;
	}
	else
	{
		mCurrentAbsAnalysisMarkIndex = mCurrentAbsAnalysisMarkIndex + static_cast<juce::int64>(detectedPeriod);
	}

	return mCurrentAbsAnalysisMarkIndex;
}

//=======================================
int AnalysisMarker::getWindowCenterOffset(CircularBuffer& circularBuffer, juce::int64 absAnalysisMarkIndex, float detectedPeriod)
{
	int circBufferSize = circularBuffer.getSize();
	int centerPos = static_cast<int>(absAnalysisMarkIndex % circBufferSize);
	int radius = static_cast<int>(detectedPeriod / 4.0f);

	juce::AudioBuffer<float>& buffer = circularBuffer.getBuffer();

	int searchStart = centerPos - radius;
	int searchEnd = centerPos + radius;

	// Handle wrapping for circular buffer
	if (searchStart < 0) searchStart = 0;
	if (searchEnd >= circBufferSize) searchEnd = circBufferSize - 1;

	int peakPos = BufferHelper::getPeakIndex(buffer, searchStart, searchEnd, centerPos);

	return peakPos - centerPos; // Return offset from analysis mark
}
