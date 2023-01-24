# Synthesize and transmit audio through BLE LE Audio with nRF5340

*Rein Gundersen Bentdal*

I have for a long time been interested in music production, and lately the creation of audio devices. Through working at Nordic Semiconductors, as a student, I learnt about transmitting audio through Bluetooth, which intrigued me. Especially the new and exiting BLE LE Audio, which is supposed to yield better audio quality with less bitrate and less latency. When playing an instrument you expect there to be very little latency between your action and the sound response. I wanted to test if it was possible to utilize the nRF5340 to generate a pleasing sound, controlled by a simple keyboard interface,  and to see if the latency was good enough to not disturb my playing.

The application put together as a sound generating device, the synthesizer, as well as a receiving headphone. LE Audio enables the possibility to send two audio streams corresponding to the left and right part of the headphone synchronized.

<p align="center">
  <img src="./assets/devices.png" />
</p>

The application is based on the Nordic [nrf5340_audio](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/nrf5340_audio/README.html) demo application, but stripped down and specialized for a synthesizer use case. The main concern of the application is the synthesizer and keyboard input part.

## System Design

<p align="center">
  <img src="./assets/synth_flowchart.png" />
</p>

The synthesizer is constructed to be highly modular, making it easy to include or exclude different modules and test with different topologies of audio synthesis. The provided setup is constructed to demonstrate different aspects of a synthesizer, such as polyphonic oscillators, effects and audio-synced time-dependency.

The synthesizer module receives information on which notes to play and to stop playing, from the button module. These notes may be transformed to create a sequencer, or in this case an arpeggiator. The arpeggiator is time-dependent, and thus needs a source of time. We must use a source of time which is synchronized with the sense of time in the processed audio. This is the function of `tick_provider`, which increments time for each audio block processed. The tick provider sends ticks to subscribing modules. This is the same method MIDI uses to synchronize different audio sources, which makes it possible to implement MIDI synchronization. 

For polyphonic synthesis we need multiple oscillators, equal to the number of notes it should be possible to play at once. `key_assign` keeps track of currently active oscillators and assigns notes to oscillators in a *first-inactive* (the oscillator which has been inactive for the longest time) manner. If there are no inactive oscillators, it will assign the new note to the *first-active* (the oscillator which has been active for the longest time) oscillator. The application is by default configured to have 5 oscillators. Even when playing an arpeggiator with more than 5 notes, it is hard to notice the last-inactive note cutting off.

The application is configured to encode mono audio, in `audio_process_start`. All modules are also constructed as mono.  It isn't too hard to modify the application to instead process stereo audio. The echo effect, for example, may by converted to a stereo ping-pong delay effect. Before the echo effect, split the mono sound into left and right channel. Then input these channels to the echo effect. Make sure to set the `pcm_size` correctly in `sw_codec_encode` for stereo encoding.

## Signal Processing

Audio is processed in blocks of `N` samples, initiated in `audio_process`. A timer ensures new blocks are processed in an interval to match audio sample rate. For further development, this should probably instead be directly synced together with the Bluetooth connection interval. May use [mpsl_radio_notification]([API documentation — nrfxlib 2.2.99 documentation (nordicsemi.com)](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrfxlib/mpsl/doc/api.html#c.mpsl_radio_notification_cfg_set)).

Using a bit depth of 16-bit. The DSP is mainly done using integers in a fixed point format. Thus a type `fixed16` is defined with associated manipulations in `interger_math`. Since the applications is specifically targeted towards the nRF5340, the applications uses included DSP instructions in the SoC in some cases, abstracted by `dsp_instruction`.

The block of code below (from `oscillator.c`) is an example of generating a sinewave with a frequency determined by `phase_increment`. The upper 8 bits of `phase_accumulate`, `phase_accumulate[24:31]`, is used to select which stored samples to use, while phase_increment[8:23]` is used to determine the interpolation value between these two samples. Then there is an applied magnitude to the signal.

```c
  for (uint32_t i = 0; i < block_size; i++) {
    /* upper 8 bit as 256-value sample index */
    uint32_t wave_index = osc->phase_accumulate >> 24;

    /* interpolate between the two samples for better audio quality */
    uint32_t interpolate_pos = (osc->phase_accumulate >> 8) & UINT16_MAX;

    block[i] = FIXED_INTERPOLATE_AND_SCALE(sinus_samples[wave_index], sinus_samples[wave_index + 1], interpolate_pos, osc->magnitude);

    /* increment waveform phase */
    osc->phase_accumulate += osc->phase_increment;
  }
```

`FIXED_INTERPOLATE_AND_SCALE` is provided in `integer_math` to efficiently perform this calculation using DSP instructions on the CPU.

With the current application configuration, about 80% of the *nRF5340* app core is used when all oscillators are active. Around 40% of the app core is used when two headphone devices are connected. A large chunk of this is probably the LC3 encoder.

### Latency

The minimum connection interval in Bluetooth Low Energy is 7.5ms. Audio processing is initiated every 7.5ms to match the connection interval. The processing should thus not result in added latency. The new LC3 codec in LE audio, which replaces the SBC codex, is supposed to have much less latency. But this has not been tested in this particular application. The input buttons is configured with a 50ms debounce time. However this does not contribute to latency. This is because the implementation is such that the button state is assumed to change state whenever a new interrupt is triggered. After the debounce time, this assumption is tested by reading the pin value. This will result in occasional wrong button states, but corrected again after the debounce time. This has not resulted in any audible artifacts from tests.

## Results

The resulting application has demonstrated a functioning system of transmitting synthesizer audio using BLE LE Audio.  For instructions on how to test the application, check out [the Github repo]([ReinBentdal/le_audio_synthesizer: Synthesize and transmit audio through BLE LE audio with nRF5340 (github.com)](https://github.com/ReinBentdal/le_audio_synthesizer)).  This is the test setup mainly used, 2 *nRF5340 Audio DK*s as headphone and 1 *nRF5340 DK* as synthesizer:

<p align="center">
  <img width="500px" src="./assets/test_setup.jpg" />
</p>

Although no quantitative analysis has been done to determine the total latency, I would say its not noticeable when playing the keyboard while listening to the resulting sound. The application has demonstrated moderate audio signal processing, which resulted in about 80% CPU usage in the app core of nRF5340. Thus more complex processing, which might be demanded in consumer synthesis devices, may be hard to pull off. That being said, I am sure there are a lot of new and exiting applications enabled by LE Audio on nRF5340.