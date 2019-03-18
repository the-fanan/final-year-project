/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

ESP8266WiFiMulti WiFiMulti;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("dibia", "fanandala");
 // WiFiMulti.addAP("peanutthedestroyer", "Yesboss1");
}

void loop() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    WiFiClient client;

    HTTPClient http;

   //http connection begins here
    if (http.begin(client, "http://10.42.0.1/gun/3")) {  // HTTP
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);//this is the JSON data needed -- used print instead of println so no newline character will be returned and JSOn will be read properly
        }
      } else {
        Serial.printf("Unable to get page contents:  %s \n", http.errorToString(httpCode).c_str());
      }

      http.end();
      delay(5000);
    } else {
      Serial.println("Unable to connect to Host");
    }
  } else {
    Serial.println("Unable to connect to WiFi");
  }
}
