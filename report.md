# Synthesizing and transmit audio on the nrf5340 through le audio
The new le audio specification brings improvements to Bluetooth audio. Lower latency and better percieved audio quality may make digital music instruments, utilizing bluetooth audio, more appealing as commercial products. 

This demo application demonstrates a simple polyphonic synthesizer using the nRF5340 Audio DK. The buttons on the device functions as a simple piano keyboard. The synthesized audio is sendt through le audio to a recieving nRF5340 Audio DK, which functions as the recieving part such as an headset.

## System design
The application is based on the nordic [nrf5340_audio](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/nrf5340_audio/README.html) demo application, but stripped down and specialized for a synthesizer use case. 

### Keys input

### Signal processing

### Transmission

## Results