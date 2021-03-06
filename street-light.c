/* The code is derived from the original file with the following disclaimer */
/* Author: Ryan Hu */

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
#include "dev/light-sensor.h"
#include "dev/leds.h"
#include <stdio.h>
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "simple-udp.h"
#include "random.h"

PROCESS(sensor_process, "Sensor process");
PROCESS(webserver_nogui_process, "Web server");

/************************************************************
 * Web server process
 ************************************************************/
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

AUTOSTART_PROCESSES(&sensor_process, &webserver_nogui_process);

#define UDP_PORT 1000
#define HISTORY 16		//Totoal number of historical data points
static int proximity[HISTORY];
static int light1[HISTORY];
static int hdata_pos;
static int old_hdata_pos; 
static int flag;
/************************************************************
 * Get the light flux and proximity value from the original readings
 ************************************************************/
static int
get_light(void)
{
  return 10 * light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) / 7;
}  

static int
get_proxi(void)
{
  return random_rand() % 2;	//This is a simulated value of a proximity sensor
}

/************************************************************
 * Define the HTML strings for the web page
 ************************************************************/
static const char *TOP = "<html><head><title>IoT Street Light Control Example</title></head><body>\n";
static const char *BOTTOM = "</body></html>\n";

/************************************************************
 * Define the HTML content buffer and ADD macro
 ************************************************************/
static char buf[256];
static int blen;
#define ADD(...) do {                                                   \
    blen += snprintf(&buf[blen], sizeof(buf) - blen, __VA_ARGS__);      \
  } while(0)
	  
/************************************************************
 * Add the chart contents to the HTML buffer
 ************************************************************/
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
    ADD("%s%d", i > 0 ? "," : "", values[(hdata_pos + i) % HISTORY]);
  }
  ADD("\">");
}

/************************************************************
 * Define the protothread to respond to HTTP GET request with the 
 * contents corresponding to the request parameters
 ************************************************************/
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
        "Prixmity: %u ",
        get_light(), get_proxi());
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
      generate_chart("Proximity", "True/False", 0, 2, proximity);
      SEND_STRING(&s->sout, buf);
    }
  }

  SEND_STRING(&s->sout, BOTTOM);

  PSOCK_END(&s->sout);
}


httpd_simple_script_t
httpd_simple_get_script(const char *name)
{
  return send_values;
}

/************************************************************
 * Define the variables and function for broadcast
 ************************************************************/
static struct simple_udp_connection broadcast_connection;
static char ip_buf[33];

static void
print_ipv6_addr(const uip_ipaddr_t *ip_addr) {
    int i;
	int len = 0;
	
    for (i = 0; i < 16; i++) {
        
	len += snprintf(&ip_buf[len], sizeof(ip_buf)-len, "%02x", ip_addr->u8[i]);
    }

	ip_buf[32] ='\0';
}

static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  print_ipv6_addr(sender_addr);
  printf("Data received on port %d from [%s]:port %d with length %d\n",
         receiver_port, ip_buf, sender_port, datalen);
  
  
}


/************************************************************
 * Define the notification function
 ************************************************************/
static void 
notify_adjacent_nodes(int i, uip_ipaddr_t addr)
{   
	if(i == 1) {
		printf("Value changed. Sending broadcast notification...");
		uip_create_linklocal_allnodes_mcast(&addr); 
		simple_udp_sendto(&broadcast_connection, "DIM_LIGHT", 9, &addr);
	}

}

/************************************************************
 * Define the sensor_process process
 ************************************************************/
PROCESS_THREAD(sensor_process, ev, data)
{
  static struct etimer timer;
  static uip_ipaddr_t addr;
  
  PROCESS_BEGIN();

  hdata_pos = 0;	//reset the historical data point position to 0
  old_hdata_pos = -1;

  etimer_set(&timer, CLOCK_SECOND * 2); //set the reading every 2 s
  SENSORS_ACTIVATE(light_sensor);

  simple_udp_register(&broadcast_connection, UDP_PORT,
                      NULL, UDP_PORT,
                      receiver);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);

    light1[hdata_pos] = get_light();
    proximity[hdata_pos] = get_proxi();
    old_hdata_pos = hdata_pos;
	
	//Check if there is a change in the proximity value
	if(old_hdata_pos != -1) {
		if(proximity[old_hdata_pos] == 0 && proximity[hdata_pos] == 1) {
			notify_adjacent_nodes( proximity[hdata_pos], addr);
		}
	}

    old_hdata_pos = hdata_pos;
    hdata_pos = (hdata_pos + 1) % HISTORY;
	
  }

  PROCESS_END();
}

