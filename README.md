<h2><strong>ESP8266 Dual Server Software</strong></h2>

This project supports client requests from both the serial port and http GET.

Setup:

1. Copy the ATnWebServer folder to your EspressIf SDK projects folder.

2. Change the WIFI_CLIENTSSID and WIFI_CLIENTPASSWORD values in the file user_config.h 
to match your local WIFI network.

#define WIFI_CLIENTSSID		"yourWIFIssid"
#define WIFI_CLIENTPASSWORD	"yourWIFIpassword"

Also, if your local area network subnet is different, also update TCPSERVERIP:

#define TCPSERVERIP			"192.168.0.106"

And the port if a different one is desired:

#define TCPSERVERPORT		9703

Operation:

The current settings (change as required and noted above to match your configuration):

ESP8266 Static IP: 192.168.0.106
ESP8266 Server Port: 9703
Router IP: 192.168.0.1

Serial port baud: 9600 bps 

While not necessary, the code assumes an LED is connected to GPIO16. This LED is ON upon 
power-up and is turned OFF once initialization completes.

A 1 second periodic timer function loop_cb() is used to read sensors. In the default state, 
the sensor readings are commented out and hard-coded values are set to facilitate initial code
checkout.

There are 8 sensors in the demo program. Two servers are available to retrieve the sensor 
readings:

1. Serial Port Server

Send the command: AT+GETSENSOR=n<cr><lf>

over the ESP8266 serial port.

where "n" is the sensor number (1-8)

Note: This server will also respond to the standard AT command set.

Example:

Send
AT+GETSENSOR=2<cr><lf>

Reply
79.2

2. Web Server:

Enter the following URL in a web browser (adjust IP & port to your settings):

http://192.168.0.106:9703/?request=GetSensors

A JSON string will be returned with the sensor values in this format:

{
"B_Pressure":"29.7",
"B_Temperature":"71.1",
"B_Altitude":"555.0",
"DS_TempInside":"79.5",
"DS_TempOutside":"79.2",
"DS_TempAttic":"88.5",
"DH_Humidity":"34.7",
"DH_Temperature":"69.1"
}
