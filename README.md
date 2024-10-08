## sdr-modem

[![CMake](https://github.com/dernasherbrezon/sdr-modem/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/dernasherbrezon/sdr-modem/actions/workflows/cmake.yml) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=dernasherbrezon_sdr-modem&metric=alert_status)](https://sonarcloud.io/dashboard?id=dernasherbrezon_sdr-modem)

Modem based on software defined radios.

## Design

![design](docs/design.png?raw=true)

## Features

 * TCP-based
 * Custom [binary protocol](https://github.com/dernasherbrezon/sdr-modem/blob/main/api.proto) based on protobuf messages.
 * Supported modulation/demodulation:
   * GMSK
 * Supported SDRs:
   * [sdr-server](https://github.com/dernasherbrezon/sdr-server)
   * [plutosdr](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html)
   * file
 * Misc:
   * Doppler's correction for satellites using SGP4 model
   * Save intermittent data onto disk for future analysis/replay

## API

 * Defined in the [api.h](https://github.com/dernasherbrezon/sdr-modem/blob/main/src/api.h)
 * And in the [api.proto](https://github.com/dernasherbrezon/sdr-modem/blob/main/api.proto)


## Configuration

Sample configuration with reasonable defaults:

[https://github.com/dernasherbrezon/sdr-modem/blob/main/src/resources/config.conf](https://github.com/dernasherbrezon/sdr-modem/blob/main/src/resources/config.conf)

## Dependencies

sdr-modem depends on several libraries:

* [libvolk](https://www.libvolk.org). It is recommended to use the latest version (Currently it is 2.x). After libvolk [installed or built](https://github.com/gnuradio/volk#building-on-most-x86-32-bit-and-64-bit-platforms), it needs to detect optimal kernels. Run the command ```volk_profile``` to generate and save profile.
* [libconfig](https://hyperrealm.github.io/libconfig/libconfig_manual.html)
* [libprotobuf-c](https://github.com/protobuf-c/protobuf-c)
* libz. Should be installed in every operational system
* libm. Same
* [libiio](https://github.com/analogdevicesinc/libiio) for plutosdr SDR (Optional)
* [libcheck](https://libcheck.github.io/check/) for tests (Optional)

All dependencies can be easily installed from [r2cloud APT repository](https://r2server.ru/apt.html):

```
sudo apt-get install curl lsb-release
curl -fsSL https://leosatdata.com/r2cloud.gpg.key | sudo gpg --dearmor -o /usr/share/keyrings/r2cloud.gpg
sudo bash -c "echo \"deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://apt.leosatdata.com $(lsb_release --codename --short) main\" > /etc/apt/sources.list.d/r2cloud.list"
sudo bash -c "echo \"deb [signed-by=/usr/share/keyrings/r2cloud.gpg] http://apt.leosatdata.com/cpu-generic $(lsb_release --codename --short) main\" > /etc/apt/sources.list.d/r2cloud-generic.list"
sudo apt-get update
sudo apt-get install libvolk2-dev libprotobuf-c-dev libconfig-dev check libiio
```

## Build

```
mkdir build
cd build
cmake ..
make
```
