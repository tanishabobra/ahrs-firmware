# AHRS Firmware: STM32F446RE

A bare-metal orientation sensor, the same kind of system used in drones, rockets, and
spacecraft to figure out which way something is pointing in real time. It reads a
motion sensor and a digital compass, fuses the two together with a Kalman filter to
produce a single stable orientation estimate, and streams that live to a 3D
visualization on a laptop.

Everything is written directly against the STM32F446 reference manual (RM0390), with no
HAL, no CubeMX-generated code, and no vendor libraries. Every peripheral (I2C, UART,
interrupts, the SysTick clock) is configured by hand at the register level.

## What it does

- Reads a 6-axis IMU (MPU-6500) and 3-axis magnetometer (QMC5883L) over I2C1
- Fuses both through a hand-written 6-state Multiplicative EKF (attitude error + gyro
  bias) with tilt-compensated magnetometer heading, into a live quaternion orientation
  estimate
- Parses NMEA sentences from a NEO-M8N GPS module over an interrupt-driven UART
- Streams the live orientation quaternion over a second UART (115200 baud, sent every
  20 ms) to a Python script that renders it as a rotating 3D cube in real time
- Runs a startup self-test on both sensors and reports failures via an LED blink code

## Hardware

- STM32 Nucleo-F446RE
- MPU-6500 accel/gyro: I2C1, SCL→PB8, SDA→PB9
- QMC5883L magnetometer: same I2C1 bus
- NEO-M8N GPS: USART6, PC6 (TX) / PC7 (RX), 9600 baud

## Architecture

```
IMU + Mag (I2C1) → 6-state MEKF → quaternion → UART telemetry → Python 3D visualizer
GPS (USART6, interrupt-driven) → NMEA parser → lat/lon/alt (standalone, not fused into the EKF)
```

The filter runs once per main-loop iteration, with `dt` measured from a 1 ms SysTick
counter. Telemetry is rate-limited separately to one quaternion every 20 ms.

## Bug log

**Two dead sensor boards.** The original MPU-6050 breakout and the original magnetometer
breakout (silkscreened "HMC5883L") were both dead. Before replacing them, every firmware
and wiring cause was ruled out: wiring continuity, pull-up voltages, AD0, GPIO speed,
peripheral reset, and full 0x08–0x77 I2C address scans. For the IMU board, a logic
analyzer capture (fx2lafw, PulseView, 1 MHz) showed correctly formed I2C traffic with a
consistent NACK. For the magnetometer board, a fully software bit-banged I2C
implementation, bypassing the hardware peripheral entirely, still got no response. Both
replacement boards worked immediately with identical code.

**WHO_AM_I register address.** Misread the MPU datasheet table ("75 hex = 117 decimal")
and coded the register address as 0x4B instead of 0x75. The bug stayed hidden while the
first board was dead and only surfaced once a working board arrived. Also: the chip
identifies as MPU-6500 (WHO_AM_I = 0x70), not MPU-6050 (0x68), despite the board's
silkscreen.

**I2C1 reset needs a real delay.** Asserting RCC_APB1RSTR and immediately clearing it is
a no-op. A delay between the two is needed for the reset to take effect.

**I2C1 wedges under rapid back-to-back transactions.** Polling accel→gyro→mag every loop
iteration with no settle time after STOP could lock the peripheral. Fixed with a
bounded-timeout wait on every status flag, a full reset-and-recover path, and a short
delay after STOP. Separately: interrupted debug sessions (breakpoint hit mid-transaction,
then Terminate) can also wedge I2C1 in a way a soft reset doesn't clear. Only a full USB
power cycle recovers it.

**HardFault with chained libm calls (root cause unconfirmed).** After tilt compensation
added a chain of `sinf`/`cosf`/`atan2f` calls, the firmware began HardFaulting with a
UsageFault (NOCP). The FPU was already enabled (CP10/CP11 set in CPACR, followed by
DSB/ISB) at the top of `main()`, so the usual cause of NOCP doesn't fit. Build:
arm-none-eabi-gcc, -O0, --specs=nano.specs. In a minimal repro (FPU + LED + math only, no
sensors), one libm call per expression ran cleanly and two in the same expression
faulted. Workaround: every libm call is split onto its own line into its own variable,
and the stack was raised to 0x1000 in the linker script. Stack overflow is the leading
suspect, but it was never confirmed with an A/B test, so this is documented as a
workaround, not a proven fix.

**GPS silently routed to the ST-LINK VCP.** On the Nucleo-64, USART2's default pins
(PA2/PA3) are wired to the ST-LINK virtual COM port through solder bridges SB13/SB14,
not to the D0/D1 header pins (that requires SB62/SB63, which ship open). A loopback test
on those pins gave clean status-register reads with zero errors, which looked like a
receive config bug but was actually a disconnected pin. Confirmed against the Nucleo-64
user manual (UM1724). Moved GPS to USART6 (PC6/PC7), which ST-LINK routing doesn't
affect.

**Wiring fault indistinguishable from a config fault at the register level.** After
moving to USART6, loopback still failed identically, even though GPIOC MODER and AFRL
read back the expected AF8 configuration in the debugger. Isolated the real fault by
bypassing USART entirely: drove PC6 as plain GPIO and read PC7's IDR directly. Found the
jumper had landed on the wrong header row. Register inspection alone could never have
caught this, since the registers were telling the truth about a peripheral that was
configured correctly but not physically connected.

**UART RX overrun from polling too slowly.** Once the physical and peripheral layers were
both confirmed correct, real GPS sentences were still decoding garbled (stray `$`
characters mid-buffer). Confirmed via the overrun (ORE) flag in USART6_SR: polling for a
received byte once per main-loop iteration was too infrequent once I2C sensor reads were
also in the loop, so the single-byte receive register was overwritten before being read.
Fixed by switching to interrupt-driven RX (`USART6_IRQHandler`, enabled via NVIC_ISER2)
into a 128-byte ring buffer, decoupling GPS reception from main-loop timing.

**Hardening: RCC clock-enable ordering.** Each peripheral clock-enable write in the GPS
UART init is followed by a dummy read of the same RCC register, so the enable completes
before any dependent configuration is written. This was added while chasing the GPS
silence. It is kept as defensive practice, but on its own it did not change the failing
result, so it is not counted as a root cause.

## Build / flash

STM32CubeIDE, no HAL. Build, then flash via Run or Debug with the board connected over
ST-LINK USB.

## Live 3D visualizer

The orientation quaternion streams over USART2 (PA2) at 115200 baud, which rides the same
USB cable as the ST-LINK VCP, so no extra wiring is needed.

```bash
pip install pyserial numpy matplotlib
python3 ahrs_visualizer.py [serial_port]
```

If no port is given, the script lists available serial ports and asks which one to use.
