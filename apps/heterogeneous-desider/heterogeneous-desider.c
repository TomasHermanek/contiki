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
#include "simple-udp.h"

#include "lib/memb.h"

#include <stdio.h>
#include <string.h>


LIST(tech_list);
MEMB(tech_memb, struct tech_struct, MAX_TECHNOLOGIES);

LIST(metrics_list);
MEMB(metrics_memb, struct metrics_struct, MAX_TECHNOLOGIES);

static uip_ipaddr_t *src_ip = NULL;

/**
 * Function allows to add technology into technology table
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

/**
 * Function allows to add metrics into Metrics table, which is linked to technology
 */
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
 * Allows to print Technology Table
 */
void print_tech_table() {
    struct tech_struct *s;

    for(s = list_head(tech_list);
        s != NULL;
        s = list_item_next(s)) {
        printf("List element number %d\n", s->number);
    }
}

/**
 * Allows to print Metrics Table
 */
void print_metrics_table() {
    struct metrics_struct *s;

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        printf("Metrics tech:  %d %d %d\n", s->energy, s->bandwidth, s->etx);
    }
}

/**
 * ToDo this section must implement keys extraction from Ondrej part
 * @param data
 * @param en
 * @param bw
 * @param etx
 */
void fill_keys(const void *data, int *en, int *bw, int *etx) {
    int number = (int) data;
    int val = strtol(data, &data, 10);

    if (val % 2 == 0) {
        *en = 50;
        *bw = 1;
        *etx = 1;
    }
    else {
        *en = 1;
        *bw = 1;
        *etx = 1;
    }
}

/**
 * Function that implements calculation of metrics
 *
 * @param ms
 * @param k_en
 * @param k_bw
 * @param k_etx
 * @return
 */
int calculate_m(struct metrics_struct *ms, int k_en, int k_bw, int k_etx) {
    return (ms->energy * k_en) + (ms->bandwidth * k_bw) + (ms->etx * k_etx);
}

/**
 * Function used for technology selection
 *
 * @param data
 * @return
 */
tech_struct *select_technology(const void *data) {
    int k_en, k_bw, k_etx, m_res = -1;
    tech_struct *selected_technology = NULL;
    struct metrics_struct *s;

    fill_keys(data, &k_en, &k_bw, &k_etx);
    //printf("Kyes %d %d %d\n", k_en, k_bw, k_etx);

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        //printf("Metrics tech:  %d %d %d\n", s->energy, s->bandwidth, s->etx);
        if (m_res == -1) {
            selected_technology = s->technology;
            m_res = calculate_m(s, k_en, k_bw, k_etx);
        }
        else {
            int m_actual = calculate_m(s, k_en, k_bw, k_etx);
            if (m_actual < m_res) {
                selected_technology = s->technology;
                m_res = calculate_m(s, k_en, k_bw, k_etx);
            }
        }
    }
    return selected_technology;
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
    // first point check flow table
    tech_struct *dst_technology;
    dst_technology = select_technology(data);

    if (dst_technology != NULL) {
        if (dst_technology->number == RPL_TECHNOLOGY) {
            printf("technology: RPL, %d\n", dst_technology->number);
            return simple_udp_sendto(c, data, datalen, to);
        }
        else if (dst_technology->number == WIFI_TECHNOLOGY) {
            printf("technology: WIFI, %d\n", dst_technology->number);
            printf("!p");
            uip_debug_ipaddr_print(to);
            printf(";%d;%d;%s", c->remote_port, c->local_port, data);
            printf("\n");
        }
    }

}

/**
 * Initialize module
 */
void init_module(uip_ipaddr_t *ip) {
    NETSTACK_MAC.off(1);
    src_ip = ip;
    tech_struct *rpl_tech = add_technology(RPL_TECHNOLOGY);

    printf("!b\n");

    printf("!r");
    uip_debug_ipaddr_print(src_ip);
    printf("\n");

    add_metrics(rpl_tech, 1, 40, 10);       // ToDO load values from config or from rpl
    process_start(&serial_connection, NULL);
}

