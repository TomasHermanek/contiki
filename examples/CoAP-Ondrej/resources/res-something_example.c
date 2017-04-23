#include <stdlib.h>
#include <string.h>
#include "rest-engine.h"
#include "symbols.h"

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

static void res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);


RESOURCE(res_example,
         "title=\"Example res: ?len=0..\";rt=\"Text\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

extern int variable;

static void
res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
//printf("Entering example resource\n");
  const char *len = NULL;
  char number[5]="123";
variable--;
//printf("2\n");
//char str[15];
sprintf(number, "%d", variable);
//printf("3\n");
  char const *const message = concat("Value: ", number);
  int length = 15; 
//printf("4\n");
  if(REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if(length < 0) {
      length = 0;
    }
    if(length > REST_MAX_CHUNK_SIZE) {
      length = REST_MAX_CHUNK_SIZE;
    }
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  } REST.set_header_content_type(response, REST.type.TEXT_PLAIN); 
//printf("5\n");
free(message);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
//printf("6\n");
  REST.set_response_payload(response, buffer, length);
//printf("7\n");
}


