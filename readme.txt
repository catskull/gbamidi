How to compile/install/whatever:

You'll need an AVR toolchain and something that can compile to ARM-thumb code
for the GBA code. I've used Debian gcc-avr for the first and a compile I
built using the summon-arm-toolchain script for the second.

Getting the code to work:
- Modify the Makefile to work with your programmer
- Run 'make program'. This'll also program the needed lock bits.
- Connect the contraption to a GBA. The GBA will probably boot up with
  an extra sound indicatin multiboot works, but nothing more yet.
- Wait 10 seconds.
- Connect the MIDI input to a MIDI output on something that can play .mid-files.
- Run make in the gbacode directory (or don't if you didn't modify the code)
- Play fwupdate.mid to the GBA.
- When the midi-file is finished, reset the GBA. You should see the synthesizers
  main UI now.

If you see a colorful but noisy screen after boot, it means the CRC check has
failed. Either the code upload had a hitch or you managed to overflow the
AVRs program memory: the gba code binary gbamidi.gba should be smaller than
14*1024 bytes.

The UI should be fairly self-explanatory, just play around with it. In the
main menu, A/B changes the page you look at and the cursor keys move around.
The MIDI channel you're modifying is above the sliders, the MIDI CC the 
setting you're modifying can also be changed at is displayed under the sliders.
Select toggles between the synthesizer and the sequencer.

In the sequencer, the cursor keys again allow you to navigate btween the 
eight channels. Left or right mutes the channel, but this only actually happens
when a track restarts. B records a track (Be careful! Pressing this will 
remove any existing note data in the track!) and A plays back all the tracks.

Most of the CCs should be obvious, but here a few less-transparent ones:

- Glissando mode: this selects when a glissando should be played. 'Off'
will never play a glissando, 'quickpress' will only glide if two keys are
pressed a little time after eachother, 'legato' will glide when two keys are 
pressed any time after eachother and 'always' will always glide.

- LFO target, range, frequency: You have three LFOs that output a triangle
waveform. These triangle waves are added to a CC you select. For example,
in the default setup CC17 applies the waveform to the pitch of channel 1, 
creating a vibrato effect.

- Sequencer quantize to, rate: The sequencer has the capability to 'pull'
recorded notes to the nearest 1/xth beat, correcting any rythm errors the
player makes. 'Quantize to' sets what to quantize to, quantize rate can be
used to make the quantizer pull 'more' or 'less'. A quantize rate of 0 
effectively turns off the quantizer.

- Bassline split/offset: If you record something on the 'B'-track and set the
bassline split to something >0, every MIDI-note lower than that value
will transpose the bass-line instead of playing a note. The bassline
offset can be used to pre-transpose it a bit.

If you have any questions, you can contact me at jeroen at spritesmods period com.

Jeroen/Sprite_tm