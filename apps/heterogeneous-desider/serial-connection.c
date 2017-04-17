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

static short question_id = 0;
const short MAX_QUESTION_ID = 10;

static char payload[150];       // ToDo refactor buff size -> size must be identical with uip_buff - headers
uint16_t sport, dport;
static int payload_len;
static uip_ipaddr_t sender_ip, receiver_ip;
struct simple_udp_connection *c = NULL;

LIST(question_list);
MEMB(question_memb, struct question_struct, MAX_SIMULTANEOUS_QUESTIONS);

/**
 * Generates new question_id, if it reaches limit, question_id starts again from 1
 *
 * @return
 */
int get_question_id() {
    if (question_id > MAX_QUESTION_ID)
        question_id = 1;
    else
        question_id++;
    return question_id;
}

/**
 * Function finds question in question_list using two individual filters
 *
 * @param question_id
 * @param to
 * @return
 */
question_struct *find_question(int question_id, uip_ipaddr_t *to) {
    struct question_struct *s;

    for(s = list_head(question_list); s != NULL; s = list_item_next(s)) {
        if (uip_ipaddr_cmp(to, &s->flow->to) || question_id == s->question_id)
            return s;
    }
    return NULL;
}

/**
 * Removes oldest question
 */
void clear_oldest_question() {
    struct question_struct *s = list_head(question_list);

    list_remove(question_list, s);
    memb_free(&question_memb, s);
}

/**
 * Adds new question to question_list
 *
 * @param flow
 * @return
 */
question_struct *add_question(flow_struct *flow)  {
    short question_id = get_question_id();
    struct question_struct *question;

    question = memb_alloc(&question_memb);

    while (question == NULL) {
        clear_oldest_question();
        question = memb_alloc(&question_memb);
    }

    question->question_id = question_id;
    question->flow = flow;

    list_add(question_list, question);
    return question;
}

/**
 * Function will ask for route validity through serial line (if target route is available using WiFi tech)
 *
 * @param flow
 */
void ask_for_route(flow_struct *flow){
    question_struct *question = find_question(-1, &flow->to);

    if (!question)
        question = add_question(flow);

    printf("?p;%d;", question->question_id);
    uip_debug_ipaddr_print(&flow->to);
    printf("\n");
    return 1;
}

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
//           (uip_is_addr_unspecified(&uip_udp_conn->ripaddr) || uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &uip_udp_conn->ripaddr))) {
//            printf("connection found");
            c=uip_udp_conn;
           // sender_ip = &uip_udp_conn->sipaddr;
//            sender_ip = &uip_udp_conn->ripaddr;
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
        heterogenous_udp_callback(c, &sender_ip, sport, &receiver_ip, dport, payload, payload_len);
    }
    return 0;
}

/**
 * Function handles requests to print data
 * m -> prints metrics table
 * f -> prints flow table
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
        if (flow->technology->type == RPL_TECHNOLOGY) {
            simple_udp_sendto(c, &payload, payload_len, &receiver_ip);            // ToDo create new sendto fucntion which sets up src IP address correctly
            leds_on(RPL_FORWARD_LED);
            printf("$p;%d;0", question_id);
        }
        else {
            leds_on(WIFI_FORWARD_LED);
            printf("$p;%d;1", question_id);
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
        question_struct *question = find_question(num[1], NULL);
        if (question) {
            if (!question->flow) {
                list_remove(question_list, question);
                memb_free(&question_memb, question);
                printf("Question dropped because flow was removed");
                return 1;
            }
            if (num[2] == 1) {
                question->flow->flags |= CNF;
            }
            else {
                question->flow->flags &= ~CNF;
            }
            question->flow->flags &= ~PND;
            list_remove(question_list, question);
            memb_free(&question_memb, question);
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