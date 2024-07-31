/*
  ==============================================================================

    FracDelay.h
    Created: 17 Oct 2018 9:22:28am
    Author:  Antares

  ==============================================================================
*/

#pragma once
#include <atomic>
#include <cassert>
#include <vector>

template <typename dspfloat_t>
class FracDelay
{
public:
    enum {
        kSetDelayMs,
        kSetFeedback,
        kSetWetLevel
    };
    
    FracDelay (float sampleRate, float maxDelayMs)
    {
        fSampleRate = sampleRate;
        fMaxDelayMs = maxDelayMs;
        
        nBufferSize = static_cast<int>(fMaxDelayMs * .001f * fSampleRate);
        
        buffer.resize((size_t)nBufferSize);
        
        reset();
    }
    
    ~FracDelay() { buffer.clear(); }
    
    void reset(void)
    {
        // zero delay line
        for (int i=0; i < nBufferSize; i++) {
            buffer[(size_t)i] = 0.;
        }
        
        nRd = 0; nWr = 0;
        fDelayOut = 0.f;
    }
    
    void setDelaySamples(float samples)
    {
        fDelaySamples = std::fmin(samples, nBufferSize - 1);
        
        // update delay line pointers
        nRd = nWr - static_cast<int>(fDelaySamples);
        if (nRd < 0) nRd += nBufferSize;
    }
    
    void setDelayMs(float delay_ms)
    {
        fDelaySamples = std::fmin(delay_ms * fSampleRate * 0.001f, nBufferSize - 1);
        
        // update delay line pointers
        nRd = nWr - static_cast<int>(fDelaySamples);
        if (nRd < 0) nRd += nBufferSize;
    }
    
    void setFeedback(float feedback_pct)
    {
        fFeedback = feedback_pct * 0.01f;
    }
    
    void setWetLevel(float wet_level)
    {
        fWetLevel = wet_level * 0.01f;
    }
    
    void setControl(int paramId, float param)
    {
        switch (paramId) {
            case kSetDelayMs:
                setDelayMs(param);
                break;
            case kSetFeedback:
                setFeedback(param);
                break;
            case kSetWetLevel:
                setWetLevel(param);
                break;
            default:
                assert(false);
                break;
        }
    }
    
    inline dspfloat_t run(dspfloat_t x_in)
    {
        dspfloat_t xn = x_in;
        dspfloat_t yn = buffer[(size_t)nRd];
        
        if (nRd == nWr && fDelaySamples < 1.0)
            yn = xn;
        
        int nRd_m1 = nRd - 1; // get read index one behind
        if (nRd_m1 < 0) nRd_m1 = nBufferSize - 1;
        
        dspfloat_t yn_m1 = buffer[(size_t)nRd_m1]; // get read sample one behind
        
        dspfloat_t fFractDelay = fDelaySamples - static_cast<int>(fDelaySamples);
        
        // do linear interpolation of output sample:
#if 0
        dspfloat_t fInterp = dLinTerp<dspfloat_t>(0, 1, yn, yn_m1, fFractDelay);
#else
        dspfloat_t fInterp = fFractDelay * yn_m1 + (1.f - fFractDelay) * yn;
#endif        
        yn = (fDelaySamples == 0) ? xn : fInterp;
        
        fDelayOut = yn;
        
        buffer[(size_t)nWr] = xn + fFeedback * yn;
        
        dspfloat_t x_out = fWetLevel * yn + (1.f - fWetLevel) * xn;
        
        if (++nWr >= nBufferSize) nWr = 0;
        if (++nRd >= nBufferSize) nRd = 0;
        
        return x_out;
    }
    
    inline dspfloat_t getDelayOut(void) { return fDelayOut; }
    
private:
    float fSampleRate;
    float fMaxDelayMs;
    
    // buffer parameters
    std::vector<dspfloat_t> buffer;
    int nBufferSize;
    
    int nWr = 0; // write index
    int nRd = 0; // read index
    
    // internal parameters
    dspfloat_t fDelaySamples = 0.0f;
    dspfloat_t fDelayOut = 0.0f;

    // user parameters
    float fFeedback = 0.0f;
    float fWetLevel = 1.0f;
    
    template< typename T>
    inline T dLinTerp(T x1, T x2, T y1, T y2, float x) const
    {
        if ((x2 - x1) == 0)
            return y1; // should not ever happen
        
        // calculate decimal position of x
        T dx = (x - x1) / (x2 - x1);
        
        // use weighted sum method of interpolating
        T result = dx * y2 + (1.f - dx) * y1;
        
        return result;
    }
};


