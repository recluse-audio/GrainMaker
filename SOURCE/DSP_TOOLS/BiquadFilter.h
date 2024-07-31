/*
  ==============================================================================

    BiquadFilter.h
    Created: 26 Aug 2018 7:59:17pm
    Author:  Antares

    Copyright © 2022 Antares Audio Technologies. All rights reserved.

  ==============================================================================
*/
#pragma once
#define TDF_II 1 // Transposed Direct Form II
//#define FILTER_DEBUG 1

#include <assert.h>

#ifdef FILTER_DEBUG
#include <iomanip>
#include <iostream>
#endif
#include <cassert>
#include <cmath>

template <typename dspfloat_t>
struct Biquad {
    dspfloat_t a0_ = 1;
    dspfloat_t a1_ = 0;
    dspfloat_t a2_ = 0;
    dspfloat_t b1_ = 0;
    dspfloat_t b2_ = 0;
};

template <typename dspfloat_t>
struct BiquadStruct {
    dspfloat_t A[3];
    dspfloat_t B[3];
};

template <typename dspfloat_t>
class BiquadFilter {
public:
    // don't change the order of these!
    enum FilterType {
        kBypass = 0,
        kOnePoleLP,
        kOnePoleHP,
        kLowpass,
        kHighpass,
        kBandpass,
        kNotch,
        kResonator,
        kPeaking,
        kLowshelf,
        kHighshelf,
        kPeakingAH
    };

    enum {
        kGainLogDb = 0,
        kGainLinear = 1
    };
    
    enum {
        kTarget = 0,
        kActive
    };
    
    BiquadFilter() { reset(); }

    ~BiquadFilter() { /* nothing to see here */ }

    /**
        Set filter state variables to zero
     */
    inline void reset(void)
    {
        x1_[0] = x2_[0] = x1_[1] = x2_[1] = 0.;
#ifndef TDF_II
        y1_[0] = y2_[0] = y1_[1] = y2_[1] = 0.;
#endif
    }

    void init(float fs)
    {
        Fs_ = fs;
        design(Fs_, Fc_, Q_, gain_, filterType_);
        reset(); // clear filter state
    }
    
    //-------------------------------------------------------------------
    // Core filter design coefficient calculator
    // from: http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
    // and Zolger "Digital Audio Signal Processing" Chapter 5 for derivation
    //
    void design(float fs, float fc, float Q, float gain, int type = kBypass, bool gainType = kGainLogDb)
    {
        assert(fs > 0. && fc > 0. && Q > 0.);
        //assert (fc < 0.5 * fs); // will crash auval in DEBUG build at 22.05Khz sample rate

        Fs_ = fs; Fc_ = fc; Q_ = Q; gain_ = gain;
        
        filterType_ = (FilterType)type;

        dspfloat_t K = tan(kPi * Fc_ / Fs_);
        dspfloat_t K2 = K * K;
        
        dspfloat_t V;
        bool boost = false;
        if (gainType == kGainLinear) {
            V = static_cast<dspfloat_t>((gain < 1.) ? 1. / gain : gain);
            boost = (gain > 1.);
        }
        else {
            V = static_cast<dspfloat_t>((gain == 0) ? 1. : pow(10.f, fabs(gain) / 20.f)); // dB to gain
            boost = (gain > 0.);
        }
        
        dspfloat_t W;
        if (type == kLowshelf || type == kHighshelf)
            W = sqrt(2 * V);
        else
            W = 1.4142135624f; // sqrt(2)

        // optimizations
        
		dspfloat_t R = 1.f / Q;
        dspfloat_t BW = fc * R;
        dspfloat_t norm = 1.f;

        b0_ = 1.;

        switch (type) {
            default:
            case kBypass:
                a0_ = 1.;
                a1_ = 0.;
                a2_ = 0.;
                b1_ = 0.;
                b2_ = 0.;
                break;
            case kOnePoleLP:
                norm = K / (K + 1);
                a0_ = norm;
                a1_ = norm;
                a2_ = 0.;
                b1_ = (1 - 1 / K) * norm;
                b2_ = 0.;
                break;
            case kOnePoleHP:
                norm = 1 / (K + 1);
                a0_ = norm;
                a1_ = -norm;
                a2_ = 0.;
                b1_ = (K - 1) * norm;
                b2_ = 0.;
                break;
            case kLowpass:
                norm = 1 / (1 + K * R + K2);
                a0_ = K2 * norm;
                a1_ = 2 * a0_;
                a2_ = a0_;
                b1_ = 2 * (K2 - 1) * norm;
                b2_ = (1 - K * R + K2) * norm;
                break;
            case kHighpass:
                norm = 1 / (1 + K * R + K2);
                a0_ = 1 * norm;
                a1_ = -2 * a0_;
                a2_ = a0_;
                b1_ = 2 * (K2 - 1) * norm;
                b2_ = (1 - K * R + K2) * norm;
                break;
            case kBandpass:
                norm = 1 / (1 + K * R + K2);
                a0_ = K * R * norm;
                a1_ = 0;
                a2_ = -a0_;
                b1_ = 2 * (K2 - 1) * norm;
                b2_ = (1 - K * R + K2) * norm;
                break;
            case kNotch:
                norm = 1 / (1 + K * R + K2);
                a0_ = (1 + K2) * norm;
                a1_ = 2 * (K2 - 1) * norm;
                a2_ = a0_;
                b1_ = a1_;
                b2_ = (1 - K * R + K2) * norm;
                break;
            case kResonator:
                b2_ = exp(-2 * kPi * BW / Fs_);
                b1_ = ((1.f - K2) / (1.f + K2)) * (-4.f * b2_) / (1.f + b2_);
                b0_ = 0.;
                a0_ = 1.f - sqrt(b2_);
                a1_ = 0.;
                a2_ = -a0_;
                break;
            case kPeaking:
                if (boost) { // boost
                    norm = 1 / (1 + R * K + K2);
                    a0_ = (1 + V * R * K + K2) * norm;
                    a1_ = 2 * (K2 - 1) * norm;
                    a2_ = (1 - V * R * K + K2) * norm;
                    b1_ = a1_;
                    b2_ = (1 - R * K + K2) * norm;
                } else { // cut
                    norm = 1 / (1 + V * R * K + K2);
                    a0_ = (1 + R * K + K2) * norm;
                    a1_ = 2 * (K2 - 1) * norm;
                    a2_ = (1 - R * K + K2) * norm;
                    b1_ = a1_;
                    b2_ = (1 - V * R * K + K2) * norm;
                }
                break;
            case kLowshelf:
                if (boost) { // boost
                    norm = 1 / (1 + kSQRT2 * K + K2);
                    a0_ = (1 + W * K + V * K2) * norm;
                    a1_ = 2 * (V * K * K - 1) * norm;
                    a2_ = (1 - W * K + V * K2) * norm;
                    b1_ = 2 * (K2 - 1) * norm;
                    b2_ = (1 - kSQRT2 * K + K2) * norm;
                } else { // cut
                    norm = 1 / (1 + W * K + V * K2);
                    a0_ = (1 + kSQRT2 * K + K2) * norm;
                    a1_ = 2 * (K2 - 1) * norm;
                    a2_ = (1 - kSQRT2 * K + K2) * norm;
                    b1_ = 2 * (V * K2 - 1) * norm;
                    b2_ = (1 - W * K + V * K2) * norm;
                }
                break;
            case kHighshelf:
                if (boost) { // boost
                    norm = 1 / (1 + kSQRT2 * K + K2);
                    a0_ = (V + W * K + K2) * norm;
                    a1_ = 2 * (K2 - V) * norm;
                    a2_ = (V - W * K + K2) * norm;
                    b1_ = 2 * (K2 - 1) * norm;
                    b2_ = (1 - kSQRT2 * K + K2) * norm;
                } else { // cut
                    norm = 1 / (V + W * K + K2);
                    a0_ = (1 + kSQRT2 * K + K2) * norm;
                    a1_ = 2 * (K2 - 1) * norm;
                    a2_ = (1 - kSQRT2 * K + K2) * norm;
                    b1_ = 2 * (K2 - V) * norm;
                    b2_ = (V - W * K + K2) * norm;
                }
                break;
            case kPeakingAH:
                // Andy Hildebrand peaking EQ (used by PitchEstimator class)
                // NOTE: this filter produces different response compared to kPeaking above!
                
                dspfloat_t omega = (2.f * kPi * Fc_) / Fs_;
                dspfloat_t sn = sin(omega);
                dspfloat_t cs = cos(omega);
                dspfloat_t alpha = sn / (2.f * Q_);
                dspfloat_t A = pow(10.f, 0.5f * gain_ / 20.f);
                
                // calculate filter coefficients
                a0_ = 1.f + alpha * A;
                a1_ = -2.f * cs;
                a2_ = 1.f - alpha * A;
                b0_ = 1.f + alpha / A;
                b1_ = -2.f * cs;
                b2_ = 1.f - alpha / A;
                
                // normalize
                a0_ /= b0_;
                a1_ /= b0_;
                a2_ /= b0_;
                b1_ /= b0_;
                b2_ /= b0_;
                b0_ = 1.;
                break;
        }
        
        // copy coefficients to target coeffs
        
        a0f_[kTarget] = a0_;
        a1f_[kTarget] = a1_;
        a2f_[kTarget] = a2_;
        b0f_[kTarget] = b0_;
        b1f_[kTarget] = b1_;
        b2f_[kTarget] = b2_;
        
        // ouput debug information
#ifdef FILTER_DEBUG // DEBUG
        std::cout << "type = " << filterType_ << "\n";
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "a0 = " << a0_ << "\n";
        std::cout << "a1 = " << a1_ << "\n";
        std::cout << "a2 = " << a2_ << "\n";
        std::cout << "b0 = " << b0_ << "\n";
        std::cout << "b1 = " << b1_ << "\n";
        std::cout << "b2 = " << b2_ << "\n";
        std::cout << "\n";
#endif
    }

    // Design a biquad filter and return coefficients in Biquad struct
    void design(Biquad<dspfloat_t>& b, int type, float f, float g, float q, float fs)
    {
        design(fs, f, q, g, type);
        b.a0_ = a0_;
        b.a1_ = a1_;
        b.a2_ = a2_;
        b.b1_ = b1_;
        b.b2_ = b2_;
    }

    // Design a biquad filter with a custom coefficient set
    void design(const BiquadStruct<dspfloat_t>& b)
    {
        a0_ = a0f_[kTarget] = b.A[0];
        a1_ = a1f_[kTarget] = b.A[1];
        a2_ = a2f_[kTarget] = b.A[2];
        b0_ = b0f_[kTarget] = b.B[0];
        b1_ = b1f_[kTarget] = b.B[1];
        b2_ = b2f_[kTarget] = b.B[2];
    }

    // Set filter Q
    void setQ(float Q)
    {
        Q_ = Q;
        design(Fs_, Fc_, Q_, gain_, filterType_);
    }

    // Set filter Fc in Hz
    void setFc(float Fc)
    {
        Fc_ = Fc;
        design(Fs_, Fc_, Q_, gain_, filterType_);
    }
    
    // Set filter Gain in dB
    void setGain(float g_db)
    {
        gain_ = g_db;
        design(Fs_, Fc_, Q_, gain_, filterType_);
    }

    // Set new filter type
    void setType(FilterType type)
    {
        filterType_ = type;
        design(Fs_, Fc_, Q_, gain_, filterType_);
    }

    // Set filter to bypass
    void setBypass(bool bypass)
    {
        if (bypass)
            design(Fs_, Fc_, Q_, gain_, BiquadFilter::kBypass);
        else
            design(Fs_, Fc_, Q_, gain_, filterType_);
    }

    /**
        Run biquad filter, mono, no coefficient smoothing
     */
    inline dspfloat_t run(dspfloat_t x)
    {
#ifdef TDF_II
        dspfloat_t y = a0f_[0] * x + x1_[0];
        x1_[0] = a1f_[0] * x - b1f_[0] * y + x2_[0];
        x2_[0] = a2f_[0] * x - b2f_[0] * y;
#else
        dspfloat_t y = a0f_[0] * x + a1f_[0] * x1_[0] + a2f_[0] * x2_[0]
                                   - b1f_[0] * y1_[0] - b2f_[0] * y2_[0];
        x2_[0] = x1_[0];
        x1_[0] = x;
        y2_[0] = y1_[0];
        y1_[0] = y;
#endif
        return static_cast<dspfloat_t>(y);
    }

    /**
        Run biquad filter, mono | stereo, no coefficient smoothing
     */
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
    {
        dspfloat_t y[2] = {0, 0};
#ifdef TDF_II
        y[0]  = a0f_[0] * xi[0] + x1_[0];
        x1_[0] = a1f_[0] * xi[0] - b1f_[0] * y[0] + x2_[0];
        x2_[0] = a2f_[0] * xi[0] - b2f_[0] * y[0];
#else
        y[0] = a0f_[0] * xi[0] + a1f_[0] * x1_[0] + a2f_[0] * x2_[0]
                                - b1f_[0] * y1_[0] - b2f_[0] * y2_[0];
        x2_[0] = x1_[0];
        x1_[0] = xi[0];
        y2_[0] = y1_[0];
        y1_[0] = y[0];
#endif
        xo[0] = y[0];

        if (stereo) {
#ifdef TDF_II
            y[1]  = a0f_[0] * xi[1] + x1_[1];
            x1_[1] = a1f_[0] * xi[1] - b1f_[0] * y[1] + x2_[1];
            x2_[1] = a2f_[0] * xi[1] - b2f_[0] * y[1];
#else
            y[1] = a0f_[0] * xi[1] + a1f_[0] * x1_[1] + a2f_[0] * x2_[1]
                                    - b1f_[0] * y1_[1] - b2f_[0] * y2_[1];
            x2_[1] = x1_[1];
            x1_[1] = xi[1];
            y2_[1] = y1_[1];
            y1_[1] = y[1];
#endif
            xo[1] = y[1];
        }
    }
    
    /**
        Run biquad filter, mono only, with coefficient smoothing
    */
    inline dspfloat_t runInterp(dspfloat_t x)
    {
        // run coefficient smoothing filters
        
        a0f_[1] = smooth_tc_ * a0f_[1] + (1.f - smooth_tc_) * a0f_[0];
        a1f_[1] = smooth_tc_ * a1f_[1] + (1.f - smooth_tc_) * a1f_[0];
        a2f_[1] = smooth_tc_ * a2f_[1] + (1.f - smooth_tc_) * a2f_[0];
        b1f_[1] = smooth_tc_ * b1f_[1] + (1.f - smooth_tc_) * b1f_[0];
        b2f_[1] = smooth_tc_ * b2f_[1] + (1.f - smooth_tc_) * b2f_[0];

        // run the filter
#ifdef TDF_II
        dspfloat_t y = a0f_[1] * x + x1_[0];
        x1_[0] = a1f_[1] * x - b1f_[1] * y + x2_[0];
        x2_[0] = a2f_[1] * x - b2f_[1] * y;
#else
        dspfloat_t y = a0f_[1] * x + a1f_[1] * x1_[0] + a2f_[1] * x2_[0]
                                   - b1f_[1] * y1_[0] - b2f_[1] * y2_[0];
        x2_[0] = x1_[0];
        x1_[0] = x;
        y2_[0] = y1_[0];
        y1_[0] = y;
#endif
        return static_cast<dspfloat_t>(y);
    }

    /**
        Run biquad filter, mono | stereo, with coefficient smoothing
     */
    inline void runInterp(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
    {
        // run coefficient smoothing filters
        
        a0f_[1] = smooth_tc_ * a0f_[1] + (1.f - smooth_tc_) * a0f_[0];
        a1f_[1] = smooth_tc_ * a1f_[1] + (1.f - smooth_tc_) * a1f_[0];
        a2f_[1] = smooth_tc_ * a2f_[1] + (1.f - smooth_tc_) * a2f_[0];
        b1f_[1] = smooth_tc_ * b1f_[1] + (1.f - smooth_tc_) * b1f_[0];
        b2f_[1] = smooth_tc_ * b2f_[1] + (1.f - smooth_tc_) * b2f_[0];

        dspfloat_t y[2] = {0, 0};
        
        // run the filter, L channel
#ifdef TDF_II
        y[0] = a0f_[1] * xi[0] + x1_[0];
        x1_[0] = a1f_[1] * xi[0] - b1f_[1] * y[0] + x2_[0];
        x2_[0] = a2f_[1] * xi[0] - b2f_[1] * y[0];
#else
        y[0] = a0f_[1] * xi[0] + a1f_[1] * x1_[0] + a2f_[1] * x2_[0]
                                - b1f_[1] * y1_[0] - b2f_[1] * y2_[0];
        x2_[0] = x1_[0];
        x1_[0] = xi[0];
        y2_[0] = y1_[0];
        y1_[0] = y[0];
#endif
        xo[0] = y[0];

        // run the filter, R channel
        if (stereo) {
#ifdef TDF_II
            y[1] = a0f_[1] * xi[1] + x1_[1];
            x1_[1] = a1f_[1] * xi[1] - b1f_[1] * y[1] + x2_[1];
            x2_[1] = a2f_[1] * xi[1] - b2f_[1] * y[1];
#else
            y[1] = a0f_[1] * xi[1] + a1f_[1] * x1_[1] + a2f_[1] * x2_[1]
                                    - b1f_[1] * y1_[1] - b2f_[1] * y2_[1];
            x2_[1] = x1_[1];
            x1_[1] = xi[1];
            y2_[1] = y1_[1];
            y1_[1] = y[1];
#endif
            xo[1] = y[1];
        }
    }

    //-----------------------------------------------------------------------
    // Set state values to those which characterize the filter
    // operating with a steady state input of x.

    void setSteadyState(dspfloat_t x)
    {
        // The DC response is at z = 1.,
        // which gives an output = (a0 + a1 + a2) / (b0 + b1 + b2)

        x1_[0] = x;
        x2_[0] = x;
#ifndef TDF_II
        dspfloat_t dcGain = (a0_ + a1_ + a2_) / (b0_ + b1_ + b2_);
        y1_[0] = x * dcGain;
        y2_[0] = x * dcGain;
#endif
    }

    //-----------------------------------------------------------------------
    // Calculate filter magnitude H(z) in dB at single frequency freq_hz
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    
    float getMagnitudeResponse(float freq_hz) const
    {
        double w = 2 * kPi * freq_hz / Fs_;

        double cos1 = cos(-1 * w);
        double cos2 = cos(-2 * w);

        double sin1 = sin(-1 * w);
        double sin2 = sin(-2 * w);

        double realZeros = a0_ + a1_ * cos1 + a2_ * cos2;
        double imagZeros = a1_ * sin1 + a2_ * sin2;

        double realPoles = 1 + b1_ * cos1 + b2_ * cos2;
        double imagPoles = b1_ * sin1 + b2_ * sin2;

        double divider = realPoles * realPoles + imagPoles * imagPoles;

        double realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
        double imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;

        double magnitude = sqrt(fmax(realHw * realHw + imagHw * imagHw, 0.));

		return 20.f * log10f((float)magnitude); // gain in dB
    }

    //-----------------------------------------------------------------------
    // Set coefficient smoothing time constant
    
    void setSmoothingMsec(float smooth_ms)
    {
        // set smoothing time constant, default = 20msec
        smooth_tc_ = onePoleCoeff(smooth_ms);
    }
    
private:
    // math constants
    static constexpr float kE = 2.716828183f;
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kSQRT2 = 1.4142135624f;

    // filter coefs (GUI)
    dspfloat_t a0_ = 1;
    dspfloat_t a1_ = 0;
    dspfloat_t a2_ = 0;
    dspfloat_t b0_ = 1;
    dspfloat_t b1_ = 0;
    dspfloat_t b2_ = 0;

    // filter coefs (DSP, Target / Active)
    dspfloat_t a0f_[2] = {1, 1};
    dspfloat_t a1f_[2] = {0, 0};
    dspfloat_t a2f_[2] = {0, 0};
    dspfloat_t b0f_[2] = {1, 1};
    dspfloat_t b1f_[2] = {0, 0};
    dspfloat_t b2f_[2] = {0, 0};

    // filter state
    dspfloat_t x1_[2] = {0, 0};
    dspfloat_t x2_[2] = {0, 0};
#ifndef TDF_II
    dspfloat_t y1_[2] = {0, 0};
    dspfloat_t y2_[2] = {0, 0};
#endif

    // smoothing coefficient
    dspfloat_t smooth_tc_ = 0;

    FilterType filterType_ = kBypass;

    // settings

	float Fs_ = 0.f; // no sample rate set yet
	float Fc_ = 1000.f;
    float Q_ = 0.7071f;
    float gain_ = 0; // dB

    inline dspfloat_t onePoleCoeff(dspfloat_t tau_ms)
    {
        assert (Fs_ != 0);
        
        if(tau_ms > 0.0) {
            return exp(-1.f / (tau_ms * .001f * Fs_));
        }
        return 1.f;
    }
    
    //------------------------------------------------------------------------
    // Fast routine to return approximate tangent value
    // input angle_coef == varies from 0 to 1.0,
    // where (angle_coef * pi / 4) is the angle in radians
    // tan_table[i] is the tan() having angle i * (pi / 4) / kFastTanTableSize
    //------------------------------------------------------------------------
    dspfloat_t fastTangent(dspfloat_t angle_coef)
    {
        const int kFastTanTableSize = 256; // power of 2
        const int kFastTanTableWrap = kFastTanTableSize - 1; // mask

        static bool init = true;
        static dspfloat_t tan_table[kFastTanTableSize];
        int n;

        if (init) {
            init = false;

            dspfloat_t angle_increment = (kPi / 4) / kFastTanTableSize;
            dspfloat_t angle = 0.;

            for (n = 0; n < kFastTanTableSize; n++) {
                tan_table[n] = tan(angle);
                angle += angle_increment;
            }
        }

        // do the math
        float angle_index_f = (float)angle_coef * kFastTanTableSize;
        int angle_index = (int)lround(angle_index_f); // round positive number to nearest integer
        dspfloat_t angle_fract = angle_index - angle_index_f; // get fraction

        dspfloat_t a0, ap1, am1;
        n = (angle_index - 1) & kFastTanTableWrap;
        am1 = tan_table[n];

        n = (n + 1) & kFastTanTableWrap;
        a0 = tan_table[n];

        n = (n + 1) & kFastTanTableWrap;
        ap1 = tan_table[n];

        // use quadratic interpolation
        dspfloat_t result = angle_fract * (angle_fract * (.5 * (am1 + ap1) - a0) + .5 * (ap1 - am1)) + a0;
        return result;
    }
};
