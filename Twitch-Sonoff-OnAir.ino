//
// Sonoff-Twitch-Streaming, Alex Tatistcheff, Jan 2018
//
// This sketch uses a Sonoff Basic (http://sonoff.itead.cc/en/products/sonoff/sonoff-basic) switch
// to check the twitch.tv API for the streaming status of a specific user.  If the user is online
// the relay will close.  Perfect for your "On Air" light.  Since the Sonoff is based on the ESP-8266
// it is easily reprogrammed through the serial connections on the board.  This does require soldering 
// a pin header to the Sonoff Basic.
//
// The LED will light on the Sonoff when the relay is closed and will flash each time the twitch API is
// checked for status.  You can also close the relay by pressing the button if the user is offline. 
// Once the relay is closed in this manner the ESP-8266 will stop querying Twitch.tv.  Press again and
// the relay will deactivate and the Sonoff will again start checking twitch.tv for the stream status.
//
//  Also includes the ablity to update the Sonoff OTA so you can update without having to crack the 
//  case open after your initial programming.  See:  https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/
//

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
//#include <ArduinoJson.h>
//#include <WiFiClientSecure.h>

#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>

//
// The name of this project.
//
// Used for:
//   Access-Point name, in config-mode
//   OTA name.
//
#define PROJECT_NAME "SONOFF-TWITCH"

const char* host = "0.0.0.0"; // This is the host running the Python3 companion script
const int tcpPort = 1234;       // Port the script is listening on

const char* twitchUser = "baxcast";

int checkInterval = 10000;  // How often to check the twitch API in ms.  Throttle rate is 30 requests/min so keep it over 2000 to be safe.
int onAirCounter = 0;
String twitchStatus = "empty";
long last_read = 0;
String streamActive = "inactive";
String onlineStatus = "offline";

// For button control and debounce
int nowButtonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// The Sonoff button is GPIO0
// The Sonoff relay is GPIO12 
// Green LED is GPIO13  (the LED is bass-ackwards HIGH = OFF, LOW = ON)
// Spare (pin 5) is GPIO14

const int relayPin = 12;
const int buttonPin = 0;
const int LEDPin = 13;
bool onAirFlag = false;
bool btnOverride = false;

void setup()
{

    // Enable our serial port.
    Serial.begin(115200);

    pinMode(relayPin, OUTPUT); // This is the Sonoff relay pin
    pinMode(LEDPin, OUTPUT);   // the LED pin
    pinMode(buttonPin, INPUT); // the button

    digitalWrite(LEDPin, HIGH); // start with LED off

    //
    // Handle WiFi setup
    //
    WiFiManager wifiManager;

    // Uncomment for testing wifi manager
    //wifiManager.resetSettings();
  
    //set custom ip for portal
    //wifiManager.setAPStaticIPConfig(IPAddress(192,168,1,4), IPAddress(192,168,1,4), IPAddress(255,255,255,0));
    wifiManager.autoConnect(PROJECT_NAME);
    
    ArduinoOTA.setHostname(PROJECT_NAME);
    ArduinoOTA.onStart([]()
    {
        Serial.println("OTA Start");
    });
    ArduinoOTA.onEnd([]()
    {
        Serial.println("OTA End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        char buf[32];
        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "Upgrade - %02u%%", (progress / (total / 100)));
        Serial.println(buf);
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.println("Error - ");
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });

    //
    // Ensure the OTA process is running & listening.
    //
    ArduinoOTA.begin();
}

void loop()
{

   // Handle any pending over the air updates.
   ArduinoOTA.handle();

   // Enter the if loop every checkInterval millisec
   long now = millis();
   if ((last_read == 0) || (abs(now - last_read) > checkInterval)) {
      if (btnOverride == false) {  // Check to see if user has pressed button
         twitchStatus = getTwitchStatus(twitchUser);       
         Serial.println("Twitch status is: " + twitchStatus);
         last_read = now;
         if (twitchStatus == "live") {
            onAirCounter = 0;
            if (onAirFlag == false) {  // last time we checked we were off air  
               Serial.println("Light and relay - ON");
               // Turn on the light
               digitalWrite(LEDPin, LOW);
               // Trigger the relay
               digitalWrite(relayPin, HIGH);
               onAirFlag = true;
            }
         } else {
            if (onAirFlag == true) {  // last time we checked we were on air   
               if (onAirCounter > 1)  {  // check the counter, we'll give it a few checks before turning off the light
                  // Turn off the light
                  Serial.println("Light and relay - OFF");
                  digitalWrite(LEDPin, HIGH);
                  digitalWrite(relayPin, LOW);
                  onAirFlag = false;
                  onAirCounter = 0;
               } else {
                  onAirCounter++;
               }
            }  //onAirFlag
         }
      }   // btnOverride
   }

   // Button debounce routine
   int btnReading = digitalRead(buttonPin);
   if (btnReading != lastButtonState) { lastDebounceTime = millis(); }
        
   if ((millis() - lastDebounceTime) > debounceDelay) {
      if (btnReading != nowButtonState) {
         nowButtonState = btnReading;
         // We've now detected a button state change (High or Low)
         if (nowButtonState == HIGH) {    // Don't care if we are detecting the button release, just want the press
          
            // The first time you press the button, if the Sonoff is OFF it will turn on the relay.
            // We will also set a flag that the state has been overriden by the user so let's not 
            // check the Twitch.tv site.  
            // Upon pressing the button again, we will return to the normal state of checking the API

            if ((onAirFlag == false) && (btnOverride == false)) {  // Check if we are off air and haven't pressed the button before
               btnOverride = true;    // Set the override flag
               Serial.println("Light and relay - ON (btn)");
               digitalWrite(LEDPin, LOW);
               digitalWrite(relayPin, HIGH);
            } else if ((onAirFlag == false) && (btnOverride == true)) {  // Check if we are off air and have previously pressed the button
               btnOverride = false;    // Set the override flag
               Serial.println("Light and relay - OFF (btn)");
               digitalWrite(LEDPin, HIGH);
               digitalWrite(relayPin, LOW);
            }
         }  // nowButtonState == HIGH
      }
               
   }  // End of button changed code
   lastButtonState = btnReading;
}
   
String getTwitchStatus (String twitchUserName) {

  WiFiClient client;

  if (!client.connect(host, tcpPort)) {
    Serial.println(">>> tcp connection failed!");
    delay(2000);
    return ">>> tcp connection failed!";
  }

  client.println(twitchUserName);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) { //wait five seconds, if no response: wait 30 seconds and return to start of loop
      Serial.println(">>> Client Timeout !");
      client.stop();
      return ">>> Client Timeout";
    }
  }
  
  Serial.print(">>> RX!: ");

  while(client.available()){
    streamActive = client.readStringUntil('\n');
    Serial.println(streamActive);

    client.stop();
    Serial.println();
    Serial.println(">>> Disconnecting.");

  }

  if(streamActive=="1") {
    Serial.println(twitchUserName+"'s Stream is LIVE!");
    onlineStatus = "live";
    //maybe do something with the blinkPin?
    //Go crazy with some LEDs perhaps?
  } else {
    Serial.println(twitchUserName+"'s Stream is not live..");
    onlineStatus = "offline";
  }
  
    
  return onlineStatus;

}

/*

  DynamicJsonBuffer jsonBuffer;
    
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return "Error";
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("Certificate matches");
  } else {
    Serial.println("Warning: Certificate doesn't match! Possible MITM.");
  }

  // For testing when user is not online, just get the first one I find
  //String url = "/helix/streams?first=1";   

  String url = "/helix/streams?user_login=" + twitchUsername;  
  Serial.print("requesting URL: ");
  Serial.println(url);

  int LEDState=digitalRead(LEDPin);
  if (LEDState == HIGH)  {  // If the LED is off
     digitalWrite(LEDPin, LOW);  // turn it on for a quick flash then back off
     delay(50);
     digitalWrite(LEDPin, HIGH);
  }
  
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Client-ID: " + clientID + "\r\n" +
               "User-Agent: Sonoff-ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  //Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.println("line = " + line);
  
  // Parse the response "line" with JSON
  JsonObject& root = jsonBuffer.parseObject(line);

  //int timeZone = root[String("[\"meta\"][\"code\"]")];
  String onlineStatus = root["data"][0]["type"];
  String viewerCount = root["data"][0]["viewer_count"];
  
  Serial.println("onlineStatus = " + onlineStatus);
  //erial.println("ViewerCount = " + viewerCount);  // Might use this in the future sometime
                                                   // If you had a display you could show the number of viewers too!
  return onlineStatus;
}

*/
