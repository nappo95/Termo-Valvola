/***************************************************
  Adafruit MQTT Library ESP8266 Example

  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino

  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DallasTemperature.h"
#include "OneWire.h"
#include "conf.h"

/************************* WiFi Access Point *********************************/

//#define WLAN_SSID       "...your SSID..."
//#define WLAN_PASS       "...your password..."


IPAddress device_ip;

/*************************   MQTT  Setup    *********************************/

// #define AIO_SERVER      "ip"
// #define AIO_SERVERPORT  1883                   // use 8883 for SSL
// #define AIO_USERNAME    "...your AIO username (see https://accounts.adafruit.com)..."
// #define AIO_KEY         "...your AIO key..."
 #define CLIENT_ID       "c4467c02-5283-4b9a-86e3-6a1d364f4633"
// #define CLIENT_DESC     ""

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, CLIENT_ID , AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish termo_satate = Adafruit_MQTT_Publish(&mqtt, CLIENT_DESC "/feeds/termo_satate");
Adafruit_MQTT_Publish temperatura = Adafruit_MQTT_Publish(&mqtt, CLIENT_DESC "/feeds/temperatura");
Adafruit_MQTT_Publish logs = Adafruit_MQTT_Publish(&mqtt, CLIENT_DESC "/feeds/logs");
Adafruit_MQTT_Publish device_state = Adafruit_MQTT_Publish(&mqtt, CLIENT_DESC "/feeds/info");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoff_valvola = Adafruit_MQTT_Subscribe(&mqtt, CLIENT_DESC "/feeds/onoff");
Adafruit_MQTT_Subscribe get_info = Adafruit_MQTT_Subscribe(&mqtt, CLIENT_DESC "/feeds/onoff");


/**************************** Termometer *************************************/

// #define ONE_WIRE_BUS D4

// #define VALVE_PIN   0

// #define LED_PIN  0

// #define VALVE_NORMAL_STATE "off"

OneWire oneWire(ONE_WIRE_BUS);
 
DallasTemperature sensors(&oneWire); 

float read_temperrature = 0;

bool valve_state = false;

/*************************** Sketch Code ************************************/



// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();
void SwitchValveState();
void PublishInfo();

void setup() {
  Serial.begin(115200);
  delay(10);

  sensors.begin();
  pinMode(VALVE_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.println(F("Termo-Valvola"));

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  device_ip = WiFi.localIP();
  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&onoff_valvola);
  mqtt.subscribe(&get_info);
}

uint32_t x=0;

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  sensors.requestTemperatures();                // Send the command to get temperatures  
  Serial.println("Temperature is: ");
  read_temperrature = sensors.getTempCByIndex(0);
  Serial.println(read_temperrature);   


  // publish the state of the valve
  if(! termo_satate.publish(valve_state))
    Serial.println(F("Failed"));
  else 
    Serial.println(F("OK!"));


  // publish the read temperature
  if(! temperatura.publish(read_temperrature))
    Serial.println(F("Failed"));
  else 
    Serial.println(F("OK!"));


  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(60000))) {
    if (subscription == &onoff_valvola) {
      Serial.print(F("Got: "));
      String message = (char *)onoff_valvola.lastread;
      Serial.println(message);

      if(message == "set-on" && valve_state == false)
        SwitchValveState();
      if(message == "set-off" && valve_state == true)
        SwitchValveState();
    }

    if (subscription == &get_info)
      PublishInfo();

  }


  // if (! photocell.publish(x++)) {
  //   Serial.println(F("Failed"));
  // } else {
  //   Serial.println(F("OK!"));
  // }

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  */
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

void SwitchValveState()
{
  if (valve_state)
  {
    if(strcmp(VALVE_NORMAL_STATE, "off") == 0)
    {
      digitalWrite(VALVE_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
    }
    else
    {
      digitalWrite(VALVE_PIN, HIGH);
      digitalWrite(LED_PIN, LOW);
    }
  }
  else
  {
    if(strcmp(VALVE_NORMAL_STATE, "off") == 0)
    {
      digitalWrite(VALVE_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
    }
    else
    {
      digitalWrite(VALVE_PIN, LOW);
      digitalWrite(LED_PIN, HIGH); 
    }
  }  
}

void PublishInfo()
{
}
  