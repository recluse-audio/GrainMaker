#include "PitchDetector.h"
#include "../../SUBMODULES/RD/SOURCE/BufferMath.h" // YIN stuff is here

//
PitchDetector::PitchDetector()
{
	differenceBuffer.setSize(1, DEFAULT_BUFFER_SIZE);
	cmndBuffer.setSize(1, DEFAULT_BUFFER_SIZE);
}

//
PitchDetector::~PitchDetector()
{
    
}

//
void PitchDetector::prepareToPlay(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;

	mHalfBlock = (blockSize / 2);
	
	differenceBuffer.setSize(1, mHalfBlock);
	differenceBuffer.clear();
	cmndBuffer.setSize(1, mHalfBlock);
	cmndBuffer.clear();
}


//
float PitchDetector::process(juce::AudioBuffer<float>& buffer)
{
	float pitchInHertz = -1;
	differenceBuffer.clear();
	cmndBuffer.clear();
	// /* Step 1: Calculates the squared difference of the signal with a shifted version of itself. */
	BufferMath::yin_difference(buffer, differenceBuffer, mHalfBlock-1);

	BufferMath::yin_normalized_difference(differenceBuffer, cmndBuffer);

	int tauEstimate = BufferMath::yin_absolute_threshold(cmndBuffer, 0.1f);

	if(tauEstimate > 0)
	{
		// doing difference buffer on purpose per yin paper
		float bestTauEstimate = BufferMath::yin_parabolic_interpolation(cmndBuffer, tauEstimate);
		pitchInHertz = mSampleRate / bestTauEstimate;
	}

	mCurrentPitchHz.store(pitchInHertz);
	return pitchInHertz;
}

//
const double PitchDetector::getCurrentPitch()
{
	return mCurrentPitchHz.load();
}


//
