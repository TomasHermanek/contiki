/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#include "heterogeneous-desider.h"

#include "serial-connection.h"

#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"

#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"

#include "lib/memb.h"

#include <stdio.h>
#include <string.h>

/**
 * Example list structure and todo
 */


LIST(tech_list);
MEMB(tech_memb, struct tech_struct, MAX_TECHNOLOGIES);



LIST(metrics_list);
MEMB(metrics_memb, struct metrics_struct, MAX_TECHNOLOGIES);

/**
 *
 * @param type
 */
tech_struct *add_technology(int type)  {
    struct tech_struct *tech = NULL;
    tech = memb_alloc(&tech_memb);
    if (tech==NULL) {
        printf("Maximum tech capacity exceeded\n");
    }
    tech->number = type;

    list_add(tech_list, tech);

    return tech;
}

void add_metrics(struct tech_struct *technology, int energy, int bandwidth, int etx) {
    struct metrics_struct *metrics = NULL;
    metrics = memb_alloc(&metrics_memb);
    if (metrics==NULL) {
        printf("Maximum tech capacity exceeded\n");
    }

    metrics->technology = technology;
    metrics->energy = energy;
    metrics->bandwidth = bandwidth;
    metrics->etx = etx;

    list_add(metrics_list, metrics);
}

/**
 *
 */
void print_tech_table() {
    struct tech_struct *s;

    for(s = list_head(tech_list);
        s != NULL;
        s = list_item_next(s)) {
        printf("List element number %d\n", s->number);
    }
}

void print_metrics_table() {
    struct metrics_struct *s;

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        printf("Metrics tech:  %d %d %d\n", s->bandwidth, s->energy, s->etx);
    }
}

int select_technology(const void *data) {
    int number = (int) data;
    printf("%s", number);

    // 1 exctract keys data
    // calculate m
}

/**
 * @param c
 * @param data
 * @param datalen
 * @param to
 * @return
 */
int heterogenous_simple_udp_sendto(struct simple_udp_connection *c,
                                   const void *data,
                                   uint16_t datalen,
                                   const uip_ipaddr_t *to)
{
    select_technology(data);
    // printf("%d\n", sizeof(technology_table) / sizeof(technology_table[0]));
    // check if there is wifi technology
    // 1. exctract metric keys k_en, k_bw, k_etx
    // 2. calculate technology using m
    return simple_udp_sendto(c, data, datalen, to);
}

/**
 * Initialize module
 */
void init_module() {
    NETSTACK_MAC.off(1);

    tech_struct *wifi_tech = add_technology(WIFI_TECHNOLOGY);
    add_metrics(wifi_tech, 40, 1, 1);
    tech_struct *rpl_tech = add_technology(RPL_TECHNOLOGY);
    add_metrics(rpl_tech, 1, 40, 10);
    print_tech_table();
    print_metrics_table();

    process_start(&serial_connection, NULL);
}

