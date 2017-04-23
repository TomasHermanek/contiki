

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "symbols.h"
#include "net/rpl/rpl.h"


#if PLATFORM_HAS_BUTTON
#include "dev/button-sensor.h"
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


extern resource_t
  //res_hello,
  //res_mirror,
  //res_chunks,
  //res_separate,
  //res_push,
  //res_event,
  //res_sub,
  //res_b1_sep_b2;
  res_example;
#if PLATFORM_HAS_LEDS
//extern resource_t res_leds/*, res_toggle*/;
#endif
#if PLATFORM_HAS_LIGHT
//#include "dev/light-sensor.h"
//extern resource_t res_light;
#endif


extern int variable=100;

static void
create_rpl_dag(uip_ipaddr_t *ipaddr)
{
  struct uip_ds6_addr *root_if;

  root_if = uip_ds6_addr_lookup(ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    uip_ipaddr_t prefix;
    
    rpl_set_root(RPL_DEFAULT_INSTANCE, ipaddr);
    dag = rpl_get_any_dag();
    uip_ip6addr(&prefix, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &prefix, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
}

static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if(state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}

static uip_ipaddr_t *
set_global_address(void)
{
  static uip_ipaddr_t ipaddr;
  int i;	
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
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

PROCESS(coapv2_example_server, "Erbium CoAPv2 Example Server");
AUTOSTART_PROCESSES(&coapv2_example_server);

PROCESS_THREAD(coapv2_example_server, ev, data)
{
//printf("LL header: %u\n", UIP_LLH_LEN);
//active = 0;
  
  PROCESS_BEGIN();

  PROCESS_PAUSE();
uip_ipaddr_t *ipaddr;
ipaddr = set_global_address();

  create_rpl_dag(ipaddr);
#if PLATFORM_HAS_BUTTON
PRINTF("mam ho\n");
#endif
  //PRINTF("Erbium CoAPv2 Example Server\n");
//process_start(&setVar, NULL);
#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif
  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  //PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  printf("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);
//set_variable(100);
variable=100;
  /* Initialize the REST engine. */
printf("LL header: %u\n", UIP_LLH_LEN);
  rest_init_engine();
//tomas
  init_module();
//printf("LL header: %u\n", UIP_LLH_LEN);
  /*
   * Bind the resources to their Uri-Path.
   * WARNING: Activating twice only means alternate path, not two instances!
   * All static variables are the same for each URI path.
   */

rest_activate_resource(&res_example, "test/example");
 // rest_activate_resource(&res_hello, "test/hello");
/*  rest_activate_resource(&res_mirror, "debug/mirror"); */
/*  rest_activate_resource(&res_chunks, "test/chunks"); */
/*  rest_activate_resource(&res_separate, "test/separate"); */
  //rest_activate_resource(&res_push, "/test/push");
/*  rest_activate_resource(&res_event, "sensors/button"); */
/*  rest_activate_resource(&res_sub, "test/sub"); */
/*  rest_activate_resource(&res_b1_sep_b2, "test/b1sepb2"); */
#if PLATFORM_HAS_LEDS
/*  rest_activate_resource(&res_leds, "actuators/leds"); */
 //rest_activate_resource(&res_toggle, "actuators/toggle");
#endif
#if PLATFORM_HAS_LIGHT
 // rest_activate_resource(&res_light, "sensors/light"); 
 // SENSORS_ACTIVATE(light_sensor);  
#endif

SENSORS_ACTIVATE(button_sensor);
//print_local_addresses();
  /* Define application-specific events here. */
  while(1) {

//PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
//PROCESS_YIELD();
PROCESS_WAIT_EVENT();

#if PLATFORM_HAS_BUTTON
	
    if(ev == sensors_event && data == &button_sensor) {
      PRINTF("*******BUTTON*******\n");
	variable=variable-20;
      //res_event.trigger();

      //res_separate.resume();
    }
#endif 
  }  

  PROCESS_END();
}
