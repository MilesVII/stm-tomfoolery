#include "stm32f411xe.h"
#include "hal_at_home.h"
#include <sys/stat.h>
#include <errno.h>

// stub syscalls
int _write(int file, char *ptr, int len) { return len; }
int _read(int file, char *ptr, int len) { return 0; }
int _close(int file) { return -1; }
int _fstat(int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { return 1; }
int _lseek(int file, int ptr, int dir) { return 0; }
void* _sbrk(int incr) {
	extern char _end; // from linker script
	static char *heap_end;
	char *prev;

	if (heap_end == 0)
		heap_end = &_end;

	prev = heap_end;
	heap_end += incr;
	return prev;
}

// sleep
volatile uint32_t ticks;

void SysTick_Handler(void) {
	ticks++;
}

void delay_ms(uint32_t ms) {
	uint32_t start = ticks;
	while ((ticks - start) < ms) {
		__NOP();
	};
}