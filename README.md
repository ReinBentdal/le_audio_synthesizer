# Synthesize and transmit audio through ble le audio with nrf5340
The new le audio specification brings improvements to Bluetooth audio. Lower latency and better perceived audio quality may make digital music instruments, utilizing Bluetooth audio, more appealing as commercial products. 

This demo application demonstrates a simple polyphonic synthesizer. The buttons on the device functions as a simple piano keyboard. The synthesized audio is send through le audio to a *nRF5340 Audio DK*, which functions as the receiving part such as an headset.

<p align="center">
  <img src="./assets/devices.png" />
</p>

## Table of contents
1. [Useful resources](#useful-resources)
2. [System design](#system-design)
3. [Signal processing](#signal-processing)
4. [Programming and testing](#programming-and-testing)

## Useful resources

- [Teensy audio library for embedded](https://github.com/PaulStoffregen/Audio)
- [nRF5340 audio application](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/nrf5340_audio/README.html)

## System design

The application is based on the Nordic [nrf5340_audio](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/nrf5340_audio/README.html) demo application, but stripped down and specialized for a synthesizer use case. 

The diagram below illustrates the general synthesizer flow.

<p align="center">
  <img src="./assets/synth_flowchart.png" />
</p>
The synthesizer is highly modular which makes it easy to include or exclude modules. The provided setup is constructed to demonstrate different aspects of a synthesizer, such as polyphonic oscillators, effects and audio-synced time-dependency.

Which notes to play or stop is sent into the synthesizer module. These notes may be transformed to create a sequencer, or in this case an arpeggiator. The arpeggiator is time-dependent, and thus needs a source of time. We must use a source of time which is synchronized with the sense of time in the processed audio. This is the function of `tick_provider`, which increments time for each audio block processed. The tick provider sends ticks to subscribing modules. This is the same method MIDI uses to synchronize different audio sources.

For polyphonic synthesis we need multiple oscillators, equal to the number of notes it should be possible to play at once. `key_assign` keeps track of currently active oscillators and assigns note to oscillator in a *first-inactive* manner. If there are no inactive oscillators it will assign the new note to the *first-active* oscillator. The application is by default configured to have 5 oscillators. Even when playing an arpeggiator with more than 5 notes, it is hard to notice the last-inactive note cutting off.

The application is configured to encode mono audio, in `audio_process_start`. All modules are also constructed as mono.  It isn't too hard to modify the application to instead process stereo audio. The echo effect, for example, may by converted to a stereo ping-pong delay effect. Before the echo effect, split the mono sound into left and right channel. Then input these channels to the echo effect. Make sure to set the `pcm_size` correctly in `sw_codec_encode` for stereo encoding.

## Signal processing

Audio is processed in blocks of `N` samples, initiated in `audio_process.c`. A timer ensures new blocks are processed in an interval to match audio sample rate. For further development, this should probably instead be directly synced together with the Bluetooth connection interval.

*TODO: briefly talk about modules construction*

### Latency

*TODO: button latency, audio processing latency, BLE latency*

## Programming and testing
*Minimum hardware requirements:*
- 1 *nRF5340 DK*
- 1 *nRF5340 audio DK* \
or
- 2 *nRF5340 audio DK*

### Programming
There is provided prebuilt binaries which works out of the box. It is recomended to program these binaries with the included `program.py` script. Run `python program.py -h` to see available options. It is recomended to verify that you are able to get the prebuilt binaries to work before building and programming from source yourself. You should program both the synthesizer board as well as the headset boards. Example of using `program.py`:

Programming synthesizer board
> python program.py --snr 1234567890 --device synth --board nrf5340_dk

Programming left headset board, snr is inferred if only one device is connected. If not, you will be promted to select from a list
> python program.py --device left

#### Building and program yourself
The application is developed using `nrf-sdk v2.0.2`. `common_net.hex` is borrowed from the `nrf5340_audio` application provided with this version of nrf-sdk, located in `bin/ble5-ctr-rpmsg_3251.hex`. The headsets has to be programmed with this audio application version to function correctly. The provided headset binaries are build from the stock `nrf5340_audio` application with added configs `CONFIG_AUDIO_HEADSET_CHANNEL_COMPILE_TIME=y` and `CONFIG_AUDIO_HEADSET_CHANNEL=x`, where x=0 => left headset, x=1 => right headset.

It is recomended to build and program the synth application through VS Code with the nrf Connect extension. Select the board you want to build for. Add `overlay-debug.conf` to your kconfig build configuration.

<p align="center">
  <img height="400px" src="./assets/nrf_connect_tab.PNG" /> <img height="400px" src="./assets/build_configuration.PNG" />
</p>



*TODO: LC3 no longer closed source?*

The application uses the LC3 codec (closed source). Make sure to include it through west:

`west config manifest.group-filter +nrf5340_audio`

`west update`

The application should now be ready to build through the build action in the nrf connect vs code extension.

### Testing
Turn on both the synthesizer board as well as 1 or 2 headset boards. If `LED1`(blue) lights up on the headset board, there is a connection. By pressing the `Play/Pause` button on the headset board, audio output is enabled. Connect speaker to the headphone aux connection on the headset. Pressing `Button 1` to `Button 4` on the synthesizer board should now result in audio from the speaker.

Image below illustrates a possible setup with *nRF5340 DK* as synth.

<p align="center">
  <img width="500px" src="./assets/test_setup.jpg" />
</p>
