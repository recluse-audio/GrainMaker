/*
  ==============================================================================

    FancyComp.cpp
    Created: 23 Mar 2022 6:09:22pm
    Author:  Andrew

  ==============================================================================
*/

#include "FancyComp.h"

template <typename dspfloat_t>
FancyComp<dspfloat_t>::FancyComp()
{
    // create lookahead delay lines
    delay[0].reset(new FixedDelay<dspfloat_t>(FixedDelay<dspfloat_t>::Type::kStereo));
    delay[1].reset(new FixedDelay<dspfloat_t>(FixedDelay<dspfloat_t>::Type::kStereo));
    
    // design compressor w/ initial sample rate
    setSampleRate(44100);
    
    cvpol = sgn<float>(0.f);
}

template <typename dspfloat_t>
FancyComp<dspfloat_t>::~FancyComp()
{
    delay[0] = nullptr;
    delay[1] = nullptr;
}


/// getFeedback() is called by PluginEditor::timerCallback() for metering

template <typename dspfloat_t>
void FancyComp<dspfloat_t>::getFeedback( float ( &pLevel )[kNumFbParams])
{
    pLevel[kRawPeakLvl] = static_cast<float>(peakLevel);
    peakReset.store(true);

    pLevel[kInputMeter] = static_cast<float>(peakMeter);
    pLevel[kOutputMeter] = static_cast<float>(outputMeter);
    pLevel[kCompGainEnv] = static_cast<float>(gainSmoothed);
    pLevel[kAutoGainEnv] = static_cast<float>(gainEnvelope);

    pLevel[kAutoAttMsec] = static_cast<float>(attMsecAuto);
    pLevel[kAutoRelMsec] = static_cast<float>(relMsecAuto);
    
    pLevel[kAutoGainDb] = LogMath::logToDb(-(cvSmoothed + cvEstimate));
    pLevel[kAutoKneeDb] = LogMath::logToDb(logKneeAuto);
    pLevel[kKneeValueDb] = LogMath::logToDb(logKnee);

    pLevel[kCrestRmsEnv] = sqrtf(static_cast<float>(crestRmsEnv));
    pLevel[kCrestFactor] = sqrtf(static_cast<float>(crestFactMax));
}

template <typename dspfloat_t>
void FancyComp<dspfloat_t>::getMeters( float ( &pLevel )[3])
{
    pLevel[0] = static_cast<float>(peakLevel);
    peakReset.store(true);
    
    pLevel[1] = static_cast<float>(outputMeter);
    pLevel[2] = static_cast<float>(gainEnvelope);
}

template <typename dspfloat_t>
void FancyComp<dspfloat_t>::clearMeters()
{
    peakLevel = 0;
    outputMeter = 0;
    peakReset.store (true);
}

/// get current EQ state - used by FilterGraph

template <typename dspfloat_t>
void FancyComp<dspfloat_t>::getEqState(EqState& state)
{
    state.eqOn = deqEqOn;
    state.solo = deqSolo;
    state.flip = deqFlip;
    state.surf = deqSurf;
    state.freq = deqSurf ? deqFreq_ : deqFreq;
    state.gain = deqGain;
    state.qual = deqQval;
    state.type = deqType;
    state.mode = deqMode && (!deqIdle || deqSwitch);
    
    // clear switch flag if it was set
    if (deqSwitch.exchange(false))
    {
        // nothing to do here
    }
}

// -------------------------------------------------------------------------------
// Get filter magnitude responses - called from FilterGraph::update() in timer thread
//
template <typename dspfloat_t>
void FancyComp<dspfloat_t>::getMagnitudeResponse(float freq_hz[], float mag_db[], int numPoints, int filterId)
{

    switch (filterId) {
        case kDynEqMain:
            // Dynamic EQ primary filter (PEQ)
            for (int n = 0; n < numPoints; n++) {
                mag_db[n] = deq.getMagnitudeResponse(freq_hz[n]);
            }
            break;
        case kDynEqSide:
            // Dynamic EQ sidechain filter (BPF)
            for (int n = 0; n < numPoints; n++) {
                mag_db[n] = scf.getMagnitudeResponse(freq_hz[n]);
            }
            break;
        case kCompScHPF:
            // Wide-band | split-band sidechain HPF
            for (int n = 0; n < numPoints; n++) {
                mag_db[n] = hpf.getMagnitudeResponse(freq_hz[n], n);
            }
            break;
        case kCompScLPF:
            // Wide-band | split-band sidechain LPF
            for (int n = 0; n < numPoints; n++) {
                mag_db[n] = lpf.getMagnitudeResponse(freq_hz[n], n);
            }
            break;
    }
}

// -------------------------------------------------------------------------------
// Set the sample rate - called by PluginProcessor::prepareToPlay()
//
template <typename dspfloat_t>
void FancyComp<dspfloat_t>::setSampleRate(float fs)
{
    fs_ = fs;

    // (re)compute all time constants
    
    attTc_ = onePoleCoeff(attackMs);
    updateReleaseCoeff();
    //relTc_ = onePoleCoeff(releaseMs - ((ballistics == kSmoothDecoupled) ? attackMs : 0));
    
    rmsTc_ = onePoleCoeff(kRmsAvgMsec);
    rampTc = onePoleCoeff(kCoeffMsec);
    crestTc = onePoleCoeff(kCrestMsec);
    meterTc = onePoleCoeff(kMeterMsec);
    smoothTc = onePoleCoeff(kSmoothMsec);
    
    // initialize lookahead delay lines
    
    delay[0]->setMaxDelayTimeMs(fs_, kDelayMsecMax);
    delay[1]->setMaxDelayTimeMs(fs_, kDelayMsecMax);

    delay[0]->setDelayTimeMs(lookaheadMs);
    delay[1]->setDelayTimeMs(lookaheadMs);
    
    // tube overdrive
    
    waveshaper.setSampleRate(fs);
    
    // (re)design sidechain filters
    
    hpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeHighpass, filterStages);
    if (filterConfig == kFilterSplitBand)
        lpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
    else
        lpf.design(fs_, lpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);

    scf.design(fs_, deqFreq, scfQval, 0, scfBiquadType);
    scf.setSmoothingMsec(deqMode ? kDynEqSmoothMs : kCoeffMsec);
    
    // re(design) dynamic EQ filter
    
    deq.design(fs_, deqFreq, deqQval, deqGain, deqBiquadType);
    deq.setSmoothingMsec((deqMode && !deqIdle) ? kDynEqSmoothMs : kCoeffMsec);
    
    // dynamic EQ filter update decimation samples
    
    filterUpdateSamples = (int)(fs_ * kDynEqUpdateMs * .001f);
    filterUpdateClock = 0; // reset clock
}

// -------------------------------------------------------------------------------
// set a parameter - called by PluginProcessor::parameterChanged()
//
template <typename dspfloat_t>
bool FancyComp<dspfloat_t>::setParam(int paramId, float param, bool smoothed)
{
    bool deqDesignNeedsUpdate = false;
    bool scfDesignNeedsUpdate = false;

    switch (paramId) {
            
            /// Compressor params
            
        case kConfig:
            // set configuration / behavior
            compConfig = (int)param;
            if (compConfig == kUseRange) {
                estimateSlope(); // updates ratio
                invertRatio = false;
            }
            else if (compConfig == kUseRatio) {
                ratioToSlope();
            }
            else if (compConfig == kUseSlope) {
                slopeToRatio();
            }
            else if (compConfig == kUseRatioInv) {
                slopeToRatio();
                invertRatio = true;
            }
            break;
            
       case kSlope:
            // slope total range is -1 -> 0 -> +1
            // slope is (-) for compresor, (+) for expander
            // slope = 0 is 1:1 ratio (bypass)
            // slope = +/- 1 is oo:1 ratio (limiter)
            slope = param;
            slopeToRatio();
            break;
            
        case kRatio:
            // set compressor slope via ratio
            // ratio = {1 .. oo} -> slope_ = {0 .. -1}
            ratio = invertRatio ? -param : param;
            ratioToSlope();
            break;
            
        case kLimit:
            // toggle limiter mode - will set infinite ratio
            limitMode = static_cast<bool>(param);
            break;
            
        case kKneeDb:
            // set compressor knee in dB
            logKnee = LogMath::dBToLog(param);
            break;
            
        case kGainDb:
            // set compressor makeup gain in dB
            logGain[kTarget] = LogMath::dBToLog(param);
            break;
            
        case kRangeDb:
            // set compressor slope and gain range in dB
            rangeDb = param;
            logRange = LogMath::dBToLog(param);
            if (compConfig == kUseRange)
                estimateSlope();
            break;
            
        case kThreshDb:
            // set compressor threshold in dB
            threshDb = param;
            if (smoothed)
                logThresh[kTarget] = LogMath::dBToLog(param);
            else
                logThresh[kActive] = LogMath::dBToLog(param);
            if (compConfig == kUseRange)
                estimateSlope();
            break;
            
        case kTopology:
            // select feedforward or feedback topology
            topology = static_cast<bool>(param);
            break;
            
            /// Compressor ballistics
            
        case kAttackMs:
            // set compressor attack time in milliseconds
            attackMs = param;
            attTc_ = onePoleCoeff(attackMs);
            updateReleaseCoeff();
            //relTc_ = onePoleCoeff(releaseMs - ((ballistics == kSmoothDecoupled) ? attackMs : 0));
            break;
            
        case kReleaseMs:
            // set compressor release time in milliseconds
            releaseMs = param;
            updateReleaseCoeff();
            //relTc_ = onePoleCoeff(releaseMs - ((ballistics == kSmoothDecoupled) ? attackMs : 0));
            break;
            
        case kAutoAttackMs:
            // set compressor max auto attack time in milliseconds
            autoAttackMaxMs = param;
            break;
            
        case kAutoReleaseMs:
            // set compressor max auto release time in milliseconds
            autoReleaseMaxMs = param;
            break;
            
        case kDetectorStyle:
            // set compressor envelope detector ballistics style
            ballistics = static_cast<int>(param);
            attTc_ = onePoleCoeff(attackMs);
            updateReleaseCoeff();
            //relTc_ = onePoleCoeff(releaseMs - ((ballistics == kSmoothDecoupled) ? attackMs : 0));
            break;
            
           // Compressor auto modes
            
        case kAutoKnee:
            // enable / disable compressor auto knee
            autoKnee = static_cast<bool>(param);
            break;
            
        case kAutoMakeup:
            // enable / disable compressor auto makeup gain
            autoMakeup = static_cast<bool>(param);
            break;
            
        case kAutoAttack:
            // enable / disable compressor auto attack
            autoAttack = static_cast<bool>(param);
            break;
            
        case kAutoRelease:
            // enable / disable compressor auto release
            autoRelease = static_cast<bool>(param);
            break;
            
            // Pre / post processing params
            
        case kBypass:
            // enable / bypass gain computer
            bypassComp = static_cast<bool>(param);
            break;
            
        case kEnable:
            // enable / bypass gain computer
            bypassComp = !static_cast<bool>(param);
            break;
            
         case kInputGainDb:
            // set the input gain in dB
            inputGain = LogMath::dbToLin(param);
            break;
          
        case kSidechainOn:
            // enable / disable external sidechain input
            sidechain = static_cast<bool>(param);
            break;
            
        case kLookaheadMs:
            // enable / disable sidechain lookahead delay
            lookahead = (param > 0);
            lookaheadMs = fmin(param, kDelayMsecMax);
            delay[0]->setDelayTimeMs(lookaheadMs);
            delay[1]->setDelayTimeMs(lookaheadMs);
            break;
            
        case kParallelMix:
            // set wet / dry (parallel) mix, 0 to 100%
            parallelMix[kTarget] = param * 0.01f; // range is 0 to 100%
            break;
            
        case kTubeDriveDb:
            // set the tube drive gain in dB
            driveGain = param;
            waveshaper.setParam(driveGain, 0);
            break;
            
            /// Side chain filtering
            
        case kFilterEnable:
            // enable / bypass sidechain filtering
            filterEnable = static_cast<bool>(param);
            break;
            
        case kFilterListen:
            // enable / disable sidechain filter audition (listen)
            filterListen = static_cast<bool>(param);
            break;
            
       case kFilterConfig:
            // set wideband, split-band, or dynamic EQ sidechain filtering
            filterConfig = static_cast<int>(param);
            if (filterConfig == kFilterSplitBand)
                lpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
            else
                lpf.design(fs_, lpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
            scfDesignNeedsUpdate = true;
            break;
            
        case kFilterStages:
            filterStages = static_cast<int>(param);
            hpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeHighpass, filterStages);
            if (filterConfig == kFilterSplitBand)
                lpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
            else
                lpf.design(fs_, lpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
            scfDesignNeedsUpdate = true;
            break;
            
        case kFilterWideHPF:
            filterWideHPF = static_cast<int>(param);
            break;
        case kFilterWideLPF:
            filterWideLPF = static_cast<int>(param);
            break;
        case kFilterTracking:
            filterConfig = (static_cast<int>(param)) ? kFilterPitchSurf : kFilterWideBand;
            break;
            
        case kLpfCutoffHz:
            // set sidechain filter LPF cutoff frequency
            lpfFc = param;
            lpf.design(fs_, lpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
            scfDesignNeedsUpdate = true;
            break;
             
         case kHpfCutoffHz:
            // set sidechain HPF filter cutoff frequency
            hpfFc = param;
            hpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeHighpass, filterStages);
            if (filterConfig == kFilterSplitBand)
                lpf.design(fs_, hpfFc, kLpfHpfQ, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterStages);
            scfDesignNeedsUpdate = true;
            break;
             
          /// Dynamic EQ parameters
            
        case kDynEqMode:
            // enable / disable dynamic EQ processing
            deqMode = static_cast<bool>(param) ? kDynEqModeOn : kDynEqModeOff;
            deq.setSmoothingMsec(deqMode == kDynEqModeOn ? kDynEqSmoothMs : kCoeffMsec);
            scf.setSmoothingMsec(deqMode == kDynEqModeOn ? kDynEqSmoothMs : kCoeffMsec);
            if (deqMode == kDynEqModeOff) {
                deqDesignNeedsUpdate = true;
                scfDesignNeedsUpdate = true;
            }
            break;
            
        case kDynEqOn:
            // enable / bypass dynamic EQ filter
            deqEqOn = static_cast<bool>(param);
            //deqBiquadType = deqEqOn ? deqTypeToBiquadType(deqType) : deqTypeToBiquadType(kDynEqOff);
            deqDesignNeedsUpdate = true;
            break;
            
        case kDynEqSolo:
            // enable / disable dynamic EQ solo mode
            deqSolo = static_cast<bool>(param);
            deqDesignNeedsUpdate = true;
            break;

         case kDynEqType:
            // set dynamic EQ filter type + companion side chain filter
            deqType = static_cast<int>(param);
            deqBiquadType = deqTypeToBiquadType(deqType);
            scfBiquadType = scfTypeToBiquadType(deqType);
            scfQval = (deqType != kDynEqPEQ) ? static_cast<float>(kLpfHpfQ) : deqQval;
            deqDesignNeedsUpdate = true;
            scfDesignNeedsUpdate = true;
            break;
            
        case kDynEqFreq:
            // set dynamic EQ filter frequency
            deqFreq = param;
            if (!deqSurf) {
                deqDesignNeedsUpdate = true;
                scfDesignNeedsUpdate = true;
            }
            break;
            
        case kDynEqQval:
            // set dynamic EQ filter Q
            deqQval = param;
            scfQval = (deqType != kDynEqPEQ) ? static_cast<float>(kLpfHpfQ) : deqQval;
            deqDesignNeedsUpdate = true;
            scfDesignNeedsUpdate = true;
            break;
            
        case kDynEqGain:
            // set dynamic EQ filter gain in dB
            deqGain = param; // dB units
            deqGainf = LogMath::dbToLin(param);
            deqDesignNeedsUpdate = true;
            break;
            
       case kDynEqFlip:
            // flip dynamic EQ filter gain
            if (deqFlip != static_cast<bool>(param))
            {
                deqFlip = static_cast<bool>(param);
                deqGain = -deqGain;
                deqGainf = 1.f / deqGainf;
                deqDesignNeedsUpdate = true;
            }
            break;
            
        case kDynEqSurf:
            // enable / disable dynamic EQ pitch tracking (surf mode)
            deqSurf = static_cast<bool>(param);
            if (!deqSurf) {
                scfDesignNeedsUpdate = true;
                deqDesignNeedsUpdate = true;
            }
            break;
            
        case kDynEqHarm:
            // set dynamic EQ harmonic to surf when in pitch-tracked mode
            deqHarm = param;
            break;
        
    }
           
    if ((deqIdle && !deqSurf) || (filterConfig == kFilterPitchSurf && !deqSurf))
    {
        // re-design dynamic EQ primary filter if not in dynamic or surf modes
        if (deqDesignNeedsUpdate)
            deqDesign.store(true);
            //deq.design(fs_, deqFreq, deqQval, deqGain, deqBiquadType);
        
        // re-design dynamic EQ sidechain filter if not in dynamic or surf modes
        if (scfDesignNeedsUpdate)
            scfDesign.store(true);
            //scf.design(fs_, deqFreq, scfQval, 0, scfBiquadType);
    }
    
    return (scfDesignNeedsUpdate || deqDesignNeedsUpdate);
}

// -------------------------------------------------------------------------------
// DSP run method - called from PluginProcessor::processBlock()
//
template <typename dspfloat_t>
float FancyComp<dspfloat_t>::run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], dspfloat_t ( &sc )[2], bool stereo)
{
    // Define some temp variables
    
    dspfloat_t x[2] = {0, 0}; // compressor input
    dspfloat_t y[2] = {0, 0}; // processed output
    dspfloat_t u[2] = {0, 0}; // sidechain input
    
    // Apply gain trim to input samples
    
    x[0] = xi[0] * inputGain;
    x[1] = xi[1] * inputGain;
    
    // Do input peak level metering w/ decay
    
    double inputLevel = fabs(x[0]);
    if (stereo) {
        inputLevel = fmax(inputLevel, fabs(x[1])); // stereo link
    }
    
    peakMeter.store(fmax(peakMeter, inputLevel) * meterTc); // apply decay
    
    // Get compressor sidechain input
    
    u[0] = (topology == kFeedback) ? sc[0] * float(cvLinCoeff) : sc[0] * inputGain;
    u[1] = (topology == kFeedback) ? sc[1] * float(cvLinCoeff) : sc[1] * inputGain;
    
    // Run sidechain filtering
    
    if (!filterEnable)
    {
        // Bypass sidechain filteing
        y[0] = u[0];
        y[1] = u[1];
    }
    else
    {
        if (filterConfig == kFilterWideBand)
        {
            // Run HPF
            if (filterWideHPF)
                hpf.run(u, u, stereo);
            
            // Run LPF
            if (filterWideLPF)
                lpf.run(u, u, stereo);
            
            y[0] = u[0];
            y[1] = u[1];
        }
        else
        if (filterConfig == kFilterSplitBand)
        {
            dspfloat_t w[2] = {0, 0};
            
            // Run parallel HPF + LPF filters (split-band mode)
            hpf.run(u, y, stereo); // HP band in y
            lpf.run(u, w, stereo); // LP band in w
            
            // Must invert LP band for 2-pole crossover!
            x[0] = (filterStages == k12dBPerOctave) ? -w[0] : w[0];
            x[1] = (filterStages == k12dBPerOctave) ? -w[1] : w[1];
        }
        else
        if (filterConfig == kFilterDynamicEQ)
        {
            // Run dynamic EQ sidechain filter
            if (deqMode || deqSolo)
                scf.runInterp(u, y, stereo);
        }
        else
        if (filterConfig == kFilterPitchSurf)
        {
            scf.runInterp(u, y, stereo);
        }
    }
    
    // Update dynamic EQ idle state
    
    bool deqIdle_ = deqIdle; // cache previous
    deqIdle = ((rangeDb == 0.f) || (threshDb == 0.f));
    
    // Test if DEQ idle state changed
    
    bool deqSwitch_ = false;
    if (deqIdle != deqIdle_)
    {
        if (deqIdle == true)
        {
            // entering idle: set default smoothing
            deq.setSmoothingMsec(kCoeffMsec);
            scf.setSmoothingMsec(kCoeffMsec);

            // force immediate filter re-design
            filterUpdateClock = filterUpdateSamples;
            
            // update flags
            deqSwitch = true; // atomic bool reset by GUI (FilterGraph)
            deqSwitch_ = true; // used below to re-design filter to default gain
        }
        else
        {
            // leaving idle: set dynamic smoothing
            deq.setSmoothingMsec(kDynEqUpdateMs);
            scf.setSmoothingMsec(kDynEqUpdateMs);
        }
    }

    if (peakReset.exchange(false))
    {
        peakLevel = 0;
        crestFactMax = 0;
    }
    
    // Check for bypass
    
    bool bypass = deqMode ? (deqIdle || !deqEqOn) : bypassComp;
    
    // Gain computer
    
    if (!bypass)
    {
        // Do peak detection
        
        double level = fabs(y[0]);
        if (stereo)
        {
            // stereo link
            level = fmax(level, fabs(y[1]));
        }
        
        // Update input peak level value
        
        peakLevel = fmax(peakLevel, level);
        
        // Crest factor calculation
        
        double crestIn = fmax(level * level, kMinVal); // prevents divide by zero
        
        crestRmsEnv = crestTc * crestRmsEnv + (1.f - crestTc) * crestIn;
        crestPeakEnv = fmax(crestIn, crestTc * crestPeakEnv + (1.f - crestTc) * crestIn);
        crestSquared = crestPeakEnv / crestRmsEnv;
        crestFactMax = fmax(crestFactMax, crestSquared);
        
        // Auto attack control (AAC)
        
        double attTc = attTc_; // get param attack coeff
        if (autoAttack)
        {
            attMsecAuto = 2.f * autoAttackMaxMs / crestSquared;
            attTc = onePoleCoeff(attMsecAuto);
        }
        
        // Auto release control (ARC)
        
        double relTc = relTc_; // get param release coeff
        if (autoRelease)
        {
            relMsecAuto = 2.f * autoReleaseMaxMs / crestSquared;
            if (ballistics == kSmoothDecoupled)
                relTc = onePoleCoeff(relMsecAuto - attMsecAuto);
            else
                relTc = onePoleCoeff(relMsecAuto);
        }
        
        // Run 1st stage ballistics
        
        // RMS smoother (opto mode)
        
        if (rmsSmooth)
        {
            rmsState = rmsTc_ * rmsState + (1.f - rmsTc_) * (level * level);
            rmsEnv = sqrtf((float)rmsState);
        }
        
        // Smooth gain + threshold parameters
        
        logGain[kActive] = rampTc * logGain[kActive] + (1.f - rampTc) * logGain[kTarget];
        logThresh[kActive] = rampTc * logThresh[kActive] + (1.f - rampTc) * logThresh[kTarget];

        // Inputs to gain computer
        
        double logIn = log(fmax(rmsSmooth ? rmsEnv : level, kMinVal));
        double logDelta = logIn - logThresh[kActive]; //

        double slope_ = (autoKnee || limitMode) ? -1.f : slope; // set ratio = oo if auto knee
        cvEstimate = logThresh[kActive] * -slope_ / 2.f; // static makeup gain estimate
        
        // Auto knee control (AKC)
        
        logKneeAuto = fmax(-(cvSmoothed + cvEstimate) * autoKneeMult, 0.f);
        
        // Gain computer + knee interpolation
        
        double logWidth = autoKnee ? logKneeAuto : logKnee;
        double logHalfWidth = logWidth / 2.f;

        double dw = logDelta + logHalfWidth; // for knee interp
        double cv = 0; // init cv to below knee (no compression)
        
        if (logDelta >= logHalfWidth)
        {
            // above knee - apply full compression
            cv = logDelta;
        }
        else
        if (logDelta > -logHalfWidth && logDelta < logHalfWidth)
        {
            // in knee - do quadratic interpolation
            cv = 1.f / (2.f * logWidth) * (dw * dw);
        }
        
        // apply slope, make cv positive for ballistics
        cv *= (topology == kFeedback) ? (1.f - fabs(ratio)) * -cvpol : (slope_ * -cvpol);
        
        // Run 2nd stage ballistics
        
        switch (ballistics)
        {
            case kSmoothDecoupled:
                cvEnvState = fmax(cv, relTc * cvEnvState + (1.f - relTc) * cv);
                cvEnvelope = attTc * cvEnvelope + (1.f - attTc) * cvEnvState;
                break;
                
            case kSmoothBranching:
                if (cv > cvEnvelope)
                    cvEnvelope = attTc * cvEnvelope + (1.f - attTc) * cv;
                else
                    cvEnvelope = relTc * cvEnvelope + (1.f - relTc) * cv;
                break;
                
            case kRootMeanSquared:
                cvEnvState = relTc * cvEnvState + (1.f - relTc) * (cv * cv);
                cvEnvelope = sqrtf((float)cvEnvState);
                break;
                
            default:
                assert(false);
                break;
        }

        
        // cv envelope is (-) for gain reduction, (+) for gain expansion
        
        cv = cvEnvelope * -cvpol;
        
        // Run auto makeup gain
        
        cvSmoothed = smoothTc * cvSmoothed + (1.f - smoothTc) * (cv - cvEstimate);
        
        if (autoMakeup)
        {
            if (noClipping && (logIn + cv - (cvSmoothed + cvEstimate) > MAXCLIPLOG)) {
                cvSmoothed = logIn + cv - cvEstimate - MAXCLIPLOG;
            }
            cv -= (cvSmoothed + cvEstimate);
        }
        else
        {
            cv += logGain[kActive];
#if 0
            if (deqMode != kDynEqModeOff)
                cv -= cvEstimate; // static makeup gain
#endif
        }
        
        // Convert from log domain back to linear to get applied gain,
        // if bypass no gain is applied (unity).
        
        cvLinCoeff = bypassComp ? 1.f : exp(cv);
    }
    else
    {
        cvLinCoeff = 1.;
        
    } // if (!bypassComp)
    
    // Update gain envelope and smoothed gain metering params
    
    gainEnvelope.store(bypassComp ? 1.f : exp(cvEnvelope * -cvpol));
    gainSmoothed.store(bypassComp ? 1.f : exp(cvSmoothed * -cvpol));

    // If doing dynamic EQ, re-calculate the filters at decimated rate

    if (filterConfig == kFilterDynamicEQ)
    {
        if (deqMode && deqEqOn && (!deqIdle || deqSurf || deqSwitch_))
        {
            if (++filterUpdateClock > filterUpdateSamples)
            {
                filterUpdateClock = 0;

                // calculate filter gain and limit to +/- 18dB
                float g = static_cast<float>(cvLinCoeff * deqGainf);
                g = fmin(g, dynEqMaxGain);
                g = fmax(g, dynEqMinGain);

                // re-design primary filter w/ updated surf frequency + computed gain
                deq.design(fs_, deqFreq_, deqQval, g, deqBiquadType, BiquadFilter<dspfloat_t>::kGainLinear);
                
                // re-design sidechain filter w/ updated surf frequency
                scf.design(fs_, deqFreq_, scfQval, 1.f, scfBiquadType, BiquadFilter<dspfloat_t>::kGainLinear);
            }
        }
        else
        {
            // re-design primary filter w/ default frequency & gain
            if (deqDesign.exchange(false))
            {
                deq.design(fs_, deqFreq, deqQval, deqGain, deqBiquadType, BiquadFilter<dspfloat_t>::kGainLogDb);
            }
            
            // re-design sidechain filter w/ default frequency
            if (scfDesign.exchange(false))
            {
                scf.design(fs_, deqFreq, scfQval, 1.f, scfBiquadType, BiquadFilter<dspfloat_t>::kGainLinear);
            }
        }
    }
    else
    if (filterConfig == kFilterPitchSurf)
    {
        if (deqSurf)
        {
            if (++filterUpdateClock > filterUpdateSamples)
            {
                filterUpdateClock = 0;

                // re-design sidechain filter w/ updated surf frequency
                scf.design(fs_, deqFreq_, scfQval, 1.f, scfBiquadType, BiquadFilter<dspfloat_t>::kGainLinear);
            }
        }
        else
        {
            // re-design sidechain filter w/ default frequency
            if (scfDesign.exchange(false))
            {
                scf.design(fs_, deqFreq, scfQval, 1.f, scfBiquadType, BiquadFilter<dspfloat_t>::kGainLinear);
            }
        }
    }

    // Apply lookahead delay
    
    if (lookahead)
    {
        delay[0]->run(x); // delay input (or split-band LP band)
        delay[1]->run(y); // delay split-band HP band (always run!)
    }
    
    // Apply gain reduction (compression) or dynamic EQ
    
    if (filterListen || deqSolo)
    {
        // y[] already contains the filtered sidechain sample(s)
    }
    else
    {
        if (filterConfig == kFilterWideBand || filterConfig == kFilterPitchSurf)
        {
            y[0] = x[0] * static_cast<dspfloat_t>(cvLinCoeff); // apply gain
            if (stereo)
                y[1] = x[1] * static_cast<dspfloat_t>(cvLinCoeff); // apply gain
        }
        else
        if (filterConfig == kFilterSplitBand)
        {
            y[0] *= static_cast<dspfloat_t>(cvLinCoeff); // compress HP band L
            y[0] += x[0];  // recombine w/ LP band L
            if (stereo)
            {
                y[1] *= static_cast<dspfloat_t>(cvLinCoeff); // compress HP band R
                y[1] += x[1]; // recombine w/ LP band R
            }
        }
        else
        if (filterConfig == kFilterDynamicEQ)
        {
            if (deqEqOn)
            {
                deq.runInterp(x, y, stereo);

            }
            else
            {
                y[0] = x[0];
                if (stereo)
                    y[1] = x[1];
            }
        }
        
        // Apply saturation model
        
        if (driveGain > 0)
            waveshaper.run(y, y, stereo);
        
        // Apply wet / dry mix (parallel compression)
        
        parallelMix[kActive] = static_cast<dspfloat_t>(rampTc) * parallelMix[kActive]
                        + (1.f - static_cast<dspfloat_t>(rampTc)) * parallelMix[kTarget];
        
        y[0] = x[0] * parallelMix[kActive] + y[0] * (1.f - parallelMix[kActive]);
        if (stereo)
            y[1] = x[1] * parallelMix[kActive] + y[1] * (1.f - parallelMix[kActive]);
    }
    
    // Do output level metering w/ decay
    
    double outputLevel = fabs(y[0]);
    if (stereo) {
        outputLevel = fmax(outputLevel, fabs(y[1])); // stereo link
    }
    
    outputMeter.store(fmax(outputMeter, outputLevel) * meterTc); // apply decay

    // Clamp output if not doing dynamic EQ
    
    if (deqMode == kDynEqModeOff)
        RangeMath::limit<dspfloat_t>(y, static_cast<dspfloat_t>(k0dBFS));
    
    xo[0] = y[0];
    xo[1] = stereo ? y[1] : x[1]; // pass-through R input if mono
    
    return static_cast<float>(cvLinCoeff);
}

template class FancyComp<float>;
template class FancyComp<double>;
