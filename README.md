Based on nRF5340 Audio: https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/nrf5340_audio/README.html

## Setup
Assumes zephyr enviorment is configured and west installed.

Build and flash using vs code nrf connect extension. Add `overlay-debug.conf` to your kconfig build configuration.\
![Image](./assets/nrf_connect_tab.PNG)
![Image](./assets/build_configuration.PNG)

The application uses the LC3 codec (closed source). Make sure to include it through west:

`west config manifest.group-filter +nrf5340_audio`

`west update`

The application should now be ready to build through the build action in the nrf connect vs code extension.
