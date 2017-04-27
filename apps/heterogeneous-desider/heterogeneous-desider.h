/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */

#ifndef CONTIKI_HETEROGENEOUS_DESIDER_H
#define CONTIKI_HETEROGENEOUS_DESIDER_H

#include "net/ip/uip.h"

#ifdef COAP_HETEROGENEOUS
#include "er-coap.h"
#include "er-coap-engine.h"
#endif

#define SIMPLE_UDP_HETEROGENEOUS 1

#define WIFI_TECHNOLOGY 1
#define RPL_TECHNOLOGY 2

#define MAX_TECHNOLOGIES 2
#define MAX_FLOWS 6

#define MODE_ROOT 1
#define MODE_NODE 2

/**
 * RED 0x01,GREEN 0x02,YELLOW 0x03,BLUE 0x04,PURPLE 0x05,AQUA 0x06,WHITE 0x07
 */

#define RPL_SEND_LED 0x04
#define WIFI_SEND_LED 0x02
#define RPL_FORWARD_LED 0x06
#define WIFI_FORWARD_LED 0x03
#define RPL_RECEIVE_LED 0x05
#define WIFI_RECEIVE_LED 0x01

#ifdef HETEROGENEOUS_STATISTICS
/**
 * Simple structure for handling basic database
 */
struct statistics {
    short rpl_sent;
    short rpl_forwarded_rpl;
    short rpl_forwarded_wifi;
    short wifi_sent;
    short wifi_forwarded_rpl;
    short wifi_forwarded_wifi;
} statistics;
#endif

/**
 * Structure defines technology unit
 */
typedef struct tech_struct {
    struct tech_struct *next;
    uint8_t type;
} tech_struct;

/**
 * Structure defines metrics unit
 */
typedef struct metrics_struct {
    struct metrics_struct *next;
    struct tech_struct *technology;
    uint8_t energy;
    uint8_t bandwidth;
    uint8_t etx;
} metrics_struct;

#define FLOW_VALIDITY 255

/**
 * flags structure -> |x|x|x|x|x|UP|PND|CNF
 * CNF -> if flow was confirmed by linux device (used by wifi technology)
 * PND -> pending, if contiki waits for response to confirmation
 * UP -> decision was made on packet send/forwarding upward
 */
#define CNF 0x01
#define PND 0x02
#define UP 0x04

/**
 * Structure that represents flow
 */
typedef struct flow_struct {
    struct flow_struct *next;
    uint8_t flow_id;
    uip_ipaddr_t to;
    uint8_t energy;
    uint8_t bandwidth;
    uint8_t etx;
    uint8_t validity;
    struct tech_struct *technology;
    char flags;
} flow_struct;

/**
 * \brief       Adds technology to technology table
 * @param type
 * @return
 */
tech_struct *add_technology(uint8_t type);

/**
 * \brief       Adds metrics to metrics table
 *
 * @param technology
 * @param energy
 * @param bandwidth
 * @param etx
 */
void add_metrics(struct tech_struct *technology, uint8_t energy, uint8_t bandwidth, uint8_t etx);

#endif //CONTIKI_HETEROGENEOUS_DESIDER_H
