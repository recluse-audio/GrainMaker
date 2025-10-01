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
I'll go top down, overly simplistic.

#### OBJECTIVE 1: Write 



### 2025-05-23 ###
I lost some work, but hopefully not much... 

Didn't push to remote often enough, 


### 2025-06-07 ###
- [x] getRangeAsAudioBlock()
- [x] writeBlockToRangeInBuffer()
- [x] calculateShiftOffset()
- [ ] updateGrainReadRange()
- [ ] calculateWriteRange()
- [ ] _windowIndexAtIndexInPeriod()
- [ ] _getWindowedGrainRangeAsBlock()

- Need to make sure I am updating lookaheadBuffer read position correctly


### 2025-06-08 ###
New strategy is to calculate a series of juce::Ranges similar to how I did with ARA. Then convert them to juce::AudioBlocks as appropriate. Most of the calculation is figuring out what to write and where

### 2025-07-12 ###
I've been working, but not taking notes, don't worry. 

At this point I am pitch shifting, but clearly have an issue with the grains that overlap one
process block to the next.  This is made clear by the effect corresponding with the block size.  But it is pitch shifting. So the problem lay in overlaps.

I started using an overlap buffer per gpt advice. Technically needs to be just long enough to hold the largest possible pitch period - 1

I made tests of `processShifting()` using incremental buffers and was able to get a clear picture of expected sample indices after shifting. All in all I found I am dropping or adding a sample somewhere, and the chain of confusion runs deep.

The fundamental issue is that `juce::Range::getLength()` and `juce::AudioBuffer::getNumSamples()` don't line up in the way you think. The range sets it's end at startIndex + length. If you use buffer num samples you'd take a buffer with 1024 samples and read to the index [1024] which is out of range relative to the buffer.

So I created a class in the `RD` repo called `BufferRange`. Not complicated at all, but ending confusion 4ever I hope.

In process of making tests for that and then replacing it everywhere I use juce::Range.... eek


### 2025-08-03 ###
I think I have two issues and they aren't buffer discontinuity as I conceived it.

I think at large block sizes the issue may be the buffer overlap issue --- maybe.

With small block sizes, the issue might be that the grains span multiple blocks


### 2025-09-19 ###
Continuing with the concept of the grains with buffers inside them being an issue. 
- not sure if this is what causes the thumping at long buffer sizes though

