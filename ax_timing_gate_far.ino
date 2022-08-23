//---RADIO STUFF---
#include <SPI.h>
#include "printf.h"
#include "RF24.h"
//---END RADIO STUFF---

String stopwatch = "stop";

const int POWER_LED = 5;
const int SIGNAL_LED = 6;
const int LASER_IN = A0;
int laser_read = 0;

//For testing with button
const int buttonPin = 2;

//For no-delay blinking (1 second intervals)
unsigned long previousMillis = 0;
const long interval = 500;
int SIGNAL_LED_STATE = LOW;

//---RADIO STUFF---
// NOTES: FAR = TRANSMITTER (TX) = RADIO NUMBER: 1 = ROLE: TRUE

// Radio initialization and role setup. This established the FAR gate as a transmitter
RF24 radio(9, 10);  // using pin 9 for the CE pin, and pin 10 for the CSN pin
uint8_t address[][6] = { "1Node", "2Node" }; // Let these addresses be used for the pair
// It is very helpful to think of an address as a path instead of as an identifying device destination to use different addresses on a pair of radios, we need a variable to uniquely identify which address this radio will use to transmit
bool radioNumber = 1;  // 0 uses address[0] to transmit, 1 uses address[1] to transmit
bool role = true;  // true = TX role, false = RX role

int heartbeatPayload = 0; //Heartbeat sends a "0"
int packetPayload = 1;    //Info packet sends a "1"
//---END RADIO STUFF---

void setup() {
//---START SETUP()---

  Serial.begin(9600);

  pinMode(POWER_LED, OUTPUT);
  pinMode(SIGNAL_LED, OUTPUT);
  pinMode(LASER_IN, INPUT);

  //For testing with button
  pinMode(buttonPin, INPUT);

  //---RADIO STUFF: SETUP---
  if (!radio.begin()) {
    Serial.println(F("ERROR: Radio hardware is not responding! Moving to infinite loop."));
    while (1) {
      // SIGNAL_LED flashes S.O.S. to indicate that radio hardware is not connected to Arduino
      flash(200); flash(200); flash(200); // S
      delay(300); // otherwise the flashes run together
      flash(500); flash(500); flash(500); // O
      flash(200); flash(200); flash(200); // S
      delay(1000);
    }  // hold in infinite loop
  }

  // More radio setup. Sets power level and establishes connection pipes to the other radios
  radio.setPALevel(RF24_PA_HIGH);  // RF24_PA_MAX is default. Prev. used RF24_PA_LOW
  radio.setPayloadSize(sizeof(heartbeatPayload));  // float datatype occupies 4 bytes
  radio.openWritingPipe(address[radioNumber]);  // always uses pipe 0
  radio.openReadingPipe(1, address[!radioNumber]);  // using pipe 1
  radio.stopListening();  // put radio in TX mode
  
  /* 
   *  DEBUG
   *    printf_begin();             // needed only once for printing details
   *    radio.printDetails();       // (smaller) function that prints raw register values  
   *    radio.printPrettyDetails(); // (larger) function that prints human readable data  
  */
  //---END RADIO STUFF: SETUP---

//---END SETUP()---
}

void loop() {
//---START LOOP()---

  //laser_read = analogRead(LASER_IN);
  //For testing with button:
  laser_read = digitalRead(buttonPin);
  
  if (laser_read) { //If beam broken
    /*  
     *   When laser is broken, 
     *   - SIGNAL_LED goes solid to indicate a transmission from FAR to NEAR gate
     *   - FAR gate sends packet of ten 1's to NEAR gate
     *  
     *   Indicates that car has ended it's acceleration run. NEAR gate will stop timing
     */
    
     int counter = 0;
     digitalWrite(SIGNAL_LED, HIGH);
     while (counter<10) { 
       //Send 10 packets saying "beam broken"
       counter++;
       if(! sendPacket() ){
         counter--; //Re-send missed packet
       }
     }
     digitalWrite(SIGNAL_LED, LOW);
  }
  else {
    /* 
     *  When laser is not broken
     *  - FAR gate sends a heartbeat to NEAR gate
     * 
     *  Indicates that NEAR and FAR gates are still connected. If NEAR gate receives heartbeat, its SIGNAL_LED will blink slowly
     */
     
     sendHeartbeat();
  }

//---END LOOP()---
}

bool sendHeartbeat() { //returns TRUE if successful, FALSE otherwise
/* 
 *  sendHeartbeat(): returns TRUE if heartbeat information sent successfully, FALSE otherwise
 *  - Heartbeat information consists of a payload of one 0
 *  - While heartbeat is sending, SIGNAL_LED will blink slowly
 */
    bool returnVal = true;
    
    // This device is a TX node

    // Radio sends heartbeat information from FAR to NEAR gate
    unsigned long start_timer = micros();                // start the timer
    bool report = radio.write(&heartbeatPayload, sizeof(heartbeatPayload));  // transmit & save the report
    unsigned long end_timer = micros();                  // end the timer

    if (report) { // If transmission successful
      Serial.print(F("Transmission successful! "));  // payload was delivered
      Serial.print(F("Time to transmit = "));
      Serial.print(end_timer - start_timer);  // print the timer result
      Serial.print(F(" us. Sent: "));
      Serial.println(heartbeatPayload);  // print payload sent

      returnVal = true;
      delay(200);

      //Blink without delay
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        digitalWrite(SIGNAL_LED, !SIGNAL_LED_STATE);
        SIGNAL_LED_STATE = !SIGNAL_LED_STATE;
      }
      
    } 
    else {
      Serial.println(F("Transmission failed or timed out"));  // payload was not delivered
      
      returnVal = false;
      blinkFast(); //To signify error
    }

    return returnVal;
}
bool sendPacket() { 
/* 
 *  sendPacket(): returns TRUE if information packet sends successfully, FALSE otherwise
 *  - Information packet triggers when FAR gate laser is broken
 *  - Note: SIGNAL_LED actions are handled in main loop(), not sendPacket() function (like they are in sendHeartbeat())
 */
    
    bool returnVal = true;

    unsigned long start_timer = micros();                // start the timer
    bool report = radio.write(&packetPayload, sizeof(packetPayload));  // transmit & save the report
    unsigned long end_timer = micros();                  // end the timer

    if (report) { // If transmission successful
      Serial.print(F("Transmission successful! "));  // payload was delivered
      Serial.print(F("Time to transmit = "));
      Serial.print(end_timer - start_timer);  // print the timer result
      Serial.print(F(" us. Sent: "));
      Serial.println(packetPayload);  // print payload sent

      returnVal = true;
    } 
    else {
      Serial.println(F("Transmission failed or timed out"));  // payload was not delivered
      blinkFast(); // signify error
      returnVal = false;
    }

    return returnVal;
  
}

void blinkNormal() {
/* blinkNormal(): blinks SIGNAL_LED normally (1 blink per second). Indicates no problem */

  digitalWrite(SIGNAL_LED, HIGH);
  delay(500);
  digitalWrite(SIGNAL_LED, LOW);
  delay(500);
}
void blinkFast() {
/* blinkFast(): blinks SIGNAL_LED rapidly. Indicates problem */

  digitalWrite(SIGNAL_LED, HIGH);
  delay(50);
  digitalWrite(SIGNAL_LED, LOW);
  delay(50);
  digitalWrite(SIGNAL_LED, HIGH);
  delay(50);
  digitalWrite(SIGNAL_LED, LOW);
  delay(50);
  digitalWrite(SIGNAL_LED, HIGH);
  delay(50);
  digitalWrite(SIGNAL_LED, LOW);
  delay(50);
}
void flash(int duration) {
/* flash(): helper function for S.O.S. functionality (when radio hardware fails to connect) */

  digitalWrite(SIGNAL_LED,HIGH);
  delay(duration);
  digitalWrite(SIGNAL_LED,LOW);
  delay(duration);
}
