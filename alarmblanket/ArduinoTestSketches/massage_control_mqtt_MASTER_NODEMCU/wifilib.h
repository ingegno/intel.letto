 
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>

//MQTT library
#include <PubSubClient.h>

bool personinbed = true;

unsigned long NTPUpdateInterval = 60000 ;
 
unsigned long NTPstartTijd;
unsigned long NTPlastUpdate;
time_t utc, localtimenow;
unsigned long huidigeTijd;
unsigned long wifireconnectTime = 0;
unsigned long mqttreconnectTime = 0;

IPAddress ip;
bool obtainedwifi = false;

// Define NTP properties
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "ca.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)
//#define NTP_ADDRESS  "europe.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)
bool UK_DATE = false;

// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Set up the MQTT client
WiFiClient espClient;
PubSubClient MQTTclient(espClient);

String date;
String t;
//const char * days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"} ;
//const char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sep", "Oct", "Nov", "Dec"} ;
//const char * ampm[] = {"AM", "PM"} ;
const char * days[] = {"Zo", "Ma", "Di", "Wo", "Do", "Vr", "Za"} ;
const char * months[] = {"Jan", "Feb", "Maa", "Apr", "Mei", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"} ;
const char * ampm[] = {"VM", "NM"} ;


// set up the wifi connection. If wait, we wait for the wifi to come up
// indefenitely
void setupWiFi(bool wait)
{
  int nrpoints;
  // Connect to WiFi network
  if (SERIALTESTOUTPUT) {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }

  //we only try to set up wifi once every 2 minutes !
  if (wait || (millis() - wifireconnectTime > 2*60000L)) {
    //clean up any old config that are still present
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    if (use_static_IP) {
      // we force a static IP for this station and set 
      IPAddress  stationIP(static_IP[0], static_IP[1], static_IP[2], static_IP[3]);
      IPAddress gateway(static_gateway_IP[0], static_gateway_IP[1], static_gateway_IP[2], static_gateway_IP[3]); // set gateway to match your network
      IPAddress subnet(255, 255, 255, 0); // set subnet mask to match yourelse
      WiFi.mode(WIFI_STA);
      delay(100);
      
      //set a static IP address
      WiFi.config(stationIP, gateway, subnet);
    } else {
      WiFi.mode(WIFI_AP);
    }
    WiFi.begin(ssid, password);
    wifireconnectTime = millis();
  }

  if (wait) {
    unsigned long startwait = millis();
    while (WiFi.status() != WL_CONNECTED) {
      nrpoints += 1;
      delay(500);
      if (SERIALTESTOUTPUT) {
        Serial.print(".");
      }
      if (nrpoints > 50) {
        nrpoints = 0;
         if (SERIALTESTOUTPUT) Serial.println(" ");
      }
      if (millis() - startwait > 20000L ) {
        return;
      }
    }
    
    if (SERIALTESTOUTPUT) {
      Serial.println("");
      Serial.println("WiFi connected");
    }
  } 
}

void MQTTsubscribe2topics() {
  //write here all MQTT topics we subscribe to. Act on it in the callback!
  MQTTclient.subscribe("intellettoMassage");
}

void MQTTpublish_reconnected() {
  //publish a message that we connected to 
  MQTTclient.publish("intellettoStatus", "Massage reconnected to MQTTserver");
}

boolean MQTTpublish(const char* topic, const char* payload) {
  // API to publish to the server. 
  // this is used by intelletto to 
  // 1. switch on/off sonoff plugs
  // 2. control massage
  // 3. control heating
  MQTTclient.publish(topic, payload);
}

void MQTTreconnect() {
  // We connect to the MQTT broker
  
  //we only try to set up mqtt connection once every 1 minutes !
  if (!MQTTclient.connected() && millis() - mqttreconnectTime > 1*60000L) {
    if (SERIALTESTOUTPUT) {
     Serial.print("Attempting MQTT connection...");
    }
    // Create a random client ID
    String clientId = "ESP8266AlarmClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (MQTTclient.connect(clientId.c_str())) {
      if (SERIALTESTOUTPUT) {
        Serial.println("connected");
      }
      // Once connected, publish an announcement ?
      MQTTpublish_reconnected();
      // ... and resubscribe to all topics
      MQTTsubscribe2topics();
    } else {
      if (SERIALTESTOUTPUT) {
        Serial.print("failed, rc=");
        Serial.print(MQTTclient.state());
        Serial.println(" try again in 5 seconds");
      }
    }
    mqttreconnectTime = millis();
  }
}

void MQTT_msg_callback(char* topic, byte* payload, unsigned int length) {
  // if a topic subscribed to in reconnect occurs, this callback is called
  // Here we only subscribe to topic intellettoMassage which is a number to 
  // indicate what massage to start. 
  // We propagate this to the SLAVE Arduino controller
  if (SERIALTESTOUTPUT) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }

  // The intelletto alarm subscribes to the intellettoMassage topic
  // which is a code (Neck, Breast, Tummy, Hip) + number 0 off, 1 mid, 2 strong

  if ((char)payload[0] == 'O') {
    //switch massage off
      send2Slave(MS_NONE);
  }
  
  if ((char)payload[0] == 'N') {
    //control command. Set a massage zone
    if ((char)payload[1] == '0') {
      // Turn Neck Massage off  
      send2Slave(MS_NECKOFF);
    } else if ((char)payload[1] == '1'){
      // Turn Neck Massage to medium
      send2Slave(MS_NECKWEAK);
    } else {
      // Turn Neck Massage to high
      send2Slave(MS_NECK);
    }
  }
  
  if ((char)payload[0] == 'B') {
    //control command. Set a massage zone
    if ((char)payload[1] == '0') {
      // Turn Massage off  
      send2Slave(MS_BREASTOFF);
    } else if ((char)payload[1] == '1'){
      // Turn Massage to medium
      send2Slave(MS_BREASTWEAK);
    } else {
      // Turn Massage to high
      send2Slave(MS_BREAST);
    }
  }
  
  if ((char)payload[0] == 'T') {
    //control command. Set a massage zone
    if ((char)payload[1] == '0') {
      // Turn Massage off  
      send2Slave(MS_BELLYOFF);
    } else if ((char)payload[1] == '1'){
      // Turn Massage to medium
      send2Slave(MS_BELLYWEAK);
    } else {
      // Turn Massage to high
      send2Slave(MS_BELLY);
    }
  }
  
  if ((char)payload[0] == 'H') {
    //control command. Set a massage zone
    if ((char)payload[1] == '0') {
      // Turn Massage off  
      send2Slave(MS_HIPOFF);
    } else if ((char)payload[1] == '1'){
      // Turn Massage to medium
      send2Slave(MS_HIPWEAK);
    } else {
      // Turn Massage to high
      send2Slave(MS_HIP);
    }
  }
}

void setupMQTTClient() {
  // to call in the setup of the arduino
  MQTTclient.setServer(mqtt_server, 1883);
  MQTTclient.setCallback(MQTT_msg_callback);
}

void handleMQTTClient() {
  // to call in the loop of the arduino
  if (!MQTTclient.connected()) {
    MQTTreconnect();
  }
  MQTTclient.loop();
}

void obtainDateTime() {
  if (WiFi.status() == WL_CONNECTED) //Check WiFi connection status
  {   
    ip = WiFi.localIP();
    obtainedwifi = true;
    date = "";  // clear the variables
    t = "";
    
    // update the NTP client and get the UNIX UTC timestamp 
    timeClient.update();
    NTPlastUpdate = millis();
    unsigned long epochTime =  timeClient.getEpochTime();

    // convert received time stamp to time_t object
    time_t local;
    utc = epochTime;
    setTime(utc);
    
    // Then convert the UTC UNIX timestamp to local time
    // normal time from zon 2 march to sun 2 nov 
    TimeChangeRule euBRU = {"BRU", Second, Sun, Mar, 2, +60};  //normal time UTC + 1 hours - change this as needed
    TimeChangeRule euUCT = {"UCT", First, Sun, Nov, 2, 0};     //daylight saving time summer: UTC - change this as needed
    Timezone euBrussel(euBRU, euUCT);
    local = euBrussel.toLocal(utc);

    // now format the Time variables into strings with proper names for month, day etc
    date += days[weekday(local)-1];
    date += ", ";
    date += day(local);
    date += " ";
    date += months[month(local)-1];
    date += ", ";
    date += year(local);

    if (UK_DATE) {
      // format the time to 12-hour format with AM/PM and no seconds
      t += hourFormat12(local);
    } else {
      // normal format hour
      t += hour(local);
    }
      t += ":";
    if(minute(local) < 10)  // add a zero if minute is under 10
      t += "0";
    t += minute(local);
    t += " ";
    if (UK_DATE) {
      t += ampm[isPM(local)];
      t += " ";
    }

    if (SERIALTESTOUTPUT) {
      // Display the date and time
      Serial.println("");
      Serial.print("Local date: ");
      Serial.print(date);
      Serial.println("");
      Serial.print("Local time: ");
      Serial.print(t);
    }
  } else {
    //reconnect
    obtainedwifi = false;
  }
}


