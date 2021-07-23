# DrumSynthLive #

Intentions for this:

- Make DrumSynth into a first-class plugin for LMMS, i.e. one with sound-changing knobs to fiddle and not just an audio renderer for files.
- Convert from integer output to floating point.
- Get rid of the horrible "read through file for every input value" logic. **Done**


## Some details on development ##

`checksums.txt` contains the list of files to run tests (`make testrun`) on.

```find ../../data/samples/drumsynth/ -name '*.ds' -exec ./test {} \; >> checksums.txt```

Ubuntu 20.04 is more or less assumed in the early stages: awk, shell, same version of GCC and so on.



### 2021-07-16 ###
Looks like I'm at the end of how far I'll get keeping the output bit-exact wrt the original. Some things I've discovered along the way:

- Buffer size must be 1200 for some reason. It clearly affects at which time the different parts are turned off after running to the end of the envelopes but oddly enough, changing the buffer size leads to differences even at position 0. Looks like having 2 or 3 of the noise components enabled trigger this, no idea why, though.
- The downsampling in the distortion section is also dependent on buffer size, but shouldn't be a problem for ratios 1-6 as long as the buffer size is divisible by all those. _Oh, 5-7 are mapped to 8, 10 and 20..._ 
- Some things assume a 44100 Hz sample rate. At least the main filter, noise filter and the overtone filter in "808 cymbal" mode use magic numbers that should be adjusted to get comparable output. Worst case, analyze what the filters do and remake them...

### 2021-07-23 ###
Listening session (or rather watching a spectrum analyzer) on the test files, comparing 44 kHz render with 1x (the baseline) and 8x oversampling. So far the output in my refactoring has been bit-exact compared to the LMMS original, so tests were conducted in plain LMMS. The main thing now is finding out what to adjust for sample rate (_Fs_). Refined the findings from previous sessions:

- Main filter seems more or less non-existant at 8x oversampling. The test file seems to have something like a 12 dB/octave HPF at 500 Hz, but there's no trace of that in the oversampled version. I hoped for at least some filtering, maybe at 500*8 or 500/8.
- Tone generation (tone.ds) is close to the original. Droop rate might need adjusting.

- Noise has a "slope" parameter that should probably be adjusted using _Fs_. Could that be the reason for a more pronounced high end (5 kHz -> )in the original (noise.ds)?
- Overtone generation seems close to unchanged (overtone[012].ds), except for mode 3 which has a filter dependent on _Fs_.
- Noise bands might be OK.
- Distortion creates some pretty prominent overtones that go missing when oversampling. Other than that it might be fine.
- Timestretch isn't the right parameter for frequency adjustment!!!

#### Noise slopes and "colors" as seen on the spectrum analyzer ####
- Blue noise (Noise/Slope = 100) = HPF from 2 kHZ, 6 dB/octave
- Azure noise (50) = HPF from 5 kHz, 6 dB/oct
- White noise (0) = what it says on the can, unfiltered goodness
- Pink noise (-50) = LPF from 2 kHz, 3 dB/oct?
- Red noise (-100) = LPF from 500 Hz, 6 dB/oct
