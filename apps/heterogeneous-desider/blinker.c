/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#include "contiki.h"



PROCESS(blinker, "blinker");

PROCESS_THREAD(blinker, ev, data)
{
    PROCESS_BEGIN();
    char led_status;
    static struct etimer timer;
    etimer_set(&timer ,CLOCK_SECOND);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        etimer_set(&timer ,CLOCK_SECOND);
        led_status = leds_get();
        leds_toggle(led_status);
    }
    PROCESS_END();
}