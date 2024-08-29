#include "PitchDetector.h"


//
PitchDetector::PitchDetector()
{

}

//
PitchDetector::~PitchDetector()
{
    
}

//
void PitchDetector::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;
    // TODO: This sets the pitch analysis window to always be the block size, but I don't think that needs to be a requirement
    mBufferSize = samplesPerBlock;
    mHalfBufferSize = samplesPerBlock / 2; // save division later in processBlock

    /* Allocate the autocorellation buffer and initialise it to zero */
	mYinBuffer.resize(mHalfBufferSize);
	std::fill(mYinBuffer.begin(), mYinBuffer.end(), 0.0f); // Initialize with zeroes if needed
}


//
void PitchDetector::process(juce::AudioBuffer<float>& buffer)
{
    int tauEstimate = -1;
	float pitchInHertz = -1;
	
	/* Step 1: Calculates the squared difference of the signal with a shifted version of itself. */
	_difference(buffer);
	
	/* Step 2: Calculate the cumulative mean on the normalised difference calculated in step 1 */
	_cumulativeMeanNormalizedDifference();
	
	/* Step 3: Search through the normalised cumulative mean array and find values that are over the threshold */
	tauEstimate = _absoluteThreshold();
	
	/* Step 5: Interpolate the shift value (tau) to improve the pitch estimate. */
	if(tauEstimate != -1)
    {
		pitchInHertz = mSampleRate / _parabolicInterpolation(tauEstimate);
	}

    mCurrentPitchHz.store(pitchInHertz);
}

//
const double PitchDetector::getCurrentPitch()
{
	return mCurrentPitchHz.load();
}


//
void PitchDetector::_difference(juce::AudioBuffer<float>& buffer)
{
	float delta;

	for(int tau = 0 ; tau < mHalfBufferSize; tau++)
    {

		for(int i = 0; i < mHalfBufferSize; i++)
        {
			float sample = buffer.getSample(0, i);
			float shiftedSample = buffer.getSample(0, i+tau);
			delta = sample - shiftedSample;
			mYinBuffer[tau] += delta * delta;
		}
	}
}

//
void PitchDetector::_cumulativeMeanNormalizedDifference()
{
	float runningSum = 0;
	mYinBuffer[0] = 1;

	/* Sum all the values in the autocorellation buffer and nomalise the result, replacing
	 * the value in the autocorellation buffer with a cumulative mean of the normalised difference */
	for (int tau = 1; tau < mHalfBufferSize; tau++) 
    {
		runningSum += mYinBuffer[tau]; // what if 0?
		if(runningSum > 0)
			mYinBuffer[tau] *= tau / runningSum;
		else
			mYinBuffer[tau] = 1.0f;
	}
}

//
int PitchDetector::_absoluteThreshold()
{
	int tau;

	/* Search through the array of cumulative mean values, and look for ones that are over the threshold 
	 * The first two positions in mYinBuffer are always so start at the third (index 2) */
	for (tau = 2; tau < mHalfBufferSize ; tau++) 
    {
		if (mYinBuffer[tau] < mThreshold) 
        {
			while (tau + 1 < mHalfBufferSize && mYinBuffer[tau + 1] < mYinBuffer[tau]) 
            {
				tau++;
			}
			/* found tau, exit loop and return
			 * store the probability
			 * From the YIN paper: The threshold determines the list of
			 * candidates admitted to the set, and can be interpreted as the
			 * proportion of aperiodic power tolerated
			 * within a periodic signal.
			 *
			 * Since we want the periodicity and and not aperiodicity:
			 * periodicity = 1 - aperiodicity */
			mProbability = 1 - mYinBuffer[tau];
			break;
		}
	}

	/* if no pitch found, tau => -1 */
	if (tau == mHalfBufferSize || mYinBuffer[tau] >= mThreshold) 
    {
		tau = -1;
		mProbability = 0;
	}

	return tau;
}

//
float PitchDetector::_parabolicInterpolation(int tauEstimate)
{
	float betterTau;
	int x0;
	int x2;
	
	/* Calculate the first polynomial coeffcient based on the current estimate of tau */
	if (tauEstimate < 1) 
    {
		x0 = tauEstimate;
	} 
	else 
    {
		x0 = tauEstimate - 1;
	}

	/* Calculate the second polynomial coeffcient based on the current estimate of tau */
	if (tauEstimate + 1 < mHalfBufferSize) 
    {
		x2 = tauEstimate + 1;
	} 
	else 
    {
		x2 = tauEstimate;
	}

	/* Algorithm to parabolically interpolate the shift value tau to find a better estimate */
	if (x0 == tauEstimate) 
    {
		if (mYinBuffer[tauEstimate] <= mYinBuffer[x2]) 
        {
			betterTau = tauEstimate;
		} 
		else 
        {
			betterTau = x2;
		}
	} 
	else if (x2 == tauEstimate) 
    {
		if (mYinBuffer[tauEstimate] <= mYinBuffer[x0]) 
        {
			betterTau = tauEstimate;
		} 
		else 
        {
			betterTau = x0;
		}
	} 
	else 
    {
		float s0, s1, s2;
		s0 = mYinBuffer[x0];
		s1 = mYinBuffer[tauEstimate];
		s2 = mYinBuffer[x2];
		// fixed AUBIO implementation, thanks to Karl Helgason:
		// (2.0f * s1 - s2 - s0) was incorrectly multiplied with -1
		betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
	}


	return betterTau;
}