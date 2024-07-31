/*
  ==============================================================================

    ChannelEQ.h
    Created: 30 Jan 2020 9:20:01pm
    Author:  Antares

  ==============================================================================
*/

#pragma once

#ifndef AKDSPLIB_MODULE
#include "BiquadFilter.h" // includes <cmath>
#include "../math/LogMath.h"
#endif
#include <vector>

#define TDF_II 1

template <typename dspfloat_t>
class ChannelEQ
{
public:
	static constexpr int kNumBiquads = 9;
	static constexpr int kNumEqBands = 7;
	static constexpr int kNumChannels = 2; // stereo

    enum EqStruct {
        kEqHPF = 0,
        kEqLF,
        kEqLMF,
        kEqMF,
        kEqHMF,
        kEqHF,
        kEqLPF,
        kEqBands
    };

    // per-band EQ settings
	enum Param {
		kType = 0,
		kFreq,
		kGain,
		kQual,
		kFlip,
		kEqOn
	};
	
	// constructor - initialize a default channel EQ
	ChannelEQ(int bands = kNumBiquads, int bins = 800)
	{
		numBiquads = bands;
        numMagBins = bins;
		setDefaults();
		reinit();
	}

	~ChannelEQ() { /* nothing to see here */ }

	void setSampleRate(float fs) {
		Fs_ = fs;
        smoothTc_ = onePoleCoeff(20.); // 20 milliseconds
		reinit();
	}
    
    
	/**
	 * Get the current sample rate;
	 *
	 * Used by pyakdsplib.
	 */
	float getSampleRate() { return Fs_; }

	// run the channel equalizer as a series of cascaded biquad filters
	inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
	{
		double x[2];
		double y[2] = {0, 0};

		x[0] = xi[0];
		x[1] = xi[1];
		
        Biquad<dspfloat_t>* ba;
        Biquad<dspfloat_t>* bt;

		int numChans = stereo ? 2 : 1;
		
        // run coefficient smoothing
        for (int n=0; n < numBiquads; n++)
        {
            ba = &bqf[0][n]; // get active coefficients
            bt = &bqf[1][n]; // get target coefficients

            ba->a0_ = smoothTc_ * ba->a0_ + (1. - smoothTc_) * bt->a0_;
            ba->a1_ = smoothTc_ * ba->a1_ + (1. - smoothTc_) * bt->a1_;
            ba->a2_ = smoothTc_ * ba->a2_ + (1. - smoothTc_) * bt->a2_;
            ba->b1_ = smoothTc_ * ba->b1_ + (1. - smoothTc_) * bt->b1_;
            ba->b2_ = smoothTc_ * ba->b2_ + (1. - smoothTc_) * bt->b2_;
        }
        
        // run the filters
		for (int k=0; k < numChans; k++)
		{
			for (int n=0; n < numBiquads; n++)
			{
				ba = &bqf[0][n]; // get active coefficients
#ifdef TDF_II
                // run direct form transposed II filter - calculate output sample
                y[k] = ba->a0_ * x[k] + bqs[k][n].x1_;
                
                // update filter state
                bqs[k][n].x1_ = ba->a1_ * x[k] - ba->b1_ * y[k] + bqs[k][n].x2_;
                bqs[k][n].x2_ = ba->a2_ * x[k] - ba->b2_ * y[k];
#else
                // run direct form I filter - calculate output sample
				y[k] = ba->a0_ * x[k] + ba->a1_ * bqs[k][n].x1_ + ba->a2_ * bqs[k][n].x2_
                                      - ba->b1_ * bqs[k][n].y1_ - ba->b2_ * bqs[k][n].y2_;
				
				// update filter state
				bqs[k][n].x2_ = bqs[k][n].x1_; bqs[k][n].x1_ = x[k];
				bqs[k][n].y2_ = bqs[k][n].y1_; bqs[k][n].y1_ = y[k];
#endif
				// next stage filter input is previous stage filter output
				x[k] = y[k];
			}
			xo[k] = y[k];
		}
	}
	
	// zero out all biquad filter state
	inline void reset(void)
	{
		for (int k=0; k < kNumChannels; k++) {
			for (int n=0; n < numBiquads; n++) {
                bqs[k][n].x1_ = bqs[k][n].x2_ = 0.;
#ifndef TDF_II
                bqs[k][n].y1_ = bqs[k][n].y2_ = 0.;
#endif
			}
		}
	}

	
	// re-initialize all filter coefficients using existing settings
	void reinit(void)
	{
		for (int n = 0; n < numBiquads; n++)
		{
			biquad.design(bqfNew[n], type[n], f[n], g[n], q[n], Fs_);
			
			// update all coefficients
			bqf[0][n].a0_ = bqf[1][n].a0_ = bqfNew[n].a0_;
			bqf[0][n].a1_ = bqf[1][n].a1_ = bqfNew[n].a1_;
			bqf[0][n].a2_ = bqf[1][n].a2_ = bqfNew[n].a2_;
			bqf[0][n].b1_ = bqf[1][n].b1_ = bqfNew[n].b1_;
			bqf[0][n].b2_ = bqf[1][n].b2_ = bqfNew[n].b2_;
		}
		
		// clear filter state
		reset();
	}

	// create default EQ settings
	void setDefaults(void)
	{
		for (int n = 0; n < kNumBiquads; n++)
		{
            f[n] = eqState[n].freq;
            g[n] = eqState[n].gain;
            q[n] = eqState[n].qual;
            
            eqOn[n] = eqState[n].eqOn;
            flip[n] = eqState[n].flip;
			type[n] = eqState[n].type;
			type_[n] = eqState[n].type; //BiquadFilter::kBypass;
		}
	}

	// set a config parameter for an EQ band
	void setControl(int band, int param, float val)
	{
		switch(param) {
			case kType:
				type[band] = (int)val;
				break;
			case kFreq:
				f[band] = val;
				break;
			case kGain:
				g[band] = val;
				break;
			case kQual:
				q[band] = val;
				break;
			case kFlip:
				flip[band] = (bool)val;
				g[band] = -g[band];
				if (flip[band]) {
				}
				break;
			case kEqOn:
				eqOn[band] = (bool)val;
				if (eqOn[band]) {
					// turn on
					type[band] = type_[band];
				} else {
					// turn off
					type_[band] = type[band];
					type[band] = BiquadFilter<dspfloat_t>::kBypass;
				}
				break;
			default:
				// eeek!
				break;
		}
		
		// do filter design
		biquad.design(bqfNew[band], type[band], f[band], g[band], q[band], Fs_);
		
		// copy coefficients to target filter
		for (int n=0; n < numBiquads; n++) {
			bqf[1][n].a0_ = bqfNew[n].a0_;
			bqf[1][n].a1_ = bqfNew[n].a1_;
			bqf[1][n].a2_ = bqfNew[n].a2_;
			bqf[1][n].b1_ = bqfNew[n].b1_;
			bqf[1][n].b2_ = bqfNew[n].b2_;
		}
	}

	/** Return the value for a control
	 *
	 * This function is used by the python wrapper
	 * pyakdsplib to check internal state.
	 */
	float getControl(int band, int param)
	{
		switch(param) {
			case kType: return type[band];
			case kFreq: return f[band];
			case kGain: return g[band];
			case kQual: return q[band];
			case kEqOn: return eqOn[band];
			default:
				return 0.0;
		}
	}

	/**
	 * Used by pyakdsplib to set the value for a single filter.
	 */
	void setBiquad(int band, int t, float freq, float qual, float gain)
	{
		type[band] = t;
		f[band] = freq;
		q[band] = qual;
		g[band] = gain;

		// do filter design
		biquad.design(bqfNew[band], type[band], f[band], g[band], q[band], Fs_);

		// point to free filter
		int l = 1 - live_;

		// copy coefficients to free filter
		for (int n=0; n < numBiquads; n++) {
			bqf[l][n].a0_ = bqfNew[n].a0_;
			bqf[l][n].a1_ = bqfNew[n].a1_;
			bqf[l][n].a2_ = bqfNew[n].a2_;
			bqf[l][n].b1_ = bqfNew[n].b1_;
			bqf[l][n].b2_ = bqfNew[n].b2_;
		}
		
		// make this filter active
		live_ = 1 - live_;
	}

    // define EQ band settings
    struct EqState
    {
        int type;
        bool eqOn;
        bool flip;
        float freq;
        float gain;
        float qual;
    };

    void getEqState(EqState& state, int eqBand)
    {
        int chan = eqBand + 1; // offset to account for HPF / LPF
        state.type = type[chan];
        state.eqOn = eqOn[chan];
        state.flip = flip[chan];
        state.freq = f[chan];
        state.gain = g[chan];
        state.qual = q[chan];
    }

	float getOctaveBw(int band) {
		return (2.f / logf(2.f)) * asinh(1.f / (2.f * q[band + 1]));
	}
	

	// get single band magnitude response
	void getMagnitudeResponse(float freq_hz[], float mag_db[], int numPoints, int band)
	{
		dspfloat_t h;
	
		switch (band) {
			case kEqHPF:
				for (int n = 0; n < numPoints; n++) {
                    this->setFreqPoint(freq_hz[n]); // calculate sin/cos values
					h = getBiquadResponse(bqfNew[0]);
					h *= getBiquadResponse(bqfNew[1]);
					mag_db[n] = linToDb((float)h);
				}
				break;
			default: // PEQ
				for (int n = 0; n < numPoints; n++) {
                    this->setFreqPoint(freq_hz[n]); // calculate sin/cos values
					h = getBiquadResponse(bqfNew[band + 1]);
					mag_db[n] = linToDb((float)h);
				}
				break;
			case kEqLPF:
				for (int n = 0; n < numPoints; n++) {
                    this->setFreqPoint(freq_hz[n]); // calculate sin/cos values
					h = getBiquadResponse(bqfNew[7]);
					h *= getBiquadResponse(bqfNew[8]);
					mag_db[n] = linToDb((float)h);
				}
				break;
		}
	}
    
#if 0
	// get magnitude curve for all bands over a vector of frequencies
	void getMagnitudeResponse(float freq_hz[], float mag_db[][800], int numPoints)
	{
		for (int n = 0; n < numPoints; n++)
		{
			dspfloat_t h;
            
            this->setFreqPoint(freq_hz[n]); // calculate sin/cos values
			
			// HPF band
			h = getBiquadResponse(bqfNew[0]);
			h *= getBiquadResponse(bqfNew[1]);
			mag_db[kEqHPF][n] = linToDb((float)h);

			// PEQ band
			for (int band = 2; band < 7; band++) {
				h = getBiquadResponse(bqfNew[band]);
				mag_db[band][n] = linToDb((float)h);
			}
			
			// LPF band
			h = getBiquadResponse(bqfNew[7]);
			h *= getBiquadResponse(bqfNew[8]);
			mag_db[kEqLPF][n] = linToDb((float)h);
		}
	}
#endif
    
#if 0
    // get aggregate filter magnitude at single frequency point
    dspfloat_t getMagnitudeResponse(float freq_hz)
    {
        dspfloat_t magnitude = 1.;
        
        this->setFreqPoint(freq_hz); // calculate sin/cos values
        
       // loop over all biquad sections
        for (int n=0; n < numBiquads; n++) {
            magnitude *= getBiquadResponse(bqfNew[n], freq_hz);
        }
        return linToDb((float)magnitude);
    }
#endif
    
private:
	static constexpr double kPi = 3.14159265358979323846;

	BiquadFilter<dspfloat_t> biquad; // for coefficient calculation
	
	Biquad<dspfloat_t> bqf[2][kNumBiquads];
	Biquad<dspfloat_t> bqfNew[kNumBiquads];
	
	struct State {
		dspfloat_t x1_ = 0;
		dspfloat_t x2_ = 0;
#ifndef TDF_II
        dspfloat_t y1_ = 0;
		dspfloat_t y2_ = 0;
#endif
	} bqs[kNumChannels][kNumBiquads];
	
	int live_ = 0;
    
    enum {
        kActive = 0,
        kTarget
    };
    
    int numEqBands = kNumEqBands;
	int numBiquads = kNumBiquads;
    int numMagBins = 800;
	
	int type[kNumBiquads];
	int type_[kNumBiquads]; // cached
	
	bool eqOn[kNumBiquads];
	bool flip[kNumBiquads];
	
	float q[kNumBiquads];
	float f[kNumBiquads];
	float g[kNumBiquads];
	
	float Fs_ = 44100.; // initial sample rate

    double smoothTc_ = 0.f; // coefficient smoothing filter
    
	// defaults
	EqState eqState[kNumBiquads] = {
        // type, eqOn, flip, f, g, q
		{ BiquadFilter<dspfloat_t>::kHighpass, true, false, 20, 0, .7071f },
		{ BiquadFilter<dspfloat_t>::kBypass, true, false, 20, 0, .7071f },
		{ BiquadFilter<dspfloat_t>::kPeaking, true, false, 200, 0, 1.414f },
		{ BiquadFilter<dspfloat_t>::kPeaking, true, false, 500, 0, 1.414f },
		{ BiquadFilter<dspfloat_t>::kPeaking, true, false, 2500, 0, 1.414f },
		{ BiquadFilter<dspfloat_t>::kPeaking, true, false, 6500, 0, 1.414f },
		{ BiquadFilter<dspfloat_t>::kPeaking, true, false, 10000, 0, 1.414f },
		{ BiquadFilter<dspfloat_t>::kLowpass, true, false, 20000, 0, .7071f },
		{ BiquadFilter<dspfloat_t>::kBypass, true, false, 20000, 0, .7071f }
	};
	
	inline float linToDb(float x) {
		return 20.f * log10f(fmax(x, 1e-04f));
	}


    // used to calculate coefficient smoothing parameter
    inline double onePoleCoeff(double tau) {
        if(tau > 0.0) {
            return exp(-1.0 / (tau * .001 * Fs_));
        }
        return 1.0;
    }
    
	// Calculates magnitude reponse |H(z)| for single biquad filter stage at freq_hz
	// from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
	//
	float getBiquadResponse(Biquad<dspfloat_t> b)
	{
		assert(Fs_ != 0);
		
		double realZeros = b.a0_ + b.a1_ * cos1 + b.a2_ * cos2;
		double imagZeros = b.a1_ * sin1 + b.a2_ * sin2;
		
		double realPoles = 1 + b.b1_ * cos1 + b.b2_ * cos2;
		double imagPoles = b.b1_ * sin1 + b.b2_ * sin2;
		
		double divider = realPoles * realPoles + imagPoles * imagPoles;
		
		double realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
		double imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;
		
		double power = realHw * realHw + imagHw * imagHw;
		return sqrtf(fmax((float)power, 0.f));
	}
	
    void setFreqPoint(float freq_hz)
    {
        double w = 2 * kPi * freq_hz / Fs_;
        
        cos1 = cos(-1 * w);
        cos2 = cos(-2 * w);
        sin1 = sin(-1 * w);
        sin2 = sin(-2 * w);
    }

    double cos1;
    double sin1;
    double cos2;
    double sin2;
    
    /**
     
     */
    void updateGraphState(float fmin, float fmax, size_t bins)
    {
        // fn is a vector that contains a logarithically spaced array of frequency points,
        // typically in the range {fmin, fmax} = {20Hz, 20Khz},
        // called from the FilterGraph constructor or when the graph is resized,
        // or on change of sample rate (prepareToPlay)
        
        std::vector<float>fn;
        fn.resize(bins);
        
        LogMath::logspace(fn.data(), fmin, fmax, (int)bins);
        
        std::vector<double> cos1w;
        std::vector<double> cos2w;
        std::vector<double> sin1w;
        std::vector<double> sin2w;
        
        //size_t bins = fn.size();
        
        cos1w.resize(bins);
        cos2w.resize(bins);
        sin1w.resize(bins);
        sin2w.resize(bins);
        
        double w;
        for (size_t n = 0; n < bins; n++) {
            w = 2.f * kPi * fn[n] / Fs_;
            cos1w[n] = cos(-1 * w);
            cos2w[n] = cos(-2 * w);
            sin1w[n] = sin(-1 * w);
            sin2w[n] = sin(-2 * w);
        }
    }
};
