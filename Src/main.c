#include <stdint.h>

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

// ---- MPU-6050/6500 helpers (unchanged, already proven working) ----

uint32_t I2C_ReadRegister(uint8_t reg_addr)
{
	uint32_t value;
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0xD0;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0xD1;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 6)) == 0 ){}
	value = *(volatile uint32_t*) 0x40005410;
	return value;
}

void I2C_ReadMulti(uint8_t reg_addr, uint8_t *buffer, uint8_t len)
{
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0xD0;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0xD1;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005418;
	for (uint8_t i = 0; i < len; i++) {
		if (i == len - 1) {
			*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
			*(volatile uint32_t*) 0x40005400 |= (1 << 9);
		}
		while( ((*(volatile uint32_t*) 0x40005414) & (1 << 6)) == 0 ){}
		buffer[i] = *(volatile uint32_t*) 0x40005410;
	}
}

// ---- QMC5883L helpers (address 0x0D -> write byte 0x1A, read byte 0x1B) ----

void QMC_WriteRegister(uint8_t reg_addr, uint8_t value)
{
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0x1A;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = value;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
}

uint32_t QMC_ReadRegister(uint8_t reg_addr)
{
	uint32_t value;
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0x1A;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0x1B;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005400 |= (1<<9);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 6)) == 0 ){}
	value = *(volatile uint32_t*) 0x40005410;
	return value;
}

void QMC_ReadMulti(uint8_t reg_addr, uint8_t *buffer, uint8_t len)
{
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0x1A;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005418;
	*(volatile uint32_t*) 0x40005410 = reg_addr;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<8);
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}
	*(volatile uint32_t*) 0x40005410 = 0x1B;
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}
	*(volatile uint32_t*) 0x40005400 |= (1<<10);
	*(volatile uint32_t*) 0x40005418;
	for (uint8_t i = 0; i < len; i++) {
		if (i == len - 1) {
			*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);
			*(volatile uint32_t*) 0x40005400 |= (1 << 9);
		}
		while( ((*(volatile uint32_t*) 0x40005414) & (1 << 6)) == 0 ){}
		buffer[i] = *(volatile uint32_t*) 0x40005410;
	}
}

int main(void)
{
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

	*(volatile uint32_t*) 0x40023840 |= (1 << 21);
	*(volatile uint32_t*) 0x40023820 |= (1 << 21);
	for (volatile int d = 0; d < 10000; d++);
	*(volatile uint32_t*) 0x40023820 &= ~(1 << 21);

	*(volatile uint32_t*) 0x40005400 = 0;
	*(volatile uint32_t*) 0x40005408 = (1 << 14);
	*(volatile uint32_t*) 0x40005404 = 16;
	*(volatile uint32_t*) 0x4000541C = 80;
	*(volatile uint32_t*) 0x40005420 = 17;
	*(volatile uint32_t*) 0x40005400 |= (1 << 0);

	uint32_t who_am_i = I2C_ReadRegister(0x75);

	// Wake the MPU-6050/6500
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

	uint32_t pwr_check = I2C_ReadRegister(0x6B);

	for (volatile int d = 0; d < 20000; d++);

	uint8_t accel_raw[6];
	I2C_ReadMulti(0x3B, accel_raw, 6);
	int16_t accel_x = (int16_t)((accel_raw[0] << 8) | accel_raw[1]);
	int16_t accel_y = (int16_t)((accel_raw[2] << 8) | accel_raw[3]);
	int16_t accel_z = (int16_t)((accel_raw[4] << 8) | accel_raw[5]);

	uint8_t gyro_raw[6];
	I2C_ReadMulti(0x43, gyro_raw, 6);
	int16_t gyro_x = (int16_t)((gyro_raw[0] << 8) | gyro_raw[1]);
	int16_t gyro_y = (int16_t)((gyro_raw[2] << 8) | gyro_raw[3]);
	int16_t gyro_z = (int16_t)((gyro_raw[4] << 8) | gyro_raw[5]);

	//----------------------------------------------------
	// QMC5883L: Chip ID check first (register 0x0D, should read 0xFF)
	//----------------------------------------------------

	uint32_t chip_id = QMC_ReadRegister(0x0D);

	//----------------------------------------------------
	// QMC5883L: configure and wake into continuous mode
	//----------------------------------------------------

	QMC_WriteRegister(0x0B, 0x01);  // SET/RESET period — datasheet-recommended
	QMC_WriteRegister(0x09, 0x49);  // Control Reg 1: OSR=256, RNG=2G, ODR=100Hz, MODE=Continuous

	for (volatile int d = 0; d < 20000; d++);  // brief settle time after mode change

	uint32_t mode_check = QMC_ReadRegister(0x09);  // should read back as 0x49

	//----------------------------------------------------
	// QMC5883L: read magnetometer data
	// NOTE: register order is LSB-first (X_LSB, X_MSB, Y_LSB, Y_MSB, Z_LSB, Z_MSB)
	// — opposite of the MPU-6050/HMC5883L, which are MSB-first
	//----------------------------------------------------

	uint8_t mag_raw[6];
	QMC_ReadMulti(0x00, mag_raw, 6);

	int16_t mag_x = (int16_t)((mag_raw[1] << 8) | mag_raw[0]);
	int16_t mag_y = (int16_t)((mag_raw[3] << 8) | mag_raw[2]);
	int16_t mag_z = (int16_t)((mag_raw[5] << 8) | mag_raw[4]);

	/* Loop forever — check chip_id, mode_check, mag_x, mag_y, mag_z here */
	for(;;);
}
