//Libraries
#include <Wire.h>//https://www.arduino.cc/en/reference/wire
#include <Adafruit_PWMServoDriver.h>//https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "FirebaseESP8266.h" 

//Port Connection
#define watersensor D9 //port definition of sensor

//WiFi Connections
#define STASSID "ABCDEF" //SSID of Mobile hotspot
#define STAPSK  "88888888" //PW of Mobile hotspot

const char* ssid = STASSID;
const char* password = STAPSK;

//Firebase Server Connections
#define FIREBASE_HOST "fbssystem-b9efe-default-rtdb.firebaseio.com" 
#define FIREBASE_AUTH "MT2pJcqkwsBh0OJyjNdiBTZL6wNwvbUkdrT6P2eV"

//Weather Data RSS from KMA
const char* host = "www.kma.go.kr";
const int httpsPort = 80;
String url = "/wid/queryDFSRSS.jsp?zone=4111156600";

//Trigger Variables
bool trig;
int trig_cnt = 0;
bool sensor, weather, manual;

//Objects
Adafruit_PWMServoDriver pca= Adafruit_PWMServoDriver(0x40);
FirebaseData firebaseData;
FirebaseJson json;

//Weather test variable
//int test_cnt = 0;

void printResult(FirebaseData &data);

void setup(){

//Serial Communication Initialization
Serial.begin(9600);
Serial.println("Initialize System");

//Sensor pinMode Setup
pinMode(watersensor, INPUT);

//Servo Motor Driver Setup
pca.begin();
pca.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
pca.writeMicroseconds(0,2500);
pca.writeMicroseconds(1,500);

delay(2000);

//WiFi Setup
WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
Serial.println("Connected to WiFi");

//Firebase Setup
Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
Firebase.reconnectWiFi(true);

firebaseData.setBSSLBufferSize(1024, 1024);
firebaseData.setResponseSize(1024);
Firebase.setReadTimeout(firebaseData, 1000 * 60);
Firebase.setwriteSizeLimit(firebaseData, "tiny");
}

void loop(){
  //Increase test counter
  //test_cnt++;

  //Trigger Declaration
  sensor = !digitalRead(watersensor);
  /* In case of weather trigger,
  you may use either test code or real RSS code. 
  Default has been set as RSS mode. 
  To use test mode, label line 87 and delabel line 38, 79, 88. */
  weather = getweatherdata(); 
  //weather = weathertest(test_cnt);
  manual = getswitchdata();

  trig = sensor || weather || manual;

  //Trigger Test
  //Will not be used in real model
  Serial.println(sensor);
  Serial.println(weather);
  Serial.println(manual);
  Serial.println(trig);
  Serial.println("    ");

  //FBS Activation
  if(trig && (!trig_cnt)){ //Activate
    manual = true;
    Firebase.setString(firebaseData, "FBS_STM/Alert_down", "true");
    pcaUp();
    trig_cnt++;
  }
  else if(!manual && trig_cnt){ //Deactivate
    pcaDown();
    Firebase.setString(firebaseData, "FBS_STM/Alert_down", "false");
    trig_cnt = 0;
  }
  delay(5000);
}

//Gate Upward
void pcaUp()
{
      for(int pos=500;pos<2200;pos+=10){
        pca.writeMicroseconds(0,2700-pos);
        pca.writeMicroseconds(1,pos);delay(10);
      }
} 

//Gate Downward
void pcaDown()
{
      for(int pos=2200;pos>500;pos-=10){
        pca.writeMicroseconds(0,2700-pos);
        pca.writeMicroseconds(1,pos);delay(10);
      }
} 

//Getting Weather Data from RSS
bool getweatherdata()
{
  String strKey = "r06";
  bool check = false; //valid bit for dataParsing
  String strTmp; //Temporary String data

  WiFiClient client; 
  
  if (!client.connect(host, httpsPort)) { //If not connected to KMA RSS
    Serial.println("connection Failed"); //Print connection failed
    return false;
  }
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n"); //Request to RSS server
 
  unsigned long t = millis(); //stopwatch
  while(1){ //Infinite loop with two break options
    if(client.available()) break; //Option 1. Return data from Client
    if(millis() - t > 10000) break; //Option 2. 10 sec passed without output data
  }
 
  while(client.available()) { 
    String data = client.readStringUntil('\n');
    delay(1); 

    strTmp = dataParsing(data, strKey, &check);
  }

  float floatTmp = strTmp.toFloat(); //Change Data Type to float

  bool result = (floatTmp > 100.0) ? true : false; 

  return result; //Return Boolean Data
}

//Data parsing for weather RSS
String dataParsing(String DATA, String str, bool *check)
{ 
  int s = -1; //Start of index
  int e = -1; //End of index
  
  if (str=="r06"){ //Parsing r06 data(Rainfall in 6 hours)
    String temp = "<r06>";
    s = DATA.indexOf(temp) + temp.length(); 
    e = DATA.indexOf("</r06>"); 
  }
  
  if( (s != -1) && (e != -1) && !(*check) ) { //See the validity
    (*check) = true; 
    return DATA.substring(s,e); //valid output
  }
  return "e"; //invalid output
}  

//Get Application Switch Data from Firebase
bool getswitchdata()
{
  String man_str; 
  bool man_bool; 

  if(Firebase.getString(firebaseData,"FBS_STM/Alert_up")) {
    man_str = firebaseData.stringData(); //type of data sent from application: string
    man_bool = (man_str == "true") ? true : false; //String to Boolean type
 }

  return man_bool;
}

// Test set for weather trigger
bool weathertest(int count)
{
  bool arr[20] = {false, false, false, true, true, true, true, false, false, false, false, false, false, false, false, false, false, false, false, false};
  
  return arr[count % 20];
} 
