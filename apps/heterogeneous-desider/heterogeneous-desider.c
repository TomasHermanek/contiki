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
#include "net/ipv6/uip6.h"

#include "lib/memb.h"

#include <stdio.h>
#include <string.h>

static uint8_t databuffer[UIP_BUFSIZE];
#define UIP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

LIST(tech_list);
MEMB(tech_memb, struct tech_struct, MAX_TECHNOLOGIES);

LIST(metrics_list);
MEMB(metrics_memb, struct metrics_struct, MAX_TECHNOLOGIES);

simple_udp_callback receiver_callback;

static struct simple_udp_connection *unicast_connection;    // Unicast connectin of parrent process
static int mode;

/**
 * Allows to find technology by type, this function is useful when duplicates are finding
 *
 * @param type
 * @return
 */
tech_struct *find_tech_by_type(int type) {
    struct tech_struct *s;

    for(s = list_head(tech_list); s != NULL; s = list_item_next(s)) {
        if (s->type == type)
            return s;
    }
    return NULL;
}


metrics_struct *find_metrics_by_tech(tech_struct *tech) {
    struct metrics_struct *s;

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        if (s->technology == tech)
            return s;
    }
    return NULL;
}

/**
 * Function allows to add technology into technology table
 * @param type
 */
tech_struct *add_technology(int type)  {
    struct tech_struct *tech = find_tech_by_type(type);            // todo ensure that tech in table is unique

    if (tech == NULL) {
        tech = memb_alloc(&tech_memb);
        if (tech==NULL) {
            printf("Maximum tech capacity exceeded\n");
        }
        tech->type = type;

        list_add(tech_list, tech);
    }
    return tech;
}

/**
 * Function allows to add metrics into Metrics table, which is linked to technology
 */
void add_metrics(struct tech_struct *technology, int energy, int bandwidth, int etx) {
    struct metrics_struct *metrics = find_metrics_by_tech(technology);

    if (metrics == NULL) {
        metrics = memb_alloc(&metrics_memb);
        if (metrics==NULL) {
            printf("Maximum tech capacity exceeded\n");
        }
        list_add(metrics_list, metrics);
    }
    metrics->technology = technology;
    metrics->energy = energy;
    metrics->bandwidth = bandwidth;
    metrics->etx = etx;
}

/**
 * Allows to print Technology Table
 */
void print_tech_table() {
    struct tech_struct *s;

    for(s = list_head(tech_list); s != NULL; s = list_item_next(s)) {
        printf("List element type %d\n", s->type);
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
    int type = (int) data;
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
        if (dst_technology->type == RPL_TECHNOLOGY) {
            printf("technology: RPL, %d\n", dst_technology->type);
            return simple_udp_sendto(c, data, datalen, to);
        }
        else if (dst_technology->type == WIFI_TECHNOLOGY) {
            printf("technology: WIFI, %d\n", dst_technology->type);
            printf("!p");
            uip_debug_ipaddr_print(to);
            printf(";%d;%d;%s", c->remote_port, c->local_port, data);
            printf("\n");
        }
    }

}


/**
 * ToDo function which must be used by Ondrej to send packet using coap
 *
 * @param c
 * @param data
 * @param len
 * @param toaddr
 * @param toport
 * @return
 */
int heterogeneous_udp_sendto(struct uip_udp_conn *c, const void *data, int len, const uip_ipaddr_t *toaddr, uint16_t toport) {
    //todo ONDREJ -> call Ondrej function for parsing k-values (for metric)
    printf("Sent using heterogeneous sent\n");
    uip_udp_packet_sendto(c, data, len, toaddr, toport);
    return 0;
}


int heterogenous_udp_register(struct simple_udp_connection *c, uint16_t local_port, uip_ipaddr_t *remote_addr,
                              uint16_t remote_port, simple_udp_callback receive_callback) {
    receiver_callback = receive_callback;
    simple_udp_register(c, local_port, remote_addr, remote_port, receive_callback);
}

int heterogeneous_forwarding_callback() {

    //memcpy(databuffer, uip_appdata, uip_datalen());

    printf("hello from the callback side\n");
    printf("packet to send from: ");
    uip_debug_ipaddr_print(&(UIP_IP_BUF->srcipaddr));
    printf(" to: ");
    uip_debug_ipaddr_print(&(UIP_IP_BUF->destipaddr));
    printf(" srcPort: %d dstPort: %d data: %s length: %d \n",
           UIP_HTONS(UIP_IP_BUF->srcport), UIP_HTONS(UIP_IP_BUF->destport), uip_appdata-4, uip_datalen());
}

void
heterogenous_udp_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    printf("3 ");
    uip_debug_ipaddr_print(&sender_addr);
    printf("\n");
    receiver_callback(c, sender_addr, sender_port, receiver_addr, receiver_port, data, datalen);
}


void print_src_ip() {
    printf("!r");
    int i;
    uint8_t state;
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
           (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
            printf(";");
        }
    }
    printf("\n");
}

/**
 * Initialize module
 */
void init_module(struct simple_udp_connection *unicast_connection, int mode) {
    NETSTACK_MAC.off(1);
    tech_struct *rpl_tech = add_technology(RPL_TECHNOLOGY);
    set_callback(heterogeneous_forwarding_callback);

    printf("!b\n");
    print_src_ip();

    add_metrics(rpl_tech, 1, 40, 10);       // ToDO load values from config or from rpl
    process_start(&serial_connection, NULL);
}

