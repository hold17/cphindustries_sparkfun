// COPENHAGEN INDUSTRIES
// Strawberry 1
// Hardware: STRAWBERRY 2.6.x CNC ("DAQUIRY")
// Firmware: J1 - Morning Glory
// Build: 0.459
// Date: 21 Sep 2017 MODIFYED By Troels Lund 27/12/2017

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

/////////////////////////
// Batteri Definitions //
/////////////////////////

float vccMax = 3.6;
float vccMin = 1.75;
static float batteriP = 0;

//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "NETGEAR";
const char WiFiPSK[] = "";

const char WEAPON_TYPE[] = "AK-47";

int id = 0;

int fireratedelay = 90;      // delay between each shot / firing rate

int O_Open_Time = 20;       // Oxygen Open Time (in ms)
//int O_Delay_Time = 0;      // Delay between open and close of oxygen valve
int P_Open_Time = 20;       // Propane Open Time (in ms)
//int P_Delay_Time = 0;      // Delay between open and close of propane valve
//int I_Open_Time = 0;       // Time of start of ignition
//int I_Delay_Time = 0;      // Duration ignition remains on

int O_level = 100;
int P_level = 100;

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

byte mac[6];                     // the MAC address of your Wifi shield
char macStr[18];
/////////////////////
// Pin Definitions //
/////////////////////
const int LED_PIN = 5; // Thing's onboard, green LED
const int ANALOG_PIN = A0; // The only analog pin on the Thing
const int DIGITAL_PIN = 12; // Digital pin to be read

const int Trigger = 14;       // Trigger
const int Input = 15;         // Mode select / other input
const int Buzz = 0;           // Buzzer
const int Blink = 5;          // On-board LED
//const int Vibrate = 3;      // Vibrator
//const int P_Valve = 8;      // PROPANE
//const int O_Valve = 10;     // OXYGEN
//const int Ignition = 4;     // Ignition

ADC_MODE(ADC_VCC);
WiFiServer server(80);


void setup() {

  initHardware();
  connectWiFi();
  delay(100);
  server.begin();
  setupMDNS();

  Serial.begin(9600);
  while (!Serial) {
    yield();
 }

  //checkBatt(); disabled for now


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


// wifi server...


  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  // Match the request
  int val = -1; // We'll use 'val' to keep track of both the
                // request type (read/set) and value if set.
  if (req.indexOf("/led/0") != -1)
    val = 1; // Will write LED high
  else if (req.indexOf("/led/1") != -1)
    val = 0; // Will write LED low
  else if (req.indexOf("/read") != -1)
    val = -2; // Will print pin reads
  else if (req.indexOf("/isWeapon") != -1)
    val = -3; // Will print pin reads
  else if (req.indexOf("/ekstra") != -1)
    val = -4; // Will print pin reads
  else if (req.indexOf("/arm") != -1)
    val = -5; // Will print pin reads
  else if (req.indexOf("/disarm") != -1)
    val = -6; // Will print pin reads
  else if (req.indexOf("/shootmode/singel") != -1)
    val = -7; // Will print pin reads
  else if (req.indexOf("/shootmode/semi") != -1)
    val = -8; // Will print pin reads
  else if (req.indexOf("/shootmode/auto") != -1)
    val = -9; // Will print pin reads
  // Otherwise request will be invalid. We'll say as much in HTML

  // Set GPIO5 according to the request
  if (val >= 0)
    digitalWrite(LED_PIN, val);

  client.flush();

  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  // If we're setting the LED, print out a message saying we did
  if (val >= 0)
  {
    s += "LED is now ";
    s += (val)?"off":"on";
  }
  else if (val == -2)
  { // If we're reading pins, print out those values:
  s += "{\"id\":"; s+= id; s+=",\"propaneLevel\":";  s+= P_level; s+=",\"propaneTime\":"; s+=P_Open_Time; s+=",\"name\": \"No name device\",\"oxygenLevel\":"; s+= O_level; s+=",\"batteryLevel\":"; s+=ESP.getVcc(); s+=",\"type\": \""; s+= WEAPON_TYPE; s+="\",\"mac\": \"";  s+= macStr;  s+= "\",\"ip\": \""; s += WiFi.localIP().toString(); s+="\",\"oxygenTime\": 0.5,\"connectionStrength\": "; s+= WiFi.RSSI();  s+=" ,\"shootingMode\": "; s+=firingMode; s+=" ,\"armed\": "; s+=armed; s+= "}";
  }
  else if (val == -3)
  { // tjek if it is a weapon
    s += "True";
  }
  else if (val == -4)
  { // tjek if it is a weapon
    s += "{ numShots :"; s+=numShots; s+="}";
  }
    else if (val == -5)
  { // Arm weapon
   armed = 1;
   if (armed) {
        Serial.println("Armed");
      } else {
        Serial.println("Disarmed");
      }
      digitalWrite(Buzz, HIGH);                                       // make long beep for armed status change
      delay(200);
      digitalWrite(Buzz, LOW);                                       // make long beep for armed status change
   EEPROM.write(15, 1);
  }
    else if (val == -6)
  { // disarm weapon
   armed = 0;
   if (armed) {
        Serial.println("Armed");
      } else {
        Serial.println("Disarmed");
      }
      digitalWrite(Buzz, HIGH);                                       // make long beep for armed status change
      delay(200);
      digitalWrite(Buzz, LOW);                                       // make long beep for armed status change
   EEPROM.write(15, 0);
  }
   else if (val == -7)
  { // Arm weapon
   firingMode = 0;
   EEPROM.write(20, firingMode);
  }
   else if (val == -8)
  { // Arm weapon
   firingMode = 1;
   EEPROM.write(20, firingMode);
  }
   else if (val == -9)
  { // Arm weapon
   firingMode = 2;
   EEPROM.write(20, firingMode);
  }
  else
  {
    s += "Invalid Request.<br> Try /led/1, /led/0, /isWeapon, /ekstra, /arm, /disarm or /read. ";
  }
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected
  // when the function returns and 'client' object is detroyed


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
    O_level = O_level - (O_Open_Time * 0.05);
    P_level = P_level - (P_Open_Time * 0.05);
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

  armed = EEPROM.read(15);          // tilføjet 1/1
  firingMode = EEPROM.read(20);     // tilføjet 1/1

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

void connectWiFi()
{
  byte ledStatus = LOW;
  Serial.println();
  Serial.println("Connecting to: " + String(WiFiSSID));
  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);

WiFi.macAddress(mac);

  // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
  // to the stated [ssid], using the [passkey] as a WPA, WPA2,
  // or WEP passphrase.
  WiFi.begin(WiFiSSID, WiFiPSK);

  // Use the WiFi.status() function to check if the ESP8266
  // is connected to a WiFi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    digitalWrite(LED_PIN, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;

    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
  }

  Serial.println("///////////////////////");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("///////////////////////");
  postHub();
}

void setupMDNS()
{
  // Call MDNS.begin(<domain>) to set up mDNS to point to
  // "<domain>.local"
  if (!MDNS.begin("thing"))
  {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

}

void initHardware()
{

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

  Serial.begin(9600);
  pinMode(DIGITAL_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(Trigger, INPUT);
  pinMode(Buzz, OUTPUT);
  // Don't need to set ANALOG_PIN as input,
  // that's all it can be.
}

void postHub(){

snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

int httpCode;
String payload;

String j;

j += "{";
j +="\"mac\""; j+=":\""; j+= macStr; j+= "\",";
j +="\"ip\""; j+= ":\""; j+= WiFi.localIP().toString(); j+= "\",";
j +="\"model\""; j+= ":\""; j+= WEAPON_TYPE; j+= "\"";
j += "}";


 if(WiFi.status()== WL_CONNECTED){ //Check WiFi connection status
   HTTPClient http; //Declare object of class HTTPClient
  do{
   http.begin("http://192.168.0.3/api/weapons"); //Specify request destination
   http.addHeader("Content-Type", "application/json"); //Specify content-type header
   Serial.println(j);
   httpCode = http.POST(j); //Send the request
   payload = http.getString();  //Get the response payload
   Serial.println(payload); //Print request response payload

  }while(httpCode != 200);
   Serial.println(httpCode); //Print HTTP return code


   http.end();  //Close connection
  } else {
    Serial.println("Error in WiFi connection");
  }

StaticJsonBuffer<50> jsonBuffer;

JsonObject& root = jsonBuffer.parseObject(payload);//parse JSON String for NinjaCloud

id = root["id"];
Serial.println("id: " + id);
  }

/*
void setStaticIP(){
// Static IP details...
IPAddress ip(192, 168, 1, 222);
IPAddress gateway(192, 168, 1, 249);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 249);

// Static IP Setup Info Here...
WiFi.config(ip, dns, gateway, subnet); //If you need Internet Access You should Add DNS also...
/WiFi.begin(ssid, password);

  }
  */