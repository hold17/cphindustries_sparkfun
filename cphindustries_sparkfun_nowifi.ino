// COPENHAGEN INDUSTRIES
// Strawberry 1
// Hardware: STRAWBERRY 2.6.x CNC ("DAQUIRY")
// Firmware: J1 - Morning Glory
// Build: 0.459
// Date: 21 Sep 2017

#include <EEPROM.h>

int fireratedelay = 90;      // delay between each shot / firing rate
//int O_Open_Time = 0;       // Oxygen Open Time (in ms)
//int O_Delay_Time = 0;      // Delay between open and close of oxygen valve
//int P_Open_Time = 0;       // Propane Open Time (in ms)
//int P_Delay_Time = 0;      // Delay between open and close of propane valve
//int P_Open_Time2 = 0;      // Propane Second Open Time (in ms) usually not used
//int P_Delay_Time2 = 0;     // Propane Second Delay Time (in ms)
//int I_Open_Time = 0;       // Time of start of ignition
//int I_Delay_Time = 0;      // Duration ignition remains on
//int ShotTime= max(max(O_Open_Time+O_Delay_Time,I_Open_Time+I_Delay_Time),max(P_Open_Time2+P_Delay_Time2,P_Open_Time+P_Delay_Time)); // Minimum time to finish a shot cycle
int ShotTime = 50;
bool armed = 1;              // Self explanatory
bool shortReset = 0;         // To be read from eeprom, TRUE if reset happened within five seconds of a shot
int numShortResets = 0;      // Count of resets within five seconds of a shot, saved to eeprom
int firingMode = 0;          // Tracks firing mode (single, burst, full auto)

int battMax = 10;            // voltage on Vin which will return 1023 from A3
int battLevel = 0;           // The readin from A3
float voltage = 0;           // Calculated voltage on Vin

unsigned long int numShots=0;       // Number of shots gun have fired, saved and read from eeprom
unsigned long lastSampleTime = 0;   // Keeps track of when numshots was last saved to eeprom
unsigned long lastshottime = 0;     // Keeps track of when the last shot happened
unsigned long interval = 5000;     // Interval of saving number of shots to eeprom (5 seconds, don't want to burden eeprom with saving every shot, don't want to lose count too much when shut of either) 
unsigned long now;                  // Number of milliseconds since boot, to be compared to lastsampletime and lastshottime

// Pin assign
const int Trigger = 14;       // Trigger
const int Input = 15;         // Mode select / other input
const int Buzz = 0;           // Buzzer
const int Blink = 5;          // On-board LED
//const int Vibrate = 3;      // Vibrator
//const int P_Valve = 8;      // PROPANE 
//const int O_Valve = 10;     // OXYGEN
//const int Ignition = 4;     // Ignition
 
void setup() {  
  Serial.begin(9600);
  while (!Serial) {
    yield();
  }
  
  //checkBatt(); disabled for now

  // Pin setup
  pinMode(Trigger, INPUT);
  pinMode(Input, INPUT);
  pinMode(Buzz, OUTPUT);
  pinMode(Blink, OUTPUT);
//  pinMode(Vibrate, OUTPUT);
  digitalWrite(Buzz, LOW);
  digitalWrite(Blink, LOW);
//  digitalWrite(Vibrate, LOW);
//  pinMode(P_Valve, OUTPUT);
//  pinMode(O_Valve, OUTPUT);
//  pinMode(Ignition, OUTPUT);

//  safetyCheckClose();
  initEEPROM();
  bootUp();
}
 
void loop() {
  now = millis();               // Update now
  
  // Enter firing code, if trigger is pulled, and it is not too soon
  if (digitalRead(Trigger) && ((now - lastshottime) > fireratedelay)) {
    triggered();
    //safetyCheckClose();
    digitalWrite(Buzz, LOW);
    digitalWrite(Blink, LOW);
  }

  if (now - lastshottime > 5000) {
    EEPROM.write(10, LOW);                                                                      // If five seconds have passed since last shot we are no longer in short reset regime
  }
  
  // dump statistics to serial and eeprom every interval
  if (now - lastSampleTime >= interval) {
    dumpData();
  }
  
  // if mode select button is pressed enter modeselect code
  if (digitalRead(Input)) {
    changeFiringMode();
  }
}
 
void fire() {                                                           // Shotfiring subroutine
  //digitalWrite(Vibrate, HIGH);                                          // Vibrate when firing
  digitalWrite(Buzz, HIGH);
  digitalWrite(Blink, HIGH);
  lastshottime = now;                                                   // Update last shot time
    for (int n=0;n<ShotTime+1;n++) {                                    // Inelegant and imprecise, but working, way of opening and closing valves at the proper time (rest of code in loop takes way less than 1ms) 
//      if (n==O_Delay_Time+O_Open_Time) {digitalWrite(O_Valve, LOW);}
//      if (n==O_Open_Time) {digitalWrite(O_Valve, HIGH);}
//      if (n==P_Delay_Time+P_Open_Time) {digitalWrite(P_Valve, LOW);}
//      if (n==P_Open_Time) {digitalWrite(P_Valve, HIGH);}
//      if (n==P_Delay_Time2+P_Open_Time2) {digitalWrite(P_Valve, LOW);}
//      if (n==P_Open_Time2) {digitalWrite(P_Valve, HIGH);}
//      if (n==I_Delay_Time+I_Open_Time) {digitalWrite(Ignition, LOW);}
//      if (n==I_Open_Time) {digitalWrite(Ignition, HIGH);}
      // let the esp8266 deal with it's own processes for a bit 
      delay(1);    
    }
    //digitalWrite(Vibrate, LOW);                                         // Stop vibrating after shot
    digitalWrite(Buzz, LOW);
    digitalWrite(Blink, LOW);
    numShots += 1;                                                      // Count shots
//    Serial.print("Number of shots fired by this gun: ");
//    Serial.print(numShots);
    // Code block to log battery level every hundred shots, for battery testing, disabled in regular software
    /*if ((numShots % 100) == 0) {                        
      
      checkBatt();
    }*/
}

void safetyCheckClose(){
  // Double check that things are closed
  //digitalWrite(P_Valve, LOW);
  //digitalWrite(O_Valve, LOW);
  //digitalWrite(Ignition, LOW);
}

void triggered() {
//if (armed && (voltage > 7)) {                                                               // Continue if weapon is armed and battery is good
  if (armed) {
    EEPROM.write(10, HIGH);                                                                     // Note that a shot is about to be fired, to count short resets, multiple shots in short order does not burden the eeprom extra,                                                                                                // since put only writes if value is different 
    EEPROM.end();
    if (firingMode == 0) {              
      fire();                                                                                 // Fire single
      delay(20); 
      while (digitalRead(Trigger))                                                            // Stay here until trigger is released
      {
        // let the esp8266 deal with it's own processes for a bit 
        yield();
      }
      delay(20);
    } else if (firingMode == 1) {                                                             // Fire burst of three
      fire();
      while (now - lastshottime < fireratedelay) {                                            // Wait for second shot
        now = millis();
      }
      fire();
      while (now - lastshottime < fireratedelay) {                                          // Wait for third shot
        now = millis();
      }
      fire();
      while(digitalRead(Trigger)) {                                                          // After burst, wait here until trigger is released
      }
      delay(20);
    } else if (firingMode == 2) {
      while (digitalRead(Trigger)) {                                                         // If in full auto, keep firing until trigger is released
        if (now - lastshottime >= fireratedelay) {
          fire(); 
        }
        now = millis();                                                                        // Update now to keep firing rate correct. 
      }
    }
  } else {                                                                                   // If weapon is not armed, or voltage is too low, give beeps instead of 
    for (int n = 0; n<3; n++) {
      digitalWrite(Buzz, HIGH);
      digitalWrite(Blink, HIGH);
      delay(100);
      digitalWrite(Buzz, LOW);
      digitalWrite(Blink, LOW);
      delay(100);
    }
  }
}

void dumpData() {
  lastSampleTime = now; 
  EEPROM.write(0, numShots);
  EEPROM.end();
  Serial.print("Number of shots fired by this gun: ");
  Serial.println(numShots);
  Serial.print("Number of short resets: ");
  Serial.println(numShortResets);
  //checkBatt(); disabled for now
}

void changeFiringMode() {
  firingMode = (firingMode + 1) % 3;                                  // increase mode select, modulo three ensures loop back to single if increased beyond full auto 
  for (int n = 0; n < firingMode + 1; n++) {                          // make 1-3 beeps depending on mode
    digitalWrite(Buzz, HIGH);
    digitalWrite(Blink, HIGH);
    delay(200);
    digitalWrite(Buzz, LOW);
    digitalWrite(Blink, LOW);
    delay(200);
    if (digitalRead(Input)) {                                         // If button is still pressed after first beep (400ms) change armed status instead 
      firingMode--;                                                   // Revert firingmode change 
      if (firingMode < 0) { 
        firingMode = 2;                                               // Modulo doesn't work for negative numbers
      }                                                               // Admittedly not the most elegant solution, but works
      armed = !armed;                                                 // switch armed mode
      n = 3;                                                          // Exit beep loop after first beep
      if (armed) {
        Serial.println("Armed");
      } else {
        Serial.println("Disarmed");
      }
      digitalWrite(Buzz, HIGH);                                       // make long beep for armed status change
      digitalWrite(Blink, HIGH);
      delay(1000);
      // save armed status
      EEPROM.write(15, armed);
    }
  }
  digitalWrite(Buzz, LOW);
  digitalWrite(Blink, LOW);
  delay(100);
  // save firingMode
  EEPROM.write(20, firingMode);
  EEPROM.end();
}

void initEEPROM() {
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Reset EEPROM on new device
  if(EEPROM.read(511) > 0) {
    Serial.println("Resetting EEPROM");
    // write a 0 to all 512 bytes of the EEPROM
    for (int i = 0; i < 512; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.end();
  }
  
  // Get data from eeprom
  numShots = EEPROM.read(0);
  numShortResets = EEPROM.read(5);
  shortReset = EEPROM.read(10);
  // Reset within five seconds of shot is deemed unintentional, is counted and armed status and mode is retained
  if (shortReset) {
    armed = EEPROM.read(15);
    firingMode = EEPROM.read(20);
    numShortResets++;
    EEPROM.write(5, numShortResets);
    EEPROM.end();
  }
}

void checkBatt() {
  // Measure battery
  battLevel = analogRead(A0);
  voltage = battLevel * (battMax / 1023.0);
  Serial.print("Battery voltage: ");
  Serial.print(voltage);
  Serial.println(" V");
  // Give warning beep if battery is low
  if (voltage < 7) {
    for (int n = 0; n < 10; n++) {
      digitalWrite(Buzz, HIGH);
      delay(50);
      digitalWrite(Buzz, LOW);
      delay(50);
    }
    Serial.println("BATTERY LOW!");
  }
}

void bootUp() {
  //Prewarming
//  digitalWrite(O_Valve, HIGH);
//  digitalWrite(Ignition, HIGH);
//  for (int n=300;n<700;n++) {
//    digitalWrite(P_Valve, HIGH);
//    delayMicroseconds(n);
//    digitalWrite(P_Valve, LOW);
//    delayMicroseconds(1000-n);
//  }
//  digitalWrite(Ignition, LOW); 
//  for (int n=0;n<1000;n++) {
//    digitalWrite(P_Valve, HIGH);
//    delayMicroseconds(700);
//    digitalWrite(P_Valve, LOW);
//    delayMicroseconds(300);
//  }
//  digitalWrite(O_Valve, LOW);
//  digitalWrite(P_Valve, LOW);
  
  // "Boot up complete" beep cycle
  digitalWrite(Buzz, HIGH);
  digitalWrite(Blink, HIGH);
  delay(100);
  digitalWrite(Buzz, LOW);
  digitalWrite(Blink, LOW);
  delay(100);
  digitalWrite(Buzz, HIGH);
  digitalWrite(Blink, HIGH);
  delay(100);
  digitalWrite(Buzz, LOW);
  digitalWrite(Blink, LOW);
  delay(100);
}

