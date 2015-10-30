/*
 * File	: user_main.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "at.h"

#include "user_interface.h"
#include "user_config.h"

#include "user_devicefind.h"
#include "user_webserver.h"
#include "c_types.h"
#include "espconn.h"
#include "../../ThirdParty/include/lwipopts.h"

#include <os_type.h>
#include <gpio.h>
#include "driver/i2c.h"
#include "driver/i2c_bmp180.h"
#include "driver/dht22.h"
#include "driver/gpio16.h"
#include "driver/ds18b20.h"

#define sleepms(x) os_delay_us(x*1000);

extern uint8_t at_wifiMode;
extern void user_esp_platform_load_param(void *param, uint16 len);

#define DELAY 1000 /* milliseconds */

LOCAL os_timer_t loop_timer;
extern int ets_uart_printf(const char *fmt, ...);
extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);
extern void wdt_feed (void);

extern uint8_t at_wifiMode;
//extern void user_esp_platform_load_param(void *param, uint16 len);

typedef enum {
	WIFI_CONNECTING,
	WIFI_CONNECTING_ERROR,
	WIFI_CONNECTED,
	TCP_DISCONNECTED,
	TCP_CONNECTING,
	TCP_CONNECTING_ERROR,
	TCP_CONNECTED,
	TCP_SENDING_DATA_ERROR,
	TCP_SENT_DATA
} tConnState;

struct espconn Conn;
esp_tcp ConnTcp;
extern int ets_uart_printf(const char *fmt, ...);
int (*console_printf)(const char *fmt, ...) = ets_uart_printf;
static char macaddr[6];
//mDNS
static char szH[40],szS[10],szN[30];
struct mdns_info thismdns;

static ETSTimer WiFiLinker;
static tConnState connState = WIFI_CONNECTING;
//static unsigned char tcpReconCount;

LOCAL os_timer_t loop_timer;
LOCAL nTcnt=0;
char tInside[8],tOutside[8],tAttic[8],tDht11[8],hDht11[8],tBmp085[8],pBmp085[8],aBmp085[8];
static int tI=630;
extern uint8 device_recon_count;
extern struct espconn user_conn;

static void platform_reconnect(struct espconn *);
static void wifi_check_ip(void *arg);
LOCAL void ICACHE_FLASH_ATTR loop_cb(void *arg);

#define DEGREES_C 0
#define DEGREES_F 1

const char *WiFiMode[] =
{
		"NULL",		// 0x00
		"STATION",	// 0x01
		"SOFTAP", 	// 0x02
		"STATIONAP"	// 0x03
};

#ifdef PLATFORM_DEBUG
// enum espconn state, see file /include/lwip/api/err.c
const char *sEspconnErr[] =
{
		"Ok",                    // ERR_OK          0
		"Out of memory error",   // ERR_MEM        -1
		"Buffer error",          // ERR_BUF        -2
		"Timeout",               // ERR_TIMEOUT    -3
		"Routing problem",       // ERR_RTE        -4
		"Operation in progress", // ERR_INPROGRESS -5
		"Illegal value",         // ERR_VAL        -6
		"Operation would block", // ERR_WOULDBLOCK -7
		"Connection aborted",    // ERR_ABRT       -8
		"Connection reset",      // ERR_RST        -9
		"Connection closed",     // ERR_CLSD       -10
		"Not connected",         // ERR_CONN       -11
		"Illegal argument",      // ERR_ARG        -12
		"Address in use",        // ERR_USE        -13
		"Low-level netif error", // ERR_IF         -14
		"Already connected"      // ERR_ISCONN     -15
};
#endif


static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	struct ip_info ipConfig;
	os_timer_disarm(&WiFiLinker);
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			wifi_get_ip_info(STATION_IF, &ipConfig);
			if(ipConfig.ip.addr != 0) {
				connState = WIFI_CONNECTED;
				#ifdef PLATFORM_DEBUG
				ets_uart_printf("WiFi connected\r\n");
				ets_uart_printf("IP: %08X\r\n",ipConfig.ip.addr);
				//Initialize mDNS Responder
				strcpy(szH,DNS_SVR);
				thismdns.host_name = &szH[0];
				strcpy(szS,DNS_SVR_NAME);
				thismdns.server_name = szS;
				thismdns.server_port =SERVER_PORT;
				strcpy(szN,DNS_TXTDATA);
				thismdns.txt_data[0] = szN;
				thismdns.ipAddr = ipConfig.ip.addr;
				espconn_mdns_init(&thismdns);
				//espconn_mdns_server_register();
				//espconn_mdns_enable();

				#endif
				connState = TCP_CONNECTING;
				//startwebserver
				sleepms(1000);
				user_webserver_init(SERVER_PORT);
				os_timer_disarm(&loop_timer);
				os_timer_setfn(&loop_timer, (os_timer_func_t *)loop_cb, (void *)0);
				os_timer_arm(&loop_timer, DELAY, 10);

				////senddata();
				return;
			}
			break;
		case STATION_WRONG_PASSWORD:
			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, wrong password\r\n");
			#endif
			break;
		case STATION_NO_AP_FOUND:
			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, ap not found\r\n");
			#endif
			break;
		case STATION_CONNECT_FAIL:
			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting fail\r\n");
			#endif
			break;
		default:
			connState = WIFI_CONNECTING;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting...\r\n");
			#endif
	}
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);
}

static void ICACHE_FLASH_ATTR get_temp_ds18b20(int sensor, int units, char * temp)
{
	int r, i;
	uint8_t addr[9], data[12];
	ds_init();
	switch(sensor) {
		case 1:
			//Sensor 1 (Replace with unique 8-byte ds18b20 Address)
			addr[0]=0x10;
			addr[1]=0x2E;
			addr[2]=0x4B;
			addr[3]=0x2F;
			addr[4]=0x00;
			addr[5]=0x08;
			addr[6]=0x00;
			addr[7]=0x5B;
			break;
		case 2:
			//Sensor 2 (Replace with unique 8-byte ds18b20 Address)
			addr[0]=0x10;
			addr[1]=0x13;
			addr[2]=0x45;
			addr[3]=0x2F;
			addr[4]=0x00;
			addr[5]=0x08;
			addr[6]=0x00;
			addr[7]=0x5E;
			break;
		default:
		    //No Sensor Address set
			break;
	}
	// perform the conversion
	ds_reset();
	select(addr);

	ds_write(DS1820_CONVERT_T, 1); // perform temperature conversion

	sleepms(1000); // sleep 1s

	ds_reset();
	select(addr);
	ds_write(DS1820_READ_SCRATCHPAD, 0); // read scratchpad

	for(i = 0; i < 9; i++)
	{
		data[i] = ds_read();
	}
	ds_reset();

	int HighByte, LowByte, TReading, SignBit, Tc_100, Whole, Fract, WholeF, FractF;
	LowByte = data[0];
	HighByte = data[1];
	TReading = (HighByte << 8) + LowByte;
	SignBit = TReading & 0x8000;  // test most sig bit
	if (SignBit) // negative
		TReading = (TReading ^ 0xffff) + 1; // 2's comp

	//Celsius
	Whole = (( ( (int)((data[1]<<8) | data[0]) * 0.5) + (data[6]/16) )*100) -25;
	Fract = Whole%100;
	Whole = Whole/100;

	//Fahrenheit
	WholeF = ((SignBit==0) ? 1 : -1) * ((Whole * 9)/5) + 32;
	if(WholeF>0) {
		SignBit=0;
	}
	FractF = ((Fract * 9)/5);
	if(units==DEGREES_C) {
		os_sprintf(temp,"%c%d.%d", SignBit ? '-' : '+', Whole, Fract < 10 ? 0 : Fract < 100 ? Fract/10 : Fract/100);
	}
	else {
		os_sprintf(temp,"%c%d.%d", SignBit ? '-' : '+', WholeF, FractF < 10 ? 0 : FractF < 100 ? FractF/10 : FractF/100);
	}
}


static void ICACHE_FLASH_ATTR setup_wifi_st_mode(void)
{
	wifi_set_opmode((wifi_get_opmode()|STATIONAP_MODE)&USE_WIFI_MODE);
	struct station_config stconfig;
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", WIFI_CLIENTSSID);
		os_sprintf(stconfig.password, "%s", WIFI_CLIENTPASSWORD);
		if(!wifi_station_set_config(&stconfig))
		{
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("ESP8266 not set station config!\r\n");
			#endif
		}
	}
    //Configure IP
	wifi_station_dhcpc_stop();
    LOCAL struct ip_info info;
    IP4_ADDR(&info.ip, 192, 168, 0, 174);
    IP4_ADDR(&info.gw, 192, 168, 0, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    wifi_set_ip_info( STATION_IF, &info);
    ets_uart_printf("ESP8266 Broadcast if:%d\r\n",wifi_get_broadcast_if());
    //wifi_set_broadcast_if(3);
    //ets_uart_printf("ESP8266 Broadcast if:%d\r\n",wifi_get_broadcast_if());
    sleepms(1000);
    wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);
	#ifdef PLATFORM_DEBUG
	ets_uart_printf("ESP8266 in STA mode configured.\r\n");
	#endif
}

void loop_cb(void *arg)
{
	char szT[32];
	DHT_Sensor DHsensor;
	DHT_Sensor_Data data;

	DHsensor.pin = 5;  //GPIO14
	DHsensor.type = DHT11;

    int32_t temperature;
    int32_t pressure;

    /*********************************************************************
     *The following code populates the Sensor values for test
     *purposes. This code should be un-commented if no sensors are
     *available and it is desired to test out the operation of this code.
     *THE INSIDE TEMPERATURE IS INCREMENTED BY 1.5 DEGREES EACH ITERATION
     *IN ORDER TO CONFIRM THIS FUNCTION IS CALLED AT A 1 Hz RATE.
     *********************************************************************/
    /*********************************************************************/
    if(tI<800) {
    	tI += 15;
    }
    else {
    	tI = 600;
    }
    os_sprintf(tInside,"%d.%d",tI/10,tI%10);
	os_sprintf(tOutside,"%s","79.2");
	os_sprintf(tAttic,"%s","88.5");
	os_sprintf(tDht11,"%s","69.1");
	os_sprintf(hDht11,"%s","34.7");
	os_sprintf(pBmp085,"%s","29.7");
	os_sprintf(tBmp085,"%s","71.1");
	os_sprintf(aBmp085,"%s","555.0");
	/***************** end of test sensor code ****************************/
    //---------------------------------------------------
    //This state machine reads 1 sensor each iteration
    //---------------------------------------------------
    /*
	switch(nTcnt%5) {
		case 0: //Read first DS18B20 Temperature Sensor
			get_temp_ds18b20(1,1,tInside);
			break;
		case 1: //Read second DS18B20 Temperature Sensor
			get_temp_ds18b20(2,1,tOutside);
			break;
		case 2: //Read third DS18B20 Temperature Sensor
			get_temp_ds18b20(3,1,tAttic);
			break;
		case 3: //Read DHT11 temperature and humidity Sensor
			DHTRead(&DHsensor, &data);
			DHTFloat2String(tDht11, ((9/5) * data.temperature)+32);
			DHTFloat2String(hDht11, data.humidity);
			break;
		case 4: //Read BMP085 Temperature and pressure Sensor
		    temperature = BMP180_GetTemperature();
		    pressure = BMP180_GetPressure(OSS_0);
	    	os_sprintf(pBmp085,"%ld.%01d", pressure/3386,(pressure%3386)/1000);
	    	os_sprintf(tBmp085,"%ld.%01d", ((temperature*18)/100) + 32,(temperature*18)%100);
	    	os_sprintf(aBmp085,"%03d", 328 * (BMP180_CalcAltitude(pressure)/100000));
			break;
		default:
			break;
	}
	*/
}

void user_rf_pre_init(void)
{
	system_phy_set_rfoption(2);
}

void user_init(void)
{
    uint8_t userbin;
    uint32_t upFlag;
    at_uartType tempUart;

    // Initialize System
    user_esp_platform_load_param((uint32 *)&tempUart, sizeof(at_uartType));
	os_delay_us(2000000); //wait 2 seconds for system to stabilize
	ets_wdt_enable();
	ets_wdt_disable();

	// Initialize Serial Port
	if(tempUart.saved == 1)
    {
        uart_init(tempUart.baud, BIT_RATE_9600);
    }
        else
    {
        uart_init(BIT_RATE_9600, BIT_RATE_9600);
    }

    // AT Server Initialization
    at_wifiMode = wifi_get_opmode();
    at_init();

    // WIFI Initialization
  	setup_wifi_st_mode();

  	wifi_get_macaddr(STATION_IF, macaddr);
	if(wifi_get_phy_mode() != PHY_MODE_11N)
		wifi_set_phy_mode(PHY_MODE_11N);
	if(wifi_station_get_auto_connect() == 0)
		wifi_station_set_auto_connect(1);

  	struct station_config stationConfig;
  	if(wifi_station_get_config(&stationConfig)) {
  		console_printf("STA config: SSID: %s, PASSWORD: %s\r\n",
  			stationConfig.ssid,
  			stationConfig.password);
  	}
  	// Wait for Wi-Fi connection and start TCP connection
  	os_timer_disarm(&WiFiLinker);
  	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
  	os_timer_arm(&WiFiLinker, 1000, 0);

    //Turn off Green Status LED indicating initialization is complete
    gpio16_output_conf();
    gpio16_output_set(0);

}
