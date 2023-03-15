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
#include <SPI.h>
#include <math.h>

// define the UUIDs for the board and the 4 characteristics
// generated here: https://www.famkruithof.net/uuid/uuidgen
const char* deviceServiceUuid = "2b6a5170-b7f2-11ed-b9d9-0800200c9a66";
const char* switchCharacteristicUuid = "3aa9bf30-bcbc-11ed-a901-0800200c9a66";
const char* testingCharacteristicUuid = "9e69c9c0-bcbc-11ed-a901-0800200c9a66";
const char* stimulationCharacteristicUuid = "7db08f70-bcbc-11ed-a901-0800200c9a66";
const char* sensorCharacteristicUuid = "925486c0-bcbc-11ed-a901-0800200c9a66";
const char* loggingCharacteristicUuid = "997fda80-bcbc-11ed-a901-0800200c9a66";
const char* logsCharacteristicUuid = "c78725f0-bcbc-11ed-a901-0800200c9a66";
const char* endCharacteristicUuid = "493a8bb0-bccb-11ed-a901-0800200c9a66";

const char* peripheralName = "SWEATsens";

typedef struct btOut_type{
  unsigned long timeOut;
  float voltOut;
};

const int union_size = sizeof(btOut_type);

typedef union btPacketOut{
 btOut_type structure;
 byte byteArray[union_size]; /* you can use other variable types if you want. Like: a 32bit integer if you have 4 8bit variables in your struct */
};

btPacketOut packet;  

// set a unique service ID for communication to the app
BLEService sweatService(deviceServiceUuid);

// define custom characteristics on custom 128-bit UUIDa, readable and writable by central
BLEByteCharacteristic switchCharacteristic(switchCharacteristicUuid, BLERead | BLEWrite);
BLEByteCharacteristic testingCharacteristic(testingCharacteristicUuid, BLERead | BLEWrite);
BLEByteCharacteristic stimulationCharacteristic(stimulationCharacteristicUuid, BLERead | BLEWrite);
BLEByteCharacteristic sensorCharacteristic(sensorCharacteristicUuid, BLERead | BLEWrite);
BLEByteCharacteristic loggingCharacteristic(loggingCharacteristicUuid, BLERead | BLEWrite);
BLECharacteristic logsCharacteristic(logsCharacteristicUuid, BLERead | BLEWrite | BLENotify, sizeof packet.byteArray, false);
BLEByteCharacteristic endCharacteristic(endCharacteristicUuid, BLERead | BLEWrite);

// define pins
const int ledPin = LED_BUILTIN; // pin to use for the LED
const int redPin = 22; // other LEDs
const int bluePin = 24;     
const int greenPin = 23;
const int pwrledPin = LED_PWR; //pin 25
const int stimPin = 9; // pin to use for stimulation electrode output
const int sensorInPin = A0; // pin to use for the electrode circuit input
const int sensorOutPin = 8; // pin to use for the electrode circuit output

// pins used in SPI output
const int csStimDac = 5;
const int csSensDac = 6;
const int csLogADC = 7;

// declare the write commands for the SPI
const byte stimWrite = 2; //0010 + 0000
const byte sensWrite = 6; //0110 + 0000
const byte logWrite = 6; //0110 + 0000

// const int clock = 13;
// const int dataIn = 12;
// const int dataOut = 11;

// variables to determine actions to take
int ledState = 0;
float stimState = 0;
float sensorState = 0;
int loggingState = 1;
int testing = 0;
float voltage = 0;
unsigned long previousMillis = 0;
unsigned long startMillis = 0;
unsigned long startLogMillis = 0;
unsigned long interval = 2000;


void startStim(float stimState) {
    // calculate the PWM value
    int stimV = floor(stimState/3.3 * 255);
    
    // set pin to output PWM
    analogWrite(stimPin, stimV);

    // need to check what happens on power off, are the values saved in register
    // floor the outputs to ensure a max of 4096
    stimV = floor(4096*stimState/2/2.048);

    // start SPI transaction
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

    // pull CS pin low to start transfer
    digitalWrite(csStimDac, LOW);

    // calcuate the bytes to write to the register
    byte firstB = stimV/256; // first 4 bits
    byte secondB = stimV%256; // next 8 bits

    // output to DAC register
    int receivedVal = SPI.transfer(stimWrite << 4 | firstB); //shift the write command left 4 and or it with the first byte and send it
    receivedVal = SPI.transfer(secondB); // send the second signal
    SPI.endTransaction(); // end the transaction

    // pull CS pin high to stop transfer
    digitalWrite(csStimDac, HIGH);

    // output to serial just in case
    Serial.println(receivedVal);

    // turn on LED for indicator
    digitalWrite(redPin, LOW);
}

void stopStim() {
    // write 0 to the pin
    analogWrite(stimPin, 0);

    // start SPI transaction
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

    // pull CS pin low to start transfer
    digitalWrite(csStimDac, LOW);

    // output to DAC register
    int receivedVal = SPI.transfer(stimWrite << 4); //shift the write command left 4 and pad with 0s
    receivedVal = SPI.transfer(0b00000000); // send the second signal of 0s
    SPI.endTransaction(); // end the transaction

    // pull CS pin high to stop transfer
    digitalWrite(csStimDac, HIGH);

    // output to serial just in case
    Serial.println(receivedVal);

    // turn LED off
    digitalWrite(redPin, HIGH);
}

void startSense(float sensorState) {
    // calculate the PWM value
    int sensorV = round(sensorState/3.3 * 255);
    
    // set pin to output PWM
    analogWrite(sensorOutPin, sensorV);

    // need to check what happens on power off, are the values saved in register
    // floor the outputs to ensure a max of 4096
    sensorV = floor(4096*sensorState/2.048);

    // start SPI transaction
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

    // pull CS pin low to start transfer
    digitalWrite(csSensDac, LOW);

    // calcuate the bytes to write to the register
    byte firstB = sensorV/256; // first 4 bits
    byte secondB = sensorV%256; // next 8 bits

    // output to DAC register
    int receivedVal = SPI.transfer(sensWrite << 4 | firstB); //shift the write command left 4 and or it with the first byte and send it
    receivedVal = SPI.transfer(secondB); // send the second signal
    SPI.endTransaction(); // end the transaction

    // pull CS pin high to stop transfer
    digitalWrite(csSensDac, HIGH);

    // output to serial just in case
    Serial.println(receivedVal);

    // turn on LED for indicator
    digitalWrite(greenPin, LOW);
}

void stopSense() {
    // write 0 to the pin
    analogWrite(sensorOutPin, 0);

   // start SPI transaction
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

    // pull CS pin low to start transfer
    digitalWrite(csSensDac, LOW);

    // output to DAC register
    int receivedVal = SPI.transfer(sensWrite << 4); //shift the write command left 4 and pad with 0s
    receivedVal = SPI.transfer(0b00000000); // send the second signal of 0s
    SPI.endTransaction(); // end the transaction

    // pull CS pin high to stop transfer
    digitalWrite(csSensDac, HIGH);

    // output to serial just in case
    Serial.println(receivedVal);
    
    // turn LED off
    digitalWrite(greenPin, HIGH);
}

void startLogging(int loggingState, BLEDevice central) {
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
    // if we ever need to change the upper limit of the input
    // https://www.arduino.cc/reference/en/language/functions/analog-io/analogreference/
    analogReadResolution(12);
    float sensorValue = analogRead(A0);

    // convert the analog reading (0 - 4095) to a voltage (0 - 3.3V):
    voltage = sensorValue * (3.3 / 4095);

    ledState = HIGH;
    digitalWrite(ledPin, ledState);
    
    // output if there is a serial connection
    if (central) {
      // format 
      packet.structure.timeOut = currentMillis;
      packet.structure.voltOut = voltage;

      logsCharacteristic.writeValue(packet.byteArray, sizeof packet.byteArray);
    }

    // output if there is a serial connection
    if (Serial) {
      // format 
      Serial.print(millis());
      Serial.print(',');
      Serial.println(voltage,4); //specify 4 digits of precision
    }
  }
}

void stopLogging() {
  // only reset to 0 if testing is not happening below
  if (loggingState == 0) {
    // otherwise turn off logging
    startLogMillis = 0;
    previousMillis = 0;

    // turn LED off
    ledState = LOW;
    digitalWrite(ledPin, ledState);
  }
}

void connectedLight() {
  digitalWrite(bluePin, LOW);
}

void disconnectedLight() {
  digitalWrite(bluePin, HIGH);
}

void onBLEConnected(BLEDevice central) {
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
  connectedLight();
}

void onBLEDisconnected(BLEDevice central) {
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
  disconnectedLight();
}

void onSwitchUpdate(BLEDevice central, BLECharacteristic characteristic) {
  
  if (characteristic.value()) {
    Serial.println("LED on");
    digitalWrite(pwrledPin, HIGH);
    ledState = 1;
  } else {
    Serial.println("LED off");
    digitalWrite(pwrledPin, LOW);
    ledState = 0;
  }
}

void onStimUpdate(BLEDevice central, BLECharacteristic characteristic) {

  if (characteristic.value()) {
    stimState = 1;
    Serial.println("Stimulation = 1.");
  } else {
    stimState = 0;
    Serial.println("Stimulation = 0.");
  }
}

void onSenseUpdate(BLEDevice central, BLECharacteristic characteristic) {

  if (characteristic.value()) {
    sensorState = 1;
    Serial.println("Sensor = 1.");
  } else {
    sensorState = 0;
    Serial.println("Sensor = 0.");
  }
}

void onTestingUpdate(BLEDevice central, BLECharacteristic characteristic) {

  if (characteristic.value()) {
    testing = 1;
    Serial.println("Testing = 1.");
  } else {
    testing = 0;
    Serial.println("Testing = 0.");
  }
}

void onLoggingUpdate(BLEDevice central, BLECharacteristic characteristic) {

  if (characteristic.value()) {
    loggingState = 1;
    Serial.println("Logging = 1.");
  } else {
    loggingState = 0;
    Serial.println("Logging = 0.");
  }
}

void endUpdate(BLEDevice central, BLECharacteristic characteristic) {

  // set the initial value for the characeristic
  switchCharacteristic.writeValue(0);
  testingCharacteristic.writeValue(0);
  stimulationCharacteristic.writeValue(0);
  sensorCharacteristic.writeValue(0);
  loggingCharacteristic.writeValue(0);
  packet.structure.timeOut = 0;
  packet.structure.voltOut = 0;
  logsCharacteristic.writeValue(packet.byteArray, sizeof packet.byteArray);

  onBLEDisconnected(central);
  central.disconnect();
}


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

  //SPI chip select pins
  digitalWrite(csSensDac, HIGH);
  digitalWrite(csStimDac, HIGH);

  // begin initialization
  if (!BLE.begin()) {

    // print error message if the bluetooth is not connected
    Serial.println("Starting Bluetooth® Low Energy module failed!");

    // force a reset of the program
    while (1);
  }

  // set advertised local name and service UUID
  BLE.setLocalName(peripheralName);
  BLE.setAdvertisedService(sweatService);

  // add the characteristics to the service
  sweatService.addCharacteristic(switchCharacteristic);
  sweatService.addCharacteristic(testingCharacteristic);
  sweatService.addCharacteristic(stimulationCharacteristic);
  sweatService.addCharacteristic(sensorCharacteristic);
  sweatService.addCharacteristic(loggingCharacteristic);
  sweatService.addCharacteristic(logsCharacteristic);
  sweatService.addCharacteristic(endCharacteristic);

  // add service
  BLE.addService(sweatService);

  // Bluetooth LE connection handlers.
  BLE.setEventHandler(BLEConnected, onBLEConnected);
  BLE.setEventHandler(BLEDisconnected, onBLEDisconnected);

  // Event driven reads.
  switchCharacteristic.setEventHandler(BLEWritten, onSwitchUpdate);
  stimulationCharacteristic.setEventHandler(BLEWritten, onStimUpdate);
  testingCharacteristic.setEventHandler(BLEWritten, onTestingUpdate);
  sensorCharacteristic.setEventHandler(BLEWritten, onSenseUpdate);
  loggingCharacteristic.setEventHandler(BLEWritten, onLoggingUpdate);
  endCharacteristic.setEventHandler(BLEWritten, endUpdate);

  // set the initial value for the characeristic
  switchCharacteristic.writeValue(0);
  testingCharacteristic.writeValue(0);
  stimulationCharacteristic.writeValue(0);
  sensorCharacteristic.writeValue(0);
  loggingCharacteristic.writeValue(0);
  packet.structure.timeOut = 0;
  packet.structure.voltOut = 0;
  logsCharacteristic.writeValue(packet.byteArray, sizeof packet.byteArray);

  // start advertising
  BLE.advertise();

  // output to serial
  Serial.println("Peripheral advertising info: ");
  Serial.print("Name: ");
  Serial.println(peripheralName);
  Serial.print("MAC: ");
  Serial.println(BLE.address());
  Serial.print("Service UUID: ");
  Serial.println(sweatService.uuid());

  Serial.println("Bluetooth device active, waiting for connections...");

  SPI.begin();

  // get board start time
  startMillis = millis();
}


// this is the main loop of the program which continuously runs
void loop() {

  // turn on power pin
  digitalWrite(pwrledPin, HIGH);

  // start by checking for a BLE connection
  // listen for BLE peripherals
  BLEDevice central = BLE.central();

  // if a central is connected to peripheral
  if (central) {

    // output to serial
    Serial.print("Connected to central: ");

    // print the central's MAC address
    Serial.println(central.address());

    // while the central is still connected to peripheral
    while (central.connected()) {

      // first check the states and perform actions accordingly
      // if there is sweat stimulation required
      if (stimState > 0){
        startStim(stimState);

      // otherwise turn the pin off
      } else {
        stopStim();
      }

      // if there is electrode volatage required
      if (sensorState > 0){
        startSense(sensorState);

      // otherwise turn the pin off
      } else {
        stopSense();
      }

      // if logging is requested
      if (loggingState > 1) {
        startLogging(loggingState, central);

      } else {
        stopLogging();
      }
        

    }

    // when the central disconnects, notify the serial, flash the LED
    Serial.print(F("Disconnected from central: ")); // F moves string to flash memory
    Serial.println(central.address());
    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin, LOW);
  }

  // first check the states and perform actions accordingly
  // if there is sweat stimulation required
  if (stimState > 0){
    startStim(stimState);

  // otherwise turn the pin off
  } else {
    stopStim();
  }

  // if there is electrode volatage required
  if (sensorState > 0){
    startSense(sensorState);

  // otherwise turn the pin off
  } else {
    stopSense();
  }

  // if logging is requested
  if (loggingState > 1) {
    startLogging(loggingState, central);

  } else {
    stopLogging();
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