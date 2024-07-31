/*
  ==============================================================================

    MultiMeter.h
    Created: 15 Jun 2022 10:23:44am
    Author:  Andrew

  ==============================================================================
*/

#pragma once
#include <cmath>
#include <atomic>
#include <cassert>

#include "Zerox.h"
#include "../math/LogMath.h"
#include "../math/TimeMath.h"

template <typename dspfloat_t>
class MultiMeter
{
public:
    // Meter types
    enum Type {
        kPeakHold = 0,
        kPeakDecay,
        kSmoothRMS,
        kCrestFact,
        kClipMeter,
        kZeroCross,
        kNumMeters
    };

    // Meter enables
    enum Mode {
        kDoPeakHold = 0x01,
        kDoPeakDecay = 0x02,
        kDoSmoothRMS = 0x04,
        kDoCrestFact = 0x08,
        kDoClipMeter = 0x10,
        kDoZeroxMeter = 0x20
    };

    // Meter units: dB or scalar (raw)
    enum Units {
        kAsScalar = 0,
        kInDecibels,
        kSquareRoot
    };

    /// Constructor
    MultiMeter()
    {   
        float sr = 44100;
        prepare(sr, kPeakDecay, kDecayTimeMsecDefault);
        prepare(sr, kSmoothRMS, kDecayTimeMsecDefault);
        prepare(sr, kCrestFact, kDecayTimeMsecDefault);
    }

    virtual ~MultiMeter() { }

    // Convert from linear (scalar) value to decibels
    float format(dspfloat_t x, dspfloat_t minval = kMinMeterValue)
    {
        // if minval == 0, return raw value (no conversion)
        dspfloat_t v = (minval == 0) ? x : 20.f * log10f(fmax(x, minval));
        return static_cast<float>(v);
    }

    /// Get a single meter value
    float getMeter(int type, float minval = kAsScalar)
    {
        float meterVal = 0;

        switch (type) {
            case kPeakHold:
                meterVal = format(peakHoldMeter, minval);
                resetPeakHold.store(true);
            case kPeakDecay:
                meterVal = format(peakDecayMeter, minval);
                break;
            case kSmoothRMS:
                meterVal = format(smoothRmsMeter, minval);
                break;
            case kCrestFact:
                meterVal = format(crestFactMeter, minval);
                break;
            case kClipMeter:
                meterVal = clippingMeter;
                resetClipping.store(true);
                break;
            case kZeroCross:
                meterVal = zeroCrossMeter;
                break;
            default:
                assert(false);
                break;
        }

        return meterVal;
    }
    
    /// Get all meter values
    void getAllMeters(dspfloat_t ( &meterVals )[kNumMeters], float minval = kAsScalar, bool reset = true)
    {
        /// copy all meter values to array
        meterVals[kPeakHold] = format(peakHoldMeter, minval);
        meterVals[kPeakDecay] = format(peakDecayMeter, minval);
        meterVals[kSmoothRMS] = format(smoothRmsMeter, minval);
        meterVals[kCrestFact] = format(crestFactMeter, minval);
        meterVals[kZeroCross] = zeroCrossMeter;
        meterVals[kClipMeter] = clippingMeter;

        /// tell DSP to clear meters
        resetPeakHold.store(reset);
        resetClipping.store(reset);
    }

    /// Calculate meter decay coefficient from milliseconds
    void prepare(float fs, int type, float decay_ms = kDecayTimeMsecDefault)
    {
        dspfloat_t coeff = TimeMath::onePoleCoeff<dspfloat_t>(decay_ms, fs, TimeMath::kDecayZolger);

        switch (type) {
            case kPeakDecay:
                decayCoeff = coeff;
                break;
            case kSmoothRMS:
                smoothCoeff = coeff;
                break;
            case kCrestFact:
                crestCoeff = coeff;
                break;
            default:
                assert(false);
                break;
        }
    }

    // Run core meter algorithm with |x| and x^2 as input
    void run(dspfloat_t x_abs, dspfloat_t x_sqr, dspfloat_t x_zcr)
    {
        // Clipping meter
        if (resetClipping.exchange(false))
            clippingMeter = false;
            
        clippingMeter = (x_abs > clipThreshold);

        // Peak hold meter
        if (resetPeakHold.exchange(false))
            peakHoldMeter = 0;

		peakHoldMeter = fmax(x_abs, peakHoldMeter);

        // Peak meter w/ exponential decay ballistic
    	peakDecayMeter = fmax(x_abs, peakDecayMeter);
        peakDecayMeter *= decayCoeff;
		
        // RMS meter w/ one pole smoothing filter
        smoothState = smoothCoeff * smoothState + (1.f - smoothCoeff) * x_sqr;
        smoothRmsMeter = sqrt(smoothState, kMinSampleValue);
        
        // Crest factor meter
        crestRmsEnv = crestCoeff * crestRmsEnv + (1.f - crestCoeff) * x_sqr;
        crestPeakEnv = fmax(x_sqr, crestCoeff * crestPeakEnv + (1.f - crestCoeff) * x_sqr);
        crestSquared = crestPeakEnv / crestRmsEnv;
        crestFactMeter = sqrt(crestSquared);
        
        // Zero crossing meter
        zeroCrossMeter = zerox.run(x_zcr);
    }

    /// Run mono-to-mono multi meter
    dspfloat_t run(dspfloat_t x)
    {
        // Get |x| and x^2 for mono sample
        dspfloat_t x_abs = fabs(x);
        dspfloat_t x_sqr = fmax(x * x, kMinSampleValue);

        // Run core meter DSP
        run(x_abs, x_sqr, x);
        return 0;
    }

    /// Run stereo-to-mono multi meter
    dspfloat_t run(dspfloat_t ( &x )[2], bool stereo = true)
    {
        // Get |x| and x^2 for stereo samples
        dspfloat_t x_sum = stereo ? 0.5f * (x[0] + x[1]) : x[0];
        dspfloat_t x_abs = fmax(fabs(x[0]), stereo ? fabs(x[1]) : 0);
    	dspfloat_t x_sqr = fmax(fmax(x[0] * x[0], stereo ? (x[1] * x[1]) : 0), kMinSampleValue);
        
        // Run core meter DSP
        run(x_abs, x_sqr, x_sum);
        return 0;
    }

    /// Set clipping threshold via exponent
    void setClipTheshold(int exponent)
    {
        // exponent of -1 = 0.9, -2 = 0.99, -3 = 0.999, etc..
        assert (exponent < 0);
        clipThreshold = 1.f - powf(10.f, exponent);
    }
    
    void resetClip() { resetClipping.store(true); }
    void resetPeak() { resetPeakHold.store(true); }
    
private:
    static constexpr float kMinMeterValue = 1e-03f; // -60dB
    static constexpr float kMinSampleValue = 1e-06f; // -120dB
    static constexpr float kDecayTimeMsecDefault = 200; // 200 milliseconds
    static constexpr float kDefaultClipThreshold = 1.f - kMinSampleValue; // 0.999999

    // meter state
    bool clippingMeter = false;
    dspfloat_t clipThreshold = kDefaultClipThreshold;

    dspfloat_t peakHoldMeter = 0;
    dspfloat_t peakDecayMeter = 0;
    dspfloat_t smoothRmsMeter = 0;
    dspfloat_t crestFactMeter = 0;

    dspfloat_t decayCoeff = 0;
    dspfloat_t crestCoeff = 0;
    dspfloat_t smoothCoeff = 0;
    dspfloat_t smoothState = 0;

    dspfloat_t crestRmsEnv = 0;
    dspfloat_t crestPeakEnv = 0;
    dspfloat_t crestSquared = 0;

    std::atomic<bool> resetPeakHold = true;
    std::atomic<bool> resetClipping = true;
    
    Zerox<dspfloat_t> zerox;
    dspfloat_t zeroCrossMeter = 0;
 };

