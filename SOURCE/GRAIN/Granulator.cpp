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

Granulator::Granulator(int numGrainBuffers)
	: mGrainBuffers(numGrainBuffers)
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

	for (auto& grainBuffer : mGrainBuffers)
	{
		grainBuffer.clear();
		grainBuffer.setSize(2, lookaheadSize);
	}
	mActiveGrainBufferIndex = 0;
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
		
	// switching of active grain buffers happens here in sample loop (not channel loop though)
	int grainBufferSize = _getActiveGrainBuffer().getNumSamples();
	if(mGrainReadPos >= grainBufferSize)
	{
		mGrainReadPos = mGrainReadPos - grainBufferSize; // wrap read pos, all grain buffers are same size
		_switchActiveBuffer();
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
	if(finalGrainReadPosThisBlock >= _getActiveGrainBuffer().getNumSamples() || mGrainReadPos == 0)
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
	mWindow->setPeriod(static_cast<int>(detectedPeriod));

	if(mCurrentAnalysisMarkIndex < 0)
		mCurrentAnalysisMarkIndex = BufferHelper::getPeakIndex(bufferToGranulate, 0, (int)detectedPeriod);
	else
		mCurrentAnalysisMarkIndex = mCurrentAnalysisMarkIndex + (int)detectedPeriod;

	int windowCenter = _getWindowCenterIndex(bufferToGranulate, mCurrentAnalysisMarkIndex, detectedPeriod);




	

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

//==========================================
int Granulator::_getNextAnalysisMarkIndex(juce::AudioBuffer<float>& buffer, float detectedPeriod, int currentIndex)
{
	int updatedIndex = currentIndex;

	if(updatedIndex < 0)
	{
		updatedIndex = BufferHelper::getPeakIndex(buffer, 0, (int)detectedPeriod);
	}
	else
	{
		int predictedMarkIndex = updatedIndex + (int)detectedPeriod;

	}
}

//==========================================
int Granulator::_getWindowCenterIndex(juce::AudioBuffer<float>& buffer, int analysisMarkIndex, float detectedPeriod)
{
	int centerIndex = analysisMarkIndex;

	int radius = (int)(detectedPeriod / 4.f);
	centerIndex = BufferHelper::getPeakIndex(buffer, predictedMarkIndex - radius, predictedMarkIndex + radius);
}

//==========================================
int Granulator::_getNextSynthMarkIndex(juce::AudioBuffer<float>& buffer, float detectedPeriod, float shiftedPeriod)
{
	if(mCurrentSynthMarkIndex < 0)
	{
		mCurrentSynthMarkIndex = mCurrentAnalysisMarkIndex; // first one lines up
	}
	else
	{

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
	}

}

//=======================================
void Granulator::_switchActiveBuffer()
{
	// Clear the buffer that is becoming inactive (the current active one)
	mGrainBuffers[mActiveGrainBufferIndex].clear();

	// Switch to the next buffer
	mActiveGrainBufferIndex = (mActiveGrainBufferIndex + 1) % static_cast<int>(mGrainBuffers.size());
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getActiveGrainBuffer()
{
	return mGrainBuffers[mActiveGrainBufferIndex];
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getInactiveGrainBuffer()
{
	int inactiveIndex = (mActiveGrainBufferIndex + 1) % static_cast<int>(mGrainBuffers.size());
	return mGrainBuffers[inactiveIndex];
}

































