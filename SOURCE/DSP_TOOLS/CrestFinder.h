/*
  ==============================================================================

    CrestFinder.h
    Created: 17 Feb 2020 4:05:55pm
    Author:  Andrew

  ==============================================================================
*/

#pragma once

#include <cmath>


template <typename dspfloat_t>
class CrestFinder
{
public:
	CrestFinder()
	{
		setSmoothTimeMs(timeMs_);
	}
	
	~CrestFinder() { /* nothing to see here */ }
	
	void setSampleRate(float fs)
	{
		fs_ = fs;
		setSmoothTimeMs(timeMs_);
	}

	// mono
	inline dspfloat_t run(dspfloat_t x_in)
	{
		double x = fmax(x_in * x_in, kMinVal);
		
		// run RMS averager
		rmsEnv_ = alpha_ * rmsEnv_ + (1. - alpha_) * x;

		// run peak detector
		peakEnv_ = fmax(x, alpha_ * peakEnv_ + (1. - alpha_) * x);
		
		// compute crest factor
		ratio_ = peakEnv_ / rmsEnv_; // fmax(kGateThresh, rmsEnv_);
		return static_cast<dspfloat_t>(ratio_);
	}
	
	// stereo
	inline dspfloat_t run(dspfloat_t ( &x_in )[2], bool stereo = false)
	{
		double x = x_in[0] * x_in[0]; // square left channel
		if (stereo) {
			x = fmax(x, x_in[1] * x_in[1]);
		}
		
		x = fmax(x, kMinVal); // set a floor at -120 dB

		// run RMS averager
		rmsEnv_ = alpha_ * rmsEnv_ + (1. - alpha_) * x;
		
		// run peak detector
		peakEnv_ = fmax(x, alpha_ * peakEnv_ + (1. - alpha_) * x);
		
		// compute crest factor
		ratio_ = peakEnv_ / rmsEnv_; // fmax(kGateThresh, rmsEnv_);
		return ratio_;
	}
	
	inline void reset(void)
	{
		ratio_ = 1.;
		rmsEnv_ = 0.;
		peakEnv_ = 0.;
	}
	
	inline float getCrestFactor(void)
	{
		return(sqrtf((float)ratio_));
	}
	
	void setSmoothTimeMs(float time_msec)
	{
		timeMs_= time_msec;
		alpha_ = exp(-1.0 / (timeMs_ * .001 * fs_));
	}
	
private:
	static constexpr double kMinVal = 0.001; // -60dB

	float fs_ = 44100;
	float timeMs_ = 200.; // time constant

	double alpha_ = 0.;
	double ratio_ = 0;
	double rmsEnv_ = 0;
	double peakEnv_ = 0;
};

