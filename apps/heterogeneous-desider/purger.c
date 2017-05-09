/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#define DEBUG 0
#include "net/ip/uip-debug.h"
#include "heterogeneous-desider.h"
#include "purger.h"

#include "contiki.h"

PROCESS(purger, "purger");

uint8_t counter = 0;

PROCESS_THREAD(purger, ev, data)
{
    PROCESS_BEGIN();
    PRINTF("Starting purger\n");

    static struct etimer timer;
    etimer_set(&timer ,CLOCK_SECOND * PURGE_INTERVAL);


    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        etimer_set(&timer ,CLOCK_SECOND * PURGE_INTERVAL);


        purge_flows();
        purge_metrics();

        counter+=PURGE_INTERVAL;

        if (counter >= METRICS_VALIDITY/5) {
            printf("?w\n");
            counter = 0;
        }
    }
    PROCESS_END();
}