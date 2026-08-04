#include <stdint.h>
#include <math.h>

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

#define RAD_TO_DEG 57.29578f  // 180/pi -- converts atan2's radians output into degrees

// ---- FPU enable (Cortex-M4 FPU is OFF by default) ----

#define CPACR (*(volatile uint32_t*) 0xE000ED88)

void FPU_Enable(void)
{
	CPACR |= (0xF << 20);          // full access to CP10/CP11 -- the FPU's coprocessor slots
	__asm volatile ("dsb");        // wait for that write to actually complete
	__asm volatile ("isb");        // flush the pipeline so no stale float instruction sneaks through
}

// ---- SysTick (1ms system tick) ----

#define SYST_CSR  (*(volatile uint32_t*) 0xE000E010)
#define SYST_RVR  (*(volatile uint32_t*) 0xE000E014)
#define SYST_CVR  (*(volatile uint32_t*) 0xE000E018)

volatile uint32_t system_ticks_ms = 0;

void SysTick_Init(void)
{
	SYST_RVR = 15999;
	SYST_CVR = 0;
	SYST_CSR = (1 << 0) | (1 << 1) | (1 << 2);
}

void SysTick_Handler(void)
{
	system_ticks_ms++;
}

// ---- I2C1 timeout/recovery helpers ----
// The STM32F4 I2C1 peripheral can wedge (stuck waiting on a flag that never
// sets) if a new transaction starts too soon after the last one ends. We
// wait up to a large-but-finite limit instead of forever; if exceeded, we
// assume the peripheral is wedged and reset it from scratch.

#define I2C_TIMEOUT_LIMIT 100000

static uint8_t I2C_WaitSR1(uint32_t bit)
{
	uint32_t timeout = I2C_TIMEOUT_LIMIT;
	while ( ((*(volatile uint32_t*) 0x40005414) & bit) == 0 ) {
		if (--timeout == 0) return 0;
	}
	return 1;
}

static void I2C1_Recover(void)
{
	*(volatile uint32_t*) 0x40023820 |= (1 << 21);
	for (volatile int d = 0; d < 10000; d++);
	*(volatile uint32_t*) 0x40023820 &= ~(1 << 21);

	*(volatile uint32_t*) 0x40005400 = 0;
	*(volatile uint32_t*) 0x40005408 = (1 << 14);
	*(volatile uint32_t*) 0x40005404 = 16;
	*(volatile uint32_t*) 0x4000541C = 80;
	*(volatile uint32_t*) 0x40005420 = 17;
	*(volatile uint32_t*) 0x40005400 |= (1 << 0);
}

static void I2C_BusFreeDelay(void)
{
	for (volatile int d = 0; d < 1000; d++);  // required bus-free gap after STOP, before next START
}

// ---- LED helpers (LD2, onboard green LED, PA5) ----

void LED_Init(void)
{
	*(volatile uint32_t*) 0x40023830 |= (1 << 0);
	*(volatile uint32_t*) 0x40020000 &= ~(3 << 10);
	*(volatile uint32_t*) 0x40020000 |=  (1 << 10);
}

void LED_On(void)  { *(volatile uint32_t*) 0x40020014 |=  (1 << 5); }
void LED_Off(void) { *(volatile uint32_t*) 0x40020014 &= ~(1 << 5); }

void LED_Blink_Error(uint8_t count)
{
	for (;;) {
		for (uint8_t i = 0; i < count; i++) {
			LED_On();
			for (volatile int d = 0; d < 500000; d++);
			LED_Off();
			for (volatile int d = 0; d < 500000; d++);
		}
		for (volatile int d = 0; d < 2000000; d++);
	}
}

// ---- MPU-6050/6500 helpers (timeout + recovery) ----

uint32_t I2C_ReadRegister(uint8_t reg_addr)
{
	uint32_t value = 0;
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005410 = 0xD0;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	if (!I2C_WaitSR1(1 << 2)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005410 = 0xD1;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
	if (!I2C_WaitSR1(1 << 6)) { I2C1_Recover(); return 0; }
	value = *(volatile uint32_t*) 0x40005410;
	while( (*(volatile uint32_t*) 0x40005400) & (1 << 9) ){}
	I2C_BusFreeDelay();
	return value;
}

void I2C_ReadMulti(uint8_t reg_addr, uint8_t *buffer, uint8_t len)
{
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005410 = 0xD0;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	if (!I2C_WaitSR1(1 << 2)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005410 = 0xD1;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005418;
	for (uint8_t i = 0; i < len; i++) {
		if (i == len - 1) {
			*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
			*(volatile uint32_t*) 0x40005400 |= (1 << 9);
		}
		if (!I2C_WaitSR1(1 << 6)) { I2C1_Recover(); return; }
		buffer[i] = *(volatile uint32_t*) 0x40005410;
	}
	while( (*(volatile uint32_t*) 0x40005400) & (1 << 9) ){}
	I2C_BusFreeDelay();
}

// ---- QMC5883L helpers (timeout + recovery) ----

void QMC_WriteRegister(uint8_t reg_addr, uint8_t value)
{
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005410 = 0x1A;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	if (!I2C_WaitSR1(1 << 2)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005410 = value;
	if (!I2C_WaitSR1(1 << 2)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
	while( (*(volatile uint32_t*) 0x40005400) & (1 << 9) ){}
	I2C_BusFreeDelay();
}

uint32_t QMC_ReadRegister(uint8_t reg_addr)
{
	uint32_t value = 0;
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005410 = 0x1A;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	if (!I2C_WaitSR1(1 << 2)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005410 = 0x1B;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return 0; }
	*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
	if (!I2C_WaitSR1(1 << 6)) { I2C1_Recover(); return 0; }
	value = *(volatile uint32_t*) 0x40005410;
	while( (*(volatile uint32_t*) 0x40005400) & (1 << 9) ){}
	I2C_BusFreeDelay();
	return value;
}

void QMC_ReadMulti(uint8_t reg_addr, uint8_t *buffer, uint8_t len)
{
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005410 = 0x1A;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	if (!I2C_WaitSR1(1 << 2)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	if (!I2C_WaitSR1(1 << 0)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005410 = 0x1B;
	if (!I2C_WaitSR1(1 << 1)) { I2C1_Recover(); return; }
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005418;
	for (uint8_t i = 0; i < len; i++) {
		if (i == len - 1) {
			*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
			*(volatile uint32_t*) 0x40005400 |= (1 << 9);
		}
		if (!I2C_WaitSR1(1 << 6)) { I2C1_Recover(); return; }
		buffer[i] = *(volatile uint32_t*) 0x40005410;
	}
	while( (*(volatile uint32_t*) 0x40005400) & (1 << 9) ){}
	I2C_BusFreeDelay();
}

int main(void)
{
	FPU_Enable();   // MUST be first -- enables float hardware before any float math runs below

	// ---- GPIOB clock + pin config for I2C1 (PB8=SCL, PB9=SDA) ----
	*(volatile uint32_t*) 0x40023830 |= (1 << 1);
	*(volatile uint32_t*) 0x40020400 &= ~((3 << 16) | (3 << 18));
	*(volatile uint32_t*) 0x40020400 |=  ((2 << 16) | (2 << 18));
	*(volatile uint32_t*) 0x40020404 |= (1 << 8);
	*(volatile uint32_t*) 0x40020404 |= (1 << 9);
	*(volatile uint32_t*) 0x40020408 &= ~((3 << 16) | (3 << 18));
	*(volatile uint32_t*) 0x40020408 |=  ((1 << 16) | (1 << 18));
	*(volatile uint32_t*) 0x4002040C &= ~((3 << 16) | (3 << 18));
	*(volatile uint32_t*) 0x40020424 &= ~((0xF << 0) | (0xF << 4));
	*(volatile uint32_t*) 0x40020424 |=  (4 << 0);
	*(volatile uint32_t*) 0x40020424 |=  (4 << 4);

	// ---- I2C1 clock enable + peripheral reset ----
	*(volatile uint32_t*) 0x40023840 |= (1 << 21);
	*(volatile uint32_t*) 0x40023820 |= (1 << 21);
	for (volatile int d = 0; d < 10000; d++);
	*(volatile uint32_t*) 0x40023820 &= ~(1 << 21);

	// ---- I2C1 peripheral configuration ----
	*(volatile uint32_t*) 0x40005400 = 0;
	*(volatile uint32_t*) 0x40005408 = (1 << 14);
	*(volatile uint32_t*) 0x40005404 = 16;
	*(volatile uint32_t*) 0x4000541C = 80;
	*(volatile uint32_t*) 0x40005420 = 17;
	*(volatile uint32_t*) 0x40005400 |= (1 << 0);

	// ---- SysTick: start the 1ms system clock ----
	SysTick_Init();

	// ---- Startup self-test ----
	LED_Init();

	uint8_t mpu_ok = (I2C_ReadRegister(0x75) == 0x70);  // 0x70 -- this board's actual chip is an MPU-6500, verified via logic analyzer
	uint8_t qmc_ok = (QMC_ReadRegister(0x0D) == 0xFF);

	if (!mpu_ok && !qmc_ok) {
		LED_Blink_Error(3);
	} else if (!mpu_ok) {
		LED_Blink_Error(1);
	} else if (!qmc_ok) {
		LED_Blink_Error(2);
	}

	// ---- Wake the MPU-6050/6500 ----
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0xD0;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = 0x6B;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0x01;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
	while( (*(volatile uint32_t*) 0x40005400) & (1 << 9) ){}
	I2C_BusFreeDelay();

	for (volatile int d = 0; d < 20000; d++);

	// ---- QMC5883L: configure and wake into continuous mode ----
	QMC_WriteRegister(0x0B, 0x01);
	QMC_WriteRegister(0x09, 0x49);

	for (volatile int d = 0; d < 20000; d++);

	// ================= Complementary filter setup =================

	float alpha = 0.98f;  // trust gyro's short-term smoothness; let accel slowly correct drift

	uint8_t accel_raw[6];
	I2C_ReadMulti(0x3B, accel_raw, 6);
	int16_t accel_x = (int16_t)((accel_raw[0] << 8) | accel_raw[1]);
	int16_t accel_z = (int16_t)((accel_raw[4] << 8) | accel_raw[5]);
	float angle = atan2f((float)accel_x, (float)accel_z) * RAD_TO_DEG;

	uint32_t last_ticks = system_ticks_ms;

	// ================= Main filter loop =================
	for (;;) {
		uint32_t now_ticks = system_ticks_ms;
		uint32_t elapsed_ms = now_ticks - last_ticks;
		last_ticks = now_ticks;
		float dt = elapsed_ms / 1000.0f;

		I2C_ReadMulti(0x3B, accel_raw, 6);
		accel_x = (int16_t)((accel_raw[0] << 8) | accel_raw[1]);
		accel_z = (int16_t)((accel_raw[4] << 8) | accel_raw[5]);
		float accel_angle = atan2f((float)accel_x, (float)accel_z) * RAD_TO_DEG;

		uint8_t gyro_raw[6];
		I2C_ReadMulti(0x43, gyro_raw, 6);
		int16_t gyro_x = (int16_t)((gyro_raw[0] << 8) | gyro_raw[1]);
		float gyro_rate_dps = gyro_x / 131.0f;

		angle = alpha * (angle + gyro_rate_dps * dt) + (1.0f - alpha) * accel_angle;

		// angle now holds the current roll estimate, in degrees.
		// Set a breakpoint below and watch it in Expressions while tilting the board.
		(void)angle;
	}
}
