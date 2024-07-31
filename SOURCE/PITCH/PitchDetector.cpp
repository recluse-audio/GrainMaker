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
}

//
void PitchDetector::init(PitchDetector::Params* pParams, int bufferSize, float threshold)
{
	/* Initialise the fields of the Yin structure passed in */
	pParams->mBufferSize = bufferSize;
	pParams->mHalfBufferSize = bufferSize / 2;
	pParams->threshold = threshold;
	pParams->probability = 0.0;

	/* Allocate the autocorellation buffer and initialise it to zero */
	pParams->yinBuffer = (float *) malloc(sizeof(float)* pParams->mHalfBufferSize);

	int16_t i;
	for(i = 0; i < pParams->mHalfBufferSize; i++)
    {
		pParams->yinBuffer[i] = 0;
	}
}

//
void PitchDetector::process(juce::AudioBuffer<float>& buffer)
{

}

//
const double PitchDetector::getCurrentPitch(PitchDetector::Params* pParams, float* buffer)
{
	int16_t tauEstimate = -1;
	float pitchInHertz = -1;
	
	/* Step 1: Calculates the squared difference of the signal with a shifted version of itself. */
	_difference(pParams, buffer);
	
	/* Step 2: Calculate the cumulative mean on the normalised difference calculated in step 1 */
	_cumulativeMeanNormalizedDifference(pParams);
	
	/* Step 3: Search through the normalised cumulative mean array and find values that are over the threshold */
	tauEstimate = _absoluteThreshold(pParams);
	
	/* Step 5: Interpolate the shift value (tau) to improve the pitch estimate. */
	if(tauEstimate != -1)
    {
		pitchInHertz = mSampleRate / _parabolicInterpolation(pParams, tauEstimate);
	}
	
	return pitchInHertz;
}




//
void PitchDetector::_difference(PitchDetector::Params* pParams, float* buffer)
{
	int16_t i;
	int16_t tau;
	float delta;

	for(tau = 0 ; tau < pParams->mHalfBufferSize; tau++)
    {

		for(i = 0; i < pParams->mHalfBufferSize; i++)
        {
			delta = buffer[i] - buffer[i + tau];
			pParams->yinBuffer[tau] += delta * delta;
		}
	}
}

//
void PitchDetector::_cumulativeMeanNormalizedDifference(PitchDetector::Params* pParams)
{
	int16_t tau;
	float runningSum = 0;
	pParams->yinBuffer[0] = 1;

	/* Sum all the values in the autocorellation buffer and nomalise the result, replacing
	 * the value in the autocorellation buffer with a cumulative mean of the normalised difference */
	for (tau = 1; tau < pParams->mHalfBufferSize; tau++) {
		runningSum += pParams->yinBuffer[tau];
		pParams->yinBuffer[tau] *= tau / runningSum;
	}
}

//
int PitchDetector::_absoluteThreshold(PitchDetector::Params* pParams)
{
	int16_t tau;

	/* Search through the array of cumulative mean values, and look for ones that are over the threshold 
	 * The first two positions in yinBuffer are always so start at the third (index 2) */
	for (tau = 2; tau < pParams->mHalfBufferSize ; tau++) 
    {
		if (pParams->yinBuffer[tau] < pParams->threshold) 
        {
			while (tau + 1 < pParams->mHalfBufferSize && pParams->yinBuffer[tau + 1] < pParams->yinBuffer[tau]) 
            {
				tau++;
			}
			/* found tau, exit loop and return
			 * store the probability
			 * From the YIN paper: The pParams->threshold determines the list of
			 * candidates admitted to the set, and can be interpreted as the
			 * proportion of aperiodic power tolerated
			 * within a periodic signal.
			 *
			 * Since we want the periodicity and and not aperiodicity:
			 * periodicity = 1 - aperiodicity */
			pParams->probability = 1 - pParams->yinBuffer[tau];
			break;
		}
	}

	/* if no pitch found, tau => -1 */
	if (tau == pParams->mHalfBufferSize || pParams->yinBuffer[tau] >= pParams->threshold) 
    {
		tau = -1;
		pParams->probability = 0;
	}

	return tau;
}

//
float PitchDetector::_parabolicInterpolation(PitchDetector::Params* pParams, int tauEstimate)
{
	float betterTau;
	int16_t x0;
	int16_t x2;
	
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
	if (tauEstimate + 1 < pParams->mHalfBufferSize) 
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
		if (pParams->yinBuffer[tauEstimate] <= pParams->yinBuffer[x2]) 
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
		if (pParams->yinBuffer[tauEstimate] <= pParams->yinBuffer[x0]) 
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
		s0 = pParams->yinBuffer[x0];
		s1 = pParams->yinBuffer[tauEstimate];
		s2 = pParams->yinBuffer[x2];
		// fixed AUBIO implementation, thanks to Karl Helgason:
		// (2.0f * s1 - s2 - s0) was incorrectly multiplied with -1
		betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
	}


	return betterTau;
}