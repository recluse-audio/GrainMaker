/**
 * Created by Ryan Devens on 2025-10-12
 */

#pragma once

#include "../Util/Juce_Header.h"

class GrainBuffer final
{
public:
	GrainBuffer() = default;
	~GrainBuffer() = default;

	// Setter and getter for length in samples
	void setLengthInSamples(juce::int64 lengthInSamples);
	juce::int64 getLengthInSamples() const;

	// Get reference to the internal buffer
	juce::AudioBuffer<float>& getBufferReference();
	const juce::AudioBuffer<float>& getBufferReference() const;

private:
	juce::AudioBuffer<float> mBuffer;
	juce::int64 mLengthInSamples = 0;
};