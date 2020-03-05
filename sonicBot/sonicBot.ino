//#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"   //Include File System Headers
//#include "SSD1306Spi.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <Ticker.h>  //Ticker Library for timer Interrupt
#include <Servo.h>

//Defines for H bridge
#define enA 4 //D2
#define in1 0 //D3
#define in2 2 //D4
#define in3 16 //D0
#define in4 D8 //S3
#define enB 5 //D1
#define DEAD_BAND 200

//Defines for Arduino OTA
//uncomment line to enable OTA update
#define OTAUpdate

Servo weaponservo;

//Defines for US
int trigger = D5;
int echo = D6;
long duration = 0;
long distance = 0;

long previousMillis = 0;
unsigned long currentMillis = 0;
int weapon_rearm_time = 200; // ms
bool weapon = true;

//ChangeMe
String botName = "myBot";
String botPassword = "";

Ticker checkIPAddress;

const char* filename = "/index.html";
const char* htmlRobotInUse = "/robotInUse.html";
int angleX = 90;
int angleY = 90;
bool deviceIsConnected = false;
bool manualMode = true;
String currentClientIPAddress;

AsyncWebServer server(80);

void setMotorSpeed(int leftMotor, int rightMotor) {
  if (leftMotor < - DEAD_BAND) {   //Turn left
    analogWrite(enA, - leftMotor); // Send PWM signal to L298N Enable pin
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    //Serial.println(leftMotor);
  } else if (leftMotor > DEAD_BAND) {
    analogWrite(enA, leftMotor); // Send PWM signal to L298N Enable pin
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  if (rightMotor < - DEAD_BAND) {   //Turn left
    analogWrite(enB, - rightMotor); // Send PWM signal to L298N Enable pin
    digitalWrite(in3, HIGH);
    digitalWrite(in4, LOW);
    //Serial.print(enB);
    //Serial.println(" Right");
  } else if (rightMotor > DEAD_BAND) {
    analogWrite(enB, rightMotor); // Send PWM signal to L298N Enable pin
    digitalWrite(in3, LOW);
    digitalWrite(in4, HIGH);
    //Serial.print(enB);
    //Serial.println(" Left");
  } else {
    //Serial.println("STOP");
    digitalWrite(in3, LOW);
    digitalWrite(in4, LOW);
  }
}

/**
   Function to convert the touch inputs from the Website to a motor speed.
*/
int angleToMotorSpeed (int angle) {
  long motorSpeed = 0;
  motorSpeed = angle * (1023 / 45) - 2048;
  if (motorSpeed < -1023) {
    motorSpeed = -1023;
  }
  if (motorSpeed > 1023) {
    motorSpeed = 1023;
  }
  return motorSpeed;
}

/**
   Function to check if a connection from a client is still alive.
*/
void checkForIPAddress() {
  if (currentClientIPAddress != "") {
    Serial.println("Reset IP Address");
    deviceIsConnected = false;
    currentClientIPAddress = "";
    angleX = 90;
    angleY = 90;
    int leftMotor = angleToMotorSpeed(angleX);
    int rightMotor = leftMotor;
    setMotorSpeed(leftMotor, rightMotor);
  }
}

void moveWheels() {
  for (int i = 0; i < 4; i++) {
    setMotorSpeed(1023, 1023);
    delay(100);
    setMotorSpeed(0, 0);
    delay(100);
  }
}

/**
   Function to receive the distance measured by the us-sensor in cm
*/
long getUSDistance() {
  digitalWrite(trigger, LOW); //Hier nimmt man die Spannung für kurze Zeit vom Trigger-Pin, damit man später beim senden des Trigger-Signals ein rauschfreies Signal hat.
  delay(5); //duration: 5 Millisekunden
  digitalWrite(trigger, HIGH); //Jetzt sendet man eine Ultraschallwelle los.
  delay(10); //Dieser „Ton“ erklingt für 10 Millisekunden.
  digitalWrite(trigger, LOW);//Dann wird der „Ton“ abgeschaltet.
  duration = pulseIn(echo, HIGH); //Mit dem Befehl „pulseIn“ zählt der Mikrokontroller die Zeit in Mikrosekunden, bis der Schall zum Ultraschallsensor zurückkehrt.
  distance = (duration / 2) * 0.03432; //Nun berechnet man die distance in Zentimetern. Man teilt zunächst die Zeit durch zwei (Weil man ja nur eine Strecke berechnen möchte und nicht die Strecke hin- und zurück). Den Wert multipliziert man mit der Schallgeschwindigkeit in der Einheit Zentimeter/Mikrosekunde und erhält dann den Wert in Zentimetern.
  return distance;
}

void setup()
{
  Serial.begin (115200); //Start serial communication
  Serial.println("Setup Begin");
  //Initialize H bridge
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
  // Set initial rotation direction
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  //Init US
  //pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

  weaponservo.attach(13);


  //Initialize File System
  if (SPIFFS.begin())
  {
    Serial.println("SPIFFS Initialize....ok");
  }
  else
  {
    Serial.println("SPIFFS Initialization...failed");
  }

  File f = SPIFFS.open(filename, "r");

  if (!f) {
    Serial.println("file open failed");
  }
  else
  {
    //Write html site to file
    Serial.println("Writing Data to File");
  }

  WiFi.setAutoConnect(0);                    //Enables or Disaple WiFi Quick Connect

  //Initialize WiFi Settings
  WiFi.softAP(botName, botPassword); //Create WiFi hotspot
  WiFi.mode(WIFI_AP);
  Serial.print("Access Point \"");
  Serial.println("\" started");
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());         // Send the IP address of the ESP8266 to the computer

  //Arduino OTA
#ifdef OTAUpdate
  ArduinoOTA.setHostname("BOTOTA");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
#endif


  /**
     Main server part starts here.
  */

  //Function to execute if Button on Homepage was pressed
  server.on("/button/", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Button pressed");
    weapon = true;
    previousMillis = millis();
    Fire();
    request -> send(200);
  });

  //Function to execute if user makes touch controls on mobile screen
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (currentClientIPAddress == "" || currentClientIPAddress == request->client()->remoteIP().toString()) {
      currentClientIPAddress = request->client()->remoteIP().toString();
      checkIPAddress.detach(); //Disable Timer
      deviceIsConnected = true;
      int paramsNr = request->params();
      //Serial.println(paramsNr);
      for (int i = 0; i < paramsNr; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "angleX") {
          angleX = p->value().toInt();
          //   Serial.print("Angle x:");
          //   Serial.println(angleX);
        } else if (p->name() == "angleY") {
          angleY = p->value().toInt();
          //    Serial.print("Angle y:");
          //    Serial.println(angleY);
        } else {
          Serial.print("unknown name: ");
          Serial.print(p->name());
          Serial.print(", value to int: ");
          Serial.println(p->value().toInt());
        }
      }
      if (request -> params() == 0) {
        request->send(SPIFFS, "/index.html");
      } else {
        request -> send(200);
      }
      checkIPAddress.attach_ms(500, checkForIPAddress); //Enable Timer again
    } else {
      request->send(SPIFFS, "/robotInUse.html");
      Serial.print("Declined Connection from: ");
      Serial.print(request->client()->remoteIP().toString());
      Serial.print(", The following device is already connected: ");
      Serial.println(currentClientIPAddress);
    }
  });
  server.begin();

  //Attach Timer Interrput to check IP Address
  checkIPAddress.attach_ms(500, checkForIPAddress);

  Serial.println("Setup Finished");
}

void Fire(){
      weaponservo.write(180);
      currentMillis = millis();

      if(currentMillis - previousMillis > weapon_rearm_time) {
          weaponservo.write(0);
          weapon = false;
          previousMillis = currentMillis;
      }
}

void loop()
{ 
  if(weapon){
    Serial.println("FIRE!");
    Fire();
  }
  if (manualMode) {

    if (deviceIsConnected) {
      int leftMotor = angleToMotorSpeed(angleX);
      int rightMotor = leftMotor;
      //Serial.print(leftMotor);
      //Serial.println(leftMotor);
      double steer = angleToMotorSpeed(angleY) / 1023.0;
      if (steer < 0) {
        leftMotor = leftMotor + steer * leftMotor;
      } else {
        rightMotor = rightMotor - steer * rightMotor;
      }
      setMotorSpeed(leftMotor, rightMotor);
    }
  } else {
    /**
       TODO: Implement code for us-sensor
       The following functions might be helpful:
           long distance = getUSDistance();         //Get us-distance in cm
           Serial.println(distance);                //print distance to console
           setMotorSpeed(1023, 1023);               //set motor speed for left and right wheel
           delay(100)                               //Add additional delay
    */
  }
#ifdef OTAUpdate
  ArduinoOTA.handle();
#endif
}
