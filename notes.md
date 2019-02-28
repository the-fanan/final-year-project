********************
*                  *
*                  *
*                  *
*RX GPI0 GPI2 GND  *
*                  *   ESP can only connect to 2.4GHz networks
*                  *
*VCC RST CHPD TX   *
*                  *
********************
ESP MODULE PIN CONFIGURATION                             FOR PROGRAMMING MODE                         FOR COMMUNICATION MODE WITH ARDUINO
-----------------------------                            --------------------                         -----------------------------------
RX - blue						 Vcc to CHPD via 100 Ohm			TX to digital pin 3
TX - orange						 GND to GPIO0 via wire				RX to digital pin 2
VCC - red 5v						 TX to Arduino TX
GND - black						 RX to arduino RX pin
RST - yellow
GPIO0 - white
GPIO2 - grey
CHPD - green
---------------------------------------------------------------------------
While programming Nano
- Processor - ATMEGA 326 (Old Bootloader)
---------------------------------------------------------------------------
For Servo
GND - brown
VCC - RED 5v
Signal - Orange
*************
*           *
*   O-----  *    position 0
*           *
*************
----------------------------------------------------------------------------
For button
- connect button from +5v pin to Digital pin (4 for this project) of button
- connect 10K Ohms resistor from digital pin to ground
----------------------------------------------------------------------------
pn532 nfc
- board is set in I2C mode.
- SDA to A4
- SCL to A5
- RST D7
- IRQ D6
