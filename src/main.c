
#include "stm32f411xe.h"
#include "hal_at_home.h"

DECLARE_GPIO_MOUT(L0, A, 1);
DECLARE_GPIO_MOUT(L1, A, 2);
DECLARE_GPIO_MOUT(L2, A, 3);
DECLARE_GPIO_MOUT(L3, A, 4);
DECLARE_GPIO_MOUT(L4, A, 5);
DECLARE_GPIO_MIN(BUTT, A, 0);
DECLARE_GPIO_MOUT(LED, C, 13);

void ledOff() {
	LED_HIGH();
}
void ledOn() {
	LED_LOW();
}

int main(void) {
	SysTick_Config(SystemCoreClock / 1000); // 1ms tick

	RCC->AHB1ENR |=
		RCC_AHB1ENR_GPIOAEN |
		RCC_AHB1ENR_GPIOCEN;

	L0_INIT(); L0_HIGH();
	L1_INIT(); L1_HIGH();
	L2_INIT(); L2_HIGH();
	L3_INIT(); L3_HIGH();
	L4_INIT(); L4_HIGH();
	LED_INIT();
	BUTT_INIT();
	#define LX(s, n) if (s & 0b0 << n) L##n##_LOW(); else L##n##_HIGH();

	uint8_t s = 0b00000;
	uint8_t bp = 0;

	while (1) {
		delay_ms(32);
		LX(s, 0);
		LX(s, 1);
		LX(s, 2);
		LX(s, 3);
		LX(s, 4);
		uint8_t bc = !BUTT_READ();
		if (!bc && bp == 1) {
			++s;
			if (s >= 32) s = 0;
		}
		bp = bc;
	}
}
