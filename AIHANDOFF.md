# AI HANDOFF — Sunmi V1s-G (w5920) kernel port

## Goal
Port 3.18.19 MT6580 kernel to Sunmi V1s-G cash register (board `rlk6580_we_c_m`, Android 6.0).
Repo: `/home/user/android_kernel_sunmi_w5920` (fork `Ssss4562/android_kernel_sunmi_w5920`, branch `master`).
Base: Infinix Hot 2 / Wiko Lenny 3 MT6580 tree. Toolchain: `~/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/`.
Build: `export ARCH=arm CROSS_COMPILE=~/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-`,
then `make O=/home/user/w5920_out HOSTCFLAGS="-fcommon" CC="${CROSS_COMPILE}gcc" -j$(nproc) zImage`
(HOSTCFLAGS mandatory — host dtc fails without `-fcommon`). Boot test image:
`magiskboot repack` with our `zImage` + `w5920.dtb` + stock ramdisk → `fastboot boot`.
Stock firmware: `~/Sunmi-V1s-G/` (boot.img, lk.bin, scatter). Device: adb, Magisk root on flashed stock.

## Hardware facts (verified live, via su on stock)
- SoC MT6580, kernel 3.18.19. Display 720x1280.
- LCM active: `xc_ili9881c_dsi_vdo_dijing_2` (`lcm=1-...` in cmdline). DTB lcmname `nt35590_AUO` (stale LK string).
- Touch: MStar `msg2xxx` on i2c-1 addr **0x5D** (DT), touch-data addr **0x26**, EINT12 (GPIO12), RST GPIO17, vtouch=VGP1/2.8V.
- Sensors (kxtj2/AP3426/ITG1010/AKM09911 in DT): **NOT populated/bound even on stock** — skip.
- Camera: main GC5025 only, no front. AF MAINAF @0-000c. Cam GPIOs 73/71/76/74, MCLK GPIO72.
- Audio: ext speaker amp enable = GPIO19. Keys: kpd map pos0=114/pos9=115 (tinno had them swapped).
- NFC pn54x @0x28 exists on stock (our defconfig has NFC off).

## Repo state (4 commits on top of origin/master)
1. `20ac769d` base: `w5920.dts`+`w5920_defconfig` (tinno-based), `mach/mt6580/w5920` DCT,
   `include/mt-plat/mt6580/w5920` cust power headers, `subStrobeInit` stub.
2. `b7d03f07` LCM `xc_ili9881c_dsi_vdo_dijing_2` (init table reversed from `lk.bin`,
   see `/tmp/opencode/lcm_extract/`). Display WORKS.
3. `7b282efc` touch msg2xxx (SN4S msg2238 base) + i2c-mt6580 fixes. Touch WORKS.
4. `7a11b9e6` speaker amp GPIO19 select. Sound WORKS (slight crackle).

## Critical findings (do not regress)
- **I2C >8B broken without two fixes** (`drivers/misc/mediatek/i2c/mt6580/`):
  `COMPATIBLE_WITH_AOSP` ccflag + auto-DMA fallback in `mt_i2c_start_xfer`
  (standalone builds lack the ALPS global flag; MTK DMA engine returns zeros here,
  controller auto-DMA buffer path is what works).
- **Touch DBBUS (0x62/0x59) is dead** (shut in stock fw?): chip-type detect always 0.
  Workaround in driver: trust DWI2C 0x26 ACK → force `CHIP_TYPE_MSG28XX`,
  mutual-cap variant, no auto-FW-update, no MP test. Firmware mode vars only
  exist for 26XXM/28XX (22xx has no mutual branch).
- **Never do extra DWI2C reads per IRQ** (not even debug): read-to-clear packets,
  an extra peek starves the handler (was root cause of "WRONG DEMO MODE HEADER").
- Touch needs `regulator_enable(vtouch)` (set_voltage alone leaves VGP1 off)
  + ~200ms settle. GT1151 must stay OFF (its probe resets the bus).
- `mt65xx_lcm_list.c` has a compile assert on empty LCM list; tinno defconfig's
  `hx8392a_dsi_cmd_3lane` never existed → keep a valid entry.
- tinno defconfigs never built upstream (missing cust headers, flashlight file);
  `CONFIG_TINNO_QUICK_CHARGING=y` required by v3702-derived `cust_charging.h`.
- WDT ~33s + obsolete `aee_wdt_irq_info()` BUG stub: any real hang destroys
  evidence (FIQ handler hits BUG before dumping). `/proc/last_kmsg` still useful.

## Known BAD (reverted, do not reapply blindly)
- **gc5025 camera port: REVERTED, causes boot hang/reboot loop.**
  After adding it, test images die ~30s after boot (WDT), even with touch disabled.
  Suspects: camera power/I2C probe path, mediaserver interaction. Reverted via
  `git checkout` of `kd_imgsensor.h`, `kd_sensorlist.*`, `camera_hw/kd_camera_hw.c`
  + deleted `gc5025_mipi_raw/` + defconfig IMGSENSOR back to tinno list.
  Donor kept: `/tmp/opencode/donors/motomtk` (gc5025 from speck99/nougat),
  `/tmp/opencode/donors/sn4s` (gc5005/gc030a/bf3905). Next attempt: separate
  branch, minimal port, watch for hangs.

## Open issues
- Sound crackle (works, distorts; digital path identical to stock, suspect analog gain).
- Touch contains debug printks + forced chip type (works, needs cleanup).
- ALSPS AP3426 driver missing (donor: elephone P8000 `ap3xx6_mtk`, needs 3.18 adapt).
- gc5035/gc5005/bf3905/gc030a not needed (no front cam on this unit).
- Other donors in `/tmp/opencode/donors/`: sn4s, p8000, lcm_collection, alcatel(3.10, ref only).

## Test loop
`make zImage` → `cat zImage w5920.dtb > zImage-dtb` → `magiskboot repack` →
`adb reboot bootloader` → `fastboot boot new-boot.img` → `adb wait-for-device`.
Flashed image (fastboot flash + TWRP Magisk) is old — test images go via `fastboot boot`.
`/proc/last_kmsg` readable with Magisk root on flashed image after a crash.
