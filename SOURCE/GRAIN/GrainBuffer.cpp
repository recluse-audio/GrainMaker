/**
 * Created by Ryan Devens on 2025-10-12
*/

#include "GrainBuffer.h"

void GrainBuffer::setLengthInSamples(juce::int64 lengthInSamples)
{
	mLengthInSamples = lengthInSamples;
}

juce::int64 GrainBuffer::getLengthInSamples() const
{
	return mLengthInSamples;
}

juce::AudioBuffer<float>& GrainBuffer::getBufferReference()
{
	return mBuffer;
}

const juce::AudioBuffer<float>& GrainBuffer::getBufferReference() const
{
	return mBuffer;
}