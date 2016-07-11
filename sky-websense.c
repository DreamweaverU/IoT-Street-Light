/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 */


#include "contiki.h"
#include "httpd-simple.h"
#include "dev/sht11/sht11-sensor.h"
#include "dev/light-sensor.h"
#include "dev/leds.h"
#include <stdio.h>
#include "powertrace.h"

//Collect view
/*
#include "net/rpl/rpl.h"
#include "dev/uart1.h"
#include "collect-common.h"
#include "collect-view.h"
#include "dev/serial-line.h"
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;
*/

PROCESS(web_sense_process, "Sense Web Demo");
PROCESS(webserver_nogui_process, "Web server");
PROCESS_THREAD(webserver_nogui_process, ev, data)
{
  PROCESS_BEGIN();

  httpd_init();

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
    httpd_appcall(data);
  }

  PROCESS_END();
}

AUTOSTART_PROCESSES(&web_sense_process,&webserver_nogui_process);//, &collect_common_process);

#define HISTORY 16
static int temperature[HISTORY];
static int light1[HISTORY];
static int sensors_pos;

/*---------------------------------------------------------------------------*/
static int
get_light(void)
{
  return 10 * light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) / 7;
}
/*---------------------------------------------------------------------------*/
static int
get_temp(void)
{
  return ((sht11_sensor.value(SHT11_SENSOR_TEMP) / 10) - 396) / 10;
}
/*---------------------------------------------------------------------------*/
static const char *TOP = "<html><head><title>Lighting Control Demo</title></head><body>\n";
static const char *BOTTOM = "</body></html>\n";
/*---------------------------------------------------------------------------*/
/* Only one single request at time */
static char buf[256];
static int blen;
#define ADD(...) do {                                                   \
    blen += snprintf(&buf[blen], sizeof(buf) - blen, __VA_ARGS__);      \
  } while(0)
static void
generate_chart(const char *title, const char *unit, int min, int max, int *values)
{
  int i;
  blen = 0;
  ADD("<h1>%s</h1>\n"
      "<img src=\"http://chart.apis.google.com/chart?"
      "cht=lc&chs=400x300&chxt=x,x,y,y&chxp=1,50|3,50&"
      "chxr=2,%d,%d|0,0,30&chds=%d,%d&chxl=1:|Time|3:|%s&chd=t:",
      title, min, max, min, max, unit);
  for(i = 0; i < HISTORY; i++) {
    ADD("%s%d", i > 0 ? "," : "", values[(sensors_pos + i) % HISTORY]);
  }
  ADD("\">");
}
static
PT_THREAD(send_values(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);

  SEND_STRING(&s->sout, TOP);

  if(strncmp(s->filename, "/index", 6) == 0 ||
     s->filename[1] == '\0') {
    /* Default page: show latest sensor values as text (does not
       require Internet connection to Google for charts). */
    blen = 0;
    ADD("<h1>Current readings</h1>\n"
        "Light: %u<br>"
        "Temperature: %u&deg; C",
        get_light(), get_temp());
    SEND_STRING(&s->sout, buf);

  } else if(s->filename[1] == '0') {
    /* Turn off leds */
    leds_off(LEDS_ALL);
    SEND_STRING(&s->sout, "Turned off leds!");

  } else if(s->filename[1] == '1') {
    /* Turn on leds */
    leds_on(LEDS_ALL);
    SEND_STRING(&s->sout, "Turned on leds!");

  } else {
    if(s->filename[1] != 't') {
      generate_chart("Light", "Light", 0, 500, light1);
      SEND_STRING(&s->sout, buf);
    }
    if(s->filename[1] != 'l') {
      generate_chart("Temperature", "Celsius", 15, 50, temperature);
      SEND_STRING(&s->sout, buf);
    }
  }

  SEND_STRING(&s->sout, BOTTOM);

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
httpd_simple_script_t
httpd_simple_get_script(const char *name)
{
  return send_values;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(web_sense_process, ev, data)
{
  static struct etimer timer;
  PROCESS_BEGIN();

  sensors_pos = 0;

  etimer_set(&timer, CLOCK_SECOND * 2);
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(sht11_sensor);

  /* Start powertracing, once every two seconds. */
  powertrace_start(CLOCK_SECOND * 5);   

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);

    light1[sensors_pos] = get_light();;
    temperature[sensors_pos] = get_temp();
    sensors_pos = (sensors_pos + 1) % HISTORY;
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

////////////////////////////
/*
void
collect_common_net_init(void)
{
#if CONTIKI_TARGET_Z1
  uart0_set_input(serial_line_input_byte);
#else
  uart1_set_input(serial_line_input_byte);
#endif
  serial_line_init();
}

void
collect_common_set_sink(void)
{
  
}

void
collect_common_net_print(void)
{

  rpl_dag_t *dag;
  uip_ds6_route_t *r;

  
  dag = rpl_get_any_dag();
  if(dag->preferred_parent != NULL) {
    PRINTF("Preferred parent: ");
    PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
    PRINTF("\n");
  }
  for(r = uip_ds6_route_head();
      r != NULL;
      r = uip_ds6_route_next(r)) {
    PRINT6ADDR(&r->ipaddr);
  }
  PRINTF("---\n");

}


void
collect_common_send(void)
{
  static uint8_t seqno;
  struct {
    uint8_t seqno;
    uint8_t for_alignment;
    struct collect_view_data_msg msg;
  } msg;
  // struct collect_neighbor *n; 
  uint16_t parent_etx;
  uint16_t rtmetric;
  uint16_t num_neighbors;
  uint16_t beacon_interval;
  rpl_parent_t *preferred_parent;
  linkaddr_t parent;
  rpl_dag_t *dag;

  if(client_conn == NULL) {
    // Not setup yet 
    return;
  }
  memset(&msg, 0, sizeof(msg));
  seqno++;
  if(seqno == 0) {
    // Wrap to 128 to identify restarts
    seqno = 128;
  }
  msg.seqno = seqno;

  linkaddr_copy(&parent, &linkaddr_null);
  parent_etx = 0;

  // Let's suppose we have only one instance
  dag = rpl_get_any_dag();
  if(dag != NULL) {
    preferred_parent = dag->preferred_parent;
    if(preferred_parent != NULL) {
      uip_ds6_nbr_t *nbr;
      nbr = uip_ds6_nbr_lookup(rpl_get_parent_ipaddr(preferred_parent));
      if(nbr != NULL) {
        // Use parts of the IPv6 address as the parent address, in reversed byte order.
        parent.u8[LINKADDR_SIZE - 1] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2];
        parent.u8[LINKADDR_SIZE - 2] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
        parent_etx = rpl_get_parent_rank((uip_lladdr_t *) uip_ds6_nbr_get_ll(nbr)) / 2;
      }
    }
    rtmetric = dag->rank;
    beacon_interval = (uint16_t) ((2L << dag->instance->dio_intcurrent) / 1000);
    num_neighbors = uip_ds6_nbr_num();
  } else {
    rtmetric = 0;
    beacon_interval = 0;
    num_neighbors = 0;
  }

  // num_neighbors = collect_neighbor_list_num(&tc.neighbor_list); 
  collect_view_construct_message(&msg.msg, &parent,
                                 parent_etx, rtmetric,
                                 num_neighbors, beacon_interval);

  //uip_udp_packet_sendto(client_conn, &msg, sizeof(msg),
  //                      &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
}
*/
