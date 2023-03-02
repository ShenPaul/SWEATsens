/*
  board_main

  This program is the main code for the SWEATsens prototype.

  This program comprises 4 main functions:
  - Data logging from the electrode circuit
  - Supplying the detection electrode circuit with a constant voltage
  - Supplying the sweat stimulation electrodes with power
  - Communicating with an app via BLE
    - Batching and transmitting data
  - Communicating with a Python program via serial
*/

// include the BLE package
#include <ArduinoBLE.h>
#include <math.h>

// set a unique service ID for communication to the app
// generated here: https://www.famkruithof.net/uuid/uuidgen
BLEService sweatService("2b6a5170-b7f2-11ed-b9d9-0800200c9a66");

// define a custom characterisitc on the custom 128-bit UUID, read and writable by central
// this characteristic acts as a switch and reads a byte of data
BLEByteCharacteristic switchCharacteristic("2b6a5170-b7f2-11ed-b9d9-0800200c9a66", BLERead | BLEWrite);

// define pins
const int ledPin = LED_BUILTIN; // pin to use for the LED
const int redPin = 22; // other LEDs
const int bluePin = 24;     
const int greenPin = 23;
const int pwrledPin = 25;
const int stimPin = 9; // pin to use for stimulation electrode output
const int sensorInPin = A0; // pin to use for the electrode circuit input
const int sensorOutPin = 8; // pin to use for the electrode circuit output

// variables to determine actions to take
int ledState = 0;
float stimState = 0;
float sensorState = 0;
int loggingState = 0;
int testing = 0;
float voltage = 0;
unsigned long previousMillis = 0;
unsigned long startMillis = 0;
unsigned long startLogMillis = 0;
unsigned long interval = 500;

// setup function
void setup() {

  // set baud rate for serial communication
  Serial.begin(9600);

  // this waits for serial connnection
  // while (!Serial);

  // set pin to modes
  pinMode(ledPin, OUTPUT);
  pinMode(stimPin, OUTPUT);
  pinMode(sensorInPin, INPUT);
  pinMode(sensorOutPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(pwrledPin, OUTPUT);

  // begin initialization
  if (!BLE.begin()) {

    // print error message if the bluetooth is not connected
    Serial.println("Starting Bluetooth® Low Energy module failed!");

    // force a reset of the program
    while (1);
  }

  // set advertised local name and service UUID
  BLE.setLocalName("SWEATsens");
  BLE.setAdvertisedService(sweatService);

  // add the characteristic to the service
  sweatService.addCharacteristic(switchCharacteristic);

  // add service
  BLE.addService(sweatService);

  // set the initial value for the characeristic
  switchCharacteristic.writeValue(0);

  // start advertising
  BLE.advertise();

  // output to serial
  Serial.println("BLE LED Peripheral");

  // get board start time
  startMillis = millis();
}


// this is the main loop of the program, which continuously loops
void loop() {

  // first check the states and perform actions accordingly
  // if there is sweat stimulation required
  if (stimState > 0){

    // calculate the PWM value
    int stimV = round(stimState/3.3 * 255);
    
    // set pin to output PWM
    analogWrite(stimPin, stimV);

    // turn on LED for indicator
    digitalWrite(redPin, LOW);

  // otherwise turn the pin off
  } else {

    // write 0 to the pin
    analogWrite(stimPin, 0);

    // turn LED off
    digitalWrite(redPin, HIGH);
  }

  // if there is electrode volatage required
  if (sensorState > 0){

    // calculate the PWM value
    int sensorV = round(sensorState/3.3 * 255);
    
    // set pin to output PWM
    analogWrite(sensorOutPin, sensorV);

    // turn on LED for indicator
    digitalWrite(greenPin, LOW);

  // otherwise turn the pin off
  } else {

    // write 0 to the pin
    analogWrite(sensorOutPin, 0);
    
    // turn LED off
    digitalWrite(greenPin, HIGH);
  }

  // if logging is requested
  if (loggingState > 1) {

    // update the desired logging interval
    interval = loggingState;

    // if there is no logging start time
    if (startLogMillis == 0) {

      // update the logging start time
      startLogMillis = millis();
      previousMillis = startLogMillis;
    }

    // log data using millis
    // get current time
    unsigned long currentMillis = millis();

    // check if enough time has passed between readings
    if (currentMillis - previousMillis > interval) {

      // update previous time if enough time has passed
      previousMillis = currentMillis;

      // read the analog signal
      float sensorValue = analogRead(A0);

      // convert the analog reading (0 - 1023) to a voltage (0 - 3.3V):
      voltage = sensorValue * (3.3 / 1023.0);

      ledState = HIGH;
      digitalWrite(ledPin, ledState);

      // output if there is a serial connection
      if (Serial) {
        // format 
        Serial.print(millis());
        Serial.print(',');
        Serial.println(voltage);
      }
      
    }

  } else {

      // only reset to 0 if testing is not happening below
      if (loggingState == 0) {
        // otherwise turn off logging
        startLogMillis = 0;
        previousMillis = 0;
      }
    
    // turn LED off
    ledState = LOW;
    digitalWrite(ledPin, ledState);

  }

  // next listen for BLE peripherals
  BLEDevice central = BLE.central();

  // if a central is connected to peripheral
  if (central) {

    // output to serial
    Serial.print("Connected to central: ");

    // print the central's MAC address
    Serial.println(central.address());

    // while the central is still connected to peripheral
    while (central.connected()) {

      // if the remote device wrote to the characteristic use the value to control the LED:
      if (switchCharacteristic.written()) {
        if (switchCharacteristic.value()) {   // any value other than 0
          Serial.println("LED on");
          digitalWrite(bluePin, LOW);         // will turn the LED on
        } else {                              // a 0 value
          Serial.println(F("LED off"));
          digitalWrite(bluePin, HIGH);          // will turn the LED off
        }
      }
    }

    // when the central disconnects, notify the serial, flash the LED
    Serial.print(F("Disconnected from central: ")); // F moves string to flash memory
    Serial.println(central.address());
    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin, LOW);
  }

  // if there is a serial run the serial commands
  if (Serial) {

    // if there is data
    if (Serial.available()) {

      // read string until timeou, 1s
      String teststr = Serial.readString();

      // remove any \r \n whitespace at the end of the String
      teststr.trim();

      // get the index of the comma
      int commaInd = teststr.indexOf(',');

      // split the string into the function and value
      int func = teststr.substring(0,commaInd).toInt();
      float val = teststr.substring(commaInd+1).toFloat();

      // update the functions based on the values
      if (func == 0) {
        testing = val;
      } else if (func == 1) {
        ledState = val;
      } else if (func == 2) {
        stimState = val;
      } else if (func == 3) {
        loggingState = val;
      } else if (func == 4) {
        sensorState = val;
      }

    }

    // if in testing mode, output a random value every 500ms
    if (testing) {

      // turn LED on
      digitalWrite(bluePin, LOW);

      // get current time
      unsigned long currentMillis = millis();

      // check if enough time has passed between readings
      if (currentMillis - previousMillis > interval) {

        // update previous time if enough time has passed
        previousMillis = currentMillis;

        // generate random voltage
        voltage = random(3300)/1000.0;

        // output to serial
        Serial.print(millis());
        Serial.print(',');
        Serial.println(voltage);

      }

    } else {

      // only reset to 0 if logging is not happening above
      if (loggingState == 0) {
        // otherwise turn off logging
        startLogMillis = 0;
        previousMillis = 0;
      }

      // turn LED off
      digitalWrite(bluePin, HIGH);
    }
  }
}