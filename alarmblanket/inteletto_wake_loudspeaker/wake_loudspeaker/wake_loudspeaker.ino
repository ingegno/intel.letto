/*************************************************** 
  Inteletto loudspeaker part made for Big Fix
  Written by Ingegno - M.Cristina Ciocci - B.Malengier.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/
// Based on:
/*************************************************** 
  This is an example for the Adafruit VS1053 Codec Breakout

  Designed specifically to work with the Adafruit VS1053 Codec Breakout 
  ----> https://www.adafruit.com/products/1381

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

/*  START USER SETTABLE OPTIONS */
bool use_static_IP = false;         //use a static IP address
uint8_t static_IP[4] = {192, 168, 1, 42}; 
uint8_t static_gateway_IP[4] = {192, 168, 1, 1};// if you want to use static IP make sure you know what the gateway IP is, here 192.168.1.1

// wifi data
// write here your wifi credentials 
//const char* ssid = "*****";   // insert your own ssid 
const char* password = "********"; // and password
const char* ssid = "intelletto";   // insert your own ssid 

//mqtt server/broker 
//const char* mqtt_server = "broker.mqtt-dashboard.com";
//const char* mqtt_server = "raspberrypi.local";
const char* mqtt_server = "192.168.1.29";  //eth0 address of the raspberry pi - Ingegno
uint8_t mqtt_server_IP[4] = {192, 168, 1, 29};
//const char* mqtt_server = "192.168.0.213";  //eth0 address of the raspberry pi - Big Fix
//uint8_t mqtt_server_IP[4] = {192, 168, 0, 213};

/*  END USER SETTABLE OPTIONS */

// debug info
#define SERIALTESTOUTPUT true

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
//following should use ESP version ~/.arduino15/packages/esp8266/hardware/esp8266/2.4.0/libraries/SD
#include <SD.h>
//wifi and timing lib
#include "wifilib.h"

// define the pins used
//#define CLK 13       // SPI Clock, shared with SD card
//#define MISO 12      // Input data, from VS1053/SD card
//#define MOSI 11      // Output data, to VS1053/SD card
// Connect CLK, MISO and MOSI to hardware SPI pins.
// NodeMCU: MISO = D6, MOSI (SI on PCB))=D7 , SCK=D5
// See http://arduino.cc/en/Reference/SPI "Connections"

// These are the pins used for the breakout example
#define BREAKOUT_RESET  D3//9      // VS1053 reset pin (output)
#define BREAKOUT_CS     D8//10     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    D1//8      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS D4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ D2       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  // create breakout-example object!
  Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
  // create shield-example object!
  //Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);
  
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  if (SERIALTESTOUTPUT) Serial.begin(9600);
  if (SERIALTESTOUTPUT) Serial.println("VS1053 Control");

  if (! musicPlayer.begin()) { // initialise the music player
     if (SERIALTESTOUTPUT) Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1); // don't do anything more
  }
  if (SERIALTESTOUTPUT) Serial.println(F("VS1053 found"));
  
   if (!SD.begin(CARDCS)) {
    if (SERIALTESTOUTPUT) Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }

  // list files
  if (SERIALTESTOUTPUT) printDirectory(SD.open("/"), 0);
  
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(volume,volume);

  // Play one file, don't return until complete
  // if (SERIALTESTOUTPUT)  Serial.println(F("Playing track 001"));
  // musicPlayer.playFullFile("TRACK001.mp3");
  // Play another file in the background, REQUIRES interrupts!
  // if (SERIALTESTOUTPUT)  Serial.println(F("Playing track 002"));
  // musicPlayer.startPlayingFile("TRACK002.mp3");

  //now we set up wifi
  
  while (WiFi.status() != WL_CONNECTED) {
   setupWiFi(true); // Connect to local Wifi
  }
  //set randomseed
  randomSeed(micros());

  // mqtt client start
  // Here we subscribe to MQTT topic intellettoLoudSp
  setupMQTTClient();
  
}

void loop() {
  //handle MQTT messages
  handleMQTTClient();
  
  // File is playing in the background
  if (trackplaying && musicPlayer.stopped()) {
    if (SERIALTESTOUTPUT) Serial.println("Done playing music");
    trackplaying = false;
  }

//  if (SERIALTESTOUTPUT && Serial.available()) {
//    char c = Serial.read();
//    
//    // if we get an 's' on the serial console, stop!
//    if (c == 's') {
//      musicPlayer.stopPlaying();
//    }
//    
//    // if we get an 'p' on the serial console, pause/unpause!
//    if (c == 'p') {
//      if (! musicPlayer.paused()) {
//        Serial.println("Paused");
//        musicPlayer.pausePlaying(true);
//      } else { 
//        Serial.println("Resumed");
//        musicPlayer.pausePlaying(false);
//      }
//    }
//  }
  
  if (stopplaying) {
    stopplaying = false;
    if (! musicPlayer.stopped()) {
      //stop current track
      if (SERIALTESTOUTPUT) Serial.println("STOPPING");
      musicPlayer.stopPlaying();
    }
  }

  if (playtrack) {
    // stop running track
    if (! musicPlayer.stopped()) {
      if (SERIALTESTOUTPUT) Serial.println("STOPPING RUNNING TRACK");
      musicPlayer.stopPlaying();
      trackplaying = false;
    }
    while (! musicPlayer.stopped()) {
      delay(10);
    }
    musicPlayer.reset();
    //musicPlayer.begin();
    // set volume first
    // Set volume for left, right channels. lower numbers == louder volume!
    if (SERIALTESTOUTPUT) {
      Serial.print("Setting volume ");Serial.println(volume);
    }
    musicPlayer.setVolume(volume,volume);
    // create file to load
    String trackname = "/";
    trackname += str_dirnr;
    trackname += "/";
    trackname += str_track;
    char trackbuffer[trackname.length()+1];
    trackname.toCharArray(trackbuffer, trackname.length()+1);
    if (SERIALTESTOUTPUT) {
        Serial.print("playing track ");
        Serial.print(trackname);
        Serial.print("  ");
        Serial.println(trackbuffer);
      }
    musicPlayer.startPlayingFile(trackbuffer);
    trackplaying = true;
    playtrack = false;
  }

  //delay(10);
}


/// File listing helper
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

