/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */


#include "serial-connection.h"
#include "heterogeneous-desider.h"

#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/**
 *
 * @param data
 * @param i
 * @param start
 * @param len
 * @return
 */
int parse_int_from_string(char *data, int *i, int start, int len) {
    char string[10];
    int j;

    for (j=start; j <= len; j++) {
        if (!isdigit(data[j])) {
            if (start < j) {
                strncpy (string, data + (start*sizeof(char)), (j - start));
                string[j-start] = '\0';
                *i = j-1;
                return strtol(string, &string, 10);;
            }
            else {
                return 0;
            }
        }
    }
    return 0;
}

/**
 *
 * @param data
 * @param len
 * @return
 */
int handle_commands(char *data, int len) {
    if (data[1] == 'w') {
        tech_struct *wifi_tech = add_technology(WIFI_TECHNOLOGY);
        int i, en = 0, bw = 0, etx = 0;

        for (i=2;i <= len; i++) {
            if (data[i] == 'e') {
                en = parse_int_from_string(data, &i, i+1, len);
            }
            else if (data[i] == 'b') {
                bw = parse_int_from_string(data, &i, i+1, len);
            }
            else if (data[i] == 'x') {
                etx = parse_int_from_string(data, &i, i+1, len);
            }
        }
        add_metrics(wifi_tech, en, bw, etx);
        return 1;
    }
    return 0;
}

int handle_prints(char *data, int len) {
    if (data[1] == 'm') {
        print_metrics_table();
        return 1;
    }
    return 0;
}

int handle_requests(char *data, int len) {
    if (data[1] == 'c') {
        print_src_ip();
        return 1;
    }
    return 0;
}

int handle_input(char *data) {
    int len = strlen(data);

    if (data[0] == '!') {
        return handle_commands(data, len);
    }
    else if (data[0] == '#') {
        return handle_prints(data, len);
    }
    else if (data[0] == '?') {
        return handle_requests(data, len);
    }
    return 0;
}


PROCESS(serial_connection, "Heterogenous serial handler");

PROCESS_THREAD(serial_connection, ev, data)
{
    PROCESS_BEGIN();
    printf("serial connection started\n");

    for(;;) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message) {
            //printf("received line: %s\n", (char *)data);
            handle_input((char *)data);
        }
    }
    PROCESS_END();
}