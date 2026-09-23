#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${PROJECT_ROOT}/scripts/bootstrap.sh"

CHIP_ROOT="${PROJECT_ROOT}/deps/connectedhomeip"
SDK_ROOT="${PROJECT_ROOT}/deps/bl_iot_sdk"
APP_ROOT="${CHIP_ROOT}/examples/hood-app/bouffalolab/bl702"
OUT_DIR="${PROJECT_ROOT}/out/hob2hood"

case "$(uname -s):$(uname -m)" in
    Linux:x86_64 | Darwin:x86_64)
        ;;
    Darwin:arm64)
        echo "error: this pinned Bouffalo SDK contains an x86_64 macOS toolchain; run this build under Rosetta or use x86_64 Linux" >&2
        exit 1
        ;;
    *)
        echo "error: supported build hosts are x86_64 Linux and Intel macOS" >&2
        exit 1
        ;;
esac

mkdir -p "${OUT_DIR}"

cd "${CHIP_ROOT}"
# Matter's activation script supplies the pinned GN/Ninja/Python environment.
# shellcheck disable=SC1091
set +u
source scripts/activate.sh -p bouffalolab
set -u

ZAP_FILE="${CHIP_ROOT}/examples/hood-app/hood-common/hood-app.zap"
MATTER_FILE="${CHIP_ROOT}/examples/hood-app/hood-common/hood-app.matter"
python3 scripts/tools/zap/generate.py "${ZAP_FILE}"
if [[ ! -f "${MATTER_FILE}" ]]; then
    echo "error: ZAP generation finished without ${MATTER_FILE}" >&2
    exit 1
fi

GN_ARGS="
custom_toolchain=\"${CHIP_ROOT}/config/bouffalolab/toolchain:riscv_gcc\"
board=\"BL706DK\"
baudrate=\"115200\"
module_type=\"BL706C-22\"
chip_enable_ethernet=false
chip_enable_wifi=false
chip_enable_openthread=true
chip_config_network_layer_ble=true
chip_mdns=\"platform\"
chip_inet_config_enable_ipv4=false
bouffalo_sdk_component_easyflash_enabled=false
chip_system_config_use_openthread_inet_endpoints=true
chip_with_lwip=false
openthread_project_core_config_file=\"openthread-core-proj-config.h\"
chip_openthread_ftd=false
openthread_package_version=\"7e32165be\"
openthread_root=\"//third_party/connectedhomeip/third_party/bouffalolab/repo/components/network/thread/openthread\"
enable_heap_monitoring=false
chip_generate_link_map_file=true
bouffalolab_sdk_root=\"${SDK_ROOT}\"
"

gn gen --check --fail-on-unused-args --add-export-compile-commands='*' \
    --root="${APP_ROOT}" --args="${GN_ARGS}" "${OUT_DIR}"
ninja -C "${OUT_DIR}"

FIRMWARE="${OUT_DIR}/chip-bl702-hood-example.bin"
if [[ ! -f "${FIRMWARE}" ]]; then
    echo "error: build finished without ${FIRMWARE}" >&2
    exit 1
fi

OBJDUMP="${SDK_ROOT}/toolchain/riscv/$(uname -s)/bin/riscv64-unknown-elf-objdump"
python3 "${PROJECT_ROOT}/scripts/verify_firmware.py" \
    "${OBJDUMP}" "${OUT_DIR}/chip-bl702-hood-example.out" "${FIRMWARE}"

echo
echo "Firmware: ${FIRMWARE}"
sha256sum "${FIRMWARE}" 2>/dev/null || shasum -a 256 "${FIRMWARE}"
