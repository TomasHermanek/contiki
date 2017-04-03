/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#ifndef CONTIKI_HETEROGENEOUS_DESIDER_H
#define CONTIKI_HETEROGENEOUS_DESIDER_H

#include "net/ip/uip.h"

/**
 * \brief       Module initialization
 */
void
init_module();

#define WIFI_TECHNOLOGY 1
#define RPL_TECHNOLOGY 2
#define MAX_TECHNOLOGIES 4

#define ENERGY 10
#define ETX 11
#define BANDWIDTH 12

typedef struct tech_struct {
    struct tech_struct *next;
    int number;
    char *name;
} tech_struct;

typedef struct metrics_struct {
    struct metrics_struct *next;
    struct tech_struct *technology;
    int energy;
    int bandwidth;
    int etx;
} metrics_struct;

tech_struct *add_technology(int type);
void add_metrics(struct tech_struct *technology, int energy, int bandwidth, int etx);
/*

void
add_technology();

int heterogenous_simple_udp_sendto(struct simple_udp_connection *c,
                               const void *data,
                               uint16_t datalen,
                               const uip_ipaddr_t *to);

struct technology_entry *
decide_technology(int k_etx, int k_en, int k_bw) {

}
 */

#endif //CONTIKI_HETEROGENEOUS_DESIDER_H