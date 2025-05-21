## JOURNAL ##

A general note catch-all for development of ArtieTune


---
### 2025-04-24 ###
First day of keeping notes here.  We will see how it goes.

Don't recall exactly where I left.  I know the granulator was working as a ring modulator, but not a pitch-synchronous pitch shifter.  

Had recently added a `processShifting()` function in `Granulator`, sort of taking a fresh run at it.  
- This is not completed yet.  


Current elevator pitch.
- Give me two buffers (`outputBuffer` , `lookaheadBuffer`) 
- `lookaheadBuffer` is larger by the "lookahead" numSamples duh
- Each `lookaheadBuffer` is one analyzed chunk of audio aka same pitch period
- Make grains out of `lookaheadBuffer`.  The grains have a length equal to the pitch period 
- Write these grains to the `outputBuffer`, offset by the lookahead
- This describes operation with no shifting


The above is a baseline mechanism, now we shift
[Don't forget about the lookahead offset as we do more offsetting]  


---
### 2025-05-08 ###
Back at it, I've given this thought but have been slow in progress.  Working on stuff with Antares all the time ofc.

Today I am trying a new approach which involves treating the lookaheadBuffer and outputBuffer as ARAAudioSource and ARAPlaybackRegion.  Or rather my work on "simple" ARA, which got far from simple.

But I MUST test-drive this!  So what is the test to be had?

