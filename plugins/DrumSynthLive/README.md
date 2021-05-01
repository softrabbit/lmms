# DrumSynthLive #

Intentions for this:

- Make DrumSynth into a first-class plugin for LMMS, i.e. one with sound-changing knobs to fiddle and not just an audio renderer for files.
- Convert from integer output to floating point.
- Get rid of the horrible "read through file for every input value" logic.

## Some details on development ##

`checksums.txt` contains filenames, lengths and checksums, compiled like this:

```find ../../data/samples/drumsynth/ -name '*.ds' -exec ./test {} \; >> checksums.txt```

Ubuntu 20.04 is more or less assumed in the early stages. awk, shell, same version of GCC and so on...
