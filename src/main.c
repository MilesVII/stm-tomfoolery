
#include "stm32f411xe.h"
#include "hal_at_home.h"
#include "ili9341/display.h"
#include "ft6336g/touch.h"
#include "max6675/thermal.h"

DECLARE_GPIO_MOUT(LED, C, 13);
DECLARE_GPIO_MIN(BUTT, A, 0);
// DECLARE_GPIO_MIN(MOSFET, A, 1);

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
		RCC_AHB1ENR_GPIOBEN |
		RCC_AHB1ENR_GPIOCEN;

	LED_INIT();
	BUTT_INIT();
	// MOSFET_INIT(); MOSFET_HIGH();

	display1_init(1);
	display1_clear(0x00, 0, 0, 240, 320);
	touch_init();
	thermal_init();
	

	uint32_t status[1] = { 0.0f };
	while (1) {
		delay_ms(200);
		float t = thermal_poll();
		status[0] = (uint32_t)(t * 100);
		display0_updateNumbers(status, 1);
	}
}
