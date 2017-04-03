/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */


#include "serial-connection.h"
#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>

PROCESS(serial_connection, "Heterogenous serial handler");
//AUTOSTART_PROCESSES(&test_serial);

PROCESS_THREAD(serial_connection, ev, data)
{
    PROCESS_BEGIN();
    printf("serial connection started\n");

    for(;;) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message) {
            printf("received line: %s\n", (char *)data);
        }
    }
    PROCESS_END();
}