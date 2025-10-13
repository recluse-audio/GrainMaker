/**
 * Granulator.cpp
 * Created by Ryan Devens on 2025-10-13
 */

#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"

Granulator::Granulator()
	: mWindow()
{
	mWindow.setSize(65535);
	// Initialize window with Hanning shape by default
	mWindow.setShape(Window::Shape::kHanning);
}

void Granulator::granulateBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& bufferToWriteTo, float grainPeriod, float emissionPeriod, Window::Shape windowShape)
{
	mWindow.setShape(windowShape);
	mWindow.setPeriod(grainPeriod);

	// Clear output buffer first
	bufferToWriteTo.clear();

	const int sourceNumSamples = bufferToGranulate.getNumSamples();
	const int outputNumSamples = bufferToWriteTo.getNumSamples();
	const int grainSize = static_cast<int>(grainPeriod);
	const int emissionStep = static_cast<int>(emissionPeriod);

	// Track positions in source and output buffers
	int readPos = 0;
	int writePos = 0;

	// Process grains until we run out of output space
	while (writePos < outputNumSamples)
	{
		// Calculate grain read range
		int readEndPos = readPos + grainSize - 1;
		if (readEndPos >= sourceNumSamples)
			readEndPos = sourceNumSamples - 1;
		RD::BufferRange readRange(readPos, readEndPos);

		// Calculate grain write range
		int writeEndPos = writePos + grainSize - 1;
		if (writeEndPos >= outputNumSamples)
			writeEndPos = outputNumSamples - 1;
		RD::BufferRange writeRange(writePos, writeEndPos);

		// Get the grain as an audio block from the source buffer
		juce::dsp::AudioBlock<float> grainBlock = BufferHelper::getRangeAsBlock(bufferToGranulate, readRange);

		// Write the grain block to the output buffer
		BufferHelper::writeBlockToBuffer(bufferToWriteTo, grainBlock, writeRange);

		// Update positions
		readPos += grainSize;
		writePos += emissionStep;

		// Wrap read position if needed
		if (readPos >= sourceNumSamples)
			readPos = readPos % sourceNumSamples;
	}
}