/*
  ==============================================================================

    Waveshaper.h
    Created: 2 Jun 2022 9:24:06am
    Author:  Andrew

  ==============================================================================
*/

#pragma once
#include <cmath>
#ifndef AKDSPLIB_MODULE
#include "BiquadFilter.h"
#include "../math/LogMath.h"
#include "../math/RangeMath.h"
#endif

template <typename dspfloat_t>
class UpComp
{
public:
    UpComp()
    {
        threshold = LogMath::dbToLin(kMinCompThreshDb);
        threshold_inv = 1.f / threshold;

        prepare(44100.f);
    }
    
    ~UpComp() {}
    
    void prepare(float sr)
    {
        SR = sr;
        smooth = pow(.5f, 1.f / (SR * .05f)); // 50 msec smooth
        attack = pow(.5f, 1.f / (SR * kAttackHalfTime));
        release = pow(.5f, 1.f / (SR * kReleaseHalfTime));
    }
    
    void setAttack(float ms)
    {
        attack = pow(.5f, 1.f / (SR * ms * .001f));
    }
    
    void setRelease(float ms)
    {
        release = pow(.5f, 1.f / (SR * ms * .001f));
    }
    
    void setThreshold(float db)
    {
        threshold = LogMath::dbToLin(db);
        threshold_inv = 1.f / threshold;
        enable = (db == 0) ? 0 : 1;

    }
    
    // process mono | stereo samples
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
    {
        if (enable)
        {
            // run envelope follower
            
            dspfloat_t x_abs = fabs(xi[0]);
            if (stereo)
                x_abs = fmax(x_abs, fabs(xi[1]));

            if (x_abs > envelope)
                envelope = attack * envelope + (1.f - attack) * x_abs;
            else
                envelope = release * envelope + (1.f - release) * x_abs;

            // apply upward compression
            
            gain = threshold_inv_f;
            if (envelope > threshold_f)
                gain = 1.f / envelope;

            xo[0] = xi[0] * gain;
            if (stereo)
                xo[1] = xi[1] * gain;
        }
        
        threshold_f = smooth * threshold_f + (1.f - smooth) * threshold;
        threshold_inv_f = smooth * threshold_inv_f + (1.f - smooth) * threshold_inv;
    }
    
private:
    float SR;
    bool enable = true;
    
    // compressor

    dspfloat_t gain = 1.f;
    dspfloat_t smooth;
    dspfloat_t attack;
    dspfloat_t release;
    
    dspfloat_t envelope = 0.f;
    dspfloat_t threshold = 1.f;
    dspfloat_t threshold_f = 1.f;
    dspfloat_t threshold_inv = 1.f;
    dspfloat_t threshold_inv_f = 1.f;

    // default processing values
    static constexpr dspfloat_t kAttackHalfTime = .00001f; // sec
    static constexpr dspfloat_t kReleaseHalfTime = .100f;  // sec
    static constexpr dspfloat_t kMinCompThreshDb = -36;
};


/**
 
 */
template <typename dspfloat_t>
class Waveshaper
{
public:
    enum Model {
        kBypass = 0,
        kArctan,
        kTriode,
        kWarmth,
        kSigmoid,
        kSymclip,
        kExpdist
    };
    
    Waveshaper()
    {
        setSampleRate(SR);
    }
    
    ~Waveshaper() { }
    
    void setSampleRate(float sr)
    {
        SR = sr;
        smooth_coeff = exp(-1.f / (50.f * .001f * SR)); // 50 msec smoothing
        dcblock_coeff = exp(-2.f * kPi * kDcBlockFreqHz / SR);
        wfilter.design(SR, wfFreq, 0.7071f, wfGain, BiquadFilter<dspfloat_t>::kLowpass);
        
        upcomp.prepare(SR);
    }
    
    void setShape(float param)
    {
        shape = param * 0.01f;
        updateCoeffs();
    }
    
    void setDrive(float param)
    {
        drive = param * 0.01f;
        updateCoeffs();
    }

    void setParam(float param1, float param2)
    {
        drive = param1 * 0.01f;
        shape = param2 * 0.01f;
        updateCoeffs();
    }
    
    void setModel(int theModel) { model = theModel; }
    void setLowpass(bool enable) { lowpass = enable; }
    
    // process mono | stereo samples
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], bool stereo = false)
    {
        
        dspfloat_t x[2] = {0, 0};
        dspfloat_t y[2] = {0, 0};
        dspfloat_t u = 0;
        
        x[0] = xi[0];
        if (stereo)
            x[1] = xi[1];
        
        if (weighting)
        {
            // run weighting filter
            u = stereo ? 0.5f * (x[0] + x[1]) : x[0];
            u = wfilter.run(u);
        }
        
        if (compress)
        {
            // run upwards compressor
            upcomp.run(x, x, stereo);
        }
        
        /// Run waveshaper function
        switch (model) {
            default:
            case kBypass:
                /// Bypass
                y[0] = x[0];
                if (stereo)
                    y[1] = x[1];
                break;
                
            case kArctan:
                /// Arctangent soft clipper
                
                y[0] = af * atan(bf * x[0]);
                if (stereo)
                    y[1] = af * atan(bf * x[1]);
                
                // coefficient smoothing
                af = smooth_coeff * af + (1.f - smooth_coeff) * a;
                bf = smooth_coeff * bf + (1.f - smooth_coeff) * b;
                
                break;
                
            case kTriode:
                /// Bendiksen triode (DAFx eq 4.13)
                
                y[0] = (x[0] == qf) ? 1.f / df + rf : (x[0] - qf) / (1.f - exp(-df * (x[0] - qf))) + rf;
                if (stereo)
                    y[1] = (x[1] == qf) ? 1.f / df + rf : (x[1] - qf) / (1.f - exp(-df * (x[1] - qf))) + rf;
                
                // coefficient smoothing
                qf = smooth_coeff * qf + (1.f - smooth_coeff) * q;
                df = smooth_coeff * df + (1.f - smooth_coeff) * d;
                rf = smooth_coeff * rf + (1.f - smooth_coeff) * r;
                
                break;
                
            case kWarmth:
                /// Hildebrand tube model
                
                if (x[0] > 0) {
                    x[0] = x[0] * (wf - (wf - 1.f) * x[0]);
                    x[0] = x[0] * (wf - (wf - 1.f) * x[0]);
                }
                else {
                    x[0] *= (wf * wf);
                    x[0] = fmax(x[0], -1.f);
                }
                y[0] = x[0];
                if (stereo)
                {
                    if (x[1] > 0) {
                        x[1] = x[1] * (wf - (wf - 1.f) * x[1]);
                        x[1] = x[1] * (wf - (wf - 1.f) * x[1]);
                    }
                    else {
                        x[1] *= (wf * wf);
                        x[1] = fmax(x[1], -1.f);
                    }
                    y[1] = x[1];
                }
                
                // coefficient smoothing
                wf = smooth_coeff * wf + (1.f - smooth_coeff) * w;
                gf = smooth_coeff * gf + (1.f - smooth_coeff) * g;

                break;
                
            case kSigmoid:
                /// Logistics sigmoid
            
                y[0] = kf / (1.f + exp(-lf * x[0])) - 0.5f * kf;
                if (stereo)
                    y[1] = kf / (1.f + exp(-lf * x[1])) - 0.5f * kf;
                
                // coefficient smoothing
                kf = smooth_coeff * kf + (1.f - smooth_coeff) * k;
                lf = smooth_coeff * lf + (1.f - smooth_coeff) * l;
                
                break;
                
            case kSymclip:
                /// Symmetric clipping (DAFx eq 4.14)
                
                if (fabs(x[0]) < 1.f/3.f)
                    y[0] = 2.f * x[0]; // linear region
                else
                if (fabs(x[0]) > 2.f/3.f)
                    y[0] = 1.f; // hard limit
                else
                if (fabs(x[0]) >= 1./3. && fabs(x[0]) <= 2./3.)
                    // soft clip region
                    y[0] = (3.f - (2.f - 3.f * x[0]) * (2.f - 3.f * x[0])) / 3.f;
                
                if (stereo)
                {
                    if (fabs(x[1]) < 1.f/3.f)
                        y[1] = 2.f * x[1]; // linear region
                    else
                    if (fabs(x[1]) > 2.f/3.f)
                        y[1] = 1.; // hard limit
                    else
                    if (fabs(x[1]) >= 1./3. && fabs(x[1]) <= 2./3.)
                        // soft clip region
                        y[1] = (3.f - (2.f - 3.f * x[1]) * (2.f - 3.f * x[1])) / 3.f;
                }
                
                break;
                
            case kExpdist:
                /// Exponential distortion (DAFx eq 4.15)
                
                y[0] = RangeMath::sgn<dspfloat_t>(x[0]) * (1.f - exp(-fabs(x[0])));
                if (stereo)
                    y[1] = RangeMath::sgn<dspfloat_t>(x[1]) * (1.f - exp(-fabs(x[1])));

                break;
        }
        
        if (dcblock)
        {
            // apply one pole + one zero DC block filter:
            // y(i) = x(i) - x(i-1) + dcblock_coeff * y(i-1), x(i-1) = x(i)
            
            dcblock_y[0] = y[0] - dcblock_x[0] + dcblock_coeff * dcblock_y[0];
            dcblock_x[0] = y[0];
            y[0] = dcblock_y[0];

            if (stereo) {
                dcblock_y[1] = y[1] - dcblock_x[1] + dcblock_coeff * dcblock_y[1];
                dcblock_x[1] = y[1];
                y[1] = dcblock_y[1];
            }
        }
        
        if (lowpass)
        {
            // one pole lowpass filter
            y[0] = lpf_coeff * y[0] + (1.f - lpf_coeff) * y[0];
            if (stereo)
                y[1] = lpf_coeff * y[1] + (1.f - lpf_coeff) * y[1];
        }
        
        if (model == kWarmth)
        {
            // gain compensation
            y[0] *= gf;
            if (stereo)
                y[1] *= gf;
        }
        
        // limit output
        RangeMath::limit<dspfloat_t>(y, k0dBFS);
        
        // output samples
        xo[0] = y[0];
        if (stereo)
            xo[1] = y[1];
    }
    
private:
    int model = kWarmth;
    bool dcblock = true;
    bool lowpass = false;
    bool compress = false;

    float SR = 44100.f; // sample rate
    
    float drive = 0.f; // normalized 0 to 1
    float shape = 0.f; // normalized 0 to 1
    
    // Arctan soft clip
    dspfloat_t a = 1.f;
    dspfloat_t af = a;
    dspfloat_t b = 1.f;
    dspfloat_t bf = b;
    
    // Bendiksen triode
    dspfloat_t d = 1.f;
    dspfloat_t df = d;
    dspfloat_t q = -1.1f;
    dspfloat_t qf = q;
    dspfloat_t r = q / (1.f - exp(d * q));
    dspfloat_t rf = r;

    // Hildebrand warmth
    dspfloat_t w = 1.f;
    dspfloat_t wf = w;
    dspfloat_t g = 1.f;
    dspfloat_t gf = g;
    
    // Logistics sigmoid
    dspfloat_t l = 1.f;
    dspfloat_t lf = l;
    dspfloat_t k = 3.f;
    dspfloat_t kf = k;

    // DC blocking filter
    dspfloat_t dcblock_x[2] = {0, 0};
    dspfloat_t dcblock_y[2] = {0, 0};
    dspfloat_t dcblock_coeff;
    
    // Weighting filter
    BiquadFilter<dspfloat_t> wfilter;
    bool weighting = true;
    int wfType = BiquadFilter<dspfloat_t>::kLowpass;
    float wfFreq = 1000; // Hz
    float wfGain = 0; // dB
    dspfloat_t wfCoeff = 1.f; // drive
    
    // One pole LPF
    dspfloat_t lpf_coeff =  0.2f; // -1.5dB @ 10KHz
    
    // Coefficient smoothing
    dspfloat_t smooth_coeff;

    // Upwards compressor
    UpComp<dspfloat_t> upcomp;
    
    // Processing constants
    static constexpr dspfloat_t k0dBFS = 0.9999f;
    static constexpr dspfloat_t kDcBlockFreqHz = 12.f; // Hz
    static constexpr dspfloat_t kPi = 3.14159265358979323846f;

    /// Update state for all waveshaper models from normalized drive and shape params
    
    void updateCoeffs(void)
    {
        // arctan soft clip
        a = RangeMath::normToRange<dspfloat_t>(shape, 0.1f, 3.0f);
        b = 1.f / atan(a);
        
        // logistics sigmoid
        k = RangeMath::normToRange<dspfloat_t>(drive, 0.1f, 3.0f);
        l = RangeMath::normToRange<dspfloat_t>(shape, 0.1f, 3.0f);

        // Benediksen triode
        d = RangeMath::normToRange<dspfloat_t>(drive, 0, 10.0);
        q = -RangeMath::normToRange<dspfloat_t>(1.f - shape, 0.1f, 3.0f);
        r = q / (1.f - exp(d * q));
        
        // Hildebrand warmth
        w = LogMath::dbToLin(6.f * drive); // drive gain 0 to 12dB
        g = LogMath::dbToLin(-0.15f * 12.f * drive); // 15% recovery
    }
};

