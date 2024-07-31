/*
  ==============================================================================

    AutoComp.h
    Created: 12 Sep 2020 10:23:56am
    Author:  Antares

  ==============================================================================
*/

#pragma once

#include "FixedDelay.h"
#include "Waveshaper.h"
#include "LevelMeter.h"
#include "BiquadFilter.h"
#include "MultiStageIIR.h"

#include "../math/LogMath.h"
#include "../math/RangeMath.h"
#include "../math/PitchMath.h"

#include <cmath>
#include <cassert>

#define PARAM2LOG(x, mn, mx) (exp(log(mn) + (x) * (log(mx) - log(mn))))

static const double MAXCLIPLOG = log(pow(10.0, -0.01 / 20.0)); // -0.01dB is clipping

template <typename dspfloat_t>
class FancyComp {
public:
    FancyComp();
    ~FancyComp();
	
    // DSP feedback - values read by GUI in timerCallback() for metering
    // used by getFeedback() method
    
    enum Feedback {
        kRawPeakLvl = 0,// raw peak detector level
        kInputMeter,    // input peak level, with decay
        kCrestFactor,   // crest factor
        kCrestRmsEnv,   // crest RMS envelope
        kCompGainEnv,   // compression gain envelope
        kOutputMeter,   // output peak level, with decay
        kAutoGainEnv,   // auto makeup gain envelope
        kKneeValueDb,   // static knee value in dB
        
        kAutoKneeDb,    // auto knee value in dB
        kAutoGainDb,    // auto gain value in dB
        kAutoAttMsec,   // auto attack value in milliseconds
        kAutoRelMsec,   // auto release value in milliseconds
        
        kNumFbParams
    };
    
    // DSP parameters set by GUI controls
    
	enum Param
	{
        // compressor params
        
		kSlope=0,
        kRatio,
        kLimit,
        kBypass,
        kEnable,
        kConfig,
		kKneeDb,
		kGainDb,
        kRangeDb,
		kThreshDb,
        kTopology,
        
        // ballistics
        
		kAttackMs,
		kReleaseMs,
        kAutoAttackMs,
        kAutoReleaseMs,
		kDetectorStyle,

        // auto mode params
        
		kAutoKnee,
		kAutoMakeup,
		kAutoAttack,
		kAutoRelease,

        // side chain params
        
		kSidechainOn,
		kLookaheadMs,
		kLpfCutoffHz,
		kHpfCutoffHz,
		kFilterEnable,
		kFilterConfig,
		kFilterStages,
		kFilterListen,
        kFilterWideHPF,
        kFilterWideLPF,
        kFilterTracking,

        // dynamic EQ params
        
        kDynEqType,
        kDynEqFreq,
        kDynEqGain,
        kDynEqQval,
        kDynEqFlip,
        kDynEqSurf,
        kDynEqHarm,
        kDynEqSolo,
        kDynEqMode,
        kDynEqOn,

        // pre / post params
        
		kParallelMix,
		kInputGainDb,
		kTubeDriveDb,
	};
    
    // compressor topology
    
    enum Topology {
        kFeedforward = false,
        kFeedback = true
    };
	
    // compressor configuration
    
    enum CompConfig {
        kUseRatio = 0,
        kUseRange,
        kUseSlope,
        kUseRatioInv
    };
    
    // compressor envelope detector ballistics
    
	enum Ballistics {
        kSmoothDecoupled = 0,
		kSmoothBranching,
        kRootMeanSquared,
        kBallisticStyles
	};
	
    // magnitude response filter types
    
    enum FilterId {
        kDynEqMain, // dynamic EQ primary filter
        kDynEqSide, // dynamic EQ analysis filter
        kCompScHPF, // compressor sidechain HPF
        kCompScLPF  // compressor sidechain LPF
    };
    
    // side chain filter configuration
    
	enum FilterConfig {
		kFilterWideBand = 0,
		kFilterSplitBand,
        kFilterDynamicEQ,
        kFilterPitchSurf
	};

    // side chain filter slope
    
	enum FilterSlope {
		k12dBPerOctave = 1, // single stage biquad
		k24dBPerOctave = 2  // two stage biquad
	};
	    
    // dynamic EQ mode
    
    enum DynEqMode {
        kDynEqModeOn = true,    // select dynamic EQ
        kDynEqModeOff = false,  // select static EQ
    };
	

    // dynamic EQ filter type
    
    enum DynEqType {
        kDynEqPEQ = 0, // peaking / bell
        kDynEqLSH,     // low shelf
        kDynEqHSH,     // high shelf
        kDynEqOff,     // off (bypass)
        kDynEqTypes
    };
    
    // Get solo status
    inline bool inSolo(void) const { return deqSolo; }
    
    // Returns ratio if calculated from slope
    inline float getRatio(void) { return ratio; }
    
    // Get side chain filter configuration
    inline int extSideChainOn(void) const { return sidechain; }
    inline int getFilterConfig(void) const { return filterConfig; }
    
    // getFeedback() is called by PluginEditor::timerCallback() for metering
    
    void getFeedback( float ( &pLevel )[kNumFbParams]);
    void getMeters( float ( &pLevel )[3]);

    void clearMeters();
    
    // Get the latency, used for latency reporting to host
    
    int getLatencySamples(void) const
    {
        return lookahead ? int(lookaheadMs * fs_ * .001f) : 0;
    }
    
    // -------------------------------------------------------------------------------
    // Get filter magnitude responses - called from FilterGraph::update() in timer thread
    //
    void getMagnitudeResponse(float freq_hz[], float mag_db[], int numPoints, int filterId);

    // define struct to hold EQ band state, used by FilterGraph
    
    struct EqState {
        bool eqOn;
        bool solo;
        bool flip;
        bool surf;
        bool mode;
        float freq;
        float gain;
        float qual;
        int type;
    };

    // get current EQ state - used by FilterGraph
    
    void getEqState(EqState& state);

    // -------------------------------------------------------------------------------
    // set a parameter - called by PluginProcessor::parameterChanged()
    //
    bool setParam(int paramId, float param, bool smoothed = true);

    // -------------------------------------------------------------------------------
    // Set the sample rate - called by PluginProcessor::prepareToPlay()
    //
    void setSampleRate(float fs);
    
    // -------------------------------------------------------------------------------
    // DSP run method - called from PluginProcessor::processBlock()
    //
    float run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], dspfloat_t ( &sc )[2], bool stereo = false);
    
    // -------------------------------------------------------------------------------
    // Update pitch for EQ surf mode, called inside PluginProcessor::processBlock()
    //
    inline void trackPitch(float freq_hz)
    {
        if (deqSurf) {
            if (freq_hz != -1)
                // update surf EQ frequency, limit to Nyquist
                deqFreq_ = fmin(freq_hz * deqHarm, fs_ / 2);
        } else {
            // restore original (parameter) EQ frequency
            deqFreq_.store(deqFreq);
        }
    }
    
    void setInvertRatio(bool invert) { invertRatio = invert; }
    
private:
    
    /// Math constants / defaults
    
    static constexpr double k0dBFS = 0.99999999;
    static constexpr double kMinVal = 1e-06; // -120 dB
    static constexpr double kLpfHpfQ = 0.7071067812f;
    static constexpr double kDynEqQDef = 1.f;
    static constexpr double kDynEqFcDef = 6500.f;

	/// Ranges / limits
    
	static constexpr float kCoeffMsec = 50.;    // coefficient de-zipper
	static constexpr float kCrestMsec = 200.;   // crest factor RMS
	static constexpr float kMeterMsec = 200.;   // level metering
    static constexpr float kRmsAvgMsec = 20.;   // RMS averaging (50Hz)
	static constexpr float kSmoothMsec = 2000.; // makeup gain

    static constexpr float kRangeLimitDb = 18.;
    static constexpr float kThreshLimitDb = -60;
    
	static constexpr float kMinAttMsec = .1f;
	static constexpr float kMaxAttMsec = 200.;
    static constexpr float kDefAttMsec = 10.;
    
	static constexpr float kMinRelMsec = 5.;
	static constexpr float kMaxRelMsec = 2000.;
    static constexpr float kDefRelMsec = 100.;
    
    static constexpr float kDynEqSmoothMs = 1.; // Dyn EQ biquad coefficient smoothing
    static constexpr float kDynEqUpdateMs = 1.; // Dyn EQ filter update decimation rate

    static constexpr float kDelayMsecMax = 20.; // maximum lookahead delay
    static constexpr float kDelayMsecDef = 0.; // default lookahead delay

    const float dynEqMaxGain = LogMath::dbToLin(kRangeLimitDb);
    const float dynEqMinGain = LogMath::dbToLin(-kRangeLimitDb);

    // enum for coefficient de-zippering
    
    enum {
        kActive = 0, // active (smoothed) coefficient
        kTarget      // target coefficient
    };
            
    float fs_ = 44100.; // sample rate

    /// Compressor config
    
    bool topology = kFeedforward;
    bool sidechain = false;
    bool lookahead = false;
    bool limitMode = false;
    bool bypassComp = false;
    bool noClipping = true;
    bool invertRatio = false;

    bool autoKnee = false;
    bool autoMakeup = false;
    bool autoAttack = false;
    bool autoRelease = false;
    
    int compConfig = kUseRange;
    int ballistics = kSmoothDecoupled;
    
    /// Compressor parameters
	
    float slope = 0.;   // slope = 0 for ratio = 1:1
    float ratio = 1.;   // ratio = 1:1 (bypass)
    float cvpol = -1.;  // cv polarity: -1 = compressor, +1 = expander

    float rangeDb = 0.; // rangeDb = 0 (bypass)
    float threshDb = 0.; // threshDb = 0 (bypass)

    float attackMs = kDefAttMsec;
	float releaseMs = kDefRelMsec;
    
    float autoAttackMaxMs = 80;
    float autoReleaseMaxMs = 1000;
    
	double attMsecAuto = 0;
	double relMsecAuto = 0;
	
	double logKnee = 0.;
	double logKneeAuto = 0.;

    double logRange = 0;

	double logGain[2] = {0., 0.};
	double logThresh[2] = {0., 0.};
	
    double autoKneeMult = 2.; // adjusts auto-knee

    /// Compressor state
    
    std::atomic<double> peakMeter = 0.;
    std::atomic<double> peakLevel = 0.;
    std::atomic<bool> peakReset = true;

    std::atomic<double> gainEnvelope = 1.;
    std::atomic<double> gainSmoothed = 1.;

	double cvEstimate = 0.;
	double cvEnvState = 0.;
	double cvSmoothed = 0.;
    double cvEnvelope = 0.;
    double cvLinCoeff = 1.;
    
    double rmsTc_ = 0.;
    double rmsEnv = 0;
    double rmsState = 0;
    bool rmsSmooth = false;
    
	double crestRmsEnv = 0;
	double crestPeakEnv = 0;
    double crestSquared = 0.;
    double crestFactMax = 0.;

	/// Time constants
	
	double attTc_ = 0.;
	double relTc_ = 0.;
	double rampTc = 0.;
	double crestTc = 0.;
	double meterTc = 0;
	double smoothTc = 0.;
	
	/// Sidechain filter control
	
    int filterConfig = kFilterDynamicEQ;
    int filterStages = k12dBPerOctave;
    
    bool filterEnable = true;
	bool filterListen = false;
    bool filterWideHPF = false;
    bool filterWideLPF = false;

    /// Sidechain filters
    
	MultiStageIIR<dspfloat_t> hpf;
	MultiStageIIR<dspfloat_t> lpf;
    
	float hpfFc = 18;
	float lpfFc = 21000.f;
    
    /// Dynamic EQ primary filter
    
    BiquadFilter<dspfloat_t> deq;  // primary PEQ
    
    int deqType = kDynEqPEQ; // PEQ
    int deqBiquadType = BiquadFilter<dspfloat_t>::kPeaking;
    
    int deqTypeToBiquadType(int type)
    {
        const int deqTypeToBiquadTypeMap[kDynEqTypes] = {
            BiquadFilter<dspfloat_t>::kPeaking,
            BiquadFilter<dspfloat_t>::kLowshelf,
            BiquadFilter<dspfloat_t>::kHighshelf,
            BiquadFilter<dspfloat_t>::kBypass
        };
        
        return deqTypeToBiquadTypeMap[type];
    }

    /// Dynamic EQ sidechain filter
    
    BiquadFilter<dspfloat_t> scf;  // sidechain BPF
    
    int scfBiquadType = BiquadFilter<dspfloat_t>::kBandpass;
    float scfQval = kDynEqQDef;

    int scfTypeToBiquadType(int type)
    {
        const int scfTypeToBiquadTypeMap[kDynEqTypes] = {
            BiquadFilter<dspfloat_t>::kBandpass,
            BiquadFilter<dspfloat_t>::kLowpass,
            BiquadFilter<dspfloat_t>::kHighpass,
            BiquadFilter<dspfloat_t>::kBypass
        };
        
        return scfTypeToBiquadTypeMap[type];
    }
    
    /// Dynamic EQ parameters
    
    std::atomic<bool> deqMode = true;    // enables Dyn EQ dynamic vs static
    std::atomic<bool> deqEqOn = true;    // enables Dyn EQ primary filter
    std::atomic<bool> deqFlip = false;   // inverts Dyn EQ gain polarity
    std::atomic<bool> deqSolo = false;   // enables Dyn EQ sidechain solo
    std::atomic<bool> deqSurf = false;   // enables Dyn EQ pitch tracking
    std::atomic<bool> deqIdle = true;    // indicates Dyn EQ in idle state
    
    float deqHarm = 1.f;          // Dyn EQ harmonic index, surf mode
    float deqGain = 0.f;          // Dyn EQ gain in dB
    float deqGainf = 1.0f;        // Dyn EQ gain linear
    float deqQval = kDynEqQDef;   // Dyn EQ Q value
    
    std::atomic<float> deqFreq = kDynEqFcDef;  // Dyn EQ center frequency in Hz
    std::atomic<float> deqFreq_ = kDynEqFcDef; // Dyn EQ center frequency in Hz, surf mode

    std::atomic<bool> deqDesign = false;   // re-design dynamic EQ primary filter
    std::atomic<bool> scfDesign = false;   // re-design dynamic EQ sidechain filter
    std::atomic<bool> deqSwitch = false;   // indicates Dyn EQ switched out of idle

    /// Dynamic EQ filter update decimation
    
    int filterUpdateClock = 0;
    int filterUpdateSamples = 0;
    
	/// Lookahead delay
	
	std::unique_ptr<FixedDelay<dspfloat_t>> delay[2];
	float lookaheadMs = kDelayMsecDef;
	
	/// Output processing
	
    float driveGain = 0.; // saturation
    
    Waveshaper<dspfloat_t> waveshaper;

    dspfloat_t inputGain = 1.; // input gain trim
	dspfloat_t parallelMix[2] = {0., 0.};

    std::atomic<double> outputMeter = 0.f;
    
	/// Math helpers

    // Calculate smoothing filter coefficient from tau in milliseconds
    inline double onePoleCoeff(double tau)
    {
        return tau > 0.0 ? exp(-1.0 / (tau * .001 * fs_)) : 0.0;
    }

    // Update release time coefficient, subtract attack time from release time if kSmoothDecoupled
    void updateReleaseCoeff()
    {
        relTc_ = onePoleCoeff(std::fmax(0, releaseMs - ((ballistics == kSmoothDecoupled) ? attackMs : 0)));
    }
    
    // sgn function - returns +1 for (+) val, -1 for (-) value
    template <typename T> T sgn(T val)
    {
        return static_cast<T>((T(0) < val) - (val < T(0)));
    }
    
    // -------------------------------------------------------------------------------
    // estimate gain computer slope from the log threshold and log range values,
    // so that calculated gain will not exceed the range for signal amplitudes
    // between the threshold and 0dBFS.
    //
    void estimateSlope(void)
    {
        slope = float(-logRange / fmin(logThresh[kTarget], -0.1));
        slope = RangeMath::limit<float>(slope, 0.99f);
        cvpol = -sgn<float>(slope);
        ratio = cvpol / (1.f - fabs(slope)); // slope to ratio
    }
    
    // converts ratio to slope
    void ratioToSlope(void)
    {
        //assert (ratio >= 1.f);
        cvpol = -sgn<float>(ratio);
        slope = cvpol * (1.f / fmax(fabs(ratio), 1.f) - 1.f);
    }
    
    // converts slope to ratio
    void slopeToRatio(void)
    {
        assert (slope >= -0.99f && slope <= 0.99f);
        cvpol = -sgn<float>(slope);
        ratio = cvpol / (1.f - fabs(slope));
    }
};
