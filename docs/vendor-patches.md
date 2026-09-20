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

Error handling is added when opening LittleFS configuration storage. Without it, a failed PSM initialization could access an invalid object and cause a hardware exception.

## `patches/bouffalolab-components.patch`

### `network/thread/openthread_port/ot_sys_iot.c`

The `CFG_USE_PSRAM` check now works correctly even when the macro is undefined. This allows the XT-ZB2 configuration to build without PSRAM and without globally suppressing preprocessor warnings.

### `platform/hosal/bl702_hal/bl_flash.c` and `bl_flash.h`

An API is added for registering a writable flash range. Boot2 protects the firmware and partition-table regions, but Matter's PSM region must remain writable by LittleFS. The range is obtained from MTD at runtime; address `0x1EA000` is not hard-coded.

### `stage/littlefs/port/lfs_xip_flash.c`

- The PSM region obtained from MTD is registered as writable.
- Erase and write operations on the BL702 use the `*_need_lock` variants while interrupts are disabled.
- The actual flash error code is returned to the caller.

These changes fix the original LittleFS formatting failure (`format ret=-1`) and make persistent fabric and credential storage operational.

## Changes that are no longer patched

The earlier development tree also changed the global build target list, Python builder, flashing helper, shared linker script, and debug boot banner. None of those changes are required by this standalone project:

- GN is invoked directly by `scripts/build.sh`.
- The linker script is stored with the application.
- `scripts/flash.sh` flashes the firmware with explicit parameters.
- Temporary LittleFS diagnostics have been removed.
