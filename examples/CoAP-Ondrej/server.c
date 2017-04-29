#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "symbols.h"
#include "net/rpl/rpl.h"
#include "dev/zoul-sensors.h"
#include "heterogeneous-desider.h"
#include "uip-debug.h"



#if PLATFORM_HAS_BUTTON
#include "dev/button-sensor.h"
#endif

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


extern resource_t res_example;

/*variable from symbols.h - used in example resource if temperature is not used*/
extern int variable=100; 

/**
 * Copied function - creating RPL tree.
 *
 */
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

/**
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

  PRINTF("IPv6 addresses: ");
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
  PROCESS_BEGIN();
  
  PROCESS_PAUSE();

  uip_ipaddr_t *ipaddr;
  ipaddr = set_global_address();
  create_rpl_dag(ipaddr);
  variable=100;
  /* Initialize the REST engine. */
  rest_init_engine();

  /*Initialize the lower layer*/
  init_module(MODE_NODE, ipaddr);

  /*Activate resource*/
  rest_activate_resource(&res_example, "test/example");

  /*Activate button*/
  SENSORS_ACTIVATE(button_sensor);

  while(1) {

    PROCESS_WAIT_EVENT();

    #if PLATFORM_HAS_BUTTON
      if((button_sensor.value(BUTTON_SENSOR_VALUE_TYPE_LEVEL) == BUTTON_SENSOR_PRESSED_LEVEL) && (ev == sensors_event) && (data == &button_sensor)) {
        PRINTF("BUTTON PRESSED\n");
	variable=variable-20;
      }
    #endif 
  }  

  PROCESS_END();
}
