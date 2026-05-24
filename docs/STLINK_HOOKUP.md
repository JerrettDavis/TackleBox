# ST-Link Hookup Guide

## Scope

This guide is for hooking an ST-Link V2 to the current SKR 2 STM32F429 board used in this workspace.

It is intended to support:

- SWD flashing
- debugger-assisted bring-up
- repeatable hardware validation with `test/run_stlink_validation.ps1`
- extracting the resident bootloader before any overwrite

## First Rule: Back Up The Existing Bootloader

Before we write any experimental bootloader build, dump the existing boot region with ST-Link.

The current firmware layout reserves the first 32 KiB of flash for a resident bootloader:

- start address: `0x08000000`
- length: `0x00008000`
- application start: `0x08008000`

Backup command:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\backup_bootloader.ps1"
```

Restore command after a successful backup:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\restore_bootloader.ps1" -InputPath ".\artifacts\bootloader-backups\<your-backup>.bin"
```

Dry-run preview without hardware:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\backup_bootloader.ps1" -DryRun
```

The script writes the binary dump plus a SHA256 manifest under `artifacts\bootloader-backups` by default.

## Windows Driver Requirement

The current backup and restore scripts use OpenOCD over ST-Link.

On Windows, that means the ST-Link probe must be bound to a libusb-compatible driver.

If `backup_bootloader.ps1` reports `ProblemCode: 28`, Windows has not installed a usable driver for the probe yet.

Working fix path for these scripts:

1. Open Zadig as Administrator.
2. Select the device named `STM32 STLink`.
3. Install `WinUSB` or `libusbK` for that device.
4. Re-run `test\backup_bootloader.ps1`.

The scripts now preflight this and fail early with a clear error instead of waiting for OpenOCD to fail later.

## What To Hook Up

The STM32F429 uses standard SWD debug signals:

- `SWDIO` on MCU `PA13`
- `SWCLK` on MCU `PA14`
- `NRST` on the MCU reset net
- `3V3` as the target reference voltage
- `GND` as common ground

Important board-label note:

- do not expect the SKR 2 silkscreen to say `PA13`
- `PA13` is the MCU net name for the debug data pin, but the board is more likely to label that pin as `SWDIO` or `SWIO`
- `PA14` is the MCU net name for the debug clock pin, and the board is more likely to label that pin as `SWCLK`

The SKR 2 board definition already uses `stlink` as a supported upload and debug transport. The practical task is finding the correct on-board header and wiring those signals one-for-one.

## Board Location

On the SKR 2 top side, the SWD area is in the lower-right quadrant of the board:

- immediately to the right of the vertical USB-A socket
- above the `PS-ON` header area
- left of the TFT / BLTouch connector area
- near the silkscreen text `SWD`

Board-relative diagram:

```text
Top view, ports facing down

  +-------------------------------------------------------------+
  |                                                             |
  |                        STM32F429 MCU                         |
  |                                                             |
  |                                                  TFT/BLTouch|
  |                                             +---------------+
  |                                             |               |
  |                               SWD area ---> |  SWD header   |
  |                                +--------+   |  near "SWD"   |
  |                                | RGB    |   |               |
  |                                +--------+   +---------------+
  |                         +-----+                                 
  |                         |USB-A|                                 
  |                         +-----+  +------+ +-------------------+ 
  |                                   PS-ON   Thermistors / IO     | 
  +-------------------------------------------------------------+
```

Use the silkscreen `SWD` marking on the board as the anchor, not the photo alone.

## ST-Link V2 To Target Mapping

Match signals by name, not by wire color.

| ST-Link V2 lead | Target signal | STM32 net | Notes |
| --- | --- | --- | --- |
| `SWDIO` | `SWDIO` | `PA13` | Required |
| `SWCLK` | `SWCLK` | `PA14` | Required |
| `GND` | `GND` | Ground | Required |
| `3V3` | `3V3` | Target reference | Required for voltage reference, do not use `5V` |
| `RST` or `NRST` | `NRST` | Reset net | Optional but recommended |

If you cannot find `PA13`, look for one of these instead:

- `SWDIO`
- `SWIO`
- a debug-data pin on the small header next to the `SWD` silkscreen

If you find `PA14`, that is the SWD clock side, not the data side.

## Minimal Wiring

If you only want flashing and basic probe connectivity:

```text
ST-Link V2        SKR 2 target
-----------       ----------------
SWDIO     ------> SWDIO / PA13
SWCLK     ------> SWCLK / PA14
GND       ------> GND
3V3       ------> 3V3 target reference
```

Recommended full wiring:

```text
ST-Link V2        SKR 2 target
-----------       ----------------
SWDIO     ------> SWDIO / PA13
SWCLK     ------> SWCLK / PA14
GND       ------> GND
3V3       ------> 3V3 target reference
NRST      ------> NRST
```

## Hookup Checklist

1. Power the board off.
2. Find the header or pads marked `SWD` near the lower-right side of the board.
3. Identify `GND` first.
4. Identify the target `3V3` reference pin.
5. Identify `SWCLK` and then find the matching `SWDIO` or `SWIO` pin on that same small debug header.
6. Add `NRST` only if you find a pin that is clearly part of the SWD/debug header.
7. Ignore unrelated `RESET` markings on LCD, EXP, or panel headers.
8. Do not connect `5V` from the ST-Link.
9. After wiring, plug the ST-Link into the PC and then power the target board.

## Practical Verification Order

Once connected:

1. Dump the resident bootloader.
2. Run a direct SWD upload.
3. Let the board reboot and re-enumerate USB CDC.
4. Run the live regression if USB comes up.

Commands:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_stlink_validation.ps1"
```

Optional motion coverage:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_stlink_validation.ps1" -IncludeMotion
```

## Bench Notes

- The current firmware target is the `STM32F429VGT6` SKR 2 variant.
- The primary firmware environment is `skr2_f429_usb`.
- USB CDC is still used for runtime command and telemetry validation after SWD flashing.
- If TMC verification is still failing, expect motion and enable commands to be blocked with `reason=tmc_unverified` even though the board flashes successfully.

## If The Header Order Is Not Printed Clearly

Use this fallback rule:

- trust the `SWD` silkscreen location on the board
- match the target pins to the standard STM32 SWD signals above
- treat `SWDIO` or `SWIO` as the practical label for MCU pin `PA13`
- treat generic `RESET` markings elsewhere on the board as unrelated unless they are clearly on the debug header
- for a first attempt, use only `SWDIO`, `SWCLK`, `GND`, and `3V3`; `NRST` can be left disconnected
- verify `GND` and `3V3` with a meter before plugging the probe in

If you want, the next step after hookup is an SWD-assisted debug pass focused on:

- TMC UART pin toggling on `PE0`
- SDIO card-detect and init path
- reset/boot timing around USB CDC enumeration