/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#define DEBUG 0
#include "net/ip/uip-debug.h"

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

#include "contiki.h"
#include "reg.h"
#include "cpu.h"
#include "dev/smwdthrosc.h"

#define UIP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

LIST(tech_list);
MEMB(tech_memb, struct tech_struct, MAX_TECHNOLOGIES);

LIST(metrics_list);
MEMB(metrics_memb, struct metrics_struct, MAX_TECHNOLOGIES);

LIST(flow_list);
MEMB(flow_memb, struct flow_struct, MAX_FLOWS);

static uip_ipaddr_t src_ip;
static uint8_t device_mode;

static uint8_t flow_id = 0;
const uint8_t MAX_FLOW_ID = 10;

int sent_wifi;
int sent_rpl;
int wr_rate = 1;
const int SIMULATED_BAT_CAPACITY = 10;


#ifdef HETEROGENEOUS_STATISTICS
static struct statistics stats;
#endif

#ifdef SIMPLE_UDP_HETEROGENEOUS
static struct simple_udp_connection *unicast_connection;    // Unicast connectin of parrent process
simple_udp_callback receiver_callback;
#endif

/**
 * Allows to find technology by type, this function is useful when duplicates are finding
 *
 * @param type
 * @return
 */
tech_struct *find_tech_by_type(uint8_t type) {
    struct tech_struct *s;

    for(s = list_head(tech_list); s != NULL; s = list_item_next(s)) {
        if (s->type == type)
            return s;
    }
    return s;
}

/**
 * Allows to find metrics by technology
 *
 * @param tech
 * @return
 */
metrics_struct *find_metrics_by_tech(tech_struct *tech) {
    struct metrics_struct *s;

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        if (s->technology == tech)
            return s;
    }
    return s;
}

/**
 * Allows to find metrics by technology
 *
 * @param tech
 * @return
 */
metrics_struct *find_metrics_by_tech_type(uint8_t type) {
    struct metrics_struct *s;

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        if (s->technology->type == type)
            return s;
    }
    return s;
}


/**
 * Allows to find flow by dst IPv6 address and metric keys
 *
 * @param to
 * @param en
 * @param bw
 * @param etx
 * @return
 */
flow_struct *find_flow(const uip_ipaddr_t *to, uint8_t en, uint8_t bw, uint8_t etx) {
    struct flow_struct *s;

    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        if (uip_ipaddr_cmp(to, &s->to) && en == s->energy && bw == s->bandwidth && etx == s->etx)
            return s;
    }
    return s;
}

/**
 * Finds flow byt its flow ID
 *
 * @param id
 * @return
 */
flow_struct *find_flow_by_id(uint8_t id) {
    struct flow_struct *s;

    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        if (id == s->flow_id)
            return s;
    }
    return s;
}

/**
 * Generates new flow_id, if it reaches limit, flow_id starts again from 1
 *
 * @return
 */
int get_flow_id() {
    if (flow_id > MAX_FLOW_ID)
        flow_id = 1;
    else
        flow_id++;
    return flow_id;
}

/**
 * Removes all flows from database
 */
void clear_flows() {
    PRINTF("Clearing flows\n");
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
    PRINTF("Removing oldest flow\n");
    struct flow_struct *s, *oldest_flow = list_head(flow_list);

    s = list_item_next(oldest_flow);
    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        if (s->validity < oldest_flow->validity)
            oldest_flow = s;
    }

    list_remove(flow_list, oldest_flow);
    memb_free(&flow_memb, oldest_flow);
}

/**
 * Based on bat capacity and sent packets over rpl and wifi, recalculates en value in metrics container
 *
 */
void recalculate_metrics() {
    PRINTF("Recalculating metrics\n");
    struct metrics_struct *wifi_metrics = find_metrics_by_tech_type(WIFI_TECHNOLOGY);
    struct metrics_struct *rpl_metrics = find_metrics_by_tech_type(RPL_TECHNOLOGY);

    uint8_t w_en_increment = wr_rate + (((sent_wifi * wr_rate) + sent_rpl) / SIMULATED_BAT_CAPACITY);
    uint8_t r_en_increment = 1 + w_en_increment / wr_rate;

    if (wifi_metrics && w_en_increment > wifi_metrics->energy) {
        if (w_en_increment >= 255)
            printf("Unable to send using wifi, low energy\n");
        else {
            PRINTF("Setting up new energy metrics for WIFI technology: %d\n", w_en_increment);
            wifi_metrics->energy = w_en_increment;
            clear_flows();
        }
    }

    if (rpl_metrics && r_en_increment > rpl_metrics->energy) {
        if (r_en_increment >= 255)
            printf("Unable to send using RPL, low energy\n");
        else {
            PRINTF("Setting up new energy metrics for RPL technology: %d\n", r_en_increment);
            rpl_metrics->energy = r_en_increment;
            clear_flows();
        }
    }
}

/**
 * Increments wifi counter and recalculates metrics
 */
void inc_wifi_sent() {
#ifdef ALLOW_SIMULATE_BATTERY
    sent_wifi++;
    recalculate_metrics();
#endif
}

/**
 * Increments rpl counter and recalculates metrics
 */
void inc_sent_rpl() {
#ifdef ALLOW_SIMULATE_BATTERY
    sent_rpl++;
    recalculate_metrics();
#endif
}

/**
 * Function allows to add technology into technology table
 *
 * @param type
 */
tech_struct *add_technology(uint8_t type)  {
    struct tech_struct *tech = find_tech_by_type(type);

    if (tech == NULL) {
        tech = memb_alloc(&tech_memb);
        if (tech==NULL) {
            printf("Maximum tech capacity exceeded\n");
        }
        PRINTF("Adding new technology type %d\n", type);
        tech->type = type;

        list_add(tech_list, tech);
    }
    return tech;
}

/**
 * Function allows to add metrics into Metrics table, which is linked to technology
 */
metrics_struct *add_metrics(struct tech_struct *technology, uint8_t energy, uint8_t bandwidth, uint8_t etx) {
    struct metrics_struct *metrics = find_metrics_by_tech(technology);

    if (metrics == NULL) {
        metrics = memb_alloc(&metrics_memb);
        if (metrics==NULL) {
            printf("Maximum tech capacity exceeded\n");
        }
        PRINTF("Adding new metrics for type %d (%d, %d, %d)\n", technology->type, energy, bandwidth, etx);
        list_add(metrics_list, metrics);
    }
    metrics->technology = technology;
    metrics->energy = energy;
    metrics->bandwidth = bandwidth;
    metrics->etx = etx;
    return metrics;
}

/**
 * Adds flow to flow_list, stores metrics, validation time and destination address. If target tech is wifi and node-mode
 * is ROOT, device needs to ask using serial line, if destination device is available using WIFI technology. This is done
 * by set up PND flag to true. If target tech is WIFI and mode is casual node, wifi always sends data to ROOT device,
 * which should be always accessible. Thus, flag is set up to CNF.
 */
flow_struct *add_flow(const uip_ipaddr_t *to, tech_struct *tech, uint8_t en, uint8_t bw, uint8_t etx) {
    struct flow_struct *flow;

    flow = memb_alloc(&flow_memb);

    while (flow == NULL) {
        clear_oldest_flow();
        flow = memb_alloc(&flow_memb);
    }

    list_add(flow_list, flow);

    uip_ipaddr_copy(&flow->to, to);

    flow->flow_id = get_flow_id();
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

    PRINTF("Adding new flow for destination: ");
    PRINT6ADDR(to);
    PRINTF(" (en: %d,bw: %d,etx: %d)\n", flow->energy, flow->bandwidth, flow->etx);
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
 * Prints counter used by dynamic energy alloc
 */
void print_energy_counter() {
    printf("Sent by Wifi: %d\nSent using RPL: %d\nWR rate: %d\n", sent_wifi, sent_rpl, wr_rate);
}

/**
 * Allows to print Metrics Table
 */
void print_metrics_table() {
    struct metrics_struct *s;

    printf("types(1->wifi, 2->RPL): energy, bandwidth, etx\n");
    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
        printf("Metrics tech(%d):  %d %d %d\n", s->technology->type, s->energy, s->bandwidth, s->etx);
    }
}

/**
 * Allows to print Flow Table
 */
void print_flow_table() {
    struct flow_struct *s;

    printf("ID: <id> Flow: -> <to> en/bw/etx - tech(1-wifi/2-rpl), valid(time), flags\n");
    for(s = list_head(flow_list); s != NULL; s = list_item_next(s)) {
        printf("ID: %d ", s->flow_id);
        printf("Flow: -> ");
        uip_debug_ipaddr_print(&s->to);
        printf(" %d/%d/%d - %d, %d, %d-", s->energy, s->bandwidth, s->etx, s->technology->type, s->validity, s->flags);
        if (s->flags & CNF)
            printf("confirmed,");
        if (s->flags & PND)
            printf("pending,");
        if (s->flags & UP)
            printf("up");
        else
            printf("down");
        printf("\n");
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
    printf("\n");
}

/**
 * Allows to extract used metric keys
 *
 * @param data
 * @param en
 * @param bw
 * @param etx
 */
void fill_keys(const void *data,uint8_t len, uint8_t *en, uint8_t *bw, uint8_t *etx) {
#ifdef SIMPLE_UDP_HETEROGENEOUS
    int type = (int) data;
    uint8_t val = strtol(data, &data, 10);
    if ((val % 4 == 0) || (val % 4 == 1)) {
//    if ((val % 2 == 1)) {
        *en = 50;
        *bw = 1;
        *etx = 1;
    }
    else {
        *en = 1;
        *bw = 1;
        *etx = 1;
    }
#endif
#ifdef COAP_HETEROGENEOUS
    struct k_val values = coap_get_k_val(data, len);
    *en = values.rem_energy;
    *bw = values.bandwidth;
   *etx = values.efx;
//
    //*en = 1;
   // *bw = 10;
    //*etx = 0;
#endif
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
int calculate_m(struct metrics_struct *ms, uint8_t k_en, uint8_t k_bw, uint8_t k_etx) {
    int m = (ms->energy * k_en) + (ms->bandwidth * k_bw) + (ms->etx * k_etx);
    PRINTF("Calculating metrics. Result for (k_en: %d,k_bw: %d,k_etx: %d) is %d\n", k_en, k_bw, k_etx, m);
    return m;
}

/**
 * Function used for technology selection
 *
 * @param data
 * @return
 */
tech_struct *select_technology(uint8_t k_en, uint8_t k_bw, uint8_t k_etx) {
    int m_res = -1;
    tech_struct *selected_technology = NULL;
    struct metrics_struct *s;

    for(s = list_head(metrics_list); s != NULL; s = list_item_next(s)) {
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
 * Adds source flow based on destination flow. Sets same technology, metrics values and flags. Adds UP flag. If
 * flow is defined before, we sets up again source technology (may change down wifi to up rpl etc.)
 *
 * @param from
 * @param dst_flow
 */
void add_from_flow(const uip_ipaddr_t *from, flow_struct *dst_flow, uint8_t tech_type) {
    uint8_t k_en = dst_flow->energy, k_bw = dst_flow->bandwidth, k_etx = dst_flow->etx;
    tech_struct *source_tech = find_tech_by_type(tech_type);

    flow_struct *flow;
    flow = find_flow(from, k_en, k_bw, k_etx);

    PRINTF("Adding from flow\n");
    if (!flow) {
        tech_struct *dst_technology = dst_flow->technology;
        flow = add_flow(from, source_tech, k_en, k_bw, k_etx);
    }
    else
        flow->technology = source_tech;
    flow->flags = dst_flow->flags | UP;
}

/**
 * Function will ask for route validity through serial line (if target route is available using WiFi tech)
 *
 * @param flow
 */
void ask_for_route(flow_struct *flow){
    PRINTF("Asking for route: ");
    PRINT6ADDR(&flow->to);
    PRINTF("\n");
    printf("?p;%d;", flow->flow_id);
    uip_debug_ipaddr_print(&flow->to);
    printf("\n");
    return 1;
}

/**
 * Sends packet using wifi (through serial line)
 *
 * @param from
 * @param to
 * @param remote_port
 * @param src_port
 * @param data
 */
void send_packet_wifi(uip_ipaddr_t *from, const uip_ipaddr_t *to, uint16_t remote_port, uint16_t src_port, const void *data, uint16_t len) {
    PRINTF("Seinding packet using WIFI technology\n");
    uint8_t *converted = (uint8_t*) data;
    printf("!p;");
    uip_debug_ipaddr_print(from);
    printf(";");
    uip_debug_ipaddr_print(to);
    printf(";%d;%d;", remote_port, src_port);

    int i = 0;
    for (i = 0; i <= len; i++) {
        printf("%02x",converted[i]);
    }
    printf("\n");
    inc_wifi_sent();
}

/**
 * Function used for send packet from source node to dst over wifi or using rpl
 *
 * @param c
 * @param data
 * @param datalen
 * @param to
 * @return
 */
int heterogenous_simple_udp_sendto(struct simple_udp_connection *c, const void *data, uint16_t datalen, const uip_ipaddr_t *to)
{
    uint8_t k_en, k_bw, k_etx;
    fill_keys(data, datalen, &k_en, &k_bw, &k_etx);
    flow_struct *flow = find_flow(to, k_en, k_bw, k_etx);

    if (!flow) {
        tech_struct *dst_technology = select_technology(k_en, k_bw, k_etx);
        flow = add_flow(to, dst_technology, k_en, k_bw, k_etx);
    }

    if (flow) {
        if (flow->flags & PND) {    // need to ask for route (pending flag active)
            PROCESS_CONTEXT_BEGIN(&serial_connection);
            ask_for_route(flow);
            PROCESS_CONTEXT_END();
        }
        if (flow->technology->type == RPL_TECHNOLOGY || !(flow->flags & CNF)) {    //dst tech is wifi or not approved wifi
            simple_udp_sendto(c, data, datalen, to);
            leds_on(RPL_SEND_LED);
            inc_sent_rpl();
#ifdef HETEROGENEOUS_STATISTICS
            stats.rpl_sent++;
#endif
        } else if (flow->technology->type == WIFI_TECHNOLOGY && (flow->flags & CNF == 1)) {
            send_packet_wifi(&src_ip, to, UIP_HTONS(c->remote_port), UIP_HTONS(c->local_port), data, datalen);
            leds_on(WIFI_SEND_LED);
#ifdef HETEROGENEOUS_STATISTICS
            stats.wifi_sent++;
#endif
        }
    }
    return 0;
}

/**
 * Function called from lower layer when node forwards data using rpl
 *
 * @return
 */
int heterogeneous_forwarding_callback() {
    uint8_t k_en, k_bw, k_etx;
    fill_keys(uip_appdata-4, uip_datalen(), &k_en, &k_bw, &k_etx);
    flow_struct *flow = find_flow(&(UIP_IP_BUF->destipaddr), k_en, k_bw, k_etx);

//    flow_struct *flow = get_flow(&(UIP_IP_BUF->destipaddr), uip_appdata-4, uip_datalen());
    if (!flow) {
        tech_struct *dst_technology = select_technology(k_en, k_bw, k_etx);
        flow = add_flow(&(UIP_IP_BUF->destipaddr), dst_technology, k_en, k_bw, k_etx);
    }

    if (flow) {
        if (flow->flags & PND) {    // need to ask for route (pending flag active)
            PROCESS_CONTEXT_BEGIN(&serial_connection);
            ask_for_route(flow);
            PROCESS_CONTEXT_END();
        }
        if (flow->technology->type == RPL_TECHNOLOGY || !(flow->flags & CNF)) {    //dst tech is wifi or not approved wifi
            leds_on(RPL_FORWARD_LED);
            inc_sent_rpl();
#ifdef HETEROGENEOUS_STATISTICS
            stats.rpl_forwarded_rpl++;
#endif
        add_from_flow(&(UIP_IP_BUF->srcipaddr), flow, RPL_TECHNOLOGY);
            return 1;
        } else if (flow->technology->type == WIFI_TECHNOLOGY && (flow->flags & CNF == 1)) {
            send_packet_wifi(&(UIP_IP_BUF->srcipaddr), &(UIP_IP_BUF->destipaddr), UIP_HTONS(UIP_IP_BUF->destport),
                             UIP_HTONS(UIP_IP_BUF->srcport), uip_appdata-4, (uint16_t)uip_datalen());
            leds_on(WIFI_FORWARD_LED);
#ifdef HETEROGENEOUS_STATISTICS
            stats.wifi_forwarded_rpl++;
#endif

        add_from_flow(&(UIP_IP_BUF->srcipaddr), flow, RPL_TECHNOLOGY);
            return 0;
        }
    }
    return 1;
}

#ifdef COAP_HETEROGENEOUS
/**
 *  This function uses er-coap example for making send decisions and sends
 *
 * @param c
 * @param data
 * @param len
 * @param toaddr
 * @param toport
 * @return
 */
int heterogeneous_udp_sendto(struct uip_udp_conn *c, const void *data, uint8_t len, const uip_ipaddr_t *toaddr, uint16_t toport) {
    PRINTF("Calling heterogeneous UDP sendto\n");
    uint8_t k_en, k_bw, k_etx;
    fill_keys(data, len, &k_en, &k_bw, &k_etx);
    flow_struct *flow = find_flow(toaddr, k_en, k_bw, k_etx);

    if (!flow) {
        tech_struct *dst_technology = select_technology(k_en, k_bw, k_etx);
        flow = add_flow(toaddr, dst_technology, k_en, k_bw, k_etx);
    }

    if (flow) {
        if (flow->flags & PND) {    // need to ask for route (pending flag active)
            PROCESS_CONTEXT_BEGIN(&serial_connection);
            ask_for_route(flow);
            PROCESS_CONTEXT_END();
        }
        if (flow->technology->type == RPL_TECHNOLOGY || !(flow->flags & CNF)) {    //dst tech is wifi or not approved wifi
//            simple_udp_sendto(c, data, datalen, to);
            uip_udp_packet_sendto(c, data, len, toaddr, toport);
            leds_on(RPL_SEND_LED);
            inc_sent_rpl();
#ifdef HETEROGENEOUS_STATISTICS
            stats.rpl_sent++;
#endif
        } else if (flow->technology->type == WIFI_TECHNOLOGY && (flow->flags & CNF == 1)) {
            send_packet_wifi(&src_ip, toaddr, UIP_HTONS(c->rport), UIP_HTONS(c->lport), data, len);
            leds_on(WIFI_SEND_LED);
#ifdef HETEROGENEOUS_STATISTICS
            stats.wifi_sent++;
#endif
        }
        flow->flags |= UP;
    }

    return 0;
}
#endif

#ifdef SIMPLE_UDP_HETEROGENEOUS
/**
 * Allows to register callback function of upper layer using simple udp
 *
 * @param c
 * @param local_port
 * @param remote_addr
 * @param remote_port
 * @param receive_callback
 * @return
 */
int heterogenous_udp_register(struct simple_udp_connection *c, uint16_t local_port, uip_ipaddr_t *remote_addr,
                              uint16_t remote_port, simple_udp_callback receive_callback) {
    receiver_callback = receive_callback;
    simple_udp_register(c, local_port, remote_addr, remote_port, receive_callback);
    return 0;
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
#endif

/**
 * Prints mote IPv6 address database in format !p;<addr1>;<addr2>; ...
 */
void print_src_ip() {
    printf("!r");
    uint8_t i;
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

#ifdef HETEROGENEOUS_STATISTICS
/**
 * Prints mote statistics about data sending
 */
void print_statistics_table() {
    printf("Rpl forwarded from rpl: %d\nRpl forwarded from Wifi: %d\nRpl sent: %d\nWifi forwarded from RPL: %d\nWifi forwarded from WiFi: %d\nWifi sent: %d\n",
           stats.rpl_forwarded_rpl, stats.rpl_forwarded_wifi, stats.rpl_sent, stats.wifi_forwarded_rpl, stats.wifi_forwarded_wifi, stats.rpl_sent);
}
#endif

/**
 * Returns node source global IPv6 address
 * @return
 */
uip_ipaddr_t *get_my_ip() {
    return &src_ip;
}

/**
 * Initialize module
 */
void init_module(uint8_t mode, const uip_ipaddr_t *ip) {
    PRINTF("Initializing module in mode: %d\n", mode);
    NETSTACK_MAC.off(1);
    leds_init();

    uip_ipaddr_copy(&src_ip, ip);
    tech_struct *rpl_tech = add_technology(RPL_TECHNOLOGY);
    set_callback(heterogeneous_forwarding_callback);
    device_mode = mode;

    printf("!b\n");
    print_src_ip();
    print_neighbours();

    add_metrics(rpl_tech, DEFAULT_RPL_EN, DEFAULT_RPL_BW, DEFAULT_RPL_ETX);
    process_start(&serial_connection, NULL);
    process_start(&blinker, NULL);
}

