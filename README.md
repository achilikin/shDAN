mmr70mod
========

Mod for MMR-70 FM Music Transmitter

Customization
=============

First get familiar with the code.

If you managed to solder some wires to Analogue Inputs then define populated ADCs using ADC_MASK.
For example, if ADC 4 to 7 populated, then define:

#define ADC_MASK 0xF0

If you experience unstable communication try to calibrate OSCCAL value and then set LOAD_OSCCAL to 1
#define LOAD_OSCCAL 0

Change init_millis() for F_CPU != 8MHz
