//
//  MultiStageIIR.h
//
//  Created by Antares on 6/10/18.
//  Copyright © 2018 Antares Audio Technologies. All rights reserved.
//

#pragma once

//#define FILTER_DEBUG 1

#ifdef FILTER_DEBUG
#include <iomanip>
#include <iostream>
#endif

#include <cassert>
#include <math.h>

template <typename dspfloat_t>
class MultiStageIIR {
public:
    static constexpr int kMaxBiquads = 4;

    struct Biquad {
        dspfloat_t a0_ = 1;
        dspfloat_t a1_ = 0;
        dspfloat_t a2_ = 0;
        dspfloat_t b1_ = 0;
        dspfloat_t b2_ = 0;
    };

    MultiStageIIR(float fs = 44100)
    {
        assert(fs != 0);
        Fs_ = fs;

        // set all filters to bypass
        for (int n = 0; n < kMaxBiquads; n++) {
            bqf[0][n].a0_ = bqf[1][n].a0_ = bqf2[n].a2_ = 1;
            bqf[0][n].a1_ = bqf[1][n].a1_ = bqf2[n].a2_ = 0;
            bqf[0][n].a2_ = bqf[1][n].a2_ = bqf2[n].a2_ = 0;
            bqf[0][n].b1_ = bqf[1][n].b1_ = bqf2[n].b1_ = 0;
            bqf[0][n].b2_ = bqf[1][n].b2_ = bqf2[n].b2_ = 0;
        }

        // zero the filter state
        for (int n = 0; n < kMaxBiquads; n++) {
            xm[n].x1_ = xm[n].x2_ = 0;
            xs[n].x1_ = xs[n].x2_ = 0;
        }
    }

    ~MultiStageIIR() { /* nothing to see here */ }

    enum Type {
        kTypeBypass = 0,
        kTypeLowpass = 1,
        kTypeHighpass = 2,
        kTypeBandpass = 3
    };

    enum Slope {
        kSlope12dB = 1, // 1 stages, 2 poles
        kSlope24dB = 2, // 2 stages, 4 poles
        kSlope36dB = 3, // 3 stages, 6 poles
        kSlope48dB = 4 // 4 stages, 8 poles
    };

    // high order bandpass filter design from LPF prototype
    // from https://www.dsprelated.com/showarticle/1128.php
    // N = LPF stages (1, 2, 3, or 4)
    // Q = filter resonance
    // fc = center frequency in Hz
    // fs = sample rate in Hz
    //
    void bandpass(float fs, float fc, float Q)
    {
        dspfloat_t bw = fc / Q;
        dspfloat_t f1 = fc - bw / 2;
        dspfloat_t f2 = fc + bw / 2;

#ifdef FILTER_DEBUG
        std::cout << std::fixed << std::setprecision(12);
#endif
        dspfloat_t theta, p_lp_re[4], p_lp_im[4];

        // determine butterworth analog lowpass filter poles
        for (int k = 0; k < N_; k++) {
            theta = (2 * k + 1) * kPi / (2 * N_);
            p_lp_re[k] = -sin(theta);
            p_lp_im[k] = cos(theta);
        }

        // pre-warp fc, f1, f2
        dspfloat_t F1 = (fs / kPi) * tan(kPi * f1 / fs);
        dspfloat_t F2 = (fs / kPi) * tan(kPi * f2 / fs);

        dspfloat_t BW_hz = F2 - F1; // Hz prewarped -3 dB bandwidth
        dspfloat_t Fc = sqrt(F1 * F2); // Hz geometric mean frequency

        // temporary scalar
        dspfloat_t c = BW_hz / (2 * Fc);

        dspfloat_t zeta_re[4];
        dspfloat_t zeta_im[4];
        dspfloat_t beta_re[4];
        dspfloat_t beta_im[4];

        // do lots of complex math
        for (int k = 0; k < N_; k++) {
            zeta_re[k] = beta_re[k] = c * p_lp_re[k];
            zeta_im[k] = beta_im[k] = c * p_lp_im[k];
            cpow2(beta_re[k], beta_im[k]);
            beta_re[k] = 1 - beta_re[k];
            beta_im[k] = -beta_im[k];
            csqrt(beta_re[k], beta_im[k]);
        }

        // more temporary scalars
        dspfloat_t w0 = 2 * kPi * Fc;
        dspfloat_t K = w0 / (2 * fs);

        dspfloat_t pa_re[8];
        dspfloat_t pa_im[8];

        // calculate analog bandpass filter poles
        for (int k = 0; k < N_; k++) {
            pa_re[2 * k + 0] = K * (zeta_re[k] - beta_im[k]);
            pa_im[2 * k + 0] = K * (zeta_im[k] + beta_re[k]);
            pa_re[2 * k + 1] = K * (zeta_re[k] + beta_im[k]);
            pa_im[2 * k + 1] = K * (zeta_im[k] - beta_re[k]);
        }

        // do bilinear transform
        for (int k = 0; k < 2 * N_; k++) {
            bilinear(pa_re[k], pa_im[k]);
        }

#ifdef FILTER_DEBUG
        for (int k = 0; k < 2 * N_; k++) {
            std::cout << "pa_re = " << pa_re[k] << "\t\tpa_im = " << pa_im[k] << "\n";
        }
        std::cout << "\n";
#endif
        dspfloat_t f0 = sqrt(f1 * f2);
        getFrequencyCoeffs((float)f0);

        dspfloat_t g0 = 1.0;

        // calculate biquad coefficients
        for (int k = 0; k < N_; k++) {
            bqf2[k].a0_ = 1.;
            bqf2[k].a1_ = 0.;
            bqf2[k].a2_ = -1.;
            bqf2[k].b1_ = -2 * pa_re[k];
            bqf2[k].b2_ = pa_re[k] * pa_re[k] + pa_im[k] * pa_im[k];

            // normalize gain
            g0 = 1.f / getBiquadResponse(bqf2[k]);
            bqf2[k].a0_ = g0;
            bqf2[k].a2_ = -g0;
        }
    }

    //
    // design lowpass, highpass, or bandpass filters of 2nd, 4th, 6th, or 8th order
    // (1, 2, 3, or 4 stages)
    //
    void design(float fs, float fc, float Q, int type, int stages)
    {
        assert(fs > 0 && fc > 0 && Q > 0);
        //assert (fc < 0.5 * fs); // will crash auval in DEBUG build at 22.05Khz sample rate
        
        Fs_ = fs;

        assert(stages > 0 && stages <= kMaxBiquads);
        N_ = stages;

        // lookup table of staggered Q values for multistage butterworth HPF/LPF designs
        static const dspfloat_t Q_[4][4] = {
            { 0.70710678f, 0., 0., 0. }, // 12dB / oct
            { 0.54119610f, 1.3065630f, 0., 0. }, // 24dB / oct
            { 0.51763809f, 0.70710678f, 1.9318517f, 0. }, // 36dB / oct
            { 0.50979558f, 0.60134489f, 0.89997622f, 2.5629154f } // 48dB / oct
        };

        dspfloat_t K = tan(kPi * fc / fs);
        dspfloat_t K2 = K * K;
        dspfloat_t norm;

        switch (type) {
        case kTypeBypass:
            for (int n = 0; n < N_; n++) {
                bqf2[n].a0_ = 1;
                bqf2[n].a1_ = 0;
                bqf2[n].a2_ = 0;
                bqf2[n].b1_ = 0;
                bqf2[n].b2_ = 0;
            }
            break;
        case kTypeLowpass:
            for (int n = 0; n < N_; n++) {
                dspfloat_t R = 1.f / Q_[N_ - 1][n];
                norm = 1 / (1 + K * R + K2);
                bqf2[n].a0_ = K2 * norm;
                bqf2[n].a1_ = 2 * bqf2[n].a0_;
                bqf2[n].a2_ = bqf2[n].a0_;
                bqf2[n].b1_ = 2 * (K2 - 1) * norm;
                bqf2[n].b2_ = (1 - K * R + K2) * norm;
            }
            break;
        case kTypeHighpass:
            for (int n = 0; n < N_; n++) {
                dspfloat_t R = 1.f / Q_[N_ - 1][n];
                norm = 1 / (1 + K * R + K2);
                bqf2[n].a0_ = 1 * norm;
                bqf2[n].a1_ = -2 * bqf2[n].a0_;
                bqf2[n].a2_ = bqf2[n].a0_;
                bqf2[n].b1_ = 2 * (K2 - 1) * norm;
                bqf2[n].b2_ = (1 - K * R + K2) * norm;
            }
            break;
        case kTypeBandpass:
            bandpass(fs, fc, Q);
            break;
        }

        // copy filter coeffs to free bank

        // point to free filter
        bool l = 1 - live_;

        for (int n = 0; n < N_; n++) {
            bqf[l][n].a0_ = bqf2[n].a0_;
            bqf[l][n].a1_ = bqf2[n].a1_;
            bqf[l][n].a2_ = bqf2[n].a2_;
            bqf[l][n].b1_ = bqf2[n].b1_;
            bqf[l][n].b2_ = bqf2[n].b2_;
        }

        // make this filter active
        live_ = 1 - live_;

#ifdef FILTER_DEBUG
        for (int n = 0; n < N_; n++) {
            std::cout << "biquad " << n + 1 << ":\n";
            std::cout << "a0 = " << bqf2[n].a0_ << "\na1 = " << bqf2[n].a1_ << "\na2 = " << bqf2[n].a2_ << "\n";
            std::cout << "b1 = " << bqf2[n].b1_ << "\nb2 = " << bqf2[n].b2_ << "\n";
            std::cout << "\n";
        }
#endif
    }

    // run cascaded biquad filters
    inline dspfloat_t run(dspfloat_t x_in)
    {
        dspfloat_t x = x_in;
        dspfloat_t y = 0;

        for (int n = 0; n < N_; n++) {
            // get pointer to current biquad stage
            const Biquad* const b = &bqf[live_][n];

            // transpose direct form II
            y = b->a0_ * x + xm[n].x1_;
            xm[n].x1_ = b->a1_ * x - b->b1_ * y + xm[n].x2_;
            xm[n].x2_ = b->a2_ * x - b->b2_ * y;

            // next stage filter input is previous stage filter output
            x = y;
        }
        return y;
    }

    // stereo
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
    {
        dspfloat_t x = xi[0];
        dspfloat_t y = 0;

        for (int n = 0; n < N_; n++) {
            // get pointer to current biquad stage
            const Biquad* const b = &bqf[live_][n];

            // transpose direct form II
            y = b->a0_ * x + xm[n].x1_;
            xm[n].x1_ = b->a1_ * x - b->b1_ * y + xm[n].x2_;
            xm[n].x2_ = b->a2_ * x - b->b2_ * y;

            // next stage filter input is previous stage filter output
            x = y;
        }
        xo[0] = y;
        
        if (stereo)
        {
            x = xi[1];
            y = 0;
            
            for (int n = 0; n < N_; n++) {
                // get pointer to current biquad stage
                const Biquad* const b = &bqf[live_][n];

                // transpose direct form II
                y = b->a0_ * x + xs[n].x1_;
                xs[n].x1_ = b->a1_ * x - b->b1_ * y + xs[n].x2_;
                xs[n].x2_ = b->a2_ * x - b->b2_ * y;

                // next stage filter input is previous stage filter output
                x = y;
            }
            xo[1] = y;
        }
    }
    
    // get cascaded filter magnitude response
    float getMagnitudeResponse(float freq_hz, int freq_bin)
    {
        if (bin_ != freq_bin) {
            bin_ = freq_bin;
            getFrequencyCoeffs(freq_hz);
        }

        double magnitude = 1.;

        // loop over all biquad sections
        for (int n = 0; n < N_; n++) {
            magnitude *= getBiquadResponse(bqf2[n]);
        }

        // convert to dB
        return (magnitude <= 1e-5) ? -100.f : 20.f * log10f((float)magnitude);
    }

	// get sin / cos values at a single frequency point
    inline void getFrequencyCoeffs(float freq_hz)
    {
        double w = 2.f * (float)kPi * freq_hz / Fs_;

        cos1 = cos(-1. * w);
        cos2 = cos(-2. * w);
        sin1 = sin(-1. * w);
        sin2 = sin(-2. * w);
    }

    // Calculates magnitude reponse |H(z)| for single biquad filter stage at freq_hz
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    dspfloat_t getBiquadResponse(const Biquad b) const
    {
        double realZeros = b.a0_ + b.a1_ * cos1 + b.a2_ * cos2;
        double imagZeros = b.a1_ * sin1 + b.a2_ * sin2;

        double realPoles = 1.f + b.b1_ * cos1 + b.b2_ * cos2;
        double imagPoles = b.b1_ * sin1 + b.b2_ * sin2;

        double divider = realPoles * realPoles + imagPoles * imagPoles;

        double realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
        double imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;

        double power = realHw * realHw + imagHw * imagHw;
        return sqrtf(fmax((float)power, 0.f));
    }

    void loadFilter(double (&filter)[kMaxBiquads][5], Biquad (&coeffs)[kMaxBiquads], int stages)
    {
        for (int m = 0; m < stages; m++) {
            filter[m][0] = coeffs[m].a0_ = bqf2[m].a0_;
            filter[m][1] = coeffs[m].a1_ = bqf2[m].a1_;
            filter[m][2] = coeffs[m].a2_ = bqf2[m].a2_;
            filter[m][3] = coeffs[m].b1_ = bqf2[m].b1_;
            filter[m][4] = coeffs[m].b2_ = bqf2[m].b2_;
        }
    }

    inline void reset(void)
    {
        // zero the filter state
        for (int n = 0; n < N_; n++) {
            xm[n].x1_ = xm[n].x2_ = 0;
            xs[n].x1_ = xs[n].x2_ = 0;
}
    }

private:
    int N_ = 4; // number of stages
    int bin_ = -1; // frequency bin
    int live_ = 0; // index to double-buffered filter data

    float Fs_ = 44100; // sample rate

    static constexpr dspfloat_t kPi = 3.141592653589793238f;

    Biquad bqf[2][kMaxBiquads];
    Biquad bqf2[kMaxBiquads];

    struct State {
        dspfloat_t x1_ = 0;
        dspfloat_t x2_ = 0;
    };
    
    State xm[kMaxBiquads];
    State xs[kMaxBiquads];

    double sin1;
    double sin2;
    double cos1;
    double cos2;

    // complex square
    inline void cpow2(dspfloat_t& a, dspfloat_t& b) const
    {
        dspfloat_t p = a * a - b * b;
        dspfloat_t q = 2 * a * b;
        a = p;
        b = q;
    }

    // complex square root
    inline void csqrt(dspfloat_t& a, dspfloat_t& b) const
    {
        dspfloat_t c = 1.f / sqrt(2.f); // make const
        dspfloat_t z = sqrt(a * a + b * b);

        dspfloat_t p = c * sqrt(a + z);
        dspfloat_t q = c * sqrt(z - a);

        a = p;
        b = (b > 0) ? q : -q;
    }

    // bilinear transform
    // (1 + z) / (1 - z)
    inline void bilinear(dspfloat_t& x, dspfloat_t& y) const
    {
        dspfloat_t a = 1 + x;
        dspfloat_t b = y;
        dspfloat_t c = 1 - x;
        dspfloat_t d = -y;

        dspfloat_t r = c * c + d * d;

        dspfloat_t re = a * c + b * d;
        dspfloat_t im = b * c - a * d;

        x = re / r;
        y = im / r;
    }
};
