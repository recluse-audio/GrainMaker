/*
  ==============================================================================

    FETComp.h
    Created: 15 Jun 2022 10:23:44am
    Author:  Andrew
    Status: Work in progress - Not fully tested!!

  ==============================================================================
*/

#pragma once
#include <atomic>
#include <cmath>
#include <cassert>

//#define FETCOMP_DEBUG 1
#ifdef FETCOMP_DEBUG
#include <iomanip>
#include <iostream>
#endif

#ifndef AKDSPLIB_MODULE
#include "../math/LogMath.h"
#include "../math/TimeMath.h"
#include "../math/RangeMath.h"
#include "../math/VoltsMath.h"

#include "Waveshaper.h"
#endif

template <typename dspfloat_t>
class FETComp
{
public:
    
    // Config parameters
    enum ParamID {
        kEnable = 0,
        kRatio,
        kRatiof,
        kAttack,
        kRelease,
        kMeterMode,
        kInputLevel,
        kOutputLevel,
        kShapeInput,
        kShapeOutput,
        kShapeParam1,
        kShapeParam2,
        kFetQBiasAdj,
        kSidechainExt,
        kSidechainHPF,
        kSidechainLPF,
        kSidechainSurf
    };
    
    enum RatioButton {
        kRatio4to1 = 0,
        kRatio8to1,
        kRatio12to1,
        kRatio20to1,
        kNumRatios
    };
    
    enum MeterButton {
        kMeterGR = 0,
        kMeterPlus4,
        kMeterPlus8,
        kMeterOff
    };
    
    /// Constructor
    FETComp()
    {
        prepare(SR);
        
        // set defaults
        
        setBiasTrim(33.75f); // sets Q bias = -1.57V to match JFET Vgs(off)
        setAttackLevel(4); // bypass
        setReleaseLevel(6);
        setRatioButton(kRatio4to1);
        
        float drive = 73; // ~2.2
        float shape = 66; // ~2.0
    
        inputShaper.setModel(Waveshaper<dspfloat_t>::kSigmoid);
        inputShaper.setParam(drive, shape); // default drive, shape
        
        outputShaper.setModel(Waveshaper<dspfloat_t>::kSigmoid);
        outputShaper.setParam(drive, shape); // default drive, shape
    }
    
    /// Destructor
    ~FETComp() { }
    
    enum JFETState {
        kJFETStateUnknown = -1,
        kJFETStateCutoff = 0,
        kJFETStateLinear = 1,
        kJFETStateSaturation = 2
    };
    
    enum Feedback {
        // level meters
        kInputMeter = 0,
        kOutputMeter,
        kGainReduction,
        
        //
        kAttackTimeMs,
        kReleaseTimeMs,
        
        //
        kThreshBias,
        kRatioScale,
        
        //
        kJFETVqbias,
        kJFETState,
        kJFETInSat,
    };
    
    /// Get DSP feedback, called from GUI timerCallback()
    void getFeedback(float ( &fb )[10])
    {
        // level meters
        fb[kInputMeter] = static_cast<float>(inputMeter);
        fb[kOutputMeter] = static_cast<float>(outputMeter);
        fb[kGainReduction] = static_cast<float>(gainReduction);

        // calculated attack and release times
        fb[kAttackTimeMs] = static_cast<float>(Tatt);
        fb[kReleaseTimeMs] = static_cast<float>(Trel);
        
        // threshold + release scalars
        fb[kThreshBias] = static_cast<float>(threshBias);
        fb[kRatioScale] = static_cast<float>(ratioScale);
        
        // JFET state
        fb[kJFETVqbias] = static_cast<float>(jfetVqb);
        fb[kJFETState] = jfetState;
        fb[kJFETInSat] = jfetInSaturation;
        jfetInSaturation.exchange(false);
        
        resetMeters.store(true);
    }
    
    void getMeters(float ( &fb )[3])
    {
        fb[kInputMeter] = static_cast<float>(inputMeter);
        fb[kOutputMeter] = static_cast<float>(outputMeter);
        fb[kGainReduction] = static_cast<float>(gainReduction);
        
        resetMeters.store(true);
    }
    
    void clearMeters()
    {
        inputMeter = 0;
        outputMeter = 0;
        resetMeters.store(true);
    }
    
    /// Set FET Q bias trim, range 0 to 100%
    void setBiasTrim(float trim)
    {
        biasTrimPos = trim;
        updateFetBias();
    }
    
    /// Input level potentiometer, range -60 to 0 dB
    void setInputLevel(float value, bool smoothed = true)
    {
        if (smoothed)
            inputGain[kTarget] = LogMath::dbToLin(value, -60);
        else
            inputGain[kActive] = LogMath::dbToLin(value, -60);
    }
    
    /// Output level potentiometer, range -60 to 0dB
    void setOutputLevel(float value, bool smoothed = true)
    {
        if (smoothed)
            outputGain[kTarget] = LogMath::dbToLin(value, -60);
        else
            outputGain[kActive] = LogMath::dbToLin(value, -60);
    }
    
    /// Attack potentiometer, range 0 to 7, bypass= (value < 1)
    void setAttackLevel(float value)
    {
        if (value < 1)
            bypassSwitch = true;
        else
        {
            bypassSwitch = false;
            attackKnobVal = RangeMath::rangeToNorm<float>(value, kAttackValMin, kAttackValMax);
            updateBallistics();
        }
    }
    
    /// Release potentiometer, range 1 to 7
    void setReleaseLevel(float value)
    {
        releaseKnobVal = RangeMath::rangeToNorm<float>(value, kReleaseValMin, kReleaseValMax);
        updateBallistics();
    }
    
    /// Ratio buttons
    void setRatioButton(int button)
    {
        ratioButton = button;
        updateRatio();
        updateThresh();
    }
    
    /// Ratio slider (experimental!)
    void setRatioSlider(float value)
    {
        ratioValue = RangeMath::rangeToNorm<float>(value, 1, 20);
        ratioScale = RangeMath::normToRange<dspfloat_t>(ratioValue, 0.f, 0.49f);
        threshBias = RangeMath::normToRange<dspfloat_t>(ratioValue, -0.85f, -6.97f);
    }
    
    /// Meter mode buttons
    void setMeterButton(int button)
    {
        meterButton = button;
    }
    
    /// Waveshaper buttons
    void setShapeInput(bool shape) { shapeInput = shape; }
    void setShapeOutput(bool shape) { shapeOutput = shape; }

    /// setControl method - called from ProcessBlock::parameterChanged
    
    void setControl(const int paramId, const float value, const bool smoothed = true)
    {
        switch (paramId) {
            case kEnable:
                // unimplemented
                break;
            case kRatio:
                setRatioButton((int)value);
                break;
            case kRatiof:
                setRatioSlider(value);
                break;
            case kAttack:
                setAttackLevel(value);
                break;
            case kRelease:
                setReleaseLevel(value);
                break;
            case kMeterMode:
                setMeterButton((int)value);
                break;
            case kInputLevel:
                setInputLevel(value, smoothed);
                break;
            case kOutputLevel:
                setOutputLevel(value, smoothed);
                break;
            case kShapeInput:
                setShapeInput((bool)value);
                break;
            case kShapeOutput:
                setShapeOutput((bool)value);
                break;
            case kShapeParam1:
                inputShaper.setShape(value);
                break;
            case kShapeParam2:
                inputShaper.setDrive(value);
                break;
            case kFetQBiasAdj:
                setBiasTrim(value);
                break;
            case kSidechainExt:
                extSidechain = static_cast<bool>(value);
                break;
                
            case kSidechainHPF:
            case kSidechainLPF:
            case kSidechainSurf:
                /// TODO
                break;
                
            default:
                assert(false);
                break;
        }
    }
    
    /// prepare method - called from PluginProcessor::prepareToPlay
    
    void prepare(float sr)
    {
        SR = sr;
        
        updateBallistics();
        
        dezipTc = TimeMath::onePoleCoeff<dspfloat_t>(50.f, SR, TimeMath::kDecayAnalog);
        meterDecay = TimeMath::onePoleCoeff<dspfloat_t>(200.f, SR, TimeMath::kDecayAnalog);

        inputShaper.setSampleRate(sr);
        outputShaper.setSampleRate(sr);
        
        reset();
    }

    /// process method - called from PluginProcessor::processBlock
    
    float run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], dspfloat_t ( &sc )[2], bool stereo = false)
    {
        /*--- Main signal path processing ---*/
        
        dspfloat_t x[2] = {0, 0};
        dspfloat_t y[2] = {0, 0};
        
        // do input peak level metering w/ decay
        
        if (resetMeters.exchange(false)) {
            inputMeter.store(0.f);
        }
        
        dspfloat_t inputLevel = fabs(xi[0]);
        if (stereo) {
            inputLevel = fmax(inputLevel, fabs(xi[1]));
        }
        
        inputMeter.store(fmax(inputMeter, inputLevel)); // no decay

        // apply input attenuator
        
        x[0] = xi[0] * inputGain[kActive];
        x[1] = stereo ? xi[1] * inputGain[kActive] : 0;

        // apply input transformer saturation
        
        if (shapeInput)
            inputShaper.run(x, x, stereo);
        
        // begin voltage domain processing
        
        dspfloat_t vpre[2] = {0, 0}; // signal preamp stage
        dspfloat_t vout[2] = {0, 0}; // signal line amp stage
        
        int numChannels = stereo ? 2 : 1;
        
        for (int n = 0; n < numChannels; n++)
        {
            // convert to volts
            
            vpre[n] = VoltsMath::sampToVolts<dspfloat_t>(x[n], VoltsMath::kVoltsRMS);
            
            // apply gain reduction (FET model)
            
            vpre[n] = modelJFET(jfetVgs, vpre[n], n, kJFETModelVCR);

            // apply preamp gain
            
            vpre[n] *= kPreAmpGain;

            // apply output attenuator
            
            vout[n] = vpre[n] * outputGain[kActive];

            // apply line amp gain
            
            vout[n] *= kLineAmpGain;
            
            // convert back to dBFS
            
            y[n] = VoltsMath::voltsToSamp<dspfloat_t>(vout[n], VoltsMath::kVoltsRMS);
        }
        
        // apply output transformer saturation
        
        if (shapeOutput)
            outputShaper.run(y, y, stereo);
        else
            RangeMath::limit<dspfloat_t>(y, 0.999999f);

        // do output peak level metering w/ decay
        
        dspfloat_t outputLevel = fabs(y[0]);
        if (stereo) {
            outputLevel = fmax(outputLevel, fabs(y[1]));
        }
        
        outputMeter.store(fmax(outputMeter, outputLevel) * meterDecay); // apply decay

        // output samples
        
        xo[0] = y[0];
        if (stereo)
            xo[1] = y[1];
        
        /*--- Control path processing ---*/
        
        // get input sample for rectification
        
        dspfloat_t Vcp_in = stereo ? 0.5f * (vpre[0] + vpre[1]) : vpre[0];

        // apply ratio and bypass
        
        Vcp_in = bypassSwitch ? 0 : Vcp_in * ratioScale;

        dspfloat_t vcp[2] = {0, 0}; // control amp voltages
        
        // apply control amp phase split and gain
        
        vcp[0] = Vcp_in * kSideAmpGain;
        vcp[1] = Vcp_in * kSideAmpGain * (-1.f); // invert phase
        
        // sum threshold bias voltage
        
        vcp[0] += threshBias;
        vcp[1] += threshBias;

        // run ballistics model
        
        jfetVgs = modelBallistics(vcp[0], vcp[1]); // jfetVgs sets the FET gain reduction
        
        // coefficient smoothing
        
        inputGain[kActive] = dezipTc * inputGain[kActive] + (1 - dezipTc) * inputGain[kTarget];
        outputGain[kActive] = dezipTc * outputGain[kActive] + (1 - dezipTc) * outputGain[kTarget];
        
        // gain reduction
        
        gainReduction.store(fmin(jfetGR[0], jfetGR[1]));
        return static_cast<float>(gainReduction);
    }
    
    // reset JFET internal state
    void reset(void)
    {
        jfetVgs = jfetEnv = jfetVqb;
        jfetId[0] = jfetId[1] = 0;
        jfetGR[0] = jfetGR[1] = 1;
    }
    
    bool extSideChainOn() const { return extSidechain; }
    
private:
    
    /// Model ballistics circuit
    inline dspfloat_t modelBallistics(dspfloat_t Vin1, dspfloat_t Vin2)
    {
        dspfloat_t u1 = fmax(Vin1 - jfetEnv, 0); // rectifier diode 1
        dspfloat_t u2 = fmax(Vin2 - jfetEnv, 0); // rectifier diode 2

        jfetEnv = relTc * jfetEnv + (1.f - relTc) * jfetVqb + (1.f - attTc) * (u1 + u2);

        return bypassSwitch ? jfetVqb : jfetEnv;
    }
    
    enum JFETModel {
        kJFETModelFull = 0,
        kJFETModelVCR
    };
    
    /// Model JFET as gain reduction element
    
    inline dspfloat_t modelJFET(dspfloat_t Vgs, dspfloat_t Vin, int channel, bool model = kJFETModelFull)
    {
        // Vgs = JFET gate-source voltage
        // Vin = JFET pre-divider voltage
        // returns: JFET drain-source voltage
        
        // NOTE when operating JFET in the ohmic region:
        // increasing Vgs = decreased resistance = more gain reduction
        // decreasing Vgs = increased resistance = less gain reduction
        // decreasing Vgs to <= Vgs_off = "open" circut (no gain reduction)
        
        // J110 JFET params (measured)
        
        const float Rin = 27e3; // input resistor
        const float Rsg = 0; //10e3; // source-ground resistor
        const float Ioff = 10e-09f; // cutoff current = 10nA
        const float Idss = .0007f; // drain-source saturation current
        const float Vgs_off = -1.57f; // threshold (pinch-off) voltage
        const float beta = Idss / (Vgs_off * Vgs_off); // transconductance
        const float lambda = 170e-3f; // channel-length modulation
        //const float Rds_on = -Vgs_off / (2.f * Idss); // on resistance
        const float Rds_off = 100e6; // off resistance

        dspfloat_t Vdsp = fmin(Vgs, 0.f) - Vgs_off; // Vdsp = drain voltage to pinchoff
        dspfloat_t Vds = jfetVd[channel]; // Vds = drain source voltage

        // determine JFET operating region
        
        jfetState = kJFETStateUnknown;
        
        if (Vdsp <= 0)
            jfetState = kJFETStateCutoff;
        else
        if (Vdsp > 0 && fabs(Vds) < Vdsp)
            jfetState = kJFETStateLinear;
        else
        if (Vdsp > 0 && fabs(Vds) >= Vdsp)
            jfetState = kJFETStateSaturation;

        assert(jfetState != kJFETStateUnknown);
        
        // update JFET saturation flag
        if (jfetState == kJFETStateSaturation)
            jfetInSaturation.store(true);
        
        if (model == kJFETModelFull)
        {
            // --Current model--
            
            // calculate JFET drain current Id
                
            dspfloat_t Id = 0;
            
            if (Vds >= 0)
            {
                // calculate Id for positive Vds
                
                switch (jfetState) {
                    case kJFETStateCutoff:
                        Id = Ioff;
                        break;
                    case kJFETStateLinear:
                        Id = beta * Vds * (2 * Vdsp - Vds) * (1 + lambda * Vds);
                        break;
                    case kJFETStateSaturation:
                        Id = beta * Vdsp * Vdsp * (1 + lambda * Vds);
                        break;
                }
            }
            else
            {
                // calculate Id for negative Vds
                
                switch (jfetState) {
                    case kJFETStateCutoff:
                        Id = -Ioff;
                        break;
                    case kJFETStateLinear:
                        Id = beta * Vds * (2 * Vdsp + Vds) * (1 - lambda * Vds);
                        break;
                    case kJFETStateSaturation:
                        Id = beta * Vdsp * Vdsp * (1 - lambda * Vds);
                        break;
                }
            }
            
            // update JFET drain current state, clamp to 2 x Idss
            jfetId[channel] = RangeMath::clamp<dspfloat_t>(Id, 2 * Idss);
            
            // update JFET drain-source voltage
            jfetVd[channel] = Vin - jfetId[channel] * Rin;
        }
        else
        if (model == kJFETModelVCR)
        {
            // --Resistance model--
            
            // calcuate resistance through JFET
            
            dspfloat_t Rds = Rds_off;
            
            if (Vds >= 0)
            {
                // calculate Rd for positive Vds
                
                switch (jfetState) {
                    case kJFETStateCutoff:
                        Rds = Rds_off;
                        break;
                    case kJFETStateLinear:
                        Rds = 1.f / (2.f * beta * Vdsp); // linearized model
                        //Rds = 1. / (2. * beta * (Vdsp + Vds));
                       break;
                    case kJFETStateSaturation:
                        Rds = 1.f / (2.f * beta * Vdsp);
                        //Rds = Rds_on;
                        break;
                }
            }
            else
            {
                // calculate Rd for negative Vds
                
                switch (jfetState) {
                    case kJFETStateCutoff:
                        Rds = Rds_off;
                        break;
                    case kJFETStateLinear:
                        Rds = 1.f / (2.f * beta * Vdsp); // linearized model
                        //Rds = 1. / (2. * beta * (Vdsp - Vds));
                        break;
                    case kJFETStateSaturation:
                        Rds = 1.f / (2.f * beta * Vdsp);
                        //Rds = Rds_on;
                        break;
                }
            }
            
            // update resistor divider ratio (gain reduction)
            jfetGR[channel] = (Rds + Rsg) / (Rin + Rds + Rsg);
            
            // update JFET drain-source voltage
            jfetVd[channel] = Vin * jfetGR[channel];
        }
        
        return jfetVd[channel]; // return JFET drain-source voltage
    }
    
    
    /// Model diode via Shockley equation
    inline dspfloat_t modelDiode(dspfloat_t Vs)
    {
        static constexpr float R = 1e6; //
        static constexpr float Is = 3e-9f; // FDH333 reverse current = 3 nA
        static constexpr float nVt = 25.852e-3f; // thermal voltage, T = 27 deg C

        // calculate current as a function of diode voltage
        
        dspfloat_t x = (Is * R / nVt) * (exp((Vs + Is * R)/ nVt));
        dspfloat_t W = lambert(x); // Lambert function approximation
        dspfloat_t I = (nVt / R) * W; // diode current
        dspfloat_t Vd = Vs - I * R; // diode voltage

        return Vd; // return diode voltage
    }
    
    /// System / circuit constants
    
    static constexpr float Vp = 30; // positive supply rail, volts
    static constexpr float Vn = -10; // negative supply rail, volts
        
    static constexpr float kPreAmpGain = 17.8f; // +25dB
    static constexpr float kLineAmpGain = 10.f; // +20dB (REV D) // G = 5 (~14dB) (REV F)
    static constexpr float kSideAmpGain = 9.0f; // +19dB;

    static constexpr float kAttackValMin = 1;
    static constexpr float kAttackValMax = 7;
    static constexpr float kReleaseValMin = 1;
    static constexpr float kReleaseValMax = 7;

    /// GUI params
    
    bool shapeInput = false;
    bool shapeOutput = false;
    bool bypassSwitch = true;
    bool extSidechain = false;
    
    int ratioButton = kRatio20to1;
    int meterButton = kMeterOff;

    float ratioValue = 1; // 1:1
    float biasTrimPos = 0; // Q bias trim pot position (%)
    float attackKnobVal = 0; // 0 to 7, 0 = bypass
    float releaseKnobVal = 1; // 1 to 7
        
    /// compressor internal state
    
    float SR = 44100.f; // sample rate
    
    dspfloat_t ratioScale;
    dspfloat_t threshBias;
    
    /// JFET state
    
    dspfloat_t jfetVqb = -1.57f; // JFET Q bias, volts
    dspfloat_t jfetVqr = 0.3375f; // JFET Q rdiv, volts (sets Q bias to -1.57V)
    dspfloat_t jfetVgs = jfetVqb; // JFET gate-source voltage
    dspfloat_t jfetEnv = jfetVqb; // JFET gate-source voltage envelope

    dspfloat_t jfetVd[2] = {0, 0}; // JFET drain voltage {L, R}
    dspfloat_t jfetId[2] = {0, 0}; // JFET drain current state {L, R}
    dspfloat_t jfetGR[2] = {1, 1}; // JFET resitor divider state {L, R}

    std::atomic<bool> jfetInSaturation = false;
    std::atomic<int> jfetState = kJFETStateCutoff;
    
    /// ballistics state
    
    dspfloat_t Ratt; // attack pot resistance
    dspfloat_t Rrel; // releae pot resistance

    dspfloat_t Tatt; // attack time milliseconds
    dspfloat_t Trel; // releae time milliseconds

    dspfloat_t attTc = 0;
    dspfloat_t relTc = 0;
    
    // metering state
    
    int meterMode = kMeterGR;
    dspfloat_t meterDecay = 0;
    
    std::atomic<bool> resetMeters { false };
    std::atomic<dspfloat_t> inputMeter = 0;
    std::atomic<dspfloat_t> outputMeter = 0;
    std::atomic<dspfloat_t> gainReduction = 0;

    dspfloat_t dezipTc = 0; // coefficient smoothing time constant
    
    dspfloat_t inputGain[2] = {.031622f, .031622f}; // default = -30dB
    dspfloat_t outputGain[2] = {.177828f, .177828f}; // default = -15dB

    // stereo channel ID
    enum Channel {
        kChL = 0,
        kChR
    };
    
    // coefficient smooth
    enum {
        kTarget=0,
        kActive
    };
    
    /// DSP classes
    
    Waveshaper<dspfloat_t> inputShaper;
    Waveshaper<dspfloat_t> outputShaper;
    
    /// Private methods
    
    /// Potentiometer model - calculates Rt, Rb vs rotation
    void potPosToOhms(float rotation, float& Rt, float& Rb, float Rpot)
    {
        // Convert potentiometer position to ohms
        // rotation: 0 (fully ccw) to 1 (fully cw)
        // Rb = resistance below
        // Rt = resistance above
        // Rpot = total potentiometer resistance
        
        float Rmin = 0.005f * Rpot; // Rmin = 0.5% of total
        float Rmax = (1.f - 0.005f) * Rpot; // Rmax = 99.5% of total
        
        Rb = rotation * (Rmax - Rmin) + Rmin;
        Rt = (1.f - rotation) * (Rmax - Rmin) + Rmin;
    }
        
    /// Adjust FEQ Q bias point
    void updateFetBias()
    {
        const float R1 = 10e3f;
        const float R2 = 4.18e3f; // sum of threshold bias resistor ladder R's
        const float R3 = 3.9e3f;
        
        // FET Q bias trim
        
        const float Rpot = 2e3f; // potentiometer total resistance
        float R4 = 0; // Rt
        float R5 = 0; // Rb

        potPosToOhms(biasTrimPos * .01f, R5, R4, Rpot);
        
        double Ka = (1.f/R1 + 1.f/R3 + 1.f/R5);
        double Kb = (1.f/R2 + 1.f/R4 + 1.f/R5);
        
        // solve linear equations with two unknowns using determinant method
        
        double D = Ka * Kb - (1.f/R5) * (1.f/R5);
        double Dx = Vn * Kb / R1 + Vn / (R2 * R5);
        double Dy = Vn * Ka / R2 + Vn / (R1 * R5);
        
        jfetVqr = Dx / D;
        jfetVqb = Dy / D;
        
        jfetVgs = jfetEnv = jfetVqb;
        
        // update threshold since it depends on jfetVqr
        
        updateThresh();
    }
    
    /// Calculate threshold bias voltage vs. selected ratio
    void updateThresh()
    {
        /// threshold voltage divider

        // resistor ladder values (ohms)
        const float R1 = 150;
        const float R2 = 470;
        const float R3 = 560;
        const float R4 = 1500;
        const float R5 = 1500;
        const float Rt = R1 + R2 + R3 + R4 + R5;

        // calculate total current
        float I = (float(jfetVqb) - Vn) / Rt;
        
        // resistor voltage drops
        float v1 = R1 * I;
        float v2 = R2 * I;
        float v3 = R3 * I;
        float v4 = R4 * I;
        float v5 = R5 * I;
        
        switch (ratioButton)
        {
            case kRatio20to1:
                // A: 20:1 ratio
                threshBias = Vn + v5;
                break;
            case kRatio12to1:
                // B: 12:1 ratio
                threshBias = Vn + v5 + v4;
                break;
            case kRatio8to1:
                // C: 8:1 ratio
                threshBias = Vn + v5 + v4 + v3;
                break;
            case kRatio4to1:
                // D: 4:1 ratio
                threshBias = Vn + v5 + v4 + v3 + v2;
                break;
            default:
                // DEBUG: total voltage drop
                threshBias = Vn + v5 + v4 + v3 + v2 + v1;
                break;
        }
        
        #ifdef FETCOMP_DEBUG
        std::cout << "threshBias(V) = " << threshBias << "\n";
        #endif
    }
    
    /// Calculate ratio voltage divider
    void updateRatio()
    {
        /// ratio voltage divider (attenuator)

        // resistor ladder values (ohms)
        
        //const float R0 = 10e3; // output level potentiometer
        const float R1 = 56e3;
        const float R2 = 68e3;
        const float R3 = 56e3;
        const float R4 = 56e3;
        const float R5 = 47e3;
        
        float Ra;
        float Rb;

        switch (ratioButton)
        {
            case kRatio20to1:
                // A: 20:1 ratio
                Ra = R1;
                Rb = R2 + R3 + R4 + R5;
                break;
            case kRatio12to1:
                // B: 12:1 ratio
                Ra = R1 + R2;
                Rb = R3 + R4 + R5;
                break;
            case kRatio8to1:
                // C: 8:1 ratio
                Ra = R1 + R2 + R3;
                Rb = R4 + R5;
                break;
            case kRatio4to1:
                // D: 4:1 ratio
                Ra = R1 + R2 + R3 + R4;
                Rb = R5;
                break;
            default:
                Ra = R1 + R2 + R3 + R4 + R5;
                Rb = 0;
                break;
        }
#if 1
        ratioScale = Rb / (Ra + Rb);
#else
        // measured ratios (Falstad / 1176N hardware)
        const float measuredRatio[4] = {0.1083f, 0.1885f, 0.2843f, 0.4943f};
        ratioScale = measuredRatio[ratioButton];
#endif
        #ifdef FETCOMP_DEBUG
        std::cout << "ratioScale = " << ratioScale << "\n";
        #endif
    }
    
    /// Update ballistics time constants from attack, release values
    void updateBallistics(void)
    {
        // Time constants

        const float Csmooth = .22e-6f; // .22 uF

        // Attack potentiometer: clockwise --> faster attack
        
        {
            const float Rpot = 25e3;
            const float Rlog = 470;

            float Rt = 0;
            float Rb = 0;
            
            potPosToOhms(attackKnobVal, Rt, Rb, Rpot);
            
            Ratt = (Rlog * Rb) / (Rlog + Rb) + Rt; // apply log taper
        }
        
        // Release potentiometer: clockwise --> faster release
        
        {
            const float Rpot = 5e6;
            const float Rlog = 270e3;
            
            float Rt = 0;
            float Rb = 0;
            
            potPosToOhms(releaseKnobVal, Rt, Rb, Rpot);
            
            Rrel = (Rlog * Rb) / (Rlog + Rb) + Rt; // apply log taper
        }
        
        Tatt = Ratt * Csmooth * 1000.f; // ms
        attTc = TimeMath::onePoleCoeff<dspfloat_t>(Tatt, SR, TimeMath::kDecayAnalog);
        
        #ifdef FETCOMP_DEBUG
        std::cout << "Tatt(ms) = " << Tatt << "\t\t" << "Ratt(ohms) = " << Ratt << "\n";
        #endif
        
        Trel = Rrel * Csmooth * 1000.f; // ms
        relTc = TimeMath::onePoleCoeff<dspfloat_t>(Trel, SR, TimeMath::kDecayAnalog);
        
        #ifdef FETCOMP_DEBUG
        std::cout << "Trel(ms) = " << Trel << "\t\t\t" << "Rrel(ohms) = " << Rrel << "\n";
        #endif
    }
    
    /// Lambert W function approximation (used in diode model)
    dspfloat_t lambert(dspfloat_t x)
    {
        dspfloat_t y = 0;
        
        if (x >= 0 && x <= 10)
        {
            // approximation for small x < e
            dspfloat_t num = 1.f + (123.f/40.f) * x + (21.f/10.f) * x * x;
            dspfloat_t den = 1.f + (143.f/40.f) * x + (713.f/240.f) * x * x;
            y = log(1 + x) * num / den;
        }
        else
        if (x > 10)
        {
            // approximation for larger x
            dspfloat_t L1 = log(x);
            dspfloat_t L2 = log(L1);
            y = L1 - L2 + L2/L1 + (L2*(L2-2))/(2*L1*L1);
        }
        
        return y;
    }
};

