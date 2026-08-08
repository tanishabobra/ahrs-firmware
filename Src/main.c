#include <stdint.h>
#include <math.h>

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

#define RAD_TO_DEG 57.29578f
#define DEG_TO_RAD 0.01745329f

#define CPACR (*(volatile uint32_t*) 0xE000ED88)

void FPU_Enable(void)
{
	CPACR |= (0xF << 20);
	__asm volatile ("dsb");
	__asm volatile ("isb");
}

void DebugCheckpoint(void)
{
	__asm volatile ("nop");
}

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
	for (volatile int d = 0; d < 1000; d++);
}

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

// ================= EKF: Multiplicative EKF (MEKF), 6-state =================

typedef struct {
	float q[4];
	float b[3];
	float P[6][6];
} EKF_State;

static void Mat6_Multiply(float A[6][6], float B[6][6], float out[6][6])
{
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			float sum = 0.0f;
			for (int k = 0; k < 6; k++) {
				sum = sum + A[i][k] * B[k][j];
			}
			out[i][j] = sum;
		}
	}
}

static void Mat6_Transpose(float A[6][6], float out[6][6])
{
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			out[j][i] = A[i][j];
		}
	}
}

static void Mat6_Add(float A[6][6], float B[6][6], float out[6][6])
{
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			out[i][j] = A[i][j] + B[i][j];
		}
	}
}

static void Quat_Multiply(const float a[4], const float b[4], float out[4])
{
	out[0] = a[0]*b[0] - a[1]*b[1] - a[2]*b[2] - a[3]*b[3];
	out[1] = a[0]*b[1] + a[1]*b[0] + a[2]*b[3] - a[3]*b[2];
	out[2] = a[0]*b[2] - a[1]*b[3] + a[2]*b[0] + a[3]*b[1];
	out[3] = a[0]*b[3] + a[1]*b[2] - a[2]*b[1] + a[3]*b[0];
}

static void Quat_Normalize(float q[4])
{
	float sumsq = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
	float mag = sqrtf(sumsq);
	q[0] = q[0] / mag;
	q[1] = q[1] / mag;
	q[2] = q[2] / mag;
	q[3] = q[3] / mag;
}

static void Quat_Conjugate(const float q[4], float out[4])
{
	out[0] =  q[0];
	out[1] = -q[1];
	out[2] = -q[2];
	out[3] = -q[3];
}

static void Quat_RotateVector(const float q[4], const float v[3], float out[3])
{
	float w = q[0], x = q[1], y = q[2], z = q[3];
	float vx = v[0], vy = v[1], vz = v[2];

	float tx = 2.0f * (y*vz - z*vy);
	float ty = 2.0f * (z*vx - x*vz);
	float tz = 2.0f * (x*vy - y*vx);

	out[0] = vx + w*tx + (y*tz - z*ty);
	out[1] = vy + w*ty + (z*tx - x*tz);
	out[2] = vz + w*tz + (x*ty - y*tx);
}

static uint8_t Mat3_Inverse(float A[3][3], float out[3][3])
{
	float a = A[0][0], b = A[0][1], c = A[0][2];
	float d = A[1][0], e = A[1][1], f = A[1][2];
	float g = A[2][0], h = A[2][1], i = A[2][2];

	float A11 =  (e*i - f*h);
	float A12 = -(d*i - f*g);
	float A13 =  (d*h - e*g);
	float A21 = -(b*i - c*h);
	float A22 =  (a*i - c*g);
	float A23 = -(a*h - b*g);
	float A31 =  (b*f - c*e);
	float A32 = -(a*f - c*d);
	float A33 =  (a*e - b*d);

	float det = a*A11 + b*A12 + c*A13;
	if (fabsf(det) < 1e-9f) return 0;

	float inv_det = 1.0f / det;

	out[0][0] = A11*inv_det; out[0][1] = A21*inv_det; out[0][2] = A31*inv_det;
	out[1][0] = A12*inv_det; out[1][1] = A22*inv_det; out[1][2] = A32*inv_det;
	out[2][0] = A13*inv_det; out[2][1] = A23*inv_det; out[2][2] = A33*inv_det;

	return 1;
}

void EKF_Init(EKF_State *ekf, float init_pitch_deg, float init_roll_deg)
{
	float pitch_rad = init_pitch_deg * DEG_TO_RAD;
	float roll_rad  = init_roll_deg  * DEG_TO_RAD;

	float cp = cosf(pitch_rad * 0.5f);
	float sp = sinf(pitch_rad * 0.5f);
	float cr = cosf(roll_rad * 0.5f);
	float sr = sinf(roll_rad * 0.5f);

	ekf->q[0] = cr*cp;
	ekf->q[1] = sr*cp;
	ekf->q[2] = cr*sp;
	ekf->q[3] = -sr*sp;
	Quat_Normalize(ekf->q);

	ekf->b[0] = 0.0f;
	ekf->b[1] = 0.0f;
	ekf->b[2] = 0.0f;

	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			ekf->P[i][j] = (i == j) ? 1.0f : 0.0f;
		}
	}
}

void EKF_Predict(EKF_State *ekf, float gyro_x, float gyro_y, float gyro_z, float dt)
{
	float wx = gyro_x - ekf->b[0];
	float wy = gyro_y - ekf->b[1];
	float wz = gyro_z - ekf->b[2];

	float half_dt = dt * 0.5f;
	float dq0 = -half_dt * (wx*ekf->q[1] + wy*ekf->q[2] + wz*ekf->q[3]);
	float dq1 =  half_dt * (wx*ekf->q[0] + wz*ekf->q[2] - wy*ekf->q[3]);
	float dq2 =  half_dt * (wy*ekf->q[0] - wz*ekf->q[1] + wx*ekf->q[3]);
	float dq3 =  half_dt * (wz*ekf->q[0] + wy*ekf->q[1] - wx*ekf->q[2]);

	ekf->q[0] = ekf->q[0] + dq0;
	ekf->q[1] = ekf->q[1] + dq1;
	ekf->q[2] = ekf->q[2] + dq2;
	ekf->q[3] = ekf->q[3] + dq3;
	Quat_Normalize(ekf->q);

	float F[6][6] = {0};
	F[0][0] = 1.0f;      F[0][1] = -dt*wz;   F[0][2] =  dt*wy;
	F[1][0] =  dt*wz;    F[1][1] = 1.0f;     F[1][2] = -dt*wx;
	F[2][0] = -dt*wy;    F[2][1] =  dt*wx;   F[2][2] = 1.0f;
	F[0][3] = -dt;  F[1][4] = -dt;  F[2][5] = -dt;
	F[3][3] = 1.0f; F[4][4] = 1.0f; F[5][5] = 1.0f;

	float gyro_noise = 0.0003f;
	float bias_noise = 0.00001f;
	float Q[6][6] = {0};
	Q[0][0] = gyro_noise; Q[1][1] = gyro_noise; Q[2][2] = gyro_noise;
	Q[3][3] = bias_noise; Q[4][4] = bias_noise; Q[5][5] = bias_noise;

	float Ft[6][6], FP[6][6], FPFt[6][6];
	Mat6_Transpose(F, Ft);
	Mat6_Multiply(F, ekf->P, FP);
	Mat6_Multiply(FP, Ft, FPFt);
	Mat6_Add(FPFt, Q, ekf->P);
}

static void EKF_UpdateVector(EKF_State *ekf, float meas_body[3], float ref_world[3], float noise)
{
	float q_conj[4];
	Quat_Conjugate(ekf->q, q_conj);
	float y_pred[3];
	Quat_RotateVector(q_conj, ref_world, y_pred);

	float innov[3];
	innov[0] = meas_body[0] - y_pred[0];
	innov[1] = meas_body[1] - y_pred[1];
	innov[2] = meas_body[2] - y_pred[2];

	float H[3][6] = {0};
	H[0][1] = -y_pred[2]; H[0][2] =  y_pred[1];
	H[1][0] =  y_pred[2]; H[1][2] = -y_pred[0];
	H[2][0] = -y_pred[1]; H[2][1] =  y_pred[0];

	float HP[3][6];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 6; j++) {
			float sum = 0.0f;
			for (int k = 0; k < 6; k++) {
				sum = sum + H[i][k] * ekf->P[k][j];
			}
			HP[i][j] = sum;
		}
	}
	float S[3][3];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			float sum = 0.0f;
			for (int k = 0; k < 6; k++) {
				sum = sum + HP[i][k] * H[j][k];
			}
			S[i][j] = sum;
		}
	}
	S[0][0] += noise;
	S[1][1] += noise;
	S[2][2] += noise;

	float Sinv[3][3];
	if (!Mat3_Inverse(S, Sinv)) return;

	float PHt[6][3];
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 3; j++) {
			float sum = 0.0f;
			for (int k = 0; k < 6; k++) {
				sum = sum + ekf->P[i][k] * H[j][k];
			}
			PHt[i][j] = sum;
		}
	}
	float K[6][3];
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 3; j++) {
			float sum = 0.0f;
			for (int k = 0; k < 3; k++) {
				sum = sum + PHt[i][k] * Sinv[k][j];
			}
			K[i][j] = sum;
		}
	}

	float dx[6];
	for (int i = 0; i < 6; i++) {
		float sum = 0.0f;
		sum = sum + K[i][0]*innov[0];
		sum = sum + K[i][1]*innov[1];
		sum = sum + K[i][2]*innov[2];
		dx[i] = sum;
	}

	float dq[4];
	dq[0] = 1.0f;
	dq[1] = 0.5f * dx[0];
	dq[2] = 0.5f * dx[1];
	dq[3] = 0.5f * dx[2];
	float dq_mag = sqrtf(dq[0]*dq[0] + dq[1]*dq[1] + dq[2]*dq[2] + dq[3]*dq[3]);
	dq[0] = dq[0] / dq_mag;
	dq[1] = dq[1] / dq_mag;
	dq[2] = dq[2] / dq_mag;
	dq[3] = dq[3] / dq_mag;

	float q_new[4];
	Quat_Multiply(dq, ekf->q, q_new);
	ekf->q[0] = q_new[0];
	ekf->q[1] = q_new[1];
	ekf->q[2] = q_new[2];
	ekf->q[3] = q_new[3];
	Quat_Normalize(ekf->q);

	ekf->b[0] += dx[3];
	ekf->b[1] += dx[4];
	ekf->b[2] += dx[5];

	float KH[6][6];
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			float sum = 0.0f;
			sum = sum + K[i][0]*H[0][j];
			sum = sum + K[i][1]*H[1][j];
			sum = sum + K[i][2]*H[2][j];
			KH[i][j] = sum;
		}
	}
	float I_KH[6][6];
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			float identity_val = (i == j) ? 1.0f : 0.0f;
			I_KH[i][j] = identity_val - KH[i][j];
		}
	}
	float P_new[6][6];
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			float sum = 0.0f;
			for (int k = 0; k < 6; k++) {
				sum = sum + I_KH[i][k] * ekf->P[k][j];
			}
			P_new[i][j] = sum;
		}
	}
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			ekf->P[i][j] = P_new[i][j];
		}
	}
}

void EKF_UpdateAccel(EKF_State *ekf, float ax, float ay, float az)
{
	float mag = sqrtf(ax*ax + ay*ay + az*az);
	if (mag < 1e-6f) return;
	float meas[3];
	meas[0] = ax / mag;
	meas[1] = ay / mag;
	meas[2] = az / mag;

	float ref[3] = {0.0f, 0.0f, 1.0f};
	float noise = 0.01f;

	EKF_UpdateVector(ekf, meas, ref, noise);
}

void EKF_UpdateMag(EKF_State *ekf, float mx, float my, float mz)
{
	float mag = sqrtf(mx*mx + my*my + mz*mz);
	if (mag < 1e-6f) return;
	float meas[3];
	meas[0] = mx / mag;
	meas[1] = my / mag;
	meas[2] = mz / mag;

	float ref[3] = {1.0f, 0.0f, 0.0f};
	float noise = 0.05f;

	EKF_UpdateVector(ekf, meas, ref, noise);
}

void Quat_ToEuler(const float q[4], float *pitch_deg, float *roll_deg, float *yaw_deg)
{
	float w = q[0], x = q[1], y = q[2], z = q[3];

	float roll_sin = 2.0f * (w*x + y*z);
	float roll_cos = 1.0f - 2.0f * (x*x + y*y);
	float roll_rad = atan2f(roll_sin, roll_cos);

	float pitch_sin = 2.0f * (w*y - z*x);
	if (pitch_sin > 1.0f) pitch_sin = 1.0f;
	if (pitch_sin < -1.0f) pitch_sin = -1.0f;
	float pitch_rad = asinf(pitch_sin);

	float yaw_sin = 2.0f * (w*z + x*y);
	float yaw_cos = 1.0f - 2.0f * (y*y + z*z);
	float yaw_rad = atan2f(yaw_sin, yaw_cos);

	*roll_deg  = roll_rad  * RAD_TO_DEG;
	*pitch_deg = pitch_rad * RAD_TO_DEG;
	*yaw_deg   = yaw_rad   * RAD_TO_DEG;
	if (*yaw_deg < 0.0f) *yaw_deg += 360.0f;
}

// ================= GPS UART (USART6, PC6=TX/CN10 pin4, PC7=RX/D9) + NMEA parsing =================

// Define this to compile in the physical-layer and UART peripheral diagnostics
// used to debug the PC6/PC7 wiring. Leave undefined for normal builds.
// #define GPS_WIRE_DIAGNOSTIC

#define USART6_SR   (*(volatile uint32_t*) 0x40011400)
#define USART6_DR   (*(volatile uint32_t*) 0x40011404)
#define USART6_BRR  (*(volatile uint32_t*) 0x40011408)
#define USART6_CR1  (*(volatile uint32_t*) 0x4001140C)

void GPS_UART_Init(void)
{
	*(volatile uint32_t*) 0x40023830 |= (1 << 2);   // RCC_AHB1ENR: GPIOCEN
	(void) *(volatile uint32_t*) 0x40023830;

	*(volatile uint32_t*) 0x40020800 &= ~((3u << 12) | (3u << 14));
	*(volatile uint32_t*) 0x40020800 |=  ((2u << 12) | (2u << 14));

	*(volatile uint32_t*) 0x40020808 &= ~((3u << 12) | (3u << 14));
	*(volatile uint32_t*) 0x40020808 |=  ((3u << 12) | (3u << 14));

	*(volatile uint32_t*) 0x40020820 &= ~((0xFu << 24) | (0xFu << 28));
	*(volatile uint32_t*) 0x40020820 |=  ((8u   << 24) | (8u   << 28));

	*(volatile uint32_t*) 0x40023844 |= (1 << 5);   // RCC_APB2ENR: USART6EN
	(void) *(volatile uint32_t*) 0x40023844;

	USART6_CR1 = 0;
	USART6_BRR = 0x683;         // 9600 baud @ 16MHz APB2
	USART6_CR1 |= (1 << 3);     // TE
	USART6_CR1 |= (1 << 2);     // RE
	USART6_CR1 |= (1 << 5);     // RXNEIE
	USART6_CR1 |= (1 << 13);    // UE, last

	*(volatile uint32_t*) 0xE000E108 |= (1 << 7);   // NVIC_ISER2 bit 7 = IRQ71 (USART6)
}

static uint8_t GPS_UART_ByteAvailable(void)
{
	return (USART6_SR & (1 << 5)) != 0;
}

static uint8_t GPS_UART_ReadByte(void)
{
	return (uint8_t)(USART6_DR & 0xFF);
}

static void GPS_UART_SendByte(uint8_t c)
{
	while (!(USART6_SR & (1 << 7))) {}
	USART6_DR = c;
}

static void GPS_UART_SendString(const char *s)
{
	while (*s) {
		GPS_UART_SendByte((uint8_t)*s);
		s++;
	}
}

#define GPS_RING_SIZE 128
static volatile uint8_t  gps_ring_buf[GPS_RING_SIZE];
static volatile uint16_t gps_ring_head = 0;
static volatile uint16_t gps_ring_tail = 0;

void USART6_IRQHandler(void)
{
	if (USART6_SR & (1 << 5)) {
		uint8_t c = (uint8_t)(USART6_DR & 0xFF);
		uint16_t next_head = (gps_ring_head + 1) % GPS_RING_SIZE;
		if (next_head != gps_ring_tail) {
			gps_ring_buf[gps_ring_head] = c;
			gps_ring_head = next_head;
		}
	}
}

static uint8_t GPS_Ring_ByteAvailable(void)
{
	return gps_ring_head != gps_ring_tail;
}

static uint8_t GPS_Ring_ReadByte(void)
{
	uint8_t c = gps_ring_buf[gps_ring_tail];
	gps_ring_tail = (gps_ring_tail + 1) % GPS_RING_SIZE;
	return c;
}

#ifdef GPS_WIRE_DIAGNOSTIC
void GPIO_LoopbackTest(void)
{
	*(volatile uint32_t*) 0x40020800 &= ~((3u << 12) | (3u << 14));
	*(volatile uint32_t*) 0x40020800 |=  (1u << 12);

	*(volatile uint32_t*) 0x40020814 |= (1 << 6);
	DebugCheckpoint();

	*(volatile uint32_t*) 0x40020814 &= ~(1 << 6);
	DebugCheckpoint();
}
#endif

#define NMEA_BUF_SIZE 96
static char nmea_buf[NMEA_BUF_SIZE];
static uint8_t nmea_idx = 0;
static volatile uint8_t nmea_sentence_ready = 0;

void GPS_UART_Poll(void)
{
	while (GPS_Ring_ByteAvailable()) {
		uint8_t c = GPS_Ring_ReadByte();
		if (c == '\n') {
			nmea_buf[nmea_idx] = '\0';
			nmea_sentence_ready = 1;
			nmea_idx = 0;
		} else if (c != '\r') {
			if (nmea_idx < NMEA_BUF_SIZE - 1) {
				nmea_buf[nmea_idx++] = c;
			} else {
				nmea_idx = 0;
			}
		}
	}
}

static uint8_t NMEA_GetField(const char *sentence, uint8_t field_index, char *out, uint8_t out_size)
{
	uint8_t current_field = 0;
	uint8_t out_pos = 0;
	const char *p = sentence;

	while (*p != '\0' && current_field < field_index) {
		if (*p == ',') current_field++;
		p++;
	}
	if (current_field != field_index) {
		out[0] = '\0';
		return 0;
	}

	while (*p != '\0' && *p != ',' && *p != '*' && out_pos < out_size - 1) {
		out[out_pos++] = *p++;
	}
	out[out_pos] = '\0';
	return 1;
}

static float NMEA_ParseFloat(const char *raw)
{
	uint8_t i = 0;
	uint8_t negative = 0;
	float whole = 0.0f;

	if (raw[i] == '-') { negative = 1; i++; }

	while (raw[i] != '\0' && raw[i] != '.') {
		whole = whole * 10.0f + (float)(raw[i] - '0');
		i++;
	}
	if (raw[i] == '.') {
		i++;
		float frac = 0.1f;
		while (raw[i] != '\0') {
			whole = whole + (float)(raw[i] - '0') * frac;
			frac = frac * 0.1f;
			i++;
		}
	}
	if (negative) whole = -whole;
	return whole;
}

static float NMEA_ToDecimalDegrees(const char *raw, char hemisphere)
{
	float whole = NMEA_ParseFloat(raw);

	int degrees_int = (int)(whole / 100.0f);
	float degrees_part = (float)degrees_int;
	float minutes_part = whole - degrees_part * 100.0f;
	float decimal_deg = degrees_part + (minutes_part / 60.0f);

	if (hemisphere == 'S' || hemisphere == 'W') {
		decimal_deg = -decimal_deg;
	}
	return decimal_deg;
}

static uint8_t NMEA_IsGGA(const char *sentence)
{
	if (sentence[0] != '$') return 0;
	return (sentence[3] == 'G' && sentence[4] == 'G' && sentence[5] == 'A');
}

typedef struct {
	float latitude;
	float longitude;
	float altitude_m;
	uint8_t fix_quality;
	uint8_t satellites;
	uint8_t valid;
} GPS_Fix;

void NMEA_ParseGGA(const char *sentence, GPS_Fix *fix)
{
	char field[16];

	NMEA_GetField(sentence, 6, field, sizeof(field));
	fix->fix_quality = (uint8_t)(field[0] - '0');
	fix->valid = (fix->fix_quality > 0);

	if (!fix->valid) return;

	char lat_raw[16], lat_hem[2];
	NMEA_GetField(sentence, 2, lat_raw, sizeof(lat_raw));
	NMEA_GetField(sentence, 3, lat_hem, sizeof(lat_hem));
	fix->latitude = NMEA_ToDecimalDegrees(lat_raw, lat_hem[0]);

	char lon_raw[16], lon_hem[2];
	NMEA_GetField(sentence, 4, lon_raw, sizeof(lon_raw));
	NMEA_GetField(sentence, 5, lon_hem, sizeof(lon_hem));
	fix->longitude = NMEA_ToDecimalDegrees(lon_raw, lon_hem[0]);

	char sat_raw[8];
	NMEA_GetField(sentence, 7, sat_raw, sizeof(sat_raw));
	fix->satellites = (uint8_t)NMEA_ParseFloat(sat_raw);

	char alt_raw[16];
	NMEA_GetField(sentence, 9, alt_raw, sizeof(alt_raw));
	fix->altitude_m = NMEA_ParseFloat(alt_raw);
}

int main(void)
{
	FPU_Enable();

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

	SysTick_Init();

	LED_Init();
	GPS_UART_Init();

#ifdef GPS_WIRE_DIAGNOSTIC
	GPIO_LoopbackTest();
	GPS_UART_Init();

	GPS_UART_SendByte('A');
	for (volatile int d = 0; d < 200000; d++);
	uint32_t diag_sr = USART6_SR;
	uint8_t diag_rxne = (diag_sr >> 5) & 1;
	uint8_t diag_rx_byte = diag_rxne ? (uint8_t)(USART6_DR & 0xFF) : 0;
	DebugCheckpoint();
#endif

	uint8_t mpu_ok = (I2C_ReadRegister(0x75) == 0x70);
	uint8_t qmc_ok = (QMC_ReadRegister(0x0D) == 0xFF);

	if (!mpu_ok && !qmc_ok) {
		LED_Blink_Error(3);
	} else if (!mpu_ok) {
		LED_Blink_Error(1);
	} else if (!qmc_ok) {
		LED_Blink_Error(2);
	}

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

	QMC_WriteRegister(0x0B, 0x01);
	QMC_WriteRegister(0x09, 0x49);

	for (volatile int d = 0; d < 20000; d++);

	uint8_t accel_raw[6];
	I2C_ReadMulti(0x3B, accel_raw, 6);
	int16_t accel_x = (int16_t)((accel_raw[0] << 8) | accel_raw[1]);
	int16_t accel_y = (int16_t)((accel_raw[2] << 8) | accel_raw[3]);
	int16_t accel_z = (int16_t)((accel_raw[4] << 8) | accel_raw[5]);
	float init_pitch = atan2f((float)accel_x, (float)accel_z) * RAD_TO_DEG;
	float init_roll  = atan2f((float)accel_y, (float)accel_z) * RAD_TO_DEG;

	EKF_State ekf;
	EKF_Init(&ekf, init_pitch, init_roll);

	GPS_Fix gps_fix = {0};

	uint32_t last_ticks = system_ticks_ms;

	for (;;) {
		uint32_t now_ticks = system_ticks_ms;
		uint32_t elapsed_ms = now_ticks - last_ticks;
		last_ticks = now_ticks;
		float dt = elapsed_ms / 1000.0f;
		if (dt <= 0.0f) dt = 0.001f;

		I2C_ReadMulti(0x3B, accel_raw, 6);
		accel_x = (int16_t)((accel_raw[0] << 8) | accel_raw[1]);
		accel_y = (int16_t)((accel_raw[2] << 8) | accel_raw[3]);
		accel_z = (int16_t)((accel_raw[4] << 8) | accel_raw[5]);

		uint8_t gyro_raw[6];
		I2C_ReadMulti(0x43, gyro_raw, 6);
		int16_t gyro_x = (int16_t)((gyro_raw[0] << 8) | gyro_raw[1]);
		int16_t gyro_y = (int16_t)((gyro_raw[2] << 8) | gyro_raw[3]);
		int16_t gyro_z = (int16_t)((gyro_raw[4] << 8) | gyro_raw[5]);

		float gyro_x_dps = gyro_x / 131.0f;
		float gyro_y_dps = gyro_y / 131.0f;
		float gyro_z_dps = gyro_z / 131.0f;
		float gyro_x_rad = gyro_x_dps * DEG_TO_RAD;
		float gyro_y_rad = gyro_y_dps * DEG_TO_RAD;
		float gyro_z_rad = gyro_z_dps * DEG_TO_RAD;

		EKF_Predict(&ekf, gyro_x_rad, gyro_y_rad, gyro_z_rad, dt);
		EKF_UpdateAccel(&ekf, (float)accel_x, (float)accel_y, (float)accel_z);

		uint8_t mag_raw[6];
		QMC_ReadMulti(0x00, mag_raw, 6);
		int16_t mag_x = (int16_t)((mag_raw[1] << 8) | mag_raw[0]);
		int16_t mag_y = (int16_t)((mag_raw[3] << 8) | mag_raw[2]);
		int16_t mag_z = (int16_t)((mag_raw[5] << 8) | mag_raw[4]);

		EKF_UpdateMag(&ekf, (float)mag_x, (float)mag_y, (float)mag_z);

		float pitch_deg, roll_deg, yaw_deg;
		Quat_ToEuler(ekf.q, &pitch_deg, &roll_deg, &yaw_deg);

		GPS_UART_Poll();
		if (nmea_sentence_ready) {
			if (NMEA_IsGGA(nmea_buf)) {
				NMEA_ParseGGA(nmea_buf, &gps_fix);
			}
			nmea_sentence_ready = 0;
		}

		DebugCheckpoint();
	}
}
