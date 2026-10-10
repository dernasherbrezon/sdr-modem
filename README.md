## sdr-modem

[![CMake](https://github.com/dernasherbrezon/sdr-modem/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/dernasherbrezon/sdr-modem/actions/workflows/cmake.yml) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=dernasherbrezon_sdr-modem&metric=alert_status)](https://sonarcloud.io/dashboard?id=dernasherbrezon_sdr-modem)

Modem based on software defined radios.

## Design

![design](docs/design.png?raw=true)

## Features

 * TCP-based
 * Custom binary protocol
 * Supported modulation/demodulation:
   * GFSK
 * Supported SDRs:
   * [sdr-server](https://github.com/dernasherbrezon/sdr-server)
   * [plutosdr](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html)
   * file
 * Misc:
   * Doppler's correction for satellites using SGP4 model
   * Save intermittent data onto disk for future analysis/replay

## API

 * Defined in the [api.h](https://github.com/dernasherbrezon/sdr-modem/blob/main/src/api.h)

## Configuration

Sample configuration with reasonable defaults:

[https://github.com/dernasherbrezon/sdr-modem/blob/main/src/resources/config.conf](https://github.com/dernasherbrezon/sdr-modem/blob/main/src/resources/config.conf)

## Dependencies

sdr-modem depends on several libraries:

* [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)
* [libconfig](https://hyperrealm.github.io/libconfig/libconfig_manual.html)
* libz. Should be installed in every operational system
* libm. Same
* [libiio](https://github.com/analogdevicesinc/libiio) for plutosdr SDR (Optional)

All dependencies can be easily installed from [r2cloud APT repository](https://r2server.ru/apt.html):

```
sudo apt-get install curl lsb-release
curl -fsSL https://leosatdata.com/r2cloud.gpg.key | sudo gpg --dearmor -o /usr/share/keyrings/r2cloud.gpg
sudo bash -c "echo \"deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://apt.leosatdata.com $(lsb_release --codename --short) main\" > /etc/apt/sources.list.d/r2cloud.list"
sudo bash -c "echo \"deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://apt.leosatdata.com/cpu-generic $(lsb_release --codename --short) main\" > /etc/apt/sources.list.d/r2cloud-generic.list"
sudo apt-get update
sudo apt-get install libliquid-dev libconfig-dev libiio-dev
```

## Build

```
mkdir build
cd build
cmake ..
make
```

## License

sdr-modem is licensed under the [Apache License 2.0](LICENSE).
