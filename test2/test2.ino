#include <SoftwareSerial.h>
SoftwareSerial softSerial(3, 2); // RX, TX
char ct;
void setup()
{
  uint32_t baud = 115200;
  Serial.begin(baud);
  softSerial.begin(baud);
  Serial.print("SETUP!! @");
  Serial.println(baud);
}
void loop()
{
  while (softSerial.available() > 0) {
    softSerial.read();
    /*char a = softSerial.read();
    //String bol = softSerial.readUntil("\n");
    if(a == '\0')
    continue;
    if(a != '\r' && a != '\n' && (a < 32))
    break;
    Serial.print(a);
   // ct = a;*/
  }
  String response = softSerial.readString();
  
  Serial.println(response);
}
