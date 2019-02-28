#include <Servo.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
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
const int openPosition = 90;
const int closePosition = 0;
#define PN532_IRQ   (6)
#define PN532_RESET (7)
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);
/**
 * Example
 *  String input = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
 *  JsonObject& root = jsonBuffer.parseObject(input);
 *  String sensor = root["sensor"];
 *  double latitude    = root["data"][0];
 *  double longitude    = root["data"][1];
 */
 //Globals defined here
 double userLatitude;
 double userLongitude;
 bool gpsDataIsValid = false;
 bool espDataIsAvailable = false;
 bool openTrigger = false;
 bool gpsPass = false;
 bool rfidPass = false;
 bool emergencyAllowPass = false;
 bool emergencyAllowIsActive = false;
 int emergencyAllowsUsed = 0;
 unsigned long lastEmergencyAllowInitiation = 0;
 int buttonState = 0; 
 /**
  * Test data 
  */
  
void setup() {
  uint32_t espBaud = 115200;
  uint32_t gpsBaud = 9600;
  Serial.begin(espBaud);
  esp8266Module.begin(espBaud);
  gpsModule.begin(gpsBaud);
  pinMode(emergencyButton, INPUT);
  triggerLock.attach(servoPin);
  pinMode(LED_BUILTIN, OUTPUT);

  //rfid
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  nfc.setPassiveActivationRetries(0xFF);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  Serial.println("Waiting for an ISO14443A card");
}

void loop() {
  DynamicJsonBuffer jsonBuffer;
  /**
   * Read data retruned from WiFi module and configure necessary parameter
   */
  while (esp8266Module.available() > 0) {
    esp8266Module.read();
  }
  //String espResponse = esp8266Module.readString();
  espDataIsAvailable = true;
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
    /**
     * Compare data to determine if access to gun should be grated
     */
     const String testData = "{\"geo_radius\": 0, \"emergency_allow\": 10, \"emergency_duration\": 5, \"emergency_duration_unit\": \"second\",\"lat\": 0, \"long\":0, \"rfid\":1341752731}";
     //GPS validation
     JsonObject& espResponseData = jsonBuffer.parseObject(testData);
     long geo_radius = espResponseData["geo_radius"];
     if (geo_radius == 0) { //perform GPS analysis disregarding location
      gpsPass = true;
     } else { //perform analysis including location
        if (gpsDataIsValid) {//perform check
          if (espDataIsAvailable) {//ensure server sent a message or else close up gun
            double serverLat = espResponseData["lat"];
            double serverLong = espResponseData["long"];
            unsigned long distanceKmToAllowedLocation = (unsigned long)TinyGPSPlus::distanceBetween(userLatitude, userLongitude, serverLat, serverLong) / 1000;
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
        long AuthorizedCardNumber = espResponseData["rfid"];
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
     int emergency_allow = espResponseData["emergency_allow"];
     long emergency_duration = espResponseData["emergency_duration"];
     String emergency_duration_unit = espResponseData["emergency_duration_unit"];
     long emergencyDurationMilli = convertEmergencyDurationTomilliSeconds(emergency_duration, emergency_duration_unit);
     if (emergencyAllowIsActive) {
        //check for last emergency allow initation
        //if now > last emergency initiation then set emegencyAllowPass to false 
        if((millis()- lastEmergencyAllowInitiation) > emergencyDurationMilli) {
          //reset all
          lastEmergencyAllowInitiation = 0;
          emergencyAllowPass = false;
          emergencyAllowIsActive = false;
        } else {
          emergencyAllowPass = true;
          emergencyAllowIsActive = true;
        }
     } else {
        if (espDataIsAvailable) {//ensure server sent a message or else close up gun
          if (emergencyAllowsUsed < emergency_allow && gpsPass) {//user can initiate an emergency gun use. only turn on emergency allow if gps is available so user does not waste emergency allow
            buttonState = digitalRead(emergencyButton);
            if (buttonState == HIGH) {
              lastEmergencyAllowInitiation = millis();//initalize to start counting
              emergencyAllowsUsed++;
              emergencyAllowIsActive = true;
              emergencyAllowPass = true;
            } else {
              emergencyAllowPass = false;
              emergencyAllowIsActive = false;
              lastEmergencyAllowInitiation = 0;
            }
          } else {
            emergencyAllowPass = false;
            emergencyAllowIsActive = false;
            lastEmergencyAllowInitiation = 0;
          }
        } else {
          emergencyAllowPass = false;
          emergencyAllowIsActive = false;
          lastEmergencyAllowInitiation = 0;
        }
        
     }

     //compare all and on!
     if (gpsPass && emergencyAllowPass && rfidPass) {
       digitalWrite(LED_BUILTIN, HIGH); 
       triggerLock.write(openPosition); 
     } else {
      digitalWrite(LED_BUILTIN, LOW); 
      triggerLock.write(closePosition); 
     }
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
