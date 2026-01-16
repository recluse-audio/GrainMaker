/**
 * Granulator.cpp
 * Created by Ryan Devens on 2025-10-13
 */

// Suppress MSVC warning C4244 from std::make_unique<Window> template instantiation in <memory>
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4244)
#endif

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

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

//====================================
void Granulator::prepare(double sampleRate, int blockSize, int lookaheadSize)
{
	mProcessBlockSize = blockSize;

	mGrainBuffer1.clear();
	mGrainBuffer1.setSize(2, lookaheadSize);
	mGrainBuffer2.clear();
	mGrainBuffer2.setSize(2, lookaheadSize);
	mActiveGrainBuffer = 0;
	mNeedToFillActive = true;
	mGrainReadPos = 0;
	mWindow->setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kHanning, blockSize); // period updated after pitch detection
    mWindow->setLooping(true);
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

	bool test = false;
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

	// this function determines if we will run out of grain data in the current block
	// block size of 256 samples will span from 0-255.
	// So if we are starting at index 768 in grainBuffer and block size is 256,
	// we won't granulate inactive buffer until next block. 
	// b/c we have all the grain data needed to finish out current process block with active grain
	int finalGrainReadPosThisBlock = mGrainReadPos + mProcessBlockSize - 1;

	// we are about to read beyond the end of the active grain buffer
	// in this processBlock, so granulate the next one and be ready to switch.
	if(finalGrainReadPosThisBlock >= mGrainBuffer1.getNumSamples())
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
	mWindow->setPeriod(static_cast<int>(detectedPeriod));

	juce::int64 readStartIndex = 0;
	juce::int64 readEndIndex = static_cast<juce::int64>(detectedPeriod - 1.f);
	juce::int64 writeStartIndex = 0;
	juce::int64 writeEndIndex = static_cast<juce::int64>(detectedPeriod - 1.f);

	while (readStartIndex < bufferToGranulate.getNumSamples() && writeStartIndex < grainBuffer.getNumSamples())
	{
		RD::BufferRange readRange; readRange.setStartIndex(readStartIndex); readRange.setEndIndex(readEndIndex);
		RD::BufferRange writeRange; writeRange.setStartIndex(writeStartIndex); writeRange.setEndIndex(writeEndIndex);

		// Get the grain as an audio block from the source buffer
		juce::dsp::AudioBlock<float> grainBlock = BufferHelper::getRangeAsBlock(bufferToGranulate, readRange);

		// Applies window as you might guess
		BufferHelper::applyWindowToBlock(grainBlock, *mWindow.get());

		// Write the grain block to the output buffer
		BufferHelper::writeBlockToBuffer(grainBuffer, grainBlock, writeRange);

		readStartIndex =  readEndIndex + (juce::int64) 1;
		readEndIndex = readStartIndex + (juce::int64) (detectedPeriod - 1.f);
		if(readEndIndex >= bufferToGranulate.getNumSamples())
			readEndIndex = bufferToGranulate.getNumSamples() - 1;

		writeStartIndex = writeStartIndex + (juce::int64) (shiftedPeriod);
		writeEndIndex = writeStartIndex + (juce::int64) (detectedPeriod - 1.f);
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

































