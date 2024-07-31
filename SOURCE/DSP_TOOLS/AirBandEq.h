/*
  ==============================================================================

    AirBandEQ.h
    Created: 15 Feb 2022 2:25:22pm
    Author:  Andrew

  ==============================================================================
*/

#pragma once
#ifndef AKDSPLIB_MODULE
#include "../math/LogMath.h"
#include "BiquadFilter.h"
#endif
#include <cmath>
#define TDF_II 1 // use Tranpose Direct Form II filter topology

template <typename dspfloat_t>
class AirBandEq
{
public:
    
    enum Param {
        kFreq = 0,
        kGain,
        kSolo,
        kEnable
    };
    
    AirBandEq() { setSampleRate(SR); }
    ~AirBandEq() { }
    
    inline bool isOn(void) const { return enable; }
    inline bool inSolo(void) const { return solo; }
    
    void setParam(int paramId, float value)
    {
        switch (paramId) {
            case kFreq:
                fc_hz = value;
                break;
            case kGain:
                g_db = g_db_ = value;
                break;
            case kSolo:
                solo = (bool)value;
                break;
            case kEnable:
                enable = (bool)value;
                break;
            default:
                // uh oh
                break;
        }
        
        design();
    }

    // set the filter sample rate
    void setSampleRate(double sr)
    {
        SR = float(sr);
        TC = onePoleCoeff(kSmoothMsec); // set coefficient smooth time
        design(); // re-design the filter w/ current params
        
        soloHPF.design(SR, fc_hz, 0.7071f, 0.f, BiquadFilter<double>::kHighpass);
    }

    // process mono | stereo samples
    inline void run(double ( &xi )[2], double ( &xo )[2], bool stereo = false)
    {
        if (!enable && !solo)
        {
            xo[0] = xi[0];
            xo[1] = xi[1];
            return;
        }
        
        if (solo)
        {
            soloHPF.runInterp(xi, xo, stereo);
        }
        else
        {
            // smooth coefficients
            b0f = TC * b0f + (1. - TC) * b0d;
            b1f = TC * b1f + (1. - TC) * b1d;
            a1f = TC * a1f + (1. - TC) * a1d;
            
            // run the filter
            double yout = 0;
            int N = stereo ? 2 : 1;
            for (int n = 0; n < N; n++)
            {
#ifdef TDF_II
                yout = b0f * xi[n] + x[n];
                x[n] = b1f * xi[n] - a1f * yout;
                xo[n] = yout; // output sample
#else
                yout = b0f * xi[n] + b1f * x[n] - a1f * y[n];
                x[n] = xi[n];
                y[n] = yout;
                xo[n] = yout; // output sample
#endif
            }
        }
    }
    
    // process mono sample only
    inline double run(double xi)
    {
        if (!enable && !solo)
            return xi;
        
        double yout = 0;
        
        if (solo)
        {
            yout = soloHPF.run(xi);
        }
        else
        {
            // smooth coefficients
            b0f = TC * b0f + (1. - TC) * b0d;
            b1f = TC * b1f + (1. - TC) * b1d;
            a1f = TC * a1f + (1. - TC) * a1d;

            // run the filter
            yout = 0;
#ifdef TDF_II
            yout = b0f * xi + x[0];
            x[0] = b1f * xi - a1f * yout;
#else
            yout = b0f * xi + b1f * x[0] - a1f * y[0];
            x[0] = xi;
            y[0] = yout;
#endif
        }
        return yout; // output sample
    }
    
    // calculate filter magnitude response over vector of frequency points
    void magnitude(float freq_hz[], float mag_db[], int numPoints)
    {
        for (int i = 0; i < numPoints; i++) {
            mag_db[i] = magnitude(freq_hz[i]);
        }
    }

    // calculate filter magnitude response in dB at a single frequency point
    float magnitude(float freq_hz)
    {
        float h = 0;
        if (solo)
        {
            h = soloHPF.getMagnitudeResponse(freq_hz);
            return h;
        }
        else
        {
            h = this->power(freq_hz);
            h = sqrtf(fmax(h, 0.f));
            return (h <= 1e-05f) ? -100.f : 20.f * log10f(h);
        }
    }
    
    // Define EQ band state
    struct EqState
    {
        bool eqOn;
        bool solo;
        float freq;
        float gain;
    };

    // Get current EQ state
    void getEqState(EqState& state)
    {
        state.eqOn = enable;
        state.solo = solo;
        state.freq = fc_hz;
        state.gain = g_db;
    }

private:
    static constexpr float kSmoothMsec = 50.f;
    static constexpr double kPi = 3.141592653589793238;

    bool solo = false;
    bool enable = true;

    float SR = 44100; // sample rate
    
    float g_db = 0; // gain in dB
    float g_db_ = 0; // cached gain in dB
    float fc_hz = 15000; // corner frequency
    
    // design filter coefficients
    dspfloat_t b0d = 1;
    dspfloat_t b1d = 0;
    dspfloat_t a1d = 0;
    
    // smoothed filter coefficients
    dspfloat_t b0f = 1;
    dspfloat_t b1f = 0;
    dspfloat_t a1f = 0;

    // filter state
    dspfloat_t x[2] = {0., 0.};
#ifndef TDF_II
    dspfloat_t y[2] = {0., 0.};
#endif

    BiquadFilter<double> soloHPF;
    
    // filter design
    // from "Matched Pole Digital Shelving Filters", Marin Vicanek 07 Sep 2019
    void design()
    {
        if (0)
        {
            // bypass coefficients
            b0d = 1.0;
            b1d = 0.0;
            a1d = 0.0;
        }
        else
        {
            double G = pow(10, g_db / 20); // convert from db to linear gain
            double fc = fc_hz / (SR / 2);  // normalize to Nyquist = 1

            double fm = 0.9;
            double phi_m = 1 - cos(kPi * fm);
            
            double scale = 2 / (kPi * kPi);
            double alpha = scale * (1 / (fm * fm) + 1 / (G * fc * fc)) - (1 / phi_m);
            double beta =  scale * (1 / (fm * fm) + G / (fc * fc)) - (1 / phi_m);

            a1d = -alpha / (1 + alpha + sqrt(1 + 2 * alpha));
            double b = -beta / (1 + beta + sqrt(1 + 2 * beta));
            
            b0d = (1 + a1d) / (1 + b);
            b1d = b * b0d;
        }
        
        // redesign solo HPF
        soloHPF.design(SR, fc_hz, 0.7071f, 0, BiquadFilter<double>::kHighpass);
    }
        
    //
    // calculate filter magnitude response
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    float power(float freq_hz) const
    {
        float w = float(2 * kPi * freq_hz / SR);
        float cos1 = cos(-1 * w);
        float sin1 = sin(-1 * w);

        float realZeros = static_cast<float>(b0d + b1d * cos1);
        float imagZeros = static_cast<float>(b1d * sin1);

        float realPoles = static_cast<float>(1.f + a1d * cos1);
        float imagPoles = static_cast<float>(a1d * sin1);

        float divider = realPoles * realPoles + imagPoles * imagPoles;

        float realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
        float imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;

        float power = realHw * realHw + imagHw * imagHw;
        return power;
    }
 
    inline double dBToGain(double db)
    {
        return pow(10., db / 20.);
    }
    
    double TC = 0; // coefficient smoothing
    
    inline double onePoleCoeff(double tau)
    {
        if(tau > 0.0) {
            return exp(-1.0 / (tau * .001 * SR));
        }
        return 1.0;
    }
};
