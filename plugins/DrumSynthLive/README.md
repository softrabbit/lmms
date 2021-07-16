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

- Buffer size must be 1200 for some reason. It clearly affects at which time the different parts are turned off after running to the end of the envelopes but oddly enough, changing the buffer size leads to differences even at position 0. 
- The downsampling in the distortion section is also dependent on buffer size, but shouldn't be a problem for ratios 1-6 as long as the buffer size is divisible by all those.
- Some things assume a 44100 Hz sample rate. At least the main filter, noise filter and the overtone filter in "808 cymbal" mode use magic numbers that should be adjusted to get comparable output. Worst case, analyze what the filters do and remake them...
- 
