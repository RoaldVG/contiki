/**
 * \file
 *         White (observer) sender program for the dual network.
 *         This program sends a messsage to the white sink whenever it is trigered via GPIO pin 1.0 by the connected black mote.
 *
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Roald Van Glabbeek
 * 		   March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly)
 */


// imported libraries.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "core/net/netstack.h"
#include "cpu/cc2538/dev/gpio.h"
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

//#include "dev/battery-sensor.h"

/*
 * receiver mote id. (needs to be set in function of the destination)
 */
uint8_t receiver0 = 0xe4; //This is normally the white sink
uint8_t receiver1 = 0xcc;   //In this network the second byte of the rime addresses is not used.
linkaddr_t destination;

/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;

/*
 * The sender mote id is set automatically from the rime address.
 */
uint8_t sender;          

/* sender power
 * possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
 *                    -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3;
 */ 
uint8_t power = 31;

/*
 * sent message counter
 */
uint16_t send = 0 ;

/*
 * Interval between consecutive probes of the triger bit P1.0
 */

#define ADC_READ_INTERVAL (CLOCK_SECOND/128)

// Bit-width of IO communication with observer
#define IO_WIDTH 11

/* 
 * Data structure of sent messages
 */
uint16_t whiteseqno=0;
uint32_t  ADCResult=0;
uint32_t  counter=0;
uint8_t   flag;

struct whitemsg {
	uint16_t blackseqno;
	uint16_t whiteseqno;
	uint32_t energy;
	uint16_t counter_ADC;
	uint16_t timestamp_app;
	uint16_t timestamp_mac;
};

void prepare_payload(void);
static void recv_uc(struct unicast_conn *c, const linkaddr_t *from);
static void sent_uc(struct unicast_conn *c, int status, int num_tx);

static const struct unicast_callbacks unicast_callbacks = {sent_uc, recv_uc};
static struct unicast_conn uc;

PROCESS(temp_process, "Sending messages to the white sink about a black mote");
AUTOSTART_PROCESSES(&temp_process);

/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/
void msg_callback(uint8_t port, uint8_t pin){
	prepare_payload();
	unicast_send(&uc, &destination);

	ADCResult=0;
	counter=0;
}
void
GPIOS_init(void)
{
	GPIO_SET_INPUT(GPIO_A_BASE,GPIO_PIN_MASK(6));		//GPIO PA6
  
 	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(1));		//GPIO PC1
  	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(2));		//GPIO PC2
  	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(3));		//GPIO PC3
	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(5));		//GPIO PC5
  	GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(6));		//GPIO PC6

	GPIO_SET_INPUT(GPIO_D_BASE,GPIO_PIN_MASK(0));		//GPIO PD0
  	GPIO_SET_INPUT(GPIO_D_BASE,GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_SET_INPUT(GPIO_D_BASE,GPIO_PIN_MASK(2));		//GPIO PD2

	GPIO_SOFTWARE_CONTROL(GPIO_A_BASE,GPIO_PIN_MASK(7));
	GPIO_SET_INPUT(GPIO_A_BASE,GPIO_PIN_MASK(7));
	GPIO_DETECT_EDGE(GPIO_A_BASE,GPIO_PIN_MASK(7));
	//GPIO_TRIGGER_SINGLE_EDGE(GPIO_A_BASE,GPIO_PIN_MASK(7));
	GPIO_TRIGGER_BOTH_EDGES(GPIO_A_BASE,GPIO_PIN_MASK(7));
	//GPIO_DETECT_RISING(GPIO_A_BASE,GPIO_PIN_MASK(7));
	GPIO_ENABLE_INTERRUPT(GPIO_A_BASE,GPIO_PIN_MASK(7));
	gpio_register_callback(msg_callback, 0, 7);
}

uint16_t
read_GPIOS(void)
{
	//reading the value in each pin
	uint16_t  blackseqno=0;

	if (GPIO_READ_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6)))	blackseqno=blackseqno+1;		
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0)))    blackseqno=blackseqno+2;
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1)))	blackseqno=blackseqno+4; 
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2)))	blackseqno=blackseqno+8;
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3)))	blackseqno=blackseqno+16; 
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4)))	blackseqno=blackseqno+32; 
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5)))    blackseqno=blackseqno+64;
	if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6)))	blackseqno=blackseqno+128; 
	if (GPIO_READ_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0)))	blackseqno=blackseqno+256;
	if (GPIO_READ_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1)))	blackseqno=blackseqno+512; 
	if (GPIO_READ_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2)))	blackseqno=blackseqno+1024;

	return blackseqno;
}

/*--------------------------------------------------------------------------------
 * Receiver function which seems necessary to operate the sender with a normal MAC
 *-------------------------------------------------------------------------------*/

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	/*PRINTF("unicast message received from %d.%d\n",
			from->u8[0], from->u8[1]);*/
}

static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  	if(linkaddr_cmp(dest, &linkaddr_null)) {
    	return;
  	}
}

/*---------------------------------------------------------------------------*/
void
prepare_payload( void )
{
	whiteseqno++;

	/*Set general info*/
	struct whitemsg msg;

	msg.blackseqno = read_GPIOS();	
	msg.whiteseqno = whiteseqno;	
	msg.energy = ADCResult;
	msg.counter_ADC = counter;
	msg.timestamp_app = RTIMER_NOW();
	msg.timestamp_mac = 0;

	packetbuf_copyfrom(&msg, sizeof(msg));
        
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
			PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

	printf("Sending packet %d with seqno %d to %x.%x\n",msg.whiteseqno,msg.blackseqno, destination.u8[0],destination.u8[1]);
}
/*---------------------------------------------------------------------------*/
void
packet_send(void)
{
	linkaddr_t destination;
	whiteseqno++;
	// define the destination address
	destination.u8[0] = receiver0;
	destination.u8[1] = receiver1;


	/*Set general info*/
	struct whitemsg msg;

        msg.blackseqno = read_GPIOS();	
    	msg.whiteseqno = whiteseqno;	
        msg.energy = ADCResult;
        msg.counter_ADC = counter;
        msg.timestamp_app = RTIMER_NOW();
        msg.timestamp_mac = 0;        

	/*Avoid sending to itself*/
	if(!linkaddr_cmp(&destination, &linkaddr_node_addr))
	{
	    packetbuf_copyfrom(&msg, sizeof(msg));
        
	    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
				PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

	    unicast_send(&uc, &destination);

            PRINTF(" Black= %u White= %u Energy= %li counter_ADC = %u App.time= %u\n",
                     msg.blackseqno,msg.whiteseqno,msg.energy,msg.counter_ADC,msg.timestamp_app);

            
	}
}
/*-------------------------------------------------------------------------------
 * Send packets with all needed statistic information
 *-------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------*/

static struct etimer et;

// Sending thread

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();
	//SENSORS_ACTIVATE(battery_sensor);

	//flag=(P1IN & BIT0);
	flag=(GPIO_READ_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2)));

	// adjust power
	//cc2420_set_txpower(power);
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, power);

	//tmp102_init();
	adc_init();
	adc_zoul.configure(SENSORS_HW_INIT,ZOUL_SENSORS_ADC3);
  	adc_zoul.configure(ZOUL_SENSORS_CONFIGURE_TYPE_DECIMATION_RATE, SOC_ADC_ADCCON_DIV_64);

	GPIOS_init();
	counter = 0;

	unicast_open(&uc, channel, &unicast_callbacks);
	// infinite loop
	destination.u8[0] = receiver0;
	destination.u8[1] = receiver1;

	/* Configure the ADC ports */
  	adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC2);
	adc_zoul.configure(ZOUL_SENSORS_CONFIGURE_TYPE_DECIMATION_RATE, SOC_ADC_ADCCON_DIV_64);
	while(1)
	{
		//    wait for the ADC_READ_INTERVAL time
		etimer_set(&et, ADC_READ_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		counter++;
		int ADC_val = adc_zoul.value(ZOUL_SENSORS_ADC2);
		//printf("%d\n",ADC_val);
		ADCResult += ADC_val;
		//PRINTF("%d\n",ADC_val);
		etimer_reset(&et);
	}
	PROCESS_END();
}
