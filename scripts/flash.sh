#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_ROOT="${PROJECT_ROOT}/deps/bl_iot_sdk"
FIRMWARE="${PROJECT_ROOT}/out/hob2hood/chip-bl702-hood-example.bin"
PARTITION_TABLE="${PROJECT_ROOT}/config/partition_cfg_1M.toml"
BOOT2="${SDK_ROOT}/tools/flash_tool/chips/bl702/builtin_imgs/boot2_isp_bl702_v6.4_rc6/boot2_isp_release.bin"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    echo "Usage: $0 [serial-port]"
    echo "Example: $0 /dev/ttyUSB0"
    echo "Environment: BAUD=500000"
    exit 0
fi

PORT="${1:-${PORT:-/dev/ttyUSB0}}"
BAUD="${BAUD:-500000}"

case "$(uname -s):$(uname -m)" in
    Linux:x86_64)
        FLASH_TOOL="${SDK_ROOT}/tools/flash_tool/bflb_iot_tool-ubuntu"
        ;;
    Linux:armv7l | Linux:armv6l)
        FLASH_TOOL="${SDK_ROOT}/tools/flash_tool/bflb_iot_tool_Rasp"
        ;;
    Darwin:x86_64)
        FLASH_TOOL="${SDK_ROOT}/tools/flash_tool/bflb_iot_tool-macos"
        ;;
    Darwin:arm64)
        echo "error: run the x86_64 flash tool under Rosetta, or flash from x86_64 Linux" >&2
        exit 1
        ;;
    *)
        echo "error: no bundled Bouffalo flash tool for $(uname -s) $(uname -m)" >&2
        exit 1
        ;;
esac

for file in "${FLASH_TOOL}" "${FIRMWARE}" "${PARTITION_TABLE}" "${BOOT2}"; do
    if [[ ! -f "${file}" ]]; then
        echo "error: missing ${file}" >&2
        exit 1
    fi
done

FIRMWARE_SIZE="$(wc -c < "${FIRMWARE}" | tr -d ' ')"
FIRMWARE_LIMIT=$((0xE0000))
if (( FIRMWARE_SIZE > FIRMWARE_LIMIT )); then
    echo "error: firmware is ${FIRMWARE_SIZE} bytes, larger than the FW partition (${FIRMWARE_LIMIT} bytes)" >&2
    exit 1
fi

chmod +x "${FLASH_TOOL}"
echo "Flashing ${FIRMWARE} through ${PORT} at ${BAUD} baud..."
"${FLASH_TOOL}" \
    --chipname=bl702 \
    --interface=uart \
    --port="${PORT}" \
    --baudrate="${BAUD}" \
    --xtal=32M \
    --firmware="${FIRMWARE}" \
    --pt="${PARTITION_TABLE}" \
    --boot2="${BOOT2}"
