/**
 * Granulator.cpp
 * Created by Ryan Devens on 2025-10-13
 */

#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"

Granulator::Granulator()
{
	mWindow = std::make_unique<Window>();
	//mWindow->setLooping(true);
}

Granulator::~Granulator()
{
	mWindow.reset();

}

//====================================
void Granulator::prepare(double sampleRate, int blockSize, int lookaheadSize)
{
	// mProcessBlockSize = blockSize;

	// mGrainBuffer1.clear();
	// mGrainBuffer1.setSize(2, lookaheadSize);
	// mGrainBuffer2.clear();
	// mGrainBuffer2.setSize(2, lookaheadSize);
	// mActiveGrainBuffer = 0;
	// mNeedToFillActive = true;

	// mWindow->setSizeShapePeriod(sampleRate, Window::Shape::kHanning, blockSize); // period updated after pitch detection
    // mWindow->setLooping(true);
}


//=======================================
void Granulator::granulate(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& processBlock,
							float detectedPeriod, float shiftedPeriod)
{
	if(_shouldGranulateToActiveBuffer())
	{
		_granulateToActiveGrainBuffer(lookaheadBuffer, detectedPeriod, shiftedPeriod);
		mNeedToFillActive = false; 
	}
	else if(_shouldGranulateToInactiveBuffer())
	{
		_granulateToInactiveGrainBuffer(lookaheadBuffer, detectedPeriod, shiftedPeriod);
	}
		
	_writeFromGrainBufferToProcessBlock(processBlock);
}

//=======================================
bool Granulator::_shouldGranulateToActiveBuffer()
{
	return mNeedToFillActive;
}

//=======================================
bool Granulator::_shouldGranulateToInactiveBuffer()
{
	bool shouldGranulate = false;
	// we are about to read beyond the end of the active grain buffer
	// in this processBlock, so granulate the next one and be ready to switch.
	if(mGrainReadPos + mProcessBlockSize >= mGrainBuffer1.getNumSamples())
	{
		shouldGranulate = true;
	}

	return shouldGranulate;
}


//=======================================
void Granulator::_granulateToActiveGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod)
{
	_granulateToGrainBuffer(bufferToGranulate, _getActiveGrainBuffer(), detectedPeriod, shiftedPeriod);
}

//=======================================
void Granulator::_granulateToInactiveGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod)
{
	_granulateToGrainBuffer(bufferToGranulate, _getInactiveGrainBuffer(), detectedPeriod, shiftedPeriod);

}

//=======================================
void Granulator::_granulateToGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& grainBuffer, float detectedPeriod, float shiftedPeriod)
{
	grainBuffer.clear();
	mWindow->setPeriod(detectedPeriod);

	juce::int64 readStartIndex = 0;
	juce::int64 readEndIndex = (juce::int64)detectedPeriod;
	juce::int64 writeStartIndex = 0;
	juce::int64 writeEndIndex = (juce::int64)detectedPeriod;

	while (readStartIndex < bufferToGranulate.getNumSamples() && writeStartIndex < grainBuffer.getNumSamples())
	{
		RD::BufferRange readRange; readRange.setStartIndex(readStartIndex); readRange.setEndIndex(readEndIndex);
		RD::BufferRange writeRange; writeRange.setStartIndex(writeStartIndex); writeRange.setEndIndex(writeEndIndex);

		// Get the grain as an audio block from the source buffer
		juce::dsp::AudioBlock<float> grainBlock = BufferHelper::getRangeAsBlock(bufferToGranulate, readRange);

		// Applies window as you might guess
		BufferHelper::applyWindowToBlock(grainBlock, *mWindow.get());

		// Write the grain block to the output buffer
		BufferHelper::writeBlockToBuffer(_getActiveGrainBuffer(), grainBlock, writeRange);

		readStartIndex =  readStartIndex + (juce::int64) detectedPeriod;
		readEndIndex = readStartIndex + (juce::int64) detectedPeriod;
		if(readEndIndex >= bufferToGranulate.getNumSamples())
			readEndIndex = bufferToGranulate.getNumSamples() - 1;

		writeStartIndex = writeStartIndex + (juce::int64) shiftedPeriod;
		writeEndIndex = writeStartIndex + (juce::int64) detectedPeriod;
		if(writeEndIndex >= grainBuffer.getNumSamples())
			writeEndIndex = grainBuffer.getNumSamples() - 1;
	}
}



//=======================================
void Granulator::_writeFromGrainBufferToProcessBlock(juce::AudioBuffer<float>& processBlock)
{
	for(int sampleIndex = 0; sampleIndex < processBlock.getNumSamples(); sampleIndex++)
	{
		for(int ch = 0; ch < processBlock.getNumChannels(); ch++)
		{
			float grainSample = _getActiveGrainBuffer().getSample(ch, mGrainReadPos);
			processBlock.setSample(ch, sampleIndex, grainSample);
		}
		mGrainReadPos++;

		// switching of active grain buffers happens here in sample loop (not channel loop though)
		if(mGrainReadPos >= mGrainBuffer1.getNumSamples())
		{
			mGrainReadPos = mGrainReadPos - mGrainBuffer1.getNumSamples(); // wrap read pos, both grain buffers are same size
			_switchActiveBuffer();
		}
	}

}

//=======================================
void Granulator::_switchActiveBuffer()
{
	if(mActiveGrainBuffer == 0)
		mActiveGrainBuffer = 1;
	else if(mActiveGrainBuffer == 1)
		mActiveGrainBuffer = 0;
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getActiveGrainBuffer()
{
	if(mActiveGrainBuffer == 0)
		return mGrainBuffer1;
	else
		return mGrainBuffer2;
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getInactiveGrainBuffer()
{
	if(mActiveGrainBuffer == 0)
		return mGrainBuffer2;
	else
		return mGrainBuffer1;
}

































