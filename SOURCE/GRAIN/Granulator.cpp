/**
 * Granulator.cpp
 * Created by Ryan Devens on 2025-10-13
 */

#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"

Granulator::Granulator()
{
	// No longer initialize Window here - it will be passed by reference
}

float Granulator::granulateBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& bufferToWriteTo, 
									float grainPeriod, float emissionPeriod, Window& window, bool timePreserving)
{
	window.setPeriod(grainPeriod);

	// Clear output buffer first
	bufferToWriteTo.clear();

	const int sourceNumSamples = bufferToGranulate.getNumSamples();
	const int outputNumSamples = bufferToWriteTo.getNumSamples();
	const int grainSize = static_cast<int>(grainPeriod);
	const int emissionStep = static_cast<int>(emissionPeriod);

	// Assert that source buffer is at least big enough for one whole grain
	jassert(sourceNumSamples >= grainSize);

	// Early return if source buffer is too small
	if (sourceNumSamples < grainSize)
		return 0.0f;

	// Track positions in source and output buffers
	int readPos = 0;
	int writePos = 0;

	// Default to 0.0 if no grain extends past the buffer
	float finalGrainPhase = 0.0f;

	// Process grains until we run out of output space
	while (readPos < sourceNumSamples)
	{
		window.resetReadPos();
		// Calculate grain read range
		int readEndPos = readPos + grainSize - 1;
		if (readEndPos >= sourceNumSamples)
			readEndPos = sourceNumSamples - 1;
		RD::BufferRange readRange(readPos, readEndPos);

		// Calculate grain write range
		int writeEndPos = writePos + grainSize - 1;

		// Check if this grain would extend past the output buffer
		if (writeEndPos >= outputNumSamples)
		{
			// This is the final grain that extends past the buffer
			writeEndPos = outputNumSamples - 1;

			// Calculate how much of this grain we actually wrote
			int samplesWritten = writeEndPos - writePos + 1;

			// Calculate the normalized phase position (0.0 to 1.0)
			finalGrainPhase = static_cast<float>(samplesWritten) / grainPeriod;
		}

		RD::BufferRange writeRange(writePos, writeEndPos);

		// Get the grain as an audio block from the source buffer
		juce::dsp::AudioBlock<float> grainBlock = BufferHelper::getRangeAsBlock(bufferToGranulate, readRange);

		// Applies window as you might guess
		BufferHelper::applyWindowToBlock(grainBlock, window);

		// Write the grain block to the output buffer
		BufferHelper::writeBlockToBuffer(bufferToWriteTo, grainBlock, writeRange);

		// Update positions
		writePos += emissionStep;
		if(timePreserving) // time preserving repeats grains as needed
		{
			// The emission of the next grain start aka writePos will occur before the completion of 
			// the read range, so next grain will also use this same readRange
			// if writePos is at or after the end of the readRange then increment
			if(writePos >= (readPos + grainSize))
			{
				readPos += grainSize;
			}
		}
		else // varispeed aka tape speed up style
		{
			readPos += grainSize;
		}

		// Stop if the next write position would be beyond the output buffer
		if (writePos >= outputNumSamples)
			break;
	}

	return finalGrainPhase;
}