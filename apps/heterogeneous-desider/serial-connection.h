/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */


#ifndef CONTIKI_SERIAL_CONNECTION_H
#define CONTIKI_SERIAL_CONNECTION_H

#define MAX_SIMULTANEOUS_QUESTIONS 3

#include "heterogeneous-desider.h"

typedef struct question_struct {
    struct question_struct *next;
    struct flow_struct *flow;
    short question_id;
} question_struct;

#endif //CONTIKI_SERIAL_CONNECTION_H

PROCESS_NAME(serial_connection);