/**
 * AnalysisMarker.h
 * Created by Ryan Devens
 *
 * Handles analysis mark tracking for TD-PSOLA pitch shifting.
 * Owned by the Processor, provides analysis mark positions to the Granulator.
 */

#pragma once
#include "../Util/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/CircularBuffer.h"

class AnalysisMarker
{
public:
	AnalysisMarker();
	~AnalysisMarker() = default;

	void prepare(double sampleRate, int blockSize);
	void reset();

	// Get next analysis mark, updates internal state
	juce::int64 getNextAnalysisMarkIndex(CircularBuffer& circularBuffer, float detectedPeriod, juce::int64 absSampleIndex);

	// Get current analysis mark without advancing
	juce::int64 getCurrentAnalysisMarkIndex() const { return mCurrentAbsAnalysisMarkIndex; }

	// Get window center for a given analysis mark
	int getWindowCenterOffset(CircularBuffer& circularBuffer, juce::int64 absAnalysisMarkIndex, float detectedPeriod);

private:
	juce::int64 mCurrentAbsAnalysisMarkIndex = -1;
	bool mIsFirstMark = true;
};
