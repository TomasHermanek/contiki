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