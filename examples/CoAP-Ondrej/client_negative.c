/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) CoAP client example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */
/*
*
*   change by Ondrej
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "er-coap-engine.h"
#include "dev/button-sensor.h"
#include "symbols.h"
#include "heterogeneous-desider.h"
#include "uip-debug.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTFDEBUG(...) printf(__VA_ARGS__)
#define PRINT6ADDRDEBUG(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDRDEBUG(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTFDEBUG(...)
#define PRINT6ADDRDEBUG(addr)
#define PRINTLLADDRDEBUG(addr)
#endif

/*Ipv6 address of the server*/
//#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xfe80, 0, 0, 0, 0xc30c, 0x0000, 0x0000, 0x0002) 
#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xaaaa, 0, 0, 0, 0x0212, 0x4b00, 0x060d, 0xb25c)
//#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xaaaa, 0, 0, 0, 0x0212, 0x4b00, 0x060d, 0x6161)

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT + 1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

/*Interval of sending requests to server*/
#define TOGGLE_INTERVAL 8

PROCESS(coapv2_example_client, "Erbium CoAPv2 Example Client");
AUTOSTART_PROCESSES(&coapv2_example_client);

uip_ipaddr_t server_ipaddr;
static struct etimer et;

/*Resource URLs*/
char *resource_url = "/test/example";
char *resource_url2 = "/test/example2";
char *resource_url3 = "/test/example3";
char *resource_url4 = "/test/example4";

/**
 * 
 * Handler function used for handling new responses
 *
 */
void
response_handler(void *response)
{
  const uint8_t *chunk;
  int temp=0;
  char str[10];
  int len = coap_get_payload(response, &chunk);
  if(len>10)
  {
    printf("Too long for number\n");
    return;
  }
  if (((char *)chunk)[0]=='0')
  {
    temp=0;
  }
  else
  {
    sprintf(str,"%.*s", len, (char *)chunk);
    temp=atoi(str);
    if (temp==0)
      printf("It is not integer\n");
    else
    {
      printf("Temperature at server is: %d\n",temp);
      //if(temp>14) {//1,2
     //    coap_add_profile(resource_url, PROFILE_SPEED, 0, 0, server_ipaddr); //2
      //   printf("Profile Changed\n");//2
         //coap_set_profile(resource_url, request, &server_ipaddr); //2 //netreba
       // if(coap_change_profile_priority(resource_url, PROFILE_LOWPOWER, &server_ipaddr, 0)==0)   //1
        //  printf("Profile Changed\n");            //1
	//}//1,2 ee
        //else //1
       //   printf("Profile cannot be changed\n"); // 1
      //}//1,2
    }
  }
  
//test
  //printf("|%.*s", len, (char *)chunk);
  //printf("\n");
}

/**
 * 
 * Copied function -set global ipv6 address and print all ipv6 addresses used by node.
 *
 */
static uip_ipaddr_t *
set_global_address(void)
{
  static uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  PRINTFDEBUG("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }

  return &ipaddr;
}

PROCESS_THREAD(coapv2_example_client, ev, data)
{
  PROCESS_BEGIN();

  static coap_packet_t request[1];

  uip_ipaddr_t *ipaddr;
  ipaddr = set_global_address();

  /*Set ip adresses of server to server_ipaddr*/
  SERVER_NODE(&server_ipaddr);

  /* Initialize the REST engine. */
  coap_init_engine();

  /*Initialize the lower layer*/
  init_module(MODE_NODE, ipaddr);

  /*Set timer*/
  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);

  SENSORS_ACTIVATE(button_sensor);
  
  /*
   * Example of adding profile to enhanced CoAP
   * Every unique entry (unique combination of resource URL and server IP address) is saved in profiles
   * Possible profiles: PROFILE_LOWPOWER, PROFILE_SECURITY, PROFILE_RELIABILITY, PROFILE_SPEED, PROFILE_MULTIMEDIA
   * 4th argument: if 1 then profiles are equal, otherwise first profile has higher priority
   */    
  coap_add_profile(resource_url, PROFILE_LOWPOWER, 8, 0, server_ipaddr);
  //coap_add_profile(resource_url,PROFILE_SPEED, PROFILE_LOWPOWER, 0, server_ipaddr); //not equal
  //coap_add_profile(resource_url,PROFILE_LOWPOWER , PROFILE_SPEED, 0, server_ipaddr);
  //coap_add_profile(resource_url, PROFILE_SPEED, 0, 1, server_ipaddr);
  //coap_add_profile(resource_url, PROFILE_SECURITY, 0, 1, server_ipaddr);
  //coap_add_profile(resource_url3, PROFILE_SECURITY, PROFILE_SPEED, 1, server_ipaddr);
     
  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&et)) {
      printf("\n--Timer started--\n");
      
      coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);

      coap_set_header_uri_path(request, resource_url);
      
      /*Choose profile for request*/
      coap_set_profile(resource_url, request, &server_ipaddr);

      const char msg[] = "Example";

      coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);


/*printf("pred odoslanim: \n");
    int i=0;
    for (i = 0; i < 25; i++)
{
  unsigned char c = ((char*)request)[i] ;
  printf ("%02x ", c) ;
}
printf("\n");*/

  //printf("metriky: %d a %d\n", coap_pkt->metric[0],coap_pkt->metric[1]);

      COAP_BLOCKING_REQUEST(&server_ipaddr, REMOTE_PORT, request,
                            response_handler);

      etimer_reset(&et);

#if PLATFORM_HAS_BUTTON
    } else if((button_sensor.value(BUTTON_SENSOR_VALUE_TYPE_LEVEL) == BUTTON_SENSOR_PRESSED_LEVEL) && (ev == sensors_event) && (data == &button_sensor)) {
       coap_change_profile_priority(resource_url, PROFILE_LOWPOWER, &server_ipaddr, 1); //1==increase, 0==decrease
 //coap_add_profile(resource_url,PROFILE_SPEED, PROFILE_LOWPOWER, 0, server_ipaddr); 
//coap_add_profile(resource_url, PROFILE_LOWPOWER, 0, 0, server_ipaddr); //2
         //printf("Profile Changed\n");

#endif
    }
  }

  PROCESS_END();
}
