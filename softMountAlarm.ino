//the web server in this project uses modified code from http://wiki.seeedstudio.com/Wifi_Shield_V2.0/

#include <SoftwareSerial.h>
#include "WiFly.h"

//pin defines
int setRelay = 7;
int resetRelay = 10;
int trig = 12;
int ech = 11;
int buzzer = 9;

//wifi setup
#define SSID "PLUSNET-X6FX"
#define KEY "ede4b94c3b"
#define AUTH WIFLY_AUTH_WPA2_PSK
SoftwareSerial wiflyUart(2, 3);
WiFly wifly(&wiflyUart);
char ip[16];

//other setup
long dur = 0;
int threshold = 500;
bool alarmSet = false;
bool distanceSetThisRound = false;
bool toggleRelayThisRound = false;
bool motionControlRelay = false;
long distance = 0;
bool relayOn = false;
bool distanceSet = false;
unsigned long motionDelayLast = 0;
unsigned long motionDelayInterval = 60000;

void setup() {
  //setup pins
  pinMode(setRelay, OUTPUT);
  pinMode(resetRelay, OUTPUT);
  pinMode(trig, OUTPUT);
  pinMode(ech, INPUT);
  pinMode(buzzer, OUTPUT);
  
  Serial.begin(9600);
  digitalWrite(resetRelay, HIGH);
  delay(100);
  digitalWrite(resetRelay, LOW);

  //connect to wifi and set server params 
  wiflyUart.begin(9600);
  delay(1000);
  wifly.reset();
  delay(1000);
  wifly.sendCommand("set ip local 80\r"); // set the local comm port to 80
  delay(100);
  wifly.sendCommand("set comm remote 0\r"); // do not send a default string when a connection opens
  delay(100);
  wifly.sendCommand("set comm open *OPEN*\r"); // set the string that the wifi shield will output when a connection is opened
  delay(100);
  Serial.println("join " SSID);
  if (wifly.join(SSID, KEY, AUTH)) {
    Serial.println("joined");
  } else {
    Serial.println("fail");
  }
  delay(5000);

  //server and debug
  wifly.sendCommand("get ip\r");
  wiflyUart.setTimeout(500);
  if(!wiflyUart.find("IP=")) {
  	Serial.println("no ip");
  	while(1);;
  } else {
	Serial.println("IP:");
  }
  char c;
  int index = 0;
  while(wifly.receive((uint8_t *)&c, 1, 300) > 0){
  	if(c == ":") {
  		ip[index] = 0;
  		break;
  	}
  	ip[index++] = c;
  	Serial.print((char)c);
  }
  Serial.println();
  while (wifly.receive((uint8_t *)&c, 1, 300) > 0);;
    Serial.println("Web server ready");
}


void loop() {
	//checks for any reqs, pin1 = alarm toggle, pin2 = set distance
  if(wifly.available()) {
  	if(wiflyUart.find("*OPEN*")) {  		
  		Serial.println("new req");
  		delay(1000);
  		if(wiflyUart.find("pin=")) {
  			Serial.println("reading data");
  			int number = (wiflyUart.read()-48);
  			switch (number){
  				case 1:
  					toggleAlarm();
  					Serial.println("alarm toggle");
  					break;
  				case 2:
  					distanceSetThisRound = true;
  					Serial.println("distnace toggle");
  					break;
  				case 3:
  					toggleRelayThisRound = true;
  					Serial.println("relay toggle");
  					break;
          case 4:
            motionControlRelay = !motionControlRelay;
            Serial.println("motion relay toggle");            
            break;
  			}
  		} else {
  			//send website
			// send HTTP header
            wiflyUart.println("HTTP/1.1 200 OK");
            wiflyUart.println("Content-Type: text/html; charset=UTF-8");
            wiflyUart.println("Content-Length: 640"); // length of HTML code, find by opening site then calcelling load and checking network tab in f12
            wiflyUart.println("Connection: close");
            wiflyUart.println();
            // send webpage's HTML code
            wiflyUart.print("<html>");
            wiflyUart.print("<head>");
            wiflyUart.print("<title>Alarm Control</title>");
            wiflyUart.print("</head>");
            wiflyUart.print("<body>");
            wiflyUart.print("<h1>Status</h1>");
            //change status strings
            if(distanceSet){
              wiflyUart.print("Distance set    \n<br>");
            } else {
              wiflyUart.print("DISTANCE NOT SET\n<br>");
            }
            if (alarmSet){
              wiflyUart.print("Alarm activated  \r<br>");
            } else {
              wiflyUart.print("Alarm deactivated\r<br>");
            }
            if (relayOn) {
              wiflyUart.print("Relay activated  \r<br>");
            } else {
              wiflyUart.print("Relay deactivated\r<br>");
            }
            if (motionControlRelay) {
              wiflyUart.print("Motion Controlled relay on \r<br>");
            } else {
              wiflyUart.print("Motion Controlled relay off\r<br>");
            }
            wiflyUart.print("<h1>Controls</h1>");
            wiflyUart.print("<button id=\"2\" class=\"led\">Set new distance</button> "); // returns code 2
            wiflyUart.print("<button id=\"1\" class=\"led\">Toggle Alarm</button> "); // returns code 1
            wiflyUart.print("<button id=\"3\" class=\"led\">Toggle Relay</button> "); // returns code 3
            wiflyUart.print("<button id=\"4\" class=\"led\">Toggle motion controlled relay</button> "); // returns code 4
            wiflyUart.print("<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/2.1.3/jquery.min.js\"></script>");
            wiflyUart.print("<script type=\"text/javascript\">");
            wiflyUart.print("$(\".led\").click(function(){");
            wiflyUart.print("var p = $(this).attr('id');"); // get id value
            wiflyUart.print("$.get(\"http://");
            wiflyUart.print("192.168.1.88");
            wiflyUart.print(":80/a\", {pin:p});");
            wiflyUart.print("});");
            wiflyUart.print("</script>");
            wiflyUart.print("</body>");
            wiflyUart.print("</html>");
        }
        Serial.println("Data sent to browser");
  	}
  } else if (toggleRelayThisRound) {
    //if toggle relay clicked
      toggleRelay();
     toggleRelayThisRound = false;
    } else if (distanceSetThisRound) {
      //if set dis is clicked
      setDistance();
      distanceSetThisRound = false;
    } else {
      //check if motion controlled relay needs to be turned off yet
      if(motionControlRelay && relayOn){
              unsigned long currentMillis = millis();
              if (currentMillis - motionDelayLast >= motionDelayInterval) {
                motionDelayLast = currentMillis;
                toggleRelay();
              }
      }
      //then, check if distance and alarm are set then ping sonar and check if matches distance withing threshold
      if(distanceSet && (alarmSet || motionControlRelay)){
        delay(2500);
        long t = pulse();
        if(t-threshold > distance || t+threshold < distance){
          Serial.println("out 1, valuse: " + (String)t + " " + (String)threshold + " " + (String)distance);
          delay(2500);
          t = pulse();
          if(t-threshold > distance || t+threshold < distance){
            Serial.println("out 2, valuse: " + (String)t + " " + (String)threshold + " " + (String)distance);
            if(motionControlRelay){
                motionDelayLast = millis();
                if (!relayOn){
                  toggleRelay();
                }
              }
            } else {
              activateAlarm();
            }
          }
        }
      
    }
 }

long pulse(){
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  return pulseIn(ech, HIGH);
}

void toggleAlarm(){
  alarmSet = !alarmSet;
  }

void setDistance(){
	Serial.println("old value: " + (String)distance + ", setting new distance value...");
  //pulse without recording to "clean" sensor - sometime the first value is way out 
  //pulse();
	delay(1000);
	long first = pulse();
	delay(1000);
	long second = pulse();
	delay(1000);
	long third = pulse();
	delay(1000);
	long fourth = pulse();
	delay(1000);
	distance = (first + second + third + fourth) / 4;
  Serial.println("set, valuse: " + (String)first + " " + (String)second + " " + (String)third + " " + (String)fourth);
	Serial.println("Distance set as " + (String)distance);
  distanceSet = true;
}

void activateAlarm(){
  tone(buzzer, 1000, 1000);
  /*//this isnt working currently, may change to sound a piezo or something
  //in final design, could sent data to a pi or another arduino in order to send GET reqs to thingpeak/ifttt
	wifly.sendCommand("set ip proto 18\r");
  delay(100);
  wifly.sendCommand("set dns name maker.ifttt.com\r"); // name of the webserver we want to connect to
  delay(100);
	wifly.sendCommand("open\r");
  delay(100);
	wiflyUart.print("GET /trigger/sonarTrigger/with/key/d3NDjW2G31zrSyHeq73p-5\n\n");*/
}

void toggleRelay(){
	if(relayOn){
		digitalWrite(resetRelay, HIGH);
  		delay(100);
 		digitalWrite(resetRelay, LOW);
	} else {
		digitalWrite(setRelay, HIGH);
  		delay(100);
  		digitalWrite(setRelay, LOW);
	}
 relayOn = !relayOn;
}
