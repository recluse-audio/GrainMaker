/*
  ==============================================================================

    LpfHpfEq.h
    Created: 1 Feb 2022 9:20:01pm
    Author:  Antares

  ==============================================================================
*/

#pragma once

#ifndef AKDSPLIB_MODULE
#include "BiquadFilter.h" // includes <cmath>
#include "../math/LogMath.h"
#endif
#include <atomic>
#include <cassert>
#include <vector>

#define TDF_II 1

template <typename dspfloat_t>
class LpfHpfEq
{
public:
    // HPF | LPF
    enum Type {
        kTypeHPF = 0,
        kTypeLPF = 1,
    };
    
    // Per-band EQ parameters
	enum ParamId {
		kType = 0, // HPF or LPF
		kFreq,
		kQual,
		kEqOn,
        kSolo,
        kSurf,
        kHarm,
        kSlope
	};
	
    // Filter slopes
    enum Slope {
        kSlope1Pole = 0,
        kSlope2Pole = 1,
        kSlope3Pole = 2,
        kSlope4Pole = 3,
        kSlopeTypes = 4
    };

    enum Stage {
        kStage1 = 0,
        kStage2 = 1
    };
    
	// Constructor
	LpfHpfEq()
	{
        // Empty
	}

    // Destructor - empty
	~LpfHpfEq() { /* nothing to see here */ }

    // Zero out the biquad filter state
    inline void reset(void)
    {
        for (int k=0; k < kNumChannels; k++) {
            for (int n=0; n < bqfStages; n++) {
                bqs[k][n].x1_ = bqs[k][n].x2_ = 0.;
#ifndef TDF_II
                bqs[k][n].y1_ = bqs[k][n].y2_ = 0.;
#endif
            }
        }
    }

    //==============================================================================
    // Call once after constructor to set EQ type to either HPF or LPF,
    //   a default center frequency and slope can optionally be set,
    //   overriding the internal default configuration..
    
    void init(int eq_type, float eq_fc = -1)
    {
        eqType = eq_type;
        
        slopeToType = (eqType == kTypeHPF) ? slopeToTypeHPF : slopeToTypeLPF;

        // select default Fc for HPF or LPF, default slope = 12dB / oct
        
        switch (eqType) {
            case kTypeHPF:
                f[0] = f[1] = f_cached = (eq_fc == -1) ? kHpfFcHzDef : eq_fc;
                type[0] = BiquadFilter<dspfloat_t>::kHighpass;
                type[1] = BiquadFilter<dspfloat_t>::kBypass;
                break;
            case kTypeLPF:
                f[0] = f[1] = f_cached = (eq_fc == -1) ? kLpfFcHzDef : eq_fc;
                type[0] = BiquadFilter<dspfloat_t>::kLowpass;
                type[1] = BiquadFilter<dspfloat_t>::kBypass;
                break;
            default:
                assert(false);
                break;
        }
    }
    
    //==============================================================================
    // Re-initialize all filter coefficients using existing settings

    void reinit(void)
    {
        for (int n = 0; n < bqfStages; n++)
        {
            biquad.design(bqfNew[n], type[n], f[n], 0.f, q[n], Fs_);
            
            // update all coefficients
            bqfMain[0][n].a0_ = bqfMain[1][n].a0_ = bqfNew[n].a0_;
            bqfMain[0][n].a1_ = bqfMain[1][n].a1_ = bqfNew[n].a1_;
            bqfMain[0][n].a2_ = bqfMain[1][n].a2_ = bqfNew[n].a2_;
            bqfMain[0][n].b1_ = bqfMain[1][n].b1_ = bqfNew[n].b1_;
            bqfMain[0][n].b2_ = bqfMain[1][n].b2_ = bqfNew[n].b2_;
        }
        
        // calculate smoothing coefficient
        smoothTc_ = onePoleCoeff(kSmoothMsec); // 20 msec

        // clear filter state
        reset();
    }

    //==============================================================================
    // Set the sample rate and re-design filters

    void setSampleRate(float fs)
    {
        Fs_ = fs;
        reinit();
    }
    
    //==============================================================================
    // Set filter parameters
    
    void setControl(int paramId, float param)
    {
        // apply the control
        
        switch (paramId) {
            case kEqOn:
                enable = (bool)param;
                design(0, kEqOn, (bool)param);
                design(1, kEqOn, (bool)param);
                break;
                
            case kSolo:
                solo = (bool)param;
                if (solo) {
                    slopeToType = (eqType == kTypeHPF) ? slopeToTypeLPF : slopeToTypeHPF;
                }
                else {
                    slopeToType = (eqType == kTypeHPF) ? slopeToTypeHPF : slopeToTypeLPF;
                }
                
                design(0, kType, static_cast<float>(slopeToType[slope][0]));
                design(1, kType, static_cast<float>(slopeToType[slope][1]));
                break;
                
            case kSurf:
                surf = (bool)param;
                if (surf)
                    f_cached = f[0];
                else {
                    design(0, kFreq, f_cached);
                    design(1, kFreq, f_cached);
                }
                break;
                
            case kFreq:
                design(0, kFreq, param);
                design(1, kFreq, param);
                break;
                
            case kQual:
                q2Pole = param;
                if (slope == kSlope2Pole) {
                    design(0, kQual, q2Pole);
                }
                break;
                
            case kSlope:
                slope = (int)param;
                q[0] = (slope == kSlope2Pole) ? q2Pole : slopeToQval[slope][0];
                q[1] = (slope == kSlope2Pole) ? kQInit : slopeToQval[slope][1];
                design(0, kType, static_cast<float>(slopeToType[slope][0]));
                design(1, kType, static_cast<float>(slopeToType[slope][1]));
                break;
                
            default:
                assert(false);
                break;
        }
        
        // swap in the new coefficients
        if (!surf) {
            update.store(true);
        }
    }

    //==============================================================================
    // Design a single biquad filter stage
    
    void design(int band, int param, float val)
    {
        switch(param) {
            case kEqOn:
                eqOn[band] = (bool)val;
                break;
                
            case kFreq:
                f[band] = val;
                break;
                
            case kQual:
                q[band] = val;
                break;
                
            case kType:
                type[band] = (int)val;
                break;
               
            default:
                assert(false);
                break;
        }
        
        // design the biquad filter
        
        biquad.design(bqfNew[band], type[band], f[band], 0.f, q[band], Fs_);
    }

    //==============================================================================
    // update pitch for surf EQ, called from audio thread before run()
    
    inline void trackPitch(float freq_hz)
    {
        if (surf && freq_hz != -1) {
            design(0, kFreq, freq_hz * surfHarm);
            design(1, kFreq, freq_hz * surfHarm);
            update.store(true); // update coeffs
        }
        else {
            f[0] = f[1] = f_cached;
        }
    }
        
    //==============================================================================
    // Run the LPF / HPF as two cascaded biquad filters
    
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
    {
        if (update.exchange(false))
        {
            // copy coefficients to target filter
            for (int n=0; n < bqfStages; n++)
            {
                bqfMain[kTarget][n].a0_ = bqfNew[n].a0_;
                bqfMain[kTarget][n].a1_ = bqfNew[n].a1_;
                bqfMain[kTarget][n].a2_ = bqfNew[n].a2_;
                bqfMain[kTarget][n].b1_ = bqfNew[n].b1_;
                bqfMain[kTarget][n].b2_ = bqfNew[n].b2_;
            }
        }

        if (!enable && !solo)
        {
            xo[0] = xi[0];
            xo[1] = xi[1];
            return;
        }
        
        dspfloat_t y[2] = {0, 0};
        dspfloat_t x[2];

        x[0] = xi[0];
        x[1] = xi[1];
        
        Biquad<dspfloat_t>* ba;
        Biquad<dspfloat_t>* bt;

        int numChans = stereo ? 2 : 1;
        
        if (!stereo) {
            // if mono, pass through R channel to output (needed for M-S mode)
            xo[1] = xi[1];
        }
            
        // run coefficient smoothing
        for (int n=0; n < bqfStages; n++)
        {
            bt = &bqfMain[kTarget][n]; // get target coefficients
            ba = &bqfMain[kActive][n]; // get active coefficients

            ba->a0_ = smoothTc_ * ba->a0_ + (1.f - smoothTc_) * bt->a0_;
            ba->a1_ = smoothTc_ * ba->a1_ + (1.f - smoothTc_) * bt->a1_;
            ba->a2_ = smoothTc_ * ba->a2_ + (1.f - smoothTc_) * bt->a2_;
            ba->b1_ = smoothTc_ * ba->b1_ + (1.f - smoothTc_) * bt->b1_;
            ba->b2_ = smoothTc_ * ba->b2_ + (1.f - smoothTc_) * bt->b2_;
        }
        
        // run the filters
        for (int k=0; k < numChans; k++)
        {
            for (int n=0; n < bqfStages; n++)
            {
                ba = &bqfMain[kActive][n]; // get active coefficients
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
    
    //==============================================================================

    // Define EQ band state
    struct EqState
    {
        int type;
        bool eqOn;
        bool solo;
        float freq;
        float qual;
    };

    // Get current EQ state
    void getEqState(EqState& state)
    {
        state.type = eqType; // HPF or LPF
        state.eqOn = enable;
        state.solo = solo;
        state.freq = f[0];
        state.qual = q[0];
    }

	// Get filter magnitude response
	void getMagnitudeResponse(float freq_hz[], float mag_db[], int numPoints)
	{
		dspfloat_t h;
	
        for (int n = 0; n < numPoints; n++)
        {
            this->setGraphFreqPoint(freq_hz[n]); // calculate sin/cos values
            h = getBiquadResponse(bqfNew[0]);
            h *= getBiquadResponse(bqfNew[1]);
            mag_db[n] = linToDb((float)h);
        }
	}
        
    // Get bandwidth in octaves from q value
    float getOctaveBw(int band) const {
        return (2.f / logf(2.f)) * asinh(1.f / (2.f * q[band + 1]));
    }
    
    bool inSolo(void) const { return solo; }
    
private:
    static constexpr int kNumBiquads = 2;
    static constexpr int kNumChannels = 2; // stereo
    
    static constexpr float kHpfFcHzDef = 20.f;
    static constexpr float kLpfFcHzDef = 20000.f;
    
    static constexpr float kSqrt2 = 1.4142135624f;
    static constexpr float kQInit = 0.5f * kSqrt2;
    
    static constexpr float kSmoothMsec = 20.f;

    BiquadFilter<dspfloat_t> biquad; // for coefficient calculation
	Biquad<dspfloat_t> bqfNew[2]; // 2 stages / 4 poles
    
    Biquad<dspfloat_t> bqfMain[2][2]; // [L | R], [kTarget, kActive]
    Biquad<dspfloat_t> bqfSolo[2][2]; // [L | R], [kTarget, kActive]

	struct State {
		dspfloat_t x1_ = 0;
		dspfloat_t x2_ = 0;
#ifndef TDF_II
        dspfloat_t y1_ = 0;
		dspfloat_t y2_ = 0;
#endif
	} bqs[kNumChannels][2];
	    
    enum {
        kActive = 0,
        kTarget = 1
    };
    
    std::atomic<bool> update { false };
    
    bool eqOn[2] = {false, false};
    bool solo = false;
    bool surf = false;
    bool enable = false;
    
    int bqfStages = 2; // two stages
    int eqType = kTypeHPF;
    int slope = kSlope2Pole;
    int type[2]; // current type
    int surfHarm = 1; // pitch tracking ('surf') harmonic

    float Fs_ = 44100.; // initial sample rate

    // Per-biquad filter parameters
    
    float q[2] = {kQInit, kQInit};
    float f[2] = {1000.f, 1000.f};
    float f_cached = 1000.f;
    float q2Pole = kQInit;
        
    // Q values for cascaded Butterworth filters
    const float slopeToQval[4][2] = {
        {0.70710678f, 0.70710678f}, // 6dB / oct
        {0.70710678f, 0.70710678f}, // 12dB / oct
        {0.70710678f, 1.00000000f}, // 18dB / oct
        {0.54119610f, 1.30656300f}  // 24dB / oct
    };

    // Biquad stage configuration for HPF
    const int slopeToTypeHPF[4][2] = {
        {BiquadFilter<dspfloat_t>::kOnePoleHP, BiquadFilter<dspfloat_t>::kBypass},   // 6dB / oct
        {BiquadFilter<dspfloat_t>::kHighpass, BiquadFilter<dspfloat_t>::kBypass},    // 12dB / oct
        {BiquadFilter<dspfloat_t>::kOnePoleHP, BiquadFilter<dspfloat_t>::kHighpass}, // 18dB / oct
        {BiquadFilter<dspfloat_t>::kHighpass, BiquadFilter<dspfloat_t>::kHighpass},  // 24dB / oct
    };

    // Biquad stage configuration for LPF
    const int slopeToTypeLPF[4][2] = {
        {BiquadFilter<dspfloat_t>::kOnePoleLP, BiquadFilter<dspfloat_t>::kBypass},   // 6dB / oct
        {BiquadFilter<dspfloat_t>::kLowpass, BiquadFilter<dspfloat_t>::kBypass},     // 12dB / oct
        {BiquadFilter<dspfloat_t>::kOnePoleLP, BiquadFilter<dspfloat_t>::kLowpass},  // 18dB / oct
        {BiquadFilter<dspfloat_t>::kLowpass, BiquadFilter<dspfloat_t>::kLowpass},    // 24dB / oct
    };
    
    const int (*slopeToType)[2];
        
    // Convert scalar gain to decibels
	float linToDb(float x)
    {
		return 20.f * log10f(fmax(x, 1e-04f)); // -80dB
	}

    // Calculate coefficient smoothing parameter
    dspfloat_t onePoleCoeff(float tau)
    {
        if(tau > 0.f) {
            return exp(-1.f / (tau * .001f * Fs_));
        }
        return 1.f;
    }

    dspfloat_t smoothTc_ = 0.f; // coefficient smoothing filter

    /// Magnitude response calculation helpers
    
    static constexpr double kPi = 3.14159265358979323846;

    double cos1;
    double sin1;
    double cos2;
    double sin2;
    
    void setGraphFreqPoint(float freq_hz)
    {
        double w = 2 * kPi * freq_hz / Fs_;
        
        cos1 = cos(-1 * w);
        cos2 = cos(-2 * w);
        sin1 = sin(-1 * w);
        sin2 = sin(-2 * w);
    }
    
    // Calculates magnitude reponse |H(z)| for single biquad filter stage at freq_hz
	// from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
	//
	float getBiquadResponse(Biquad<dspfloat_t> b)
	{
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
    
#if 0
    int numMagBins = 0;

    // std::vector version of graph data calculation (work in progress)
    //
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
        assert(Fs_ != 0);
        for (size_t n = 0; n < bins; n++) {
            w = 2.f * kPi * fn[n] / Fs_;
            cos1w[n] = cos(-1 * w);
            cos2w[n] = cos(-2 * w);
            sin1w[n] = sin(-1 * w);
            sin2w[n] = sin(-2 * w);
        }
        
        numMagBins = bins;
    }
    
#endif
};
