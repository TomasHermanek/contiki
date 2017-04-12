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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/**
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
 * Parses IPv6 string address and returns uip_ipaddr_t structure
 * @param ip_string
 * @return
 */
uip_ipaddr_t *string_ipv6_to_uip_ipaddr(char *ip_string) {
    static uip_ipaddr_t ipv6;
    char *end_str, *token = strtok_r(ip_string, ":", &end_str);
    int ip[8], i = 0;

    while (token != NULL) {
        int number = (int)strtol(token, NULL, 16);
        token = strtok_r(NULL, ";", &end_str);
        ip[i] = number;
        i++;
    }
    uip_ip6addr(&ipv6, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
    return &ipv6;
}

/**
 * Parses incoming packet, which is in format <cmd>;<srcIp>;<dstIp>;<udp.sport>;<udp.dport>;<payload>
 *
 * @param data
 * @param len
 * @return
 */
int parse_incoming_packet(char *data, int len, uint16_t *sport, uint16_t *dport, struct simple_udp_connection *c,
                            char *payload, int payload_len, const uip_ipaddr_t *sender_ip, const uip_ipaddr_t *receiver_ip) {
    int i = 0;
    printf("data %s\n", data);
    char *end_str, *token = strtok_r(data, ";", &end_str);

    while (token != NULL)  {
//        printf("address %d\n", token);
        printf("token %s\n", token);
//        printf("data %s\n", data);
//        printf("index %d\n", i);
//        printf("%d\n");
        switch (i) {
            case 0:
                break;
            case 1:
                sender_ip = string_ipv6_to_uip_ipaddr(token);
                break;
            case 2:
                receiver_ip = string_ipv6_to_uip_ipaddr(token);
                break;
            case 3:
                *sport = strtol(token, &token, 10);
                break;
            case 4:
                *dport = strtol(token, &token, 10);
                break;
            case 5:
                break;
            case 6:
                break;
        }
        token = strtok_r(NULL, ";", &end_str);
        i++;
    }

    printf("1 ");
    uip_debug_ipaddr_print(&sender_ip);
    printf("\n");

    for(uip_udp_conn = &uip_udp_conns[0]; uip_udp_conn < &uip_udp_conns[UIP_UDP_CONNS]; ++uip_udp_conn) {
        printf("connection no %d lport: %d rport: %d\n", uip_udp_conn, uip_udp_conn->lport, uip_udp_conn->rport);
        if(uip_udp_conn->lport != 0 &&
           UIP_HTONS(*dport) == uip_udp_conn->lport &&
           (uip_udp_conn->rport == 0 || UIP_HTONS(*sport) == uip_udp_conn->rport)) {
//           (uip_is_addr_unspecified(&uip_udp_conn->ripaddr) || uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &uip_udp_conn->ripaddr))) {
            printf("connection found");
            c=uip_udp_conn;
           // sender_ip = &uip_udp_conn->sipaddr;
//            sender_ip = &uip_udp_conn->ripaddr;
//            printf("printing IP: ");
//            uip_debug_ipaddr_print(&uip_udp_conn->ripaddr);
//            printf("\n");
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
        return 1;
    }
    else if (data[1] == 'p') {
        printf("Incoming packet from serial\n");
        uint16_t sport, dport;
        char *payload = NULL;
        int payload_len = 0;
        const uip_ipaddr_t *sender_ip = NULL, *receiver_ip=NULL;
        struct simple_udp_connection *c = NULL;

        parse_incoming_packet(data, len, &sport, &dport, c, payload, payload_len, sender_ip, receiver_ip);
        printf("2 ");
        uip_debug_ipaddr_print(&sender_ip);
        printf("\n");
        heterogenous_udp_callback(c, sender_ip, sport, receiver_ip, dport, payload, payload_len);

//        PROCESS_CONTEXT_BEGIN(c->client_process);
//        c->receive_callback(c,
//                            &(UIP_IP_BUF->srcipaddr),
//                            UIP_HTONS(UIP_IP_BUF->srcport),
//                            &(UIP_IP_BUF->destipaddr),
//                            UIP_HTONS(UIP_IP_BUF->destport),
//                            databuffer, uip_datalen());
//        PROCESS_CONTEXT_END();

    }
    return 0;
}

int handle_prints(char *data, int len) {
    if (data[1] == 'm') {
        print_metrics_table();
        return 1;
    }
    return 0;
}

int handle_requests(char *data, int len) {
    if (data[1] == 'c') {
        print_src_ip();
        print_mode();
        return 1;
    }
    if (data[1] == 'n') {
        print_neighbours();
        return 1;
    }
    return 0;
}

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
    return 0;
}


PROCESS(serial_connection, "Heterogenous serial handler");

PROCESS_THREAD(serial_connection, ev, data)
{
    PROCESS_BEGIN();
    printf("serial connection started\n");

    for(;;) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message) {
            //printf("received line: %s\n", (char *)data);
            handle_input((char *)data);
        }
    }
    PROCESS_END();
}