/*
  ==============================================================================

    NoiseSource.h
    Created: 2 Jul 2019 4:33:21pm
    Author:  Andrew

  ==============================================================================
*/

#pragma once
#ifndef AKDSPLIB_MODULE
#include "BiquadFilter.h" // includes <cmath>
#endif
#include <cstdlib>
#include <ctime>

template <typename dspfloat_t>
class NoiseSource
{
public:
    enum Model {
        kModelWhite=0,
        kModelPink1,
        kModelPink2
    };
    
    enum Stereo {
        kCorrelated = 0,
        kUncorrelated
    };
    
    enum Mode {
        kWithColor = 0,
        kUncolored
    };
    
    enum Param {
        kModel=0,
        kLevel,
        kColor,
		kBqfFc,
		kBqfQv,
		kBqfDb,
		kBqfId
    };
    
    NoiseSource() { setSampleRate(44100.); }
    
    ~NoiseSource() { /* nothing to see here */ }

    void setSampleRate(float fs)
    {
        fs_ = fs;
		bqf_.design(fs_, bqfFreq_, bqfQval_, bqfGain_, bqfType_);
        
        // set seed for rand()
        std::srand((unsigned int)time(nullptr));
    }
    
    void setParam(int pid, float val)
    {
        switch (pid) {
            case kModel:
                model_ = (int)val;
                break;
            case kLevel:
                level_ = val * .01f; // 0 -> 100 range
                break;
			case kColor:
			case kBqfFc:
				bqfFreq_ = val;
				bqf_.design(fs_, bqfFreq_, bqfQval_, bqfGain_, bqfType_);
				break;
			case kBqfQv:
				bqfQval_ = val;
				bqf_.design(fs_, bqfFreq_, bqfQval_, bqfGain_, bqfType_);
				break;
			case kBqfDb:
				bqfGain_ = val;
				bqf_.design(fs_, bqfFreq_, bqfQval_, bqfGain_, bqfType_);
				break;
			case kBqfId:
				bqfType_ = (int)val;
				bqf_.design(fs_, bqfFreq_, bqfQval_, bqfGain_, bqfType_);
				break;
      	}
    }

    // Noise generator mono version
    //
    inline float run(int model = -1, bool calibration = false)
    {
        int m = (model != -1) ? model : model_; // get model
        
        // get random number, scale to {-0.5, +0.5}
        float white = float((double)rand() * kRandMaxInverse - .5f);
        double pink = 0;
        
        // run pinking filter
        // from http://www.firstpr.com.au/dsp/pink-noise/
        //
        if (m == kModelPink1)
        {
            // economy (44.1KHz - need to fix for higher sample rates!)
            b0 = 0.99765f * b0 + white * 0.0990460f;
            b1 = 0.96300f * b1 + white * 0.2965164f;
            b2 = 0.57000f * b2 + white * 1.0526913f;
            pink = b0 + b1 + b2 + white * 0.1848f;
        }
        else
        if (m == kModelPink2)
        {
            // high quality (44.1KHz - need to fix for higher sample rates!)
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
        }
		
		noise_ = (m == kModelWhite) ? 2.f * white : float(pink);
		
        float xout = 0;
		
		if (calibration) {
			xout = noise_ * 0.2511886432f; // -12dB
		} else {
			xout = (float)bqf_.runInterp(noise_) * level_;
		}
        return xout;
    }
	
    // Noise generator stereo version 1
    //
    inline void run(double ( &x )[2])
    {
        x[0] = run(); // colored noise in L
        x[1] = noise_; // raw noise in R
    }

    // Noise generator stereo version 2
    //
	inline void run(double ( &x )[2], bool uncorrelated)
    {
		x[0] = run(); // processed noise in L
		x[1] = uncorrelated ? run() : x[0]; // correlated | uncorrelated in R
	}

private:

	int model_ = kModelWhite;
	
    float fs_;
	float noise_;
	float level_ = 0.5;

    // pinking filter state
    float b0=0;
    float b1=0;
    float b2=0;
    float b3=0;
    float b4=0;
    float b5=0;
    float b6=0;

	// coloring filter
	BiquadFilter<dspfloat_t> bqf_;
	
	float bqfFreq_ = 3500;
	float bqfQval_ = 0.7071f;
	float bqfGain_ = 0;
	int bqfType_ = BiquadFilter<dspfloat_t>::kHighpass;
	
	// constants
	static constexpr double kRandMaxInverse = 1./ ((double) RAND_MAX);
};
