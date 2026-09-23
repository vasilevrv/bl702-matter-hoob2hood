#!/usr/bin/env python3
"""Reject a linked image that still uses the wrapping LMAC154 clock for OpenThread."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def disassemble(objdump: Path, elf: Path, symbol: str) -> str:
    return subprocess.check_output(
        [str(objdump), "-d", f"--disassemble={symbol}", str(elf)],
        text=True,
    )


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: verify_firmware.py OBJDUMP ELF BIN")

    objdump, elf, firmware = map(Path, sys.argv[1:])
    for path in (objdump, elf, firmware):
        if not path.is_file():
            raise SystemExit(f"missing build artifact: {path}")

    for symbol in (
        "otPlatAlarmMilliStartAt",
        "otPlatAlarmMilliGetNow",
        "otPlatAlarmMicroStartAt",
        "otPlatAlarmMicroGetNow",
        "otPlatRadioGetNow",
    ):
        assembly = disassemble(objdump, elf, symbol)
        if "<bl_timer_now_us64>" not in assembly or "<zb_timer_get_current_time_us>" in assembly:
            raise SystemExit(f"{symbol} does not use the continuous 64-bit clock")

    assembly = disassemble(objdump, elf, "otSysProcessDrivers")
    if "<ot_watchdog_heartbeat>" not in assembly:
        raise SystemExit("OpenThread task does not update the watchdog heartbeat")

    firmware_size = firmware.stat().st_size
    if firmware_size > 0xE0000:
        raise SystemExit(f"firmware is too large for the 1 MiB partition: {firmware_size} bytes")

    print(f"Firmware checks passed: continuous Thread clock, Thread watchdog, {firmware_size} bytes")


if __name__ == "__main__":
    main()
