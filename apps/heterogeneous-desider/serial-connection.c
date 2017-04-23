/*
 * Copyright (c) 2017, Tomas Hermanek.
 * All rights reserved.
 */


#include "serial-connection.h"
#include "heterogeneous-desider.h"

#include "contiki.h"
#include "dev/serial-line.h"
#include "net/ip/simple-udp.h"
#include "net/ip/uip.h"
#include "lib/memb.h"
#include "lib/list.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>


static char payload[50];       // ToDo refactor buff size -> size must be identical with uip_buff - headers
uint16_t sport, dport;
static int payload_len;
static uip_ipaddr_t sender_ip, receiver_ip;
struct simple_udp_connection *c = NULL;


/**
 * Parses IPv6 string address and returns uip_ipaddr_t structure
 * @param ip_string
 * @return
 */
uip_ipaddr_t *string_ipv6_to_uip_ipaddr(char *ip_string, uip_ipaddr_t *ipv6) {
    char *end_str, *token = strtok_r(ip_string, ":", &end_str);
    int ip[8], i = 0;

    while (token != NULL) {
        int number = (int)strtol(token, NULL, 16);
        token = strtok_r(NULL, ":", &end_str);
        ip[i] = number;
        i++;
    }
    uip_ip6addr(ipv6, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
    return ipv6;
}

/**
 * Parses incoming packet, which is in format <cmd>;<srcIp>;<dstIp>;<udp.sport>;<udp.dport>;<payload>
 *
 * @param data
 * @param len
 * @return
 */
int parse_incoming_packet(char *data, int len, uint16_t *sport, uint16_t *dport, struct simple_udp_connection *c,
                            char *payload, int *payload_len, uip_ipaddr_t *sender_ip, uip_ipaddr_t *receiver_ip, int i) {
    char *end_str, *token = strtok_r(data, ";", &end_str);
    int meta;

    while (token != NULL)  {
        switch (i) {
            case 0:
                meta =  strtol(token, &token, 10);
                break;
            case 1:
                string_ipv6_to_uip_ipaddr(token, sender_ip);
                break;
            case 2:
                string_ipv6_to_uip_ipaddr(token, receiver_ip);
                break;
            case 3:
                *sport = strtol(token, &token, 10);
                break;
            case 4:
                *dport = strtol(token, &token, 10);
                break;
            case 5:
                strcpy(payload, token);
                *payload_len = strlen(token) + 1;
                break;
            case 6:
                break;
        }
        token = strtok_r(NULL, ";", &end_str);
        i++;
    }

    for(uip_udp_conn = &uip_udp_conns[0]; uip_udp_conn < &uip_udp_conns[UIP_UDP_CONNS]; ++uip_udp_conn) {
        if(uip_udp_conn->lport != 0 &&
           UIP_HTONS(*dport) == uip_udp_conn->lport &&
           (uip_udp_conn->rport == 0 || UIP_HTONS(*sport) == uip_udp_conn->rport)) {
            c=uip_udp_conn;
        }
    }
    return meta;
}

/**
 * Allows to copy first digit int part and transforms it to int
 *
 * @param data
 * @param i
 * @param start
 * @param len
 * @return
 */
int parse_int_from_string(char *data, int *i, int start, int len) {
    char string[10];
    int j;

    for (j=start; j <= len; j++) {
        if (!isdigit(data[j])) {
            if (start < j) {
                strncpy (string, data + (start*sizeof(char)), (j - start));
                string[j-start] = '\0';
                *i = j-1;
                return strtol(string, &string, 10);;
            }
            else {
                return 0;
            }
        }
    }
    return 0;
}

/**
 * Handles commands sent over serial
 * w -> register wifi technology with metrics
 * p -> request to packet send (target may be this mote or another one)
 *
 * @param data
 * @param len
 * @return
 */
int handle_commands(char *data, int len) {
    if (data[1] == 'w') {
        tech_struct *wifi_tech = add_technology(WIFI_TECHNOLOGY);
        int i, en = 0, bw = 0, etx = 0;

        for (i=2;i <= len; i++) {
            if (data[i] == 'e') {
                en = parse_int_from_string(data, &i, i+1, len);
            }
            else if (data[i] == 'b') {
                bw = parse_int_from_string(data, &i, i+1, len);
            }
            else if (data[i] == 'x') {
                etx = parse_int_from_string(data, &i, i+1, len);
            }
        }
        add_metrics(wifi_tech, en, bw, etx);
        clear_flows();
        return 1;
    }
    else if (data[1] == 'p') {
        parse_incoming_packet(data, len, &sport, &dport, c, &payload, &payload_len, &sender_ip, &receiver_ip, 0);
        if (uip_ipaddr_cmp(&receiver_ip, get_my_ip()))
            heterogenous_udp_callback(c, &sender_ip, sport, &receiver_ip, dport, payload, payload_len);
        else {
            uip_udp_packet_forward(sender_ip, receiver_ip, sport, dport, &payload, payload_len);
            leds_on(RPL_FORWARD_LED);
        }
    }
    return 0;
}

/**
 * Function handles requests to print data
 * m -> prints metrics table
 * f -> prints flow table
 * s -> prints statistics
 *
 * @param data
 * @param len
 * @return
 */
int handle_prints(char *data, int len) {
    printf(PRINT_START_SYMBOL);
    if (data[1] == 'm') {
        print_metrics_table();
    } else if (data[1] == 'f') {
        print_flow_table();
    } else if (data[1] == 's') {
        print_statistics_table();
    }
    printf(PRINT_END_SYMBOL);
    return 1;
}

/**
 * Function handles incoming requests to contiki device
 * c -> request for configuration details
 * n -> request for neighbour table (used by root)
 *
 * @param data
 * @param len
 * @return
 */
int handle_requests(char *data, int len) {
    if (data[1] == 'c') {
        print_src_ip();
        print_mode();
        return 1;
    }
    else if (data[1] == 'n') {
        print_neighbours();
        return 1;
    }
    else if (data[1] == 'p') {
        int question_id;

        question_id = parse_incoming_packet(data, len, &sport, &dport, c, &payload, &payload_len, &sender_ip, &receiver_ip, -1);

        flow_struct *flow = get_flow(&receiver_ip, &payload, payload_len);
        // todo setup flags
        // todo add stats
        if (flow->technology->type == RPL_TECHNOLOGY) {
            //(c, &payload, payload_len, &receiver_ip);            // ToDo create new sendto fucntion which sets up src IP address correctly
            uip_udp_packet_forward(sender_ip, receiver_ip, sport, dport, &payload, payload_len);
            printf("$p;%d;0;\n", question_id);
            leds_on(RPL_FORWARD_LED);
        }
        else {
            printf("$p;%d;1;\n", question_id);
            leds_on(WIFI_FORWARD_LED);
        }
        return 1;
    }
    return 0;
}

/**
 * Function handles incoming responses to requests
 * p -> response for "ask_for_route" request
 *
 * @param data
 * @param len
 * @return
 */
int handle_responses(char *data, int len) {
    if (data[1] == 'p') {
        char *end_str, *token = strtok_r(data, ";", &end_str);
        int i = 0, num[3];

        while (token != NULL) {
            int number = (int)strtol(token, NULL, 10);
            token = strtok_r(NULL, ";", &end_str);
            num[i] = number;
            i++;
        }

        flow_struct *flow = find_flow_by_id(num[1], NULL);
        if (flow) {
            if (num[2] == 1) {
                flow->flags |= CNF;
            }
            else {
                flow->flags &= ~CNF;
            }
            flow->flags &= ~PND;
        }
        return 1;
    }
    return 0;
}

/**
 * Function handles input data and multiplexes them into separate functions differs them using symbols !#?$
 *
 * @param data
 * @return
 */
int handle_input(char *data) {
    int len = strlen(data);

    if (data[0] == '!') {
        return handle_commands(data, len);
    }
    else if (data[0] == '#') {
        return handle_prints(data, len);
    }
    else if (data[0] == '?') {
        return handle_requests(data, len);
    }
    else if (data[0] == '$') {
        return handle_responses(data, len);
    }
    return 0;
}

/**
 * Process which waits for serial line input
 */

PROCESS(serial_connection, "Heterogenous serial handler");

PROCESS_THREAD(serial_connection, ev, data)
{
    PROCESS_BEGIN();

    for(;;) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message) {
            handle_input((char *)data);
        }
    }
    PROCESS_END();
}