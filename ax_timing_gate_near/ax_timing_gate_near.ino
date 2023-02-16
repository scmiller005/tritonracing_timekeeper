/* -= TRITON RACING TIMEKEEPER V1.2 =-
 *  
 * Changelog
 *  - Added manual keyboard override for Autocross and Acceleration modes
 *  - Continued working out some bugs
 */

//---RADIO STUFF---
#include <SPI.h>
#include "printf.h"
#include "RF24.h"
//---END RADIO STUFF---

String stopwatch = "stop";

const int POWER_LED = 4;
const int SIGNAL_LED = 5;
const int HW_LED = 6;
const int LASER_IN = A0;
int laser_read;

//For no-delay blinking (1 second intervals)
unsigned long previousMillis = 0;
const long interval = 500;
int SIGNAL_LED_STATE = LOW;

char input_gate_mode;
String gate_mode = "";

//For testing with a button
const int buttonPin = 2;

//---RADIO STUFF---
// instantiate an object for the nRF24L01 transceiver
RF24 radio(9, 10);  // using pin 9 for the CE pin, and pin 10 for the CSN pin
// Let these addresses be used for the pair
uint8_t address[][6] = { "1Node", "2Node" };
// It is very helpful to think of an address as a path instead of as an identifying device destination to use different addresses on a pair of radios, we need a variable to uniquely identify which address this radio will use to transmit
bool radioNumber = 0;  // 0 uses address[0] to transmit, 1 uses address[1] to transmit
bool role = false;  // true = TX role, false = RX role

int payload = 0; // This variable is only used to get the size of an int

//---END RADIO STUFF---

void setup() {
//---START SETUP()---

  pinMode(POWER_LED, OUTPUT);
  pinMode(SIGNAL_LED, OUTPUT);
  pinMode(HW_LED, OUTPUT);
  pinMode(LASER_IN, INPUT);
  Serial.begin(9600);

  //For testing with a button
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

  // Reminder: Near radio = Recieve (RX) = radioNumber: 0 = role: false

  // More radio setup. Sets power level and establishes connection pipes to the other radios
  radio.setPALevel(RF24_PA_MAX);  // RF24_PA_MAX is default. Prev. used RF24_PA_LOW
  radio.setPayloadSize(sizeof(payload)); 
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address[radioNumber]);  // set the TX address of the RX node into the TX pipe. always uses pipe 0
  radio.openReadingPipe(1, address[!radioNumber]);    // set the RX address of the TX node into a RX pipe. using pipe 1
  radio.startListening();  // put radio in RX mode (do radio.stopListening(); to put the radio in TX mode)

  /* 
   *  DEBUG
   *    printf_begin();             // needed only once for printing details
   *    radio.printDetails();       // (smaller) function that prints raw register values  
   *    radio.printPrettyDetails(); // (larger) function that prints human readable data  
  */
  //---END RADIO STUFF: SETUP---

  // UI shit. It looks cool, cry about it
  Serial.println("Triton Racing TimeKeeper v1.2");
//  displayTrident();

  // Mode selection: Allows the user to input the mode (Acceleration or Autocross) they want via Serial
  // - Note: Serial monitor needs to be open
  while ( (!(input_gate_mode == '1')) || (!(input_gate_mode == '2')) ) {
    //1: Acceleration --> 2 gate sets. Far side radios beam breaking to near side. No lap functionality
    //2: Autocross -----> 1 gate set. Beam breaking triggers new lap.
    Serial.println("Please input your mode. Enter 1 for Acceleration, Enter 2 for Autocross");
    delay(5000);
    input_gate_mode = Serial.read();
    Serial.println(input_gate_mode); //DEBUG 

    if ( (input_gate_mode == '1') || (input_gate_mode == '2') ) {
      //Serial.println("BREAK"); //DEBUG
      break;
    }
  }

  //Serial.println("END WHILE"); //DEBUG

  // More UI stuff
  if (input_gate_mode == '1') {
    gate_mode = "acceleration";
    Serial.println("You are running in ACCELERATION mode");
    Serial.println(">> Reminder: is the SIGNAL_LED blinking? This means the radios are connected.");
  }
  else if (input_gate_mode == '2') {
    gate_mode = "autocross";
    Serial.println("You are running in AUTOCROSS mode");
  }

  Serial.println();
  Serial.println("Keyboard Shortcuts: Start = 7. Stop = 8. Reset = 9");

  Serial.println("*****************************");
  Serial.println();

//---END SETUP()---
}

unsigned long startMillis;
unsigned long currentMillis;
unsigned long elapsedMillis;

char key_input = 0; //Variable tracking input while program is running
String override_status = "";

int payloadCounter = 0;

void loop() {
//---START LOOP()---
  /**************************************
     ACCELERATION MODE
  ***************************************/
  if (gate_mode == "acceleration") {
    //Serial.println("ACCELERATION MODE ENTERED SUCCESSFULLY"); //DEBUG
    //TIMER CODE FOR 2 PAIRS, NEAR AND FAR
    /* 
     * Notes:
     * - Near module blinking LED just means its successfully receiving heartbeats
     * - Solid LED = receiving packet
     * - It CANNOT detect if a packet has failed to send
     */

    /* TO-DO
     * XX Integrate timing code into Acceleration_near gate. Test with two seperate buttons (one acting as near, one as far)
     * XX Add radio code to Acceleration_near and Acceleration_far
     *    - Near: Radio constantly checks for connection + packet. If no connection, write red_led HIGH. Once packet, stop timer
     *    - Far: Radio constantly sends connection pulse + packet. Once laser break, send packet
     * XX Add laser code to near gate: once laser break, start timer
     * 4) Test both with buttons instead of lasers
     * 5) Add lasers to mix
     * PROGRAM SIGNAL LED AND POWER LED
     * -------
     * 6) Test rigirously, in indoor and outdoor settings
     * 7) Design review (?) (Or wait until PCB)
     * 8) Solder together very carefully
     * 9) Enclose
     */
    
    while (1) { // Creating a new loop() function inside the Acceleration mode

      //laser_read = analogRead(LASER_IN);

      //For testing with button
      laser_read = digitalRead(buttonPin);

      //---RADIO STUFF---
      // Reminder: this radio is a RX module
      uint8_t pipe;

      // Constantly checks for payload, heartbeat or otherwise
      if (radio.available(&pipe)) {              // is there a payload? get the pipe number that recieved it
        uint8_t bytes = radio.getPayloadSize();  // get the size of the payload
        radio.read(&payload, bytes);             // fetch payload from FIFO

        // In order for the NEAR module to verify the packet of ten 1's has been fully received
        if (payload == 1) {
          digitalWrite(SIGNAL_LED, HIGH);
          payloadCounter++;
        }
        
        //Blink without delay when there is a payload
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
          previousMillis = currentMillis;
          digitalWrite(SIGNAL_LED, !SIGNAL_LED_STATE);
          SIGNAL_LED_STATE = !SIGNAL_LED_STATE;
        }

        /* DEBUG
          Serial.print(F("Received "));
          Serial.print(bytes);  // print the size of the payload
          Serial.print(F(" bytes on pipe "));
          Serial.print(pipe);  // print the pipe number
          Serial.print(F(": "));
          Serial.println(payload);  // print the payload's value 
        */

      }
      //---END RADIO STUFF---

      //SCANNING FOR START/STOP/RESET (7/8/9) KEYBOARD INPUT
      if (Serial.available() > 0) {
        key_input = Serial.read();

        if (key_input == '7') { //START MANUAL OVERRIDE
          if (stopwatch == "stop") {
            override_status = "start";
          }
          else {
            Serial.println("Please STOP watch before STARTING");
          }
        }
        else if (key_input == '8') { //STOP MANUAL OVERRIDE
          if (stopwatch == "run") {
            override_status = "stop";
          }
          else {
            Serial.println("Stopwatch must be RUNNING to STOP");
          }
        }
        else if (key_input == '9') { //RESET MANUAL OVERRIDE
          //RESET starts the timer back at 0 (whether stopped or running)
          //If running, continue running
          if (stopwatch == "run") {
            printPreviousTime();
            Serial.println("STOPWATCH RESET");
            startMillis = millis();
            digitalWrite(HW_LED, LOW);
            delay(2000); //So sensor doesn't double-count the car
            digitalWrite(HW_LED, HIGH);
            stopwatch = "run";
          }
          //If stopped, continue stopped
          else if (stopwatch == "stop") {
            printPreviousTime();
            Serial.println("STOPWATCH RESET");
            digitalWrite(HW_LED, LOW);
          }
        }
        key_input = 0; //Reset key_input value
      }

      //RUNNING THE STOPWATCH
      if (stopwatch == "run") {
        currentMillis = millis();
        elapsedMillis = (currentMillis - startMillis);
        digitalWrite(HW_LED, HIGH);
      }
      //STOPPING THE STOPWATCH (ONLY CONTROLLED BY MANUAL OVERRIDE)
      if (payloadCounter == 10 || (override_status == "stop") ) { // Stopwatch will only stop when all ten 1's have been received (or when manual override is triggered)
        stopwatch = "stop";
        printPreviousTime_adjusted(); // Adjusted time takes into account the transmission time of the info packet from FAR to NEAR gate
        Serial.println("STOPWATCH STOPPED");
        digitalWrite(HW_LED, LOW);
        payloadCounter = 0;
        
        delay(2000); //Button debouncing. Note: delay will cause the radios to disconnect for a bit
      }
      //STARTING THE STOPWATCH
      else if (laser_read || (override_status == "start") ) {
        stopwatch = "run";
        Serial.println("STOPWATCH STARTED");


        startMillis = millis();

        delay(2000); //Button/laser debouncing
      }

      override_status = ""; //Reset override_status value
    }
  }

  /**************************************
     AUTOCROSS MODE
  ***************************************/
  else if (gate_mode == "autocross") { //LAP TIMING, ONE GATE
    //Serial.println("AUTOCROSS MODE ENTERED SUCCESSFULLY"); //DEBUG
        
    while (1) {

      laser_read = analogRead(LASER_IN);

      //SCANNING FOR START/STOP/RESET (7/8/9) KEYBOARD INPUT
      if (Serial.available() > 0) {
        key_input = Serial.read();

        if (key_input == '7') { //START MANUAL OVERRIDE
          if (stopwatch == "stop") {
            override_status = "start";
          }
          else {
            Serial.println("Please STOP watch before STARTING");
          }
        }
        else if (key_input == '8') { //STOP MANUAL OVERRIDE
          if (stopwatch == "run") {
            override_status = "stop";
          }
          else {
            Serial.println("Stopwatch must be RUNNING to STOP");
          }
        }
        else if (key_input == '9') { //RESET MANUAL OVERRIDE
          //RESET starts the timer back at 0 (whether stopped or running)
          //If running, continue running
          if (stopwatch == "run") {
            printPreviousTime();
            Serial.println("STOPWATCH RESET");
            startMillis = millis();
            digitalWrite(HW_LED, LOW);
            delay(2000); //So sensor doesn't double-count the car
            digitalWrite(HW_LED, HIGH);
            stopwatch = "run";
          }
          //If stopped, continue stopped
          else if (stopwatch == "stop") {
            printPreviousTime();
            Serial.println("STOPWATCH RESET");
            digitalWrite(HW_LED, LOW);
          }
        }
        key_input = 0; //Reset key_input value
      }

      //RUNNING THE STOPWATCH
      if (stopwatch == "run") {
        currentMillis = millis();
        elapsedMillis = (currentMillis - startMillis);
        digitalWrite(HW_LED, HIGH);
      }

      //LAPPING THE STOPWATCH
      if (laser_read > 1000 && stopwatch == "run") { //If beam broken && stopwatch is currently running
        stopwatch = "run"; //State doesn't change
        printPreviousTime();
        startMillis = millis();

        digitalWrite(HW_LED, LOW);
        delay(2000); //So sensor doesn't double-count the car
        digitalWrite(HW_LED, HIGH);
      }

      //STOPPING THE STOPWATCH (ONLY CONTROLLED BY MANUAL OVERRIDE)
      if (override_status == "stop") {
        stopwatch = "stop";
        printPreviousTime();
        Serial.println("STOPWATCH STOPPED");
        digitalWrite(HW_LED, LOW);
      }
      //STARTING THE STOPWATCH
      else if ( (laser_read > 1000 && stopwatch == "stop") || (override_status == "start") ) {
        stopwatch = "run";
        Serial.println("STOPWATCH STARTED");

        startMillis = millis();
        delay(2000); //So sensor doesn't double-count the car
      }

      override_status = ""; //Reset override_status value
    }
  }
  /**************************************
     UNKNOWN MODE
  ***************************************/
  else {
    Serial.println("ERROR: Unknown Mode. Exiting...");
    exit(0);
  }

//---END LOOP()---
}

//----------------------------------------------------------

/**************************************
   HELPER FUNCTIONS
***************************************/
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

  digitalWrite(SIGNAL_LED, HIGH);
  delay(duration);
  digitalWrite(SIGNAL_LED, LOW);
  delay(duration);
}
String timeMillis(unsigned long Hourtime, unsigned long Mintime, unsigned long Sectime, unsigned long MStime) {
/* timeMillis(): formats the hour, minute, second, and ms variables to be printed to Serial */
  
  String dataTemp = "";
  if (Hourtime < 10)  { dataTemp = dataTemp + "0" + String(Hourtime) + "h:"; }
  else { dataTemp = dataTemp + String(Hourtime) + "h:"; }

  if (Mintime < 10)  { dataTemp = dataTemp + "0" + String(Mintime) + "m:"; }
  else { dataTemp = dataTemp + String(Mintime) + "m:"; }

  if (Sectime < 10) { dataTemp = dataTemp + "0" + String(Sectime) + "s:"; }
  else { dataTemp = dataTemp + String(Sectime) + "s:"; }

  dataTemp = dataTemp + String(MStime) + "ms";

  return dataTemp;
}
void printPreviousTime() {
/* printPreviousTime(): prints the previous lap time */

  unsigned long durMS = (elapsedMillis % 1000);     //Milliseconds
  unsigned long durSS = (elapsedMillis / 1000) % 60; //Seconds
  unsigned long durMM = (elapsedMillis / (60000)) % 60; //Minutes
  unsigned long durHH = (elapsedMillis / (3600000)); //Hours
  durHH = durHH % 24;
  String durMilliSec = timeMillis(durHH, durMM, durSS, durMS);
  Serial.println(durMilliSec);
}
void printPreviousTime_adjusted() { 
/* printPreviousTime_adjusted(): prints the previous lap time. Takes into account ~555ms delay for transmitting packet */

  unsigned long durMS = ((elapsedMillis-555) % 1000);     //Milliseconds
  unsigned long durSS = (elapsedMillis / 1000) % 60; //Seconds
  unsigned long durMM = (elapsedMillis / (60000)) % 60; //Minutes
  unsigned long durHH = (elapsedMillis / (3600000)); //Hours
  durHH = durHH % 24;
  String durMilliSec = timeMillis(durHH, durMM, durSS, durMS);
  Serial.println(durMilliSec);
}

 /*
void displayTrident() {
 displayTrident(): displays the Trident seen upon opening Serial
  
  Serial.println("...................");
  Serial.println(".........&.........");
  Serial.println("........&&&........");
  Serial.println("..&&...&&&&&...&&..");
  Serial.println("...&&...&&&...&&...");
  Serial.println("....&&..&&&..&&....");
  Serial.println(".....&&&&&&&&&.....");
  Serial.println(".........&.........");
  Serial.println(".........&.........");
  Serial.println(".........&.........");
  Serial.println(".........&.........");
  Serial.println("...................");
  Serial.println();
}
*/
