
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "..\..\Credentials\CredentialsPvarela.h"
#include "..\..\Credentials\Blynk_Credentials.h"
#include "..\..\Credentials\ThingSpeak_Personal_Channels.h"
#include <ThingSpeak.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"
#define MANUAL_BUTTON					V0
#define TIME_DISPLAY					V1
#define TEMPERATURE_GAUGE				V2
#define CONSIGNA_SLIDER					V3
#define AUTOMATIC_BUTTON				V4
#define TIME_TOTAL_HOURS_DISPLAY		V5
#define CALENTADOR_RADIADOR_SELECTOR	V6

#define RADIADOR_TARGET		1
#define CALDERA_TARGET		0

//To be changed when RADIADOR TARGET is connected
//#define DEVICE_CONTROL_TARGET		CALDERA_TARGET
#define DEVICE_CONTROL_TARGET		RADIADOR_TARGET

int relay = D4;  //relay pin to esp8266 - 01 model gp2 pin in app
int timeSecondsRunning = 0;
long int timeSecondsTotalRunning = 0;
int timeHoursRunning = 0;
SimpleTimer timer;
int statusManual = 0;
int statusAutomatic = 0;
int currentStatus,oldStatus =0;
WiFiClient  client;
static float tempSalon = 10;
static float consigna = 20.0;
int statusCode;
int everySec = 1;
WiFiServer TelnetServer(8266);
int deviceControl = 0;
char* deviceControlLogic[2] = {"Caldera","Radiador"};

void Init_All_Controls()
{
	currentStatus = 0;
	oldStatus = 0;
	statusManual = 0;
	statusAutomatic = 0;

	Blynk.virtualWrite(MANUAL_BUTTON, 0);
	Blynk.virtualWrite(AUTOMATIC_BUTTON, 0);
	Blynk.virtualWrite(TIME_DISPLAY, 0);
	Blynk.setProperty(MANUAL_BUTTON, "color", BLYNK_GREEN);
	Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_GREEN);

	digitalWrite( relay, LOW);
}

void taskUpdateValues()
{
	Serial.printf("Target %s\n\r",deviceControlLogic[deviceControl]);
	if(deviceControl == DEVICE_CONTROL_TARGET)
	{
    	if((statusManual == 1) ||((statusAutomatic == 1)&&(tempSalon < consigna)))
		{
			timeSecondsRunning += 1;
			timeSecondsTotalRunning += 1;

			currentStatus = 1;
			if((statusAutomatic == 1)&&(tempSalon < consigna))
			{
				Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_YELLOW);
			}
		}
		else
		{
			currentStatus = 0;
			if((statusAutomatic == 1)&&(tempSalon >= consigna))
			{
				Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_GREEN);
			}
		}
		if((oldStatus == 0)&&(currentStatus == 1))
		{
			Serial.println ("Activating Heater!");
			digitalWrite( relay, HIGH);
			timeSecondsRunning = 0;
		}
		else if((oldStatus == 1)&&(currentStatus == 0))
		{
			Serial.println ("Deactivating Heater!");
			digitalWrite( relay, LOW);
		}
		oldStatus = currentStatus;
		if (--everySec == 0)
		{
			everySec = 1;
			if (statusCode == 200)
			{
			  Serial.printf("%ds,Temperature in Salon %.2f ºC (%s)\n\r",
					  timeSecondsTotalRunning,tempSalon,deviceControlLogic[deviceControl]);
			}
			else
			{
			  Serial.println("Unable to read channel / No internet connection");
			}
			Blynk.virtualWrite(TIME_DISPLAY, timeSecondsRunning/60);
			Blynk.virtualWrite(TEMPERATURE_GAUGE, tempSalon);
			Blynk.virtualWrite(TIME_TOTAL_HOURS_DISPLAY, timeSecondsTotalRunning/3600);
		}
	}
    else
    {
    	Serial.printf("Temperatura controlada por Radiador eléctrico...\n\r");
    }
}

BLYNK_CONNECTED() {
    Blynk.syncAll();
}

BLYNK_WRITE(AUTOMATIC_BUTTON)
{
	if(deviceControl == DEVICE_CONTROL_TARGET)
	{
		statusAutomatic = param.asInt(); // assigning incoming value from pin V4 to a variable

		if(statusAutomatic == 1)
		{
			Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_GREEN);
			timeSecondsRunning=0;
			statusManual = 0;
			Blynk.setProperty(MANUAL_BUTTON, "color", BLYNK_GREEN);
			Blynk.virtualWrite(MANUAL_BUTTON, 0);
		}
		else if(statusAutomatic == 0)
		{
			digitalWrite( relay, LOW);
			Blynk.setProperty(AUTOMATIC_BUTTON, "color",BLYNK_GREEN);
		}
	}
}

BLYNK_WRITE(CALENTADOR_RADIADOR_SELECTOR)
{
	deviceControl = param.asInt(); // assigning incoming value from pin V5 to a variable
	Serial.printf("Target changed to %s\n\r",deviceControlLogic[deviceControl]);
	Serial.println ("Deactivating Heater!");
	Init_All_Controls();
}


BLYNK_WRITE(CONSIGNA_SLIDER)
{
	if(deviceControl == DEVICE_CONTROL_TARGET)
	{
		consigna = param.asFloat(); // assigning incoming value from pin V3 to a variable
		Serial.printf("Consigna changed to %f, status automatic %d\n\r",consigna,statusAutomatic);
	}

}

BLYNK_WRITE(MANUAL_BUTTON)
{
	if(deviceControl == DEVICE_CONTROL_TARGET)
	{
		statusManual = param.asInt(); // assigning incoming value from pin V0 to a variable
	  if(statusManual == 1)
	  {
		Blynk.setProperty(MANUAL_BUTTON, "color", BLYNK_YELLOW);
		timeSecondsRunning=0;
		statusAutomatic = 0;
		Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_GREEN);
		Blynk.virtualWrite(AUTOMATIC_BUTTON, 0);


	  }
	  else if(statusManual == 0)
	  {
		Blynk.setProperty(MANUAL_BUTTON, "color",BLYNK_GREEN);
		Serial.println("Pin set to LOW");
	  }
	}
}

void setup()
{
	Serial.begin(115200);
	TelnetServer.begin();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	Serial.println("Connectiing to wifi...");
	if (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}
	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	Blynk.begin(auth, ssid, password);
	timer.setInterval(1000L, taskUpdateValues);
	pinMode(relay, OUTPUT);
	ThingSpeak.begin(client);
	Init_All_Controls();

	ArduinoOTA.onStart([]() {
	  Serial.println("OTA Start");
	});
	ArduinoOTA.onEnd([]() {
	  Serial.println("OTA End");
	  Serial.println("Rebooting...");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
	  Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
	  Serial.printf("Error[%u]: ", error);
	  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
	  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
	  else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
	  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
	  else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.begin();
}

//In the loop function include Blynk.run() command.
void loop()
{
	Blynk.run();
	timer.run();
	tempSalon = ThingSpeak.readFloatField(channelTempSalon, FieldNumber1, thingSpeakReadAPIKey_Salon);
	statusCode = ThingSpeak.getLastReadStatus();
	ArduinoOTA.handle();
}



