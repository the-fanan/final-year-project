#include <Servo.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

//Define serials and pins
SoftwareSerial esp8266Module(3, 2); // RX, TX
SoftwareSerial gpsModule(9, 8);
Servo triggerLock; 
const int emergencyButton = 4;
const int servoPin = 5;
const int openPosition = 90;
const int closePosition = 160;
//Define Objects
TinyGPSPlus gps;
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
    /**
     * Compare data to determine if access to gun should be grated
     */
     const String testData = "{\"geo_radius\": 0, \"emergency_allow\": 3, \"emergency_duration\": 5, \"emergency_duration_unit\": \"minute\",\"lat\": 0, \"long\":0}";
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
     rfidPass = true;
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
        if (emergencyAllowsUsed < emergency_allow) {//user can initiate an emergency gun use
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
     }

     //compare all and on!
     if (gpsPass && emergencyAllowPass && rfidPass) {
       digitalWrite(LED_BUILTIN, HIGH); 
     } else {
      digitalWrite(LED_BUILTIN, LOW); 
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
