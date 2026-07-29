#include <stdint.h>

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

int main(void)
{
	// Enable GPIOB clock
	*(volatile uint32_t*) 0x40023830 |= (1 << 1);

	// PB8, PB9 -> Alternate Function mode
	*(volatile uint32_t*) 0x40020400 &= ~((3 << 16) | (3 << 18));
	*(volatile uint32_t*) 0x40020400 |=  ((2 << 16) | (2 << 18));

	// Open-drain
	*(volatile uint32_t*) 0x40020404 |= (1 << 8);
	*(volatile uint32_t*) 0x40020404 |= (1 << 9);

	// Medium speed
	*(volatile uint32_t*) 0x40020408 &= ~((3 << 16) | (3 << 18));
	*(volatile uint32_t*) 0x40020408 |=  ((1 << 16) | (1 << 18));

	// No pull-up / pull-down
	*(volatile uint32_t*) 0x4002040C &= ~((3 << 16) | (3 << 18));

	// AF4 for PB8/PB9
	*(volatile uint32_t*) 0x40020424 &= ~((0xF << 0) | (0xF << 4));
	*(volatile uint32_t*) 0x40020424 |=  (4 << 0);
	*(volatile uint32_t*) 0x40020424 |=  (4 << 4);

	// Enable I2C1 clock
	*(volatile uint32_t*) 0x40023840 |= (1 << 21);

	// Reset I2C1 — with a real delay between assert and release
	*(volatile uint32_t*) 0x40023820 |= (1 << 21);
	for (volatile int d = 0; d < 10000; d++);
	*(volatile uint32_t*) 0x40023820 &= ~(1 << 21);

	// Disable peripheral before configuration
	*(volatile uint32_t*) 0x40005400 = 0;

	// OAR1: bit 14 must always be kept at 1 per RM0390
	*(volatile uint32_t*) 0x40005408 = (1 << 14);

	// Configure I2C timing
	*(volatile uint32_t*) 0x40005404 = 16;      // CR2 = 16 MHz
	*(volatile uint32_t*) 0x4000541C = 80;      // CCR = 80 (100 kHz)
	*(volatile uint32_t*) 0x40005420 = 17;      // TRISE = 17

	// Enable I2C peripheral
	*(volatile uint32_t*) 0x40005400 |= (1 << 0);

	//----------------------------------------------------
	// WHO_AM_I read
	//----------------------------------------------------

	uint32_t who_am_i;

	*(volatile uint32_t*) 0x40005400 |= (1<<8);      // CR1: START
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}  // wait for SB
	*(volatile uint32_t*) 0x40005410 = 0xD0;         // DR: address+WRITE
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}  // wait for ADDR
	*(volatile uint32_t*) 0x40005418;                // read SR2, clears ADDR
	*(volatile uint32_t*) 0x40005410 = 0x75;         // DR: WHO_AM_I register pointer
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 2)) == 0 ){}  // wait for BTF
	*(volatile uint32_t*) 0x40005400 |= (1<<8);      // CR1: REPEATED START
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 0)) == 0 ){}  // wait for SB
	*(volatile uint32_t*) 0x40005410 = 0xD1;         // DR: address+READ
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 1)) == 0 ){}  // wait for ADDR
	*(volatile uint32_t*) 0x40005400 &= ~(1 << 10);  // CR1: clear ACK (NACK next byte)
	*(volatile uint32_t*) 0x40005418;                // read SR2, clears ADDR
	*(volatile uint32_t*) 0x40005400 |= (1<<9);      // CR1: STOP
	while( ((*(volatile uint32_t*) 0x40005414) & (1 << 6)) == 0 ){}  // wait for RxNE
	who_am_i = *(volatile uint32_t*) 0x40005410;     // read the actual WHO_AM_I byte

	/* Loop forever — check who_am_i in the debugger here */
	for(;;);
}
