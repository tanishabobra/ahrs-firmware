# AHRS Firmware — STM32F446RE

A bare-metal orientation sensor — the same kind of system used in drones, rockets, and
spacecraft to figure out exactly which way something is pointing in real time. It reads
a motion sensor and a digital compass, fuses the two together with a Kalman filter to
produce a single stable orientation estimate, and streams that live to a 3D visualization
on a laptop.

Everything is written directly against the STM32F446 reference manual (RM0390) — no
HAL, no CubeMX-generated code, no vendor libraries. Every peripheral (I2C, UART,
interrupts, the SysTick clock) is configured by hand at the register level.

## What it does

- Reads a 6-axis IMU (MPU-6500) and 3-axis magnetometer (QMC5883L) over I2C1
- Fuses both through a hand-written 6-state Multiplicative EKF (quaternion + gyro bias)
  into a live orientation estimate
- Parses NMEA sentences from a NEO-M8N GPS module over an interrupt-driven UART
- Streams the live orientation quaternion over a second UART to a Python script that
  renders it as a rotating 3D cube in real time
- Runs a startup self-test on both sensors, reports failures via an LED blink code

## Hardware

- STM32 Nucleo-F446RE
- MPU-6500 accel/gyro — I2C1, SCL→PB8, SDA→PB9
- QMC5883L magnetometer — same I2C1 bus
- NEO-M8N GPS — USART6, PC6 (TX) / PC7 (RX), 9600 baud

## Architecture

```
IMU + Mag (I2C1) → 6-state MEKF → quaternion → UART telemetry → Python 3D visualizer
GPS (USART6, interrupt-driven) → NMEA parser → lat/lon/alt (standalone, not fused into the EKF)
```

## Bug log

**Two dead sensor boards.** Original MPU-6050 breakout and original magnetometer
breakout were both dead. Confirmed via wiring checks, pull-up verification, I2C address
scans, and for the magnetometer, a full bit-banged software I2C implementation
bypassing the hardware peripheral entirely. Replaced both.

**WHO_AM_I mismatch.** Misread the MPU datasheet table entry as decimal instead of hex —
expected 0x4B, actual register value is 0x75. Also: the chip identifies as MPU-6500
(WHO_AM_I = 0x70), not MPU-6050 (0x68), despite the board's silkscreen.

**I2C1 reset needs a real delay.** RCC_APB1RSTR assert immediately followed by clear is
a no-op — needs a delay between the two or the reset doesn't take effect.

**I2C1 wedges under rapid back-to-back transactions.** Polling accel→gyro→mag every
loop iteration with no settle time after STOP could lock the peripheral. Fixed with a
bounded-timeout wait on every status flag, a full reset-and-recover path, and a short
delay after STOP. Separately: interrupted debug sessions (breakpoint hit mid-transaction,
then Terminate) can also wedge I2C1 in a way a soft reset doesn't clear — needs a full
USB power cycle to recover.

**libm toolchain bug.** On this arm-none-eabi-gcc build at -O0 with --specs=nano.specs,
combining two or more libm float calls (sinf, cosf, atan2f) in a single C expression
corrupts FPU state and HardFaults. Isolated with a minimal repro (FPU + LED + math only,
no sensors) — one call in an expression always worked, two never did. Fix: split every
libm call onto its own line into its own variable.

**GPS silently routed to the ST-LINK VCP.** USART2 on PA2/PA3 — the Nucleo's default
pins for that peripheral — are, by default board config, wired to the ST-LINK virtual COM
port via solder bridges SB13/SB14, not to the D0/D1 header pins (that requires SB62/SB63,
which ship open). A loopback test on those pins gave clean status-register reads with
zero errors, which looked like a receive config bug but was actually total disconnection.
Confirmed against the Nucleo-64 user manual. Moved GPS to USART6 (PC6/PC7), unaffected
by ST-LINK routing.

**Wiring fault indistinguishable from a config fault at the register level.** After
moving to USART6, loopback still failed identically, even though MODER/AFRL/clock
registers all read back correct. Isolated the real fault by bypassing USART entirely —
drove one pin as plain GPIO, read the other pin's IDR directly. Found the jumper had
landed on the wrong header row. No amount of register inspection would have caught this,
since the registers were telling the truth about a peripheral that was configured
correctly but wasn't physically connected to anything.

**RCC clock-enable timing race.** A register write immediately following a peripheral
clock-enable write could execute before the clock domain was actually active, and get
silently dropped. Fixed by adding a dummy read of the enable register right after each
clock-enable write, forcing the write to land before any dependent config proceeds.

**UART RX overrun from polling too slowly.** Once the physical and peripheral layers
were both confirmed correct, real GPS sentences were still decoding garbled. Root-caused
via the USART overrun (ORE) flag: polling for a received byte once per main loop
iteration was too infrequent once I2C sensor reads were also in the loop, so the
single-byte hardware receive register was getting overwritten before being read. Fixed
by switching to interrupt-driven RX into a 128-byte ring buffer, decoupling GPS reception
from main loop timing entirely.

## Build / flash

STM32CubeIDE, no HAL. Build, then flash via Run or Debug with the board on ST-LINK USB.

## Live 3D visualizer

Orientation quaternion streams over USART2 (PA2), which rides the same USB cable as the
ST-LINK VCP

```bash
pip install pyserial numpy matplotlib
python3 ahrs_visualizer.py
```
