#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define HZ 100

void timer_init(void);
uint32_t timer_ticks(void);
void sleep_ms(uint32_t ms);

#endif
