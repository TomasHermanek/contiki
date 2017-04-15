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

#define MAX_TECHNOLOGIES 2
#define MAX_FLOWS 5

#define MODE_ROOT 1
#define MODE_NODE 2

#define RPL_SEND_LED 1
#define WIFI_SEND_LED 2
#define RPL_FORWARD_LED 3
#define WIFI_FORWARD_LED 4
#define RPL_RECEIVE_LED 1
#define WIFI_RECEIVE_LED 2

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
 * flags structure -> |x|x|x|x|x|x|PND|CNF
 * CNF -> if flow was confirmed by linux device (used by wifi technology)
 * PND -> pending, if contiki waits for response to confirmation
 */
#define CNF 0x01
#define PND 0x02

/**
 * Structure that represents flow
 */
typedef struct flow_struct {
    struct flow_struct *next;
    uip_ipaddr_t to;
    int energy;
    int bandwidth;
    int etx;
    int validity;
    struct tech_struct *technology;
    char flags;
} flow_struct;

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
