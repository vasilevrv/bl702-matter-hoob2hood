# Vendor code changes

This repository does not contain full copies of Matter or the Bouffalo SDK. `bootstrap.sh` downloads the exact pinned upstream revisions and applies two small patch files.

## `patches/connectedhomeip.patch`

### `examples/platform/bouffalolab/common/plat/platform.cpp`

OTA code is guarded by `CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR`. OTA is disabled in this firmware, so the unused requestor is not created and does not consume RAM.

### `scripts/tests/requirements.txt`

BlueZoo is installed only when Python 3.11 or newer is used. The package is required by Matter's Linux tests but is not needed to build the firmware. This lets the standard Python 3.10 installation on Ubuntu 22.04 prepare the build environment without failing on the incompatible `bluezoo>=1.0.2` dependency.

### `src/app/clusters/network-commissioning/ThreadScanResponse.cpp`

The temporary Thread scan result array is moved from the heap to the handler stack. This preserves the BL702's limited heap during commissioning and prevents the `Memory Allocate Failed` error during `AddNOC`.

### `src/platform/bouffalolab/common/BflbConfig_littlefs.cpp`

LittleFS reads, writes, closes, KVS access, and factory-reset operations now validate their return values and reject truncated scalar values. Factory reset now propagates directory-read, directory-close, file-close, and delete failures instead of reporting a partial reset as successful. Without these checks, damaged or failed PSM operations could be reported as successful and later cause invalid state or a hardware exception.

Raw factory reset additionally requires the PSM partition to match the checked-in 1 MiB layout (`0xE4000`, 64 KiB) and requires flash-write protection to accept that region. An invalid partition table cannot redirect D27 reset to another flash region.

### `examples/platform/bouffalolab/common/iot_sdk/platform_port.cpp`

The idle hook feeds a 60-second hardware watchdog only while the application task, Matter event loop, and OpenThread task publish recent heartbeats. Stack overflow, allocation failure, and assertion handlers reset the controller instead of leaving it in an infinite loop.

### `examples/platform/bouffalolab/common/plat/main.cpp`

The hardware watchdog is started during boot, and fatal application errors reset the controller instead of hanging permanently. The D27 factory-reset check runs before LittleFS is mounted; when held at boot it erases the PSM partition directly and reboots, so a corrupted filesystem can still be recovered. The same raw PSM erase is used for the runtime D27 factory-reset path.

### `src/platform/bouffalolab/common/ConfigurationManagerImpl.cpp`

Reboot-count and total-operational-hours persistence are disabled. They are not needed by this application and would cause unnecessary writes to the PSM flash partition. Matter credentials, fabric data, and Thread state remain persistent.

### `src/platform/bouffalolab/common/ThreadStackManagerImpl.cpp`

The actual Matter/OpenThread task publishes a liveness heartbeat after processing tasklets and radio events. The hood application wakes it once per second, allowing the hardware watchdog to detect a stuck Thread task even if Matter and the local button still run.

### `examples/hood-app/bouffalolab/bl702/BUILD.gn`

The Matter OTA Requestor is disabled because this 1 MiB firmware uses UART flashing and does not provide an OTA image slot. This also avoids reserving RAM for an unused feature.

## `patches/bouffalolab-components.patch`

### `network/thread/openthread_port/ot_sys_iot.c`

The `CFG_USE_PSRAM` check now works correctly even when the macro is undefined. This allows the XT-ZB2 configuration to build without PSRAM and without globally suppressing preprocessor warnings.

### `network/thread/openthread_port/ot_alarm.c`

OpenThread time now comes from the BL702's genuinely 64-bit machine timer (`bl_timer_now_us64`). The LMAC154 `zb_timer_get_current_time_us()` function has a 64-bit return type but reads a 32-bit symbol counter, so it wraps after about 19 hours. The millisecond alarm uses a static FreeRTOS timer, one queue command per restart, and bounded one-hour chunks for long deadlines. The 32-bit microsecond alarm still programs the LMAC154 hardware timer, using the machine timer for its current-time value. This keeps the OpenThread clocks continuous across the LMAC154 counter rollover.

### `platform/hosal/bl702_hal/bl_flash.c` and `bl_flash.h`

An API is added for registering a writable flash range. Boot2 protects the firmware and partition-table regions, but Matter's PSM region must remain writable by LittleFS. The range is obtained from MTD at runtime and accepted only if it is aligned, lies beyond the firmware/partition table, and fits in physical flash. Low-level erase, write, and read failures are propagated to LittleFS instead of always returning success.

### `stage/littlefs/port/lfs_xip_flash.c`

- The PSM region obtained from MTD is registered as writable.
- On BL702, the PSM location and size must match the checked-in 1 MiB layout. MTD and mutex-allocation failures are checked.
- Erase and write operations on the BL702 use the `*_need_lock` variants while interrupts are disabled.
- The actual flash error code is returned to the caller.
- A failed mount formats only an entirely erased partition. Nonblank corrupted storage is preserved; hold D27 during boot to erase it explicitly.

These changes fix the original LittleFS formatting failure (`format ret=-1`) and make persistent fabric and credential storage operational.

### `network/thread/openthread_port/ot_settings_littlefs.c`

Thread settings are written with `LFS_O_TRUNC`, and file, directory, and wipe-marker errors are checked. Reads report the actual stored length and do not conceal read failures with a successful close. Partial writes are reported as errors. A failed wipe leaves the marker for retry instead of silently reporting a successful reset.

## Changes that are no longer patched

The earlier development tree also changed the global build target list, Python builder, flashing helper, shared linker script, and debug boot banner. None of those changes are required by this standalone project:

- GN is invoked directly by `scripts/build.sh`.
- The linker script is stored with the application.
- `scripts/flash.sh` flashes the firmware with explicit parameters.
- Temporary LittleFS diagnostics have been removed.
