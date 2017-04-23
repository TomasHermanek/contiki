/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#include "heterogeneous-desider.h"

#include "serial-connection.h"
#include "blinker.h"

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

#define UIP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

LIST(tech_list);
MEMB(tech_memb, struct tech_struct, MAX_TECHNOLOGIES);

LIST(metrics_list);
MEMB(metrics_memb, struct metrics_struct, MAX_TECHNOLOGIES);

LIST(flow_list);
MEMB(flow_memb, struct flow_struct, MAX_FLOWS);

simple_udp_callback receiver_callback;

static struct simple_udp_connection *unicast_connection;    // Unicast connectin of parrent process
static uip_ipaddr_t src_ip;
static int device_mode;

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

flow_struct *find_flow(const uip_ipaddr_t *to, int en, int bw, int etx) {
    struct flow_struct *s;

    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        if (uip_ipaddr_cmp(to, &s->to) && en == s->energy && bw == s->bandwidth && etx == s->etx)
            return s;
    }
    return NULL;
}

/**
 * Removes all flows from database
 */
void clear_flows() {
    struct flow_struct *s;

    while(s = list_head(flow_list)) {
        list_remove(flow_list, s);
        memb_free(&flow_memb, s);
    }
}

/**
 * Removes oldest flow
 */
void clear_oldest_flow() {
    struct flow_struct *s, *oldest_flow = list_head(flow_list);

    s = list_item_next(s);
    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        if (s->validity < oldest_flow->validity)
            oldest_flow = s;
    }

    list_remove(flow_list, oldest_flow);
    memb_free(&flow_memb, oldest_flow);
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
 * Adds flow to flow_list, stores metrics, validation time and destination address. If target tech is wifi and node-mode
 * is ROOT, device needs to ask using serial line, if destination device is available using WIFI technology. This is done
 * by set up PND flag to true. If target tech is WIFI and mode is casual node, wifi always sends data to ROOT device,
 * which should be always accessible. Thus, flag is set up to CNF.
 */
flow_struct *add_flow(const uip_ipaddr_t *to, tech_struct *tech, int en, int bw, int etx) {
    struct flow_struct *flow;

    flow = memb_alloc(&flow_memb);

    while (flow == NULL) {
        clear_oldest_flow();
        flow = memb_alloc(&flow_memb);
    }

    list_add(flow_list, flow);

    uip_ipaddr_copy(&flow->to, to);

    flow->technology = tech;
    flow->energy = en;
    flow->bandwidth = bw;
    flow->etx = etx;
    flow->validity = FLOW_VALIDITY;

    if (tech->type == WIFI_TECHNOLOGY)
        if (device_mode == MODE_ROOT)
            flow->flags = PND;
        else
            flow->flags = CNF;
    else
        flow->flags = 0;

    return flow;
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
 * Allows to print Flow Table
 */
void print_flow_table() {
    struct flow_struct *s;

    printf("Flow: -> to en/bw/etx - tech(1-wifi/2-rpl), valid(time), flags\n");
    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        printf("Flow: -> ");
        uip_debug_ipaddr_print(&s->to);
        printf(" %d/%d/%d - %d, %d, %d\n", s->energy, s->bandwidth, s->etx, s->technology->type, s->validity, s->flags);
    }
}

/**
 * Prints all border router neighbours todo check if deeper leafs are aso part of this print
 */
void print_neighbours() {
    uip_ds6_route_t *r;
    printf("!n;");
    for(r = uip_ds6_route_head(); r != NULL; r = uip_ds6_route_next(r)) {
        uip_debug_ipaddr_print(&r->ipaddr);
        printf(";");
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

    if ((val % 4 == 0) || (val % 4 == 1)) {
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

flow_struct *get_flow(const uip_ipaddr_t *to, const void *data, uint16_t datalen) {
    int k_en, k_bw, k_etx;
    fill_keys(data, &k_en, &k_bw, &k_etx);

    flow_struct *flow;
    flow = find_flow(to, k_en, k_bw, k_etx);

    if (!flow) {
        tech_struct *dst_technology;
        dst_technology = select_technology(data);
        flow = add_flow(to, dst_technology, k_en, k_bw, k_etx);
    }
}

void send_packet_wifi(uip_ipaddr_t *from, const uip_ipaddr_t *to, int remote_port, int src_port, const void *data) {
    printf("!p;");
    uip_debug_ipaddr_print(from);
    printf(";");
    uip_debug_ipaddr_print(to);
    printf(";%d;%d;%s", remote_port, src_port, data);
    printf("\n");
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
    flow_struct *flow = get_flow(to, data, datalen);

    if (flow != NULL) {
        if (flow->flags & PND) {    // need to ask for route (pending flag active)
            PROCESS_CONTEXT_BEGIN(&serial_connection);
            ask_for_route(flow);
            PROCESS_CONTEXT_END();
        }
        if (flow->technology->type == RPL_TECHNOLOGY || !(flow->flags & CNF)) {    //dst tech is wifi or not approved wifi
//            printf("technology: RPL, %d\n", flow->technology->type);
            simple_udp_sendto(c, data, datalen, to);
            leds_on(RPL_SEND_LED);
        } else if (flow->technology->type == WIFI_TECHNOLOGY && (flow->flags & CNF == 1)) {
            send_packet_wifi(&src_ip, to, c->remote_port, c->local_port, data);
            leds_on(WIFI_SEND_LED);
        }
    }

}

int heterogeneous_forwarding_callback() {
    flow_struct *flow = get_flow(&(UIP_IP_BUF->destipaddr), uip_appdata-4, uip_datalen());

    if (flow != NULL) {
        if (flow->flags & PND) {    // need to ask for route (pending flag active)
            PROCESS_CONTEXT_BEGIN(&serial_connection);
            ask_for_route(flow);
            PROCESS_CONTEXT_END();
        }
        if (flow->technology->type == RPL_TECHNOLOGY || !(flow->flags & CNF)) {    //dst tech is wifi or not approved wifi  # todo make sure that tech rpl exists!
//            printf("technology: RPL, %d\n", flow->technology->type);
            leds_on(RPL_FORWARD_LED);
            return 1;
        } else if (flow->technology->type == WIFI_TECHNOLOGY && (flow->flags & CNF == 1)) {
            send_packet_wifi(&(UIP_IP_BUF->srcipaddr), &(UIP_IP_BUF->destipaddr), UIP_HTONS(UIP_IP_BUF->destport),
                             UIP_HTONS(UIP_IP_BUF->srcport), uip_appdata-4);
//            printf("packet to send from: ");
//            uip_debug_ipaddr_print(&(UIP_IP_BUF->srcipaddr));
//            printf(" to: ");
//            uip_debug_ipaddr_print(&(UIP_IP_BUF->destipaddr));
//            printf(" srcPort: %d dstPort: %d data: %s length: %d \n",
//                   UIP_HTONS(UIP_IP_BUF->srcport), UIP_HTONS(UIP_IP_BUF->destport), uip_appdata-4, uip_datalen());
            leds_on(WIFI_FORWARD_LED);
            return 0;
        }
    }
    return 1;
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

/**
 * This function calls main process callback function with data, parsed from serial line
 *
 * @param c
 * @param sender_addr
 * @param sender_port
 * @param receiver_addr
 * @param receiver_port
 * @param data
 * @param datalen
 */
void heterogenous_udp_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    leds_on(WIFI_RECEIVE_LED);
    if (receiver_callback)
        receiver_callback(c, sender_addr, sender_port, receiver_addr, receiver_port, data, datalen);
}

/**
 * Prints mote IPv6 address database in format !p;<addr1>;<addr2>; ...
 */
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
 * Prints mote mode in format !c<mode>
 */
void print_mode() {
    printf("!c%d\n", device_mode);
}


uip_ipaddr_t *get_my_ip() {
    return &src_ip;
}

/**
 * Initialize module
 */
void init_module(int mode, const uip_ipaddr_t *ip) {
    NETSTACK_MAC.off(1);
    leds_init();

    uip_ipaddr_copy(&src_ip, ip);
    tech_struct *rpl_tech = add_technology(RPL_TECHNOLOGY);
    set_callback(heterogeneous_forwarding_callback);
    device_mode = mode;

    printf("!b\n");
    print_src_ip();
    print_neighbours();

    add_metrics(rpl_tech, 1, 40, 10);       // ToDO load values from config or from rpl
    process_start(&serial_connection, NULL);
    process_start(&blinker, NULL);
}

