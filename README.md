# Hob2Hood Matter for XT-ZB2 (BL702)

This firmware turns an XT-ZB2 module into a Matter over Thread device for controlling a range hood. Home Assistant exposes a four-speed fan and a separate light. Commands are sent to the hood controller over UART.

## Features

- Matter over Thread with BLE commissioning;
- endpoint 1: Matter Fan Control with speeds 0-4;
- endpoint 2: Matter On/Off for the light;
- UART1 TX on D23 and RX on D25, 1200 baud, 8N1;
- debounced button on D27: the first short press synchronizes to all-off, rapid follow-up presses select a mode, and a five-second hold performs a factory reset;
- fan and light state are kept in RAM only and start in the off state after every reboot;
- product name `Hob2Hood`, manufacturer `Salu Workshop`, and hardware version `BL702`.

UART commands:

| State | Byte |
|---|---:|
| Fan off | `0` |
| Speed 1 | `1` |
| Speed 2 | `2` |
| Speed 3 | `3` |
| Intensive mode | `4` |
| Light on | `L` |
| Light off | `l` |

After at least five seconds without a button press, the next short press always sends `X` and turns everything off. Further presses made within five seconds cycle through these states:

```text
light -> light + speed 1 -> light + speed 2 -> light + speed 3 -> all off
```

Every short press restarts the five-second window. When the window expires, the following press again turns everything off instead of relying on the last state known to this module. The mode changes after the button is released. A long press does not send a normal command; after five seconds it starts a Matter factory reset.

## Supported build hosts

- Linux x86_64;
- Intel macOS x86_64.

The compiler included in the pinned Bouffalo SDK is available for x86_64 macOS only. On Apple Silicon, run the build under Rosetta; the script intentionally does not enable Rosetta automatically.

```bash
arch -x86_64 ./scripts/build.sh
```

Git, Python 3, and standard build tools are required. On Ubuntu, start with:

```bash
sudo apt-get update
sudo apt-get install -y git python3 curl unzip xz-utils build-essential
```

## Building from scratch

```bash
git clone <REPOSITORY-URL>
cd hob2hood-matter
./scripts/build.sh
```

The first build downloads the pinned Matter and Bouffalo SDK revisions together with the required submodules. This takes some time and several gigabytes of disk space. Subsequent builds reuse the `deps/` directory.

The build also checks the linked firmware: OpenThread must use the continuous 64-bit BL702 clock, the Thread task must feed its watchdog heartbeat, and the image must fit the 1 MiB flash layout. A build that fails any of these checks must not be flashed.

The resulting firmware is written to:

```text
out/hob2hood/chip-bl702-hood-example.bin
```

ZAP may print warnings about the reduced light cluster set and Thread Diagnostics during code generation. This is expected: optional functionality that Home Assistant does not need was removed to conserve BL702 RAM. The build continues after these warnings.

To prepare the dependencies without building the firmware, run:

```bash
./scripts/bootstrap.sh
```

Pinned upstream repositories and commit SHAs are defined in `versions.env`. Application sources are stored in `app/hood-app`, while the minimal vendor changes are kept in `patches/`.

## Flashing

Put the BL702 into UART boot mode in the same way used for the test examples, connect a USB-to-UART adapter, and run:

```bash
./scripts/flash.sh /dev/ttyUSB0
```

The default flashing baud rate is 500000. To use a different rate:

```bash
BAUD=115200 ./scripts/flash.sh /dev/ttyUSB0
```

The script always uses:

- the 1 MiB flash layout from `config/partition_cfg_1M.toml`;
- the `32M` crystal setting;
- the standard Bouffalo boot2 `v6.4-rc6`;
- no Device Tree image.

The 1 MiB partition layout places Matter/Thread settings in a different PSM region from the old 2 MiB layout. After switching layouts, expect to commission the device again; flashing the new image alone does not preserve the old pairing data.

After flashing succeeds, take the module out of boot mode and reboot it. The debug UART runs at 115200 baud, 8N1.

## Adding the device to Home Assistant

A working Matter integration, a Thread Border Router, and current Thread credentials on the phone are required.

1. In Home Assistant, start adding a Matter device.
2. Enter the manual setup code `34970112332` or scan the QR code printed in the boot log.
3. After commissioning, the device appears as `Hob2Hood`.

If the device has already been commissioned or was removed from Home Assistant, perform a factory reset first: connect D27 to GND and hold it for five seconds. This works both during normal operation and when D27 is already held while the module boots. Power cycling never triggers a factory reset.

If a nonblank PSM filesystem is damaged, the firmware deliberately does not erase Matter/Thread credentials automatically. The serial log reports a refused format. Hold D27 to GND for five seconds while powering on to erase PSM and start pairing again; this intentionally removes the old credentials.

## Connecting the hood controller

```text
XT-ZB2 D23 (TX, 3.3 V)  -> hood controller RX
XT-ZB2 GND               -> hood controller GND
```

D25 is needed only if a return channel is added later. Before connecting the boards, verify that the input of the 5 V controller recognizes 3.3 V as a logic high. Never apply 5 V to a BL702 input.

Do not power the XT-ZB2 from 5 V through a resistor divider. Use a proper 3.3 V regulator with enough transient-current headroom and suitable decoupling capacitors.

## Reproducibility

- dependencies are pinned to complete commit SHAs;
- the data model (`.zap` and `.matter`) is generated with tools from the same pinned Matter revision;
- vendor changes are applied as separate, verifiable patch files;
- GitHub Actions runs the same `./scripts/build.sh` command only when a release is published, then attaches the `.bin`, link map, and partition table directly to that release.

Every vendor modification is documented in [docs/vendor-patches.md](docs/vendor-patches.md).

## Licenses

The application is based on Project CHIP/Matter and is distributed under the Apache License 2.0. Downloaded dependencies retain their respective licenses and are not included in this repository.
