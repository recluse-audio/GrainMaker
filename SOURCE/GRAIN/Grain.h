/**
 * Grain.h
 * Created by Ryan Devens
 *
 * Represents a single grain for TD-PSOLA processing.
 * Each grain owns a buffer that stores pre-windowed audio data.
 * Windowing is applied by Granulator when the grain is created.
 */

#pragma once
#include "../Util/Juce_Header.h"

class Grain
{
public:
	Grain();
	~Grain();

	bool isActive = false;
	std::tuple<juce::int64, juce::int64, juce::int64> mAnalysisRange { -1, -1, -1 };
	std::tuple<juce::int64, juce::int64, juce::int64> mSynthRange { -1, -1, -1 };

	// Prepare the grain's buffer - call once during setup
	void prepare(int maxGrainSize, int numChannels);

	// Get read-only access to the grain's buffer
	const juce::AudioBuffer<float>& getBuffer() const { return mBuffer; }

	// Get writable access to the grain's buffer (for Granulator to fill)
	juce::AudioBuffer<float>& getBuffer() { return mBuffer; }

	void reset();

private:
	friend class Granulator;
	juce::AudioBuffer<float> mBuffer;
};
