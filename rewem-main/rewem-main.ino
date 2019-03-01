#include <ArduinoJson.h>
#include <Servo.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
//Define serials, pins and objects
SoftwareSerial esp8266Module(3, 2); // RX, TX
SoftwareSerial gpsModule(9, 8);
Servo triggerLock; 
TinyGPSPlus gps;
const int emergencyButton = 4;
const int servoPin = 5;
const int redLED = 10;
const int blueLED = 11;
const int greenLED = 12;
const int openPosition = 90;
const int closePosition = 0;
//#define PN532_IRQ   (6)
//#define PN532_RESET (7)
Adafruit_PN532 nfc(6, 7);
 //Globals defined here
 double userLatitude;
 double userLongitude;
 bool gpsDataIsValid = false;
 bool espDataIsAvailable = false;
 bool gpsPass = false;
 bool rfidPass = false;
 bool emergencyAllowPass = false;
 bool emergencyAllowIsActive = false;
 int emergencyAllowsUsed = 0;
 unsigned long lastEmergencyAllowInitiation = 0;
 int buttonState; 
void setup() {
  Serial.begin(115200);
  esp8266Module.begin(115200);
  gpsModule.begin(9600);
  pinMode(emergencyButton, INPUT);
  triggerLock.attach(servoPin);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  //display 
  //rfid
  nfc.begin();
  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  nfc.setPassiveActivationRetries(0x9);
  // configure board to read RFID tags
  nfc.SAMConfig();
}
void loop() {
  DynamicJsonDocument doc(300);
  /**
   * Read data retruned from WiFi module and configure necessary parameter
   */
  while (esp8266Module.available() > 0) {
    esp8266Module.read();
  }
  //String espResponse = esp8266Module.readString();
  espDataIsAvailable = true;
  char testData[] = "{\"gr\":0,\"ea\":10,\"ed\":30,\"edu\":\"second\",\"lt\":0,\"lg\":0,\"r\":1341752731}";
//getting error here because of data size
//  Serial.println(testData.length());
  DeserializationError error = deserializeJson(doc, testData);
  if (error) {
    Serial.println("there was error");
  }
  JsonObject espResponseData = doc.as<JsonObject>();
  /**
   * Read data retruned from GPS module and configure necessary parameters
   */
   while (gpsModule.available() > 0 ) {
     gps.encode(gpsModule.read());
   }
   if (gps.location.isValid()) {//verify that valid location is returned
      userLatitude = gps.location.lat();
      userLongitude = gps.location.lng();
      gpsDataIsValid = true;
   } else {
      gpsDataIsValid = false;
   }
   /**
    * Read data returned from RFID module
    */
    boolean cardReadSuccessful;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    cardReadSuccessful = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
    delay(500);
    /**
     * Compare data to determine if access to gun should be grated
     */
     //GPS validation
     double geo_radius = espResponseData["gr"];
     if (geo_radius == 0) { //perform GPS analysis disregarding location
      gpsPass = true;
     } else { //perform analysis including location
        if (gpsDataIsValid) {//perform check
          if (espDataIsAvailable) {//ensure server sent a message or else close up gun
            double serverLat = espResponseData["lt"];
            double serverLong = espResponseData["lg"];
            double distanceKmToAllowedLocation = (double)TinyGPSPlus::distanceBetween(userLatitude, userLongitude, serverLat, serverLong) / 1000;
            if (distanceKmToAllowedLocation < geo_radius) {
              gpsPass = true;
            } else {
              gpsPass = false;
            }
          } else {
            gpsPass = false;
          }
        } else {//no need or check, disallow
          gpsPass = false;
        }
     }
     //do RFID validation
     if (cardReadSuccessful) {
        long AuthorizedCardNumber = espResponseData["r"];
        long CardNumber = 0;
        for (uint8_t i=0; i < uidLength; i++) 
        {
          long len=1+floor(log10(long(uid[i])));
          long mult = 1;
          mult = mult * (pow(10,len) + 1);
          CardNumber *= mult;
          CardNumber = CardNumber + long(uid[i]);
        }
        if (CardNumber == AuthorizedCardNumber) {
          rfidPass = true;
        } else {
          rfidPass = false;
        }
     } else {
      rfidPass = false;
     }
     //emergency duration validation 
     int emergency_allow = espResponseData["ea"];
     long emergency_duration = espResponseData["ed"];
     String emergency_duration_unit = espResponseData["edu"];
     long emergencyDurationMilli = convertEmergencyDurationTomilliSeconds(emergency_duration, emergency_duration_unit);
     if (emergencyAllowIsActive) {
        //check for last emergency allow initation
        //if now > last emergency initiation then set emegencyAllowPass to false 
        if((millis()- lastEmergencyAllowInitiation + 500) > emergencyDurationMilli) { // the 500 is added because of internal delay from system component
          //disable
          disableEmergencyAllow();
        } else {
          enableEmergencyAllow();
        }
     } else {
        if (espDataIsAvailable) {//ensure server sent a message or else close up gun
          if (emergencyAllowsUsed < emergency_allow && gpsPass) {//user can initiate an emergency gun use. only turn on emergency allow if gps is available so user does not waste emergency allow
            buttonState = digitalRead(emergencyButton);
            if (buttonState == HIGH) {
              lastEmergencyAllowInitiation = millis();//initalize to start counting
              emergencyAllowsUsed++;
              enableEmergencyAllow();
            } else {
              disableEmergencyAllow();
            }
          } else {
            disableEmergencyAllow();
          }
        } else {
          disableEmergencyAllow();
        }
     }
     
     //compare all and on!
     if (gpsPass && emergencyAllowPass && rfidPass) {
       digitalWrite(LED_BUILTIN, HIGH); 
       triggerLock.write(openPosition); 
       //on green off others
       digitalWrite(greenLED, HIGH); 
       digitalWrite(redLED, LOW); 
       digitalWrite(blueLED, LOW); 
     } else {
      digitalWrite(LED_BUILTIN, LOW); 
      triggerLock.write(closePosition); 
      //on yellow if all that is left is RFID
       if (gpsPass && emergencyAllowPass) {
          digitalWrite(greenLED, LOW); 
          digitalWrite(redLED, LOW); 
          digitalWrite(blueLED, HIGH); 
       } else {
        //on red off others
         digitalWrite(greenLED, LOW); 
         digitalWrite(redLED, HIGH); 
         digitalWrite(blueLED, LOW); 
       }
      
     }
}
void enableEmergencyAllow() {
  emergencyAllowPass = true;
  emergencyAllowIsActive = true;
}
void disableEmergencyAllow() {
  emergencyAllowPass = false;
  emergencyAllowIsActive = false;
  lastEmergencyAllowInitiation = 0;
}
long convertEmergencyDurationTomilliSeconds(long duration, String unit) {
  if (unit == "hour") {
    return duration * 60 * 60 * 1000;
  }

  if (unit == "minute") {
    return duration * 60 * 1000;
  }

  if (unit == "second") {
    return duration * 1000;
  }
  return duration * 60 * 1000;//default is minute
}
