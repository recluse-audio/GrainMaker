/*
  ==============================================================================

    NoiseGate.h
    Created: 11 Nov 2021 11:34:50am
    Author:  Andrew
    Description: NoiseGate widget, from DAFX 2nd Edition Udo Zolzer pg 113-114

  ==============================================================================
*/

#pragma once

#include "../math/LogMath.h"
#include <atomic>
#include <cassert>
#include <cmath>

template <typename dspfloat_t>
class NoiseGate
{
public:
    
    enum Param {
        kBypass = 0,
        kHoldMsec,
        kAttackMsec,
        kReleaseMsec,
        kReductionDb,
        kThresholdDb,
        kHysteresisDb,
        kSmoothCoeffHz,
   };
    
    NoiseGate()
    {
        updateThresholds();
        setSampleRate(44100.);
    }
    
    ~NoiseGate() { /* nothing to see here */ }
    
    // reset gate
    inline void reset(void)
    {
        g = g_ = 1.; // gate open
        lthcnt = uthcnt = 0; // reset counters
    }
    
    // mono processing
    inline dspfloat_t run(dspfloat_t xi)
    {
        dspfloat_t x = fabs(xi);
        dspfloat_t ng = gate(x);
        return xi * ng;
    }
    
    // stereo processing (in-place)
    inline void run(dspfloat_t ( &xi )[2])
    {
        dspfloat_t x = fmax(fabs(xi[0]), fabs(xi[1]));
        dspfloat_t ng = gate(x);
        xi[0] *= ng;
        xi[1] *= ng;
    }

    // stereo processing (in-place) with aux side chain input
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t u)
    {
        dspfloat_t ng = gate(fabs(u));
        xi[0] *= ng;
        xi[1] *= ng;
    }

    // stereo processing, separate input / output samples
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2])
    {
        dspfloat_t x = fmax(fabs(xi[0]), fabs(xi[1]));
        dspfloat_t ng = gate(x);
        xo[0] = xi[0] * ng;
        xo[1] = xi[1] * ng;
    }
    
    // stereo processing, separate input / output samples, with aux side chain input
    inline void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], dspfloat_t u)
    {
        dspfloat_t ng = gate(fabs(u));
        xo[0] = xi[0] * ng;
        xo[1] = xi[1] * ng;
    }

    // set control parameters
    void setParam(int paramId, float value)
    {
        switch(paramId) {
            case kBypass:
                bypass.store((bool)value);
                break;
            case kHoldMsec:
                hldMsec = value;
                ht.store(hldMsec * .001f * fs_);
                break;
            case kAttackMsec:
                attMsec = value;
                att.store(attMsec * .001f * fs_);
                break;
            case kReleaseMsec:
                relMsec = value;
                rel.store(relMsec * .001f * fs_);
                break;
            case kReductionDb:
                gr = LogMath::dbToLin(reduceDb);
                break;
            case kThresholdDb:
                threshDb = value;
                updateThresholds();
                break;
            case kHysteresisDb:
                hysterDb = value;
                updateThresholds();
                break;
            case kSmoothCoeffHz:
                smoothHz = value;
                updateSmoothCoeff();
                break;
            default:
                // uh oh
                break;
        }
    }
    
    float getThreshDb(void) const { return threshDb; }
    
    void setSampleRate(float fs)
    {
        fs_ = fs;
        
        // re-init smoothing filter
        updateSmoothCoeff();
        
        // re-init time-dependent params
        setParam(kHoldMsec, hldMsec);
        setParam(kAttackMsec, attMsec);
        setParam(kReleaseMsec, relMsec);
        
        reset();
    }
    
    bool isActive(void) const { return active; }
        
private:
    float fs_ = 0; // sample rate
    
    float tc = 0.97f; // smoothing filter initial pole location
    float smoothHz = 200.f; // smoothing rate in Hz, ts = 5ms
    
    float hldMsec = 5.f;  // hold time in milliseconds
    float attMsec = 1.f;  // attack time in milliseconds
    float relMsec = 10.f; // release time in milliseconds
    
    float threshDb = -70; // threshold in dB, default = -70dB
    float hysterDb = -3; // hysteresis in dB, default = -3dB
    float reduceDb = -100; // reduction in dB, default = -100dB

    std::atomic<float> ht;  // hold time in samples
    std::atomic<float> att; // attack time in samples
    std::atomic<float> rel; // release time in samples
        
    std::atomic<long> lthcnt = 0; // time in samples below lower threshold
    std::atomic<long> uthcnt = 0; // time in samples above upper threshold
    std::atomic<long> i = 1; // total samples
    
    std::atomic<dspfloat_t> ltrhold; // threshold value for activating gate
    std::atomic<dspfloat_t> utrhold; // threshold value for de-activating gate, > ltrhold
    
    dspfloat_t g = 1.; // applied gain
    dspfloat_t g_ = 1.; // previous gain
    dspfloat_t gr = LogMath::dbToLin(reduceDb); // gain reduction (TODO)
    
    // smoothing filter state
    
    std::atomic<dspfloat_t> a0 = 1.;
    std::atomic<dspfloat_t> b1 = 0.;
    std::atomic<dspfloat_t> b2 = 0.;
    std::atomic<dspfloat_t> h1 = 0.;
    std::atomic<dspfloat_t> h2 = 0.;
    
    // control / status flags
    
    std::atomic<bool> bypass = false;
    std::atomic<bool> active = false;
    
    // gain computer
    
    inline dspfloat_t gate(dspfloat_t xi)
    {
        // envelop detection - run smoothing filter
        
        dspfloat_t h = a0 * xi - b1 * h1 - b2 * h2;
        
        h2.store(h1); h1.store(h); // update filter state
        
        // run gain calculation
        
        if ((h <= ltrhold) || ((h < utrhold) && (lthcnt > 0)))
        {
            // value below the lower threshold?
            
            lthcnt++;
            uthcnt = 0;
            
            if (lthcnt > ht)
            {
                // time below the lower threshhold longer than the hold time?
                if (lthcnt > rel + ht)
                    g = 0.f; // gate is fully closed
                else
                    g = 1.f - (lthcnt - ht) / rel; // fade gain to 0
            }
            else
            if (i < ht && lthcnt == i)
            {
                g = 0.f; // gate is closed
            }
            else
            {
                g = 1.f; // gate is open
            }
        }
        else
        if ((h >= utrhold) || ((h > ltrhold) && (uthcnt > 0)))
        {
            // value above the upper threshold or is the signal being faded in?
            
            uthcnt++;
            lthcnt = 0;
            
            // has the gate been activated or isn't the signal faded in yet?
            if (g_ < 1.f)
                g = fmax(uthcnt / att, g_); // fade gain to 1
            else
                g = 1.f; // gate is open
        }
        else
        {
            // should not get here, but just in case...
            
            lthcnt = 0;
            uthcnt = 0;
            
            g = g_; // use previous gain
        }
        
        g_ = g; // save new gain as previous
        active = (g < 1.f);
        i++;
        
        return bypass ? 1.f : g;
    }
    
    void updateThresholds(void)
    {
        utrhold.store(LogMath::dbToLin(threshDb));
        ltrhold.store(LogMath::dbToLin(threshDb + hysterDb));
        assert(ltrhold <= utrhold);
    }
    
    static constexpr float kPi = 3.14159265426f;
    void updateSmoothCoeff(void)
    {
        tc = exp(-2.f * kPi * smoothHz / fs_);
        a0 = (1 - tc) * (1 - tc);
        b1 = -2 * tc;
        b2 = tc * tc;
        h1 = 0;
        h2 = 0;
    }
};
