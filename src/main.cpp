
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
//#include "..\..\Credentials\CredentialsPvarela.h"
#include "../../Credentials/CredentialsLugo.h"
#include "..\..\Credentials\Blynk_Credentials.h"
#include "..\..\Credentials\ThingSpeak_Personal_Channels.h"
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ThingSpeak.h>
#include <WiFiUdp.h>
#include <Time.h>
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

#define HYSTERESIS	0.2

#define PERIOD_TO_READ_FROM_THINGSPEAK	30000

//Identification, to be published when UDP request it
WiFiUDP UDP;
char packet[255];
#define UDP_PORT 4000

// Initialize Telegram BOT
#define BOTtoken "683023753:AAFpv8x1nRmf1QMXdRPvK--XL9S2sI3HFgc"

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "675668025"
WiFiClientSecure clientS;
UniversalTelegramBot bot(BOTtoken, clientS);


//To be changed when RADIADOR TARGET is connected
//#define DEVICE_CONTROL_TARGET		CALDERA_TARGET
#define DEVICE_CONTROL_TARGET		RADIADOR_TARGET

int relay = D2;  //relay pin to esp8266 - 01 model gp2 pin in app
int boardLed = D4;
int timeSecondsRunning = 0;
long int timeSecondsTotalRunning = 0;
int timeHoursRunning = 0;
SimpleTimer timer;
int statusManual = 0;
int statusAutomatic = 0;
int currentStatus,oldStatus =0;
WiFiClient  client;
static float tempReferencia = 10;
static float consigna = 20.0;
int statusCode;
int everySec = 1;
WiFiServer TelnetServer(8266);
int deviceControl = 0;
char* deviceControlLogic[2] = {"Caldera","Radiador"};
long int timeCurrent=0;
long int timeLast=0;
// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

time_t SetDateTime(int y, int m, int d, int h, int mi, int s  )
{
	tmElements_t DateTime ;
	DateTime.Second = s;
	DateTime.Minute = mi;
	DateTime.Hour = h;
	DateTime.Day = d ;
	DateTime.Month = m ;
	DateTime.Year = y -1970 ;

	return makeTime(DateTime);
}

time_t convertToTime(String calTimestamp) {
  struct tm tm;
  String year = calTimestamp.substring(0, 4);
  String month = calTimestamp.substring(5, 7);
  if (month.startsWith("0")) {
    month = month.substring(1);
  }
  String day = calTimestamp.substring(8, 10);
  if (day.startsWith("0")) {
    month = day.substring(1);
  }
  tm.tm_year = year.toInt();
  tm.tm_mon = month.toInt();
  tm.tm_mday = day.toInt();
  String hour = calTimestamp.substring(11, 13);
  tm.tm_hour = hour.toInt();
  String min = calTimestamp.substring(14, 16);
  tm.tm_min = min .toInt();
  String sec = calTimestamp.substring(17, 19);
  tm.tm_sec = sec.toInt();

  return SetDateTime(tm.tm_year,tm.tm_mon, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Hola, " + from_name + ".\n";
      welcome += deviceDescription;
      bot.sendMessage(chat_id, welcome, "");
    }
  }
}

void botCheckMessages()
{
  if (millis() > lastTimeBotRan + botRequestDelay)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

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

	pinMode(relay, OUTPUT);
	pinMode(boardLed, OUTPUT);
	digitalWrite( relay, LOW);
	digitalWrite( boardLed, LOW);
	ThingSpeak.writeField(channelTempLugo, FieldCaldera,0, thingSpeakReadAPIKey_Lugo);
}

void taskUpdateValues()
{
	Serial.printf("Target %s\n\r",deviceControlLogic[deviceControl]);
	if(deviceControl == DEVICE_CONTROL_TARGET)
	{
    	if((statusManual == 1) ||((statusAutomatic == 1)&&(tempReferencia < (consigna - HYSTERESIS))))
		{
			timeSecondsRunning += 1;
			timeSecondsTotalRunning += 1;

			currentStatus = 1;
			if((statusAutomatic == 1)&&(tempReferencia < consigna))
			{
				Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_YELLOW);
			}
		}
		else
		{
			currentStatus = 0;
			if((statusAutomatic == 1)&&(tempReferencia >= (consigna + HYSTERESIS)))
			{
				Blynk.setProperty(AUTOMATIC_BUTTON, "color", BLYNK_GREEN);
			}
		}
		if((oldStatus == 0)&&(currentStatus == 1))
		{
			Serial.println ("Activating Heater!");
			digitalWrite( relay, HIGH);
			digitalWrite( boardLed,HIGH);
			ThingSpeak.writeField(channelTempLugo, FieldCaldera,currentStatus, thingSpeakReadAPIKey_Lugo);
			timeSecondsRunning = 0;
		}
		else if((oldStatus == 1)&&(currentStatus == 0))
		{
			Serial.println ("Deactivating Heater!");
			digitalWrite( relay, LOW);
			digitalWrite( boardLed,LOW);
			ThingSpeak.writeField(channelTempLugo, FieldCaldera,currentStatus, thingSpeakReadAPIKey_Lugo);
		}
		oldStatus = currentStatus;
		if (--everySec == 0)
		{
			everySec = 1;
			if (statusCode == 200)
			{
				Serial.printf("%ds,Temperature in Referencia (Lugo) %.2f ºC (%s)\n\r",
					  timeSecondsTotalRunning,tempReferencia,deviceControlLogic[deviceControl]);
				Blynk.virtualWrite(TIME_DISPLAY, timeSecondsRunning/60);
				Blynk.virtualWrite(TEMPERATURE_GAUGE, tempReferencia);
				Blynk.virtualWrite(TIME_TOTAL_HOURS_DISPLAY, timeSecondsTotalRunning/3600);
			}
			else
			{
			  Serial.println("Unable to read channel / No internet connection");
			}

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
			digitalWrite( boardLed,LOW);
			Blynk.setProperty(AUTOMATIC_BUTTON, "color",BLYNK_GREEN);
			ThingSpeak.writeField(channelTempLugo, FieldCaldera,0, thingSpeakReadAPIKey_Lugo);
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

	String botText;
	Serial.begin(115200);
	TelnetServer.begin();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	Serial.println("Connecting to wifi...");
	if (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}
	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	// Print ESP32 Local IP Address
	Serial.println(WiFi.localIP());
	Serial.print("MAC: ");
	Serial.println(WiFi.macAddress());
	bot.sendMessage(CHAT_ID, "Wifi connected (" + String(WiFi.localIP().toString())+", MAC "+WiFi.macAddress() +  ")");
	botText = deviceDescription + "Connected to " + (String)ssid + " (IP:" + WiFi.localIP().toString() + ", MAC: " + WiFi.macAddress() + ")";
	Serial.println(botText);
	bot.sendMessage(CHAT_ID, botText, "");

	Blynk.begin(auth, ssid, password);
	timer.setInterval(1000L, taskUpdateValues);

	ThingSpeak.begin(client);
	Init_All_Controls();
	tempReferencia = ThingSpeak.readFloatField(channelTempLugo, FieldNumber1, thingSpeakReadAPIKey_Lugo);
	statusCode = ThingSpeak.getLastReadStatus();
	if (statusCode == 200)
	{
		timeLast = millis();
	}
	else
	{
		timeLast = 0;
	}
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

	// Begin listening to UDP port
	UDP.begin(UDP_PORT);
	Serial.print("Listening on UDP port ");
	Serial.println(UDP_PORT);
}

void IdentificationRequest()
{
	deviceDescription = deviceDescription + " Mi IP es "+ String(WiFi.localIP().toString());

	// If packet received...
	int packetSize = UDP.parsePacket();
	char buffer[100];
	if (packetSize)
	{
		int len = UDP.read(packet, 255);
		if (len > 0)
		{
		  packet[len] = '\0';
		}
		Serial.print("Packet received: ");
		Serial.println(packet);
		String packetS = packet;

		if (packetS.equals("Soy Pablo. Quien eres?"))
		{
			// Send return packet
			UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
			deviceDescription.toCharArray(buffer,100);
			UDP.write(buffer);
			UDP.endPacket();
		}
		else
		{
			// Send return packet
			UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
			unknownConnection.toCharArray(buffer,100);
			UDP.write(buffer);
			UDP.endPacket();
		}
	}
}

//In the loop function include Blynk.run() command.
void loop()
{
	Blynk.run();
	timer.run();
	timeCurrent = millis();
	if(timeCurrent >= timeLast)
	{
		if((timeCurrent-timeLast) >= PERIOD_TO_READ_FROM_THINGSPEAK)
		{
			Serial.println("Getting data from ThingSpeak");
			tempReferencia = ThingSpeak.readFloatField(channelTempLugo, FieldNumber1, thingSpeakReadAPIKey_Lugo);
			statusCode = ThingSpeak.getLastReadStatus();
			timeLast = timeCurrent;
			ThingSpeak.writeField(channelTempLugo, FieldCaldera,currentStatus, thingSpeakReadAPIKey_Lugo);
		}
	}
	else
	{
		if((timeLast - timeCurrent) >= PERIOD_TO_READ_FROM_THINGSPEAK)
		{
			tempReferencia = ThingSpeak.readFloatField(channelTempLugo, FieldNumber1, thingSpeakReadAPIKey_Lugo);
			statusCode = ThingSpeak.getLastReadStatus();
			timeLast = timeCurrent;
		}
	}
	time_t lastSampleSent;
	String tempReferenceTime;
	tempReferenceTime = ThingSpeak.readCreatedAt(channelTempLugo, thingSpeakReadAPIKey_Lugo);
	lastSampleSent = convertToTime(tempReferenceTime);
	Blynk.virtualWrite(V7, tempReferenceTime);
	ArduinoOTA.handle();
	botCheckMessages();
	IdentificationRequest();
}



