/*
 * SinCosOsc.h
 *
 *  Created on: Feb 8 2019
 *      Author: Antares
 *
 *  Gordon-Smith sin/cos quadrature oscillator
 *  From: "Designing Audio Effect Plug-Ins in C++" by Will Pirkle
 */

#pragma once
#include <cmath>


template <typename dspfloat_t>
class SinCosOsc
{
public:
    
    enum {
        kNoSmoothing = 0,
        kWithSmoothing = 1
    };

    enum {
        kActive = 0,
        kTarget
    };
    
    SinCosOsc() { setSampleRate(44100.); }
    ~SinCosOsc() { }

    void reset(void)
    {
        double theta = -2.0F * kPi * m_freqHz / m_sampleRate;
        m_yn  = std::sin(theta);
        m_yqn = std::cos(theta);
    }
 
    void setSampleRate(float sampleRate)
    {
        m_sampleRate = sampleRate;
        m_envStep = 1.f / (m_envTime * m_sampleRate);
        
        m_cEpsilon[kTarget] = 2.0F * std::sin( kPi * m_freqHz / m_sampleRate  );
        
        m_cSmooth = exp(-1.f / (0.05f * m_sampleRate)); // 50 milliseconds smooth time

        reset();
    }
    
    void setEnvelopeMsec(float env_msec)
    {
        m_envTime = env_msec * .001f;
        m_envStep = 1.f / (m_envTime * m_sampleRate);
    }
    
    inline void setFrequency(float newFreqHz, int smoothing = kNoSmoothing)
    {
        if (newFreqHz != 0)  {
            m_freqHz = newFreqHz;
            m_cEpsilon[kTarget] = 2.0F * std::sin( kPi * m_freqHz / m_sampleRate  );
            if (!smoothing) {
                m_cEpsilon[kActive] = m_cEpsilon[kTarget];
            }
            m_envTrig = 1.f; // start attack
        } else {
            m_envTrig = 0.f; // start release
        }
    }
   
    inline void getOutput(dspfloat_t& sinval, dspfloat_t& cosval)
    {
        // run smoothing filter
        
        m_cEpsilon[kActive] = m_cSmooth * m_cEpsilon[kActive] + (1.f - m_cSmooth) * m_cEpsilon[kTarget];
        
        // calculate next samples
        
        double f_yqn = m_yqn - m_cEpsilon[kActive] * m_yn;
        double f_yn  = m_cEpsilon[kActive] * f_yqn + m_yn;
        m_yqn = f_yqn;
        m_yn = f_yn;
        
        // calculate envelope gain
        
        if (m_envTrig == 1.f) {
            // attack - do linear fade to 1
            m_envGain += m_envStep;
            m_envGain = std::fmin(m_envGain, 1.f);
        }
        else
        if (m_envTrig == 0.f) {
            // release - do linear fade to 0
            m_envGain -= m_envStep;
            m_envGain = std::fmax(m_envGain, 0.f);
        }
        
        // apply envelope gain and output samples
        
        sinval = static_cast<dspfloat_t>(f_yn * m_envGain * m_level);
        cosval = static_cast<dspfloat_t>(f_yqn * m_envGain * m_level);
    }
    
private:
    static constexpr double kPi = 3.141592653589793238;
    
    float m_sampleRate = 0;
    float m_freqHz = 1000;
    float m_level = 0.9999f;

    float m_envTime = 0.005f; // sec
    float m_envGain = 0;
    float m_envStep = 0;
    float m_envTrig = 0;

    double m_yqn = 0;
    double m_yn = 0;
    double m_cEpsilon[2] = {0, 0};
    double m_cSmooth = 0;
};


