#include <stdlib.h>
#include <string.h>
#include "rest-engine.h"
#include "symbols.h"
#include "dev/dht22.h"
#include "dev/zoul-sensors.h"

static void res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_example,
         "title=\"Example res: ?len=0..\";rt=\"Text\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

extern int variable;//test

static void
res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
  printf("Entering example resource\n"); 
  int temperature = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
  printf("Temperature: %d mC", temperature);
  temperature=temperature/1000;
  const char *len = NULL;
  //char number[5]="123";
  variable--; //test
   /*test*/
  //snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%d", variable);

  snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%d", temperature);

  int length = 9; 
  if(REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if(length < 0) {
      length = 0;
    }
    if(length > REST_MAX_CHUNK_SIZE) {
      length = REST_MAX_CHUNK_SIZE;
    }
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); 
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}


