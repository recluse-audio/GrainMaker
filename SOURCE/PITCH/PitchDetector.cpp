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
	// mHalfBufferSize = samplesPerBlock / 2;
    int analysisSize = samplesPerBlock / 2; // save division later in processBlock
	mHalfBufferSize = analysisSize; // TODO: Pick one of these to use
	
    /* Allocate the autocorellation buffer and initialise it to zero */
	mYinBuffer.resize(analysisSize);
	mDifference.resize(analysisSize);
	mNormalizedDifference.resize(analysisSize);

	std::fill(mYinBuffer.begin(), mYinBuffer.end(), 0.0f); // Initialize with zeroes if needed
	std::fill(mDifference.begin(), mDifference.end(), 0.0f); // Initialize with zeroes if needed
	std::fill(mNormalizedDifference.begin(), mNormalizedDifference.end(), 0.0f); // Initialize with zeroes if needed

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
		auto bestPeriodEstimate = _getBestTau(tauEstimate);
		pitchInHertz = mSampleRate / bestPeriodEstimate;
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
			mDifference[tau] += delta * delta;
		}
	}
}

//
void PitchDetector::_cumulativeMeanNormalizedDifference()
{
	float runningSum = 0;
	mDifference[0] = 1;

	/* Sum all the values in the autocorellation buffer and nomalise the result, replacing
	 * the value in the autocorellation buffer with a cumulative mean of the normalised difference */
	for (int tau = 1; tau < mHalfBufferSize; tau++) 
    {
		runningSum += mDifference[tau]; // what if 0?
		if(runningSum > 0)
			mNormalizedDifference[tau] = mDifference[tau] * (tau / runningSum);
		else
			mNormalizedDifference[tau] = 1.0f;
	}
}

// smallest value of tau that gives a minimum of cumulative diff that is above the threshold
int PitchDetector::_absoluteThreshold()
{
	int tau = 0;

	/* Search through the vector of cumulative mean values, and look for ones that are over the threshold 
	 * The first two positions in vector are always so start at the third (index 2) */
	for (tau = 2; tau < mHalfBufferSize ; tau++) 
    {
		if (mNormalizedDifference[tau] < mThreshold) 
        {
			bool nextTauLessThanHalfBufferSize = tau + 1 < mHalfBufferSize;
			while (tau + 1 < mHalfBufferSize && mNormalizedDifference[tau + 1] < mNormalizedDifference[tau]) 
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
			mProbability = 1 - mNormalizedDifference[tau];
			break;
		}
	}

	/* if no pitch found, tau => -1 */
	if (tau == mHalfBufferSize || mNormalizedDifference[tau] >= mThreshold) 
    {
		tau = -1;
		mProbability = 0;
	}

	return tau;
}

//
float PitchDetector::_getBestTau(int tauEstimate)
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
		if (mNormalizedDifference[tauEstimate] <= mNormalizedDifference[x2]) 
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
		if (mNormalizedDifference[tauEstimate] <= mNormalizedDifference[x0]) 
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