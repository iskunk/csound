<CsoundSynthesizer>
<CsOptions>
; Select audio/midi flags here according to platform
; Audio out   Audio in    No messages
-odac           -iadc     -d     ;;;RT audio I/O
; For Non-realtime ouput leave only the line below:
; -o vranh.wav -W ;;; for file output any platform
</CsOptions>
<CsInstruments>
;Example by Andres Cabrera

sr=44100
ksmps=128
nchnls=2

gitab ftgen 0, 0, 16, -7, 0, 128, 0


instr 1
  krange init p4
  kcps init p5
  ioffset init p6

  kav1 init 0
  kav2 init 0
  kcount init 0

  ;       table   krange  kcps  ielements   idstoffset  iseed  isize ioffset
  vrandh  gitab,  krange, kcps,     3,         3,       0.2,   0,   ioffset

  kfreq1 table 3, gitab
  kfreq2 table 4, gitab
  kfreq3 table 5, gitab

  ;Change the frequency of three oscillators according to the random values
  aosc1 oscili 4000, kfreq1, 1
  aosc2 oscili 2000, kfreq2, 1
  aosc3 oscili 4000, kfreq3, 1

  outs aosc1+aosc2, aosc3+aosc2
endin

</CsInstruments>
<CsScore>
f 1 0 1024 10 1
;             krange  kcps    ioffset
i 1 0 	5	100	1	300
i 1 5 	5	300	1	400
i 1 10 	5	100	2	1000
i 1 15 	5	400	4	1000
i 1 20 	5	1000	8	2000
i 1 25 	5	250	16	300
e

</CsScore>
</CsoundSynthesizer> 