/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#ifndef CONTIKI_HETEROGENEOUS_DESIDER_H
#define CONTIKI_HETEROGENEOUS_DESIDER_H

#include "net/ip/uip.h"
#include "er-coap.h"
#include "er-coap-engine.h"

#define WIFI_TECHNOLOGY 1
#define RPL_TECHNOLOGY 2

#define MAX_TECHNOLOGIES 4

#define MODE_ROOT = 1
#define MODE_NODE = 2

/**
 * Structure defines technology unit
 */
typedef struct tech_struct {
    struct tech_struct *next;
    int type;
    char *name;
} tech_struct;

/**
 * Structure defines metrics unit
 */
typedef struct metrics_struct {
    struct metrics_struct *next;
    struct tech_struct *technology;
    int energy;
    int bandwidth;
    int etx;
} metrics_struct;

/**
 * \brief       Module initialization
 */
void
init_module();

/**
 * \brief       Adds technology to technology table
 * @param type
 * @return
 */
tech_struct *add_technology(int type);

/**
 * \brief       Adds metrics to metrics table
 *
 * @param technology
 * @param energy
 * @param bandwidth
 * @param etx
 */
void add_metrics(struct tech_struct *technology, int energy, int bandwidth, int etx);

#endif //CONTIKI_HETEROGENEOUS_DESIDER_H
