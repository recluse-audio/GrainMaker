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

### 2025-10-12
Introduced CLAUDE Code after deliberation.
Pro: Increased Dev Speed
Con: Expose my IP, weaken my dev skills?

- Decision: Use CLAUDE Code
- Why? 
	1. It is free from work currently
	2. My IP is not that safe on this computer currently, and will likely end up on github as well as resume thing (actually i think this seals it I do want to put this on github not just the droplet)



#### Claude Code Instructions: ####


.) Create a Definition for the class GrainBuffer. It should own a juce::AudioBuffer<float> called mBuffer, and a juce::int64 called mLengthInSamples. Provide functions and definitions called setLengthInSamples(juce::int64) and getLengthInSamples().


.) Create a function called getBufferReference() which returns a reference to mBuffer

TEST .)Create catch2 tests for the GrainBuffer in test_GrainBuffer.cpp. Keep them simple, making sure setters works, as well as getBufferREference()

.) In the GrainShifter class, add an array of 2 GrainBuffer called mGrainBuffers. They should both be initialized to be 2 channels and in prepare() then should get set to lookaheadBufferNumSamples in length.

.) Create a test in test_GrainShifter that instantiates a PluginProcessor and calls prepare to play with varied configurations.
- rules
- lookaheadBufferNumSamples is 1024 for block sizes of 512 and less
- if block size is greater than 512, lookaheadBufferNumSamples is double the block size
- lookaheadBufferNumSamples is same at 44100 and 48000, 88200 and 96000, and 176000 and 192000, doublng at each of these pairs the lookaheadBufferNumSamples


---

.) In the GrainShifter class, add an int called mActiveGrainIndex and init to 0. 

---

.) In the GrainShifter class: Add private member juce::int64 mGrainReadIndex with comment "This will be the read index for both grain buffers, when it is larger than the current grain buffers length in samples (after shifting), we know to switch to the other buffer

---

TEST .) In the test_GrainShifter.cpp: Add a catch2 test that checks that mGrainReadIndex is incremented for each sample in the outputBuffer argument of processShifting

---

TEST .) In test_GrainShifter.cpp: Add a catch2 test that checks that the if mGrainReadIndex is equal to or larger than the size of the current active grain buffer getLengthInSamples(), it wraps around the grain buffer's length in samples and swithces mActiveGrainIndex between 0 and 1.

### 2025-10-13 ###
#### Claude Instructions Cont. ####
.) In the GrainShifter class:



### 2025-10-21 ###
Realized that I will likely need to repeat grains. New approach is to duplicate previous grain if new write position happens before the end of the current grain readPos. This will happen when shifting up, but not necessarily every readRange will have multiple emission points every time.