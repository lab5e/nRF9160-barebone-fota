# nRF9160 Barebone FOTA/OTA/DFU using Span
* Demonstrates how to do FOTA on NRF9160 using Span.
* Demonstrates running basic AT-commands.
* Built and tested using ncs 2.0.0 SDK and toolchain on the circuitdojo nRF9160 Feather


## Building

1. Open Visual Studio Code from the nRF Toolchain manager 
1. Make sure that the nRFC Connect extension is installed
1. Open the project and build from the nRF Connect extension
1. Program the device using

```bash
newtmgr -c serial4 image upload build/zephyr/app_update.bin
```

## Configuring target firmware in span

Firmware images have to be signed. This example is using the default key for MCUBoot and image. This key should not be used in a production environment.
The image that is uploaded is found in the build folder (app_to_sign.bin)

```bash
python <path to ncs>\bootloader\mcuboot\scripts\imgtool.py sign --key <path to ncs>\bootloader\mcuboot\root-ec-p256.pem --header-size 0x200 --align 4 --version 1.0.0+42 --pad-header --slot-size 0x78000 app_to_sign.bin <name of signed image>.bin
```

> Open a command shell from the toolchain manager in order to get the correct Python environment.

The device has to be registered in span with IMEI, IMSI and name before firmware can be uploaded. The device will also have to belong to a collection.

Both the device and the collection will have unique IDs in Span. If uploading a new image from a build environment, we will use spancli. Mac , Linux and Windows releases are available for download [here](https://github.com/lab5e/spancli/releases).

**Firmware management** can be enabled both on the collection level and on the device level. To enable firmware management for a device we can use spancli:

```bash
span collection update /token <span access token> /collection-id <collection-id> /firmware-management:device
```

**Uploading firmware** can be done by executing the following command:

```bash
span fw upload /token <span access token> /image <signed image> /collection-id <collection-id> /version <version>
```

**Setting target firmware version** for a device can be done by executing the following command:

```bash
span device update /token <span access token> /collection-id <collection-id> /device-id <device-id> /firmware-version <version>
```


## Running the code

To see log output, you can connect the device via USB to a terminal emulator (coolterm, PUTTY or similar). Parameters are 115200 8N1. 

The device will report to span using COAP and will receive a TLV encoded response telling the device if a firmware update is scheduled, server, port and path for download.

If a new version is available, the code will erase flash bank 1, then use the download client to fetch the image using COAP blockwise transfer. Each block will be written to flash. After the last block is received the device will reboot and MCUBoot will test the image. If the image is ok, MCUBoot will boot the newly downloaded image.

A detailed description of the process can be found [here](https://lab5e.com/blog/2022/5/8/fota-coap/).

## Gotchas

* MCUBoot can be configured to only allow upgrades. That means that it will discard images with a lower version number than the currently running version
* Not erasing flash bank 1 will most likely result in the FOTA process failing the second time it is run.
* Not calling boot_write_image_confirmed() will result in MCUBoot reverting to the previous image after an otherwise successful FOTA process.
* Uploading an unsigned image or an image using a different key than MCUBoot will cause MCUBoot to discard the image.

> Tip: Circuit Dojo has an excellent YouTube tutorial discussing MCUBoot on the nRF9160. This can be found [here](https://www.youtube.com/watch?v=YgMmxa6gRqA)