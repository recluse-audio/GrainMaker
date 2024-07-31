/*
  ==============================================================================

    Compressor.h
    Created: 9 Oct 2018 10:35:06am
    Author:  Antares

  ==============================================================================
*/

#pragma once

#include <math.h>
#include "Follower.h"
#define DYNAMIC_MAKEUP 1

/*
 	This compression algorithm is based on Sybil
 */
template <typename dspfloat_t>
class Compressor
{
public:
	enum Param {
		kKnee=0,
		kRatio,
		kAngle,
		kLimit,
		kThresh,
		kEnable,
		kMakeup,
		kAttack,
		kRelease
	};
	
	enum Level {
		kEnvelope=0,
		kCompGainDb
	};
	
    Compressor() { /* nothing to see here */ };
    ~Compressor() { /* nothing to see here */ };
	
	void setSampleRate(float fs) {
		double decaySamples	= kGainSmoothTimeMs * .001 * fs;
		smoothTc_ = exp(-1.0 / decaySamples);
	};
	
	// gain computer processing
	//
    inline float run(float xenv)
    {
		level_ = xenv;
		levelDb_ = gainToDb(level_);

		// Zolger - Pirkle algorithm (with knee control)
		
		double cv = slope_;
		
		// apply knee interpolation
		if (kneeDb_ > 0) {
			if (levelDb_ > (threshDb_ - 0.5 * kneeDb_) && levelDb_ < (threshDb_ + 0.5 * kneeDb_)) {
				double x[2];
				double y[2];
				x[0] = threshDb_ - 0.5 * kneeDb_;
				x[1] = threshDb_ + 0.5 * kneeDb_;
				x[1] = std::fmin(0, x[1]);
				y[0] = 0;
				y[1] = cv;
				cv = lagrpol(&x[0], &y[0], 2, levelDb_);
			}
		}
		
		// compute gain	reduction
		compDb_ = std::fmin(0, cv * (threshDb_ - levelDb_));
		
		// TODO: apply ballistics here to compDb_
		
		cdevDb_ = smoothTc_ * cdevDb_ + (1. - smoothTc_) * (compDb_ - cestDb_);
#ifdef DYNAMIC_MAKEUP
		makeupDb_ = makeup_ ? -(cdevDb_ + cestDb_) : 0;
#else
		makeupDb_ = makeup_ ? -cestDb_ : 0; // static makeup
#endif
		gainr_ = enable_ ? dBToGain(compDb_ + makeupDb_) : 1.;
        return gainr_;
    };
	
	// update controls from GUI parameters
	//
	void setControl(int param, float val)
	{
		switch (param) {
			case kThresh:
				// range is 0 to -60dB
				threshDb_ = val;
				break;
			case kKnee:
				// range is 0 to 20dB
				kneeDb_ = val;
				break;
			case kRatio:
				// range is 1:1 to 20:1
				ratio_ = val;
				slope_ = 1.0 - (1. / ratio_);
				break;
			case kAngle:
				// range is 0 to 45 degrees
				angle_ = val * kPi / 180.; // 90. - (180. / kPi) * std::atan(val);
				ratio_ = 1.0 / std::tanf(angle_);
				slope_ = 1.0 - (1. / ratio_);
				break;
			case kLimit:
				// sets limiter mode
				if ((bool)val) {
					slope_ = 1.0;
				} else {
					slope_ = 1.0 - (1. / ratio_);
				}
				break;
			case kEnable:
				// enables compression
				enable_ = (bool)val;
				break;
			case kMakeup:
				// enables makeup gain
				makeup_ = (bool)val;
				break;
		}
		
		// calculate estimated gain reduction
		cestDb_ = threshDb_ * slope_ / 2.;
	};
	
	// getters for metering

	inline float getMakeupDb(void) {
		return (makeupDb_);
	}
	
	inline void getLevels(double ( &levels )[2])
	{
		levels[0] = level_;
		levels[1] = compDb_;
	};
	
	inline float getThreshold(void) {
		return threshDb_;
	};
	
	inline void clearLevels(void) {
		level_ = 0;
		compDb_ = 0;
	};
	
private:
	// math constants
	static constexpr double kPi = 3.14159265358979323846;
	static constexpr double kMinVal = 1e-6;
	static constexpr double kMaxGainDb = 60.;
	static constexpr double kGainSmoothTimeMs = 2000;
	
	// member variables
	bool enable_ = true;
	bool makeup_ = false;
	
	dspfloat_t level_ = 0.;
	dspfloat_t gainr_ = 1.;
	dspfloat_t ratio_ = 2.4;
	dspfloat_t slope_ = 1.0 - (1. / ratio_);
	dspfloat_t angle_ = 0;

	dspfloat_t compDb_ = 0.;
	dspfloat_t cestDb_ = 0.;	// static make-up gain
	dspfloat_t cdevDb_ = 0.;    // smoothed gain reduction
	dspfloat_t kneeDb_ = 0.;
	dspfloat_t levelDb_ = 0.;
	dspfloat_t threshDb_ = -20;
	dspfloat_t makeupDb_ = 0.;
	dspfloat_t smoothTc_ = 0.;  // smoothing time constant

	// helper methods
	
	inline float dBToGain(float db)  {
        return powf(10.0f, db / 20.0f);
    };
	
	inline float dBToLogE(float db)  {
		return logf(powf(10.0f, db / 20.0f));
	};

	inline float gainToDb(float gain) {
		return 20.f * log10f(fmax(gain, kMinVal));
	};
	
	// Lagrange interpolation
	inline double lagrpol(double* x, double* y, int n, double xbar)
	{
		double fx = 0.0;
		double l = 1.0;
		for (int i = 0; i < n; i++)	{
			l = 1.0;
			for (int j = 0; j < n; j++)	{
				if (j != i)
					l *= (xbar - x[j])/(x[i] - x[j]);
			}
			fx += l * y[i];
		}
		return (fx);
	};
};
