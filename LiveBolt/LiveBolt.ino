#include <Keypad.h> //include library for the 4x4 matrix keypad
#include <sha1.h>   //include library for hashing
#include <TOTP.h>   //include One-Time-Password library

#include <NTPClient.h> //include library for receiving data from NTP servers
#include <WiFi101.h> // for WiFi 101 shield or MKR1000
#include <WiFiUdp.h> //library needed by NTPClient.h

#include <Adafruit_GFX.h>  //libraries needed for miniature LCD screen
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 32, &Wire); //initialize miniature LCD screen



const char *ssid     = "WIFI SSID NAME HERE";
const char *password = "WIFI PASSWORD HERE";

const byte ROWS = 4;   //initialize # of rows and columns used by keypad
const byte COLS = 4;

const int trigPin = 41;  //initialize pin numbers used by ultrasonic sensor
const int echoPin = 39;

String temp = "";
String latestComplete = ""; //initialize String buffers and flag variables

String code = "123456";     //initialize string to hold code fetched from NTP server

int lock = 5;
int redLED = 3;      //initialize various pin numbers for LED, solenoid, buzzer, and interior release button
int greenLED = 2;
int blueLED = 10;
int buzzer = 6;
int interiorRelease = 9;

long duration;   //initialize temp variables for ultrasonic sensor
int distance;

char hexaKeys[ROWS][COLS] = { //initialize the button values
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte colPins[COLS] = {28, 26, 24, 22}; //set the pins for the columns
byte rowPins[ROWS] = {36, 34, 32, 30}; //set the pins for the rows

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);  //make keypad instance to read

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);  //create UDP client and NTP over UDP instances

uint8_t hmacKey[] = {0x4d, 0x79, 0x4c, 0x65, 0x67, 0x6f, 0x44, 0x6f, 0x6f, 0x72};  //Hex key copied from OTP generator site linked in README.MD
TOTP totp = TOTP(hmacKey, 10);  

char* totpCode; 
char inputCode[7];   //initialize few other variables for reading keypad and comparing code
int inputCode_idx;

void setup() {
  Serial.begin(9600);
  pinMode(lock, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);   //initialize all pins as input/output as required
  pinMode(buzzer, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(interiorRelease, INPUT);
  inputCode_idx = 0;
  WiFi.setPins(8, 7, 4);      //initialize WiFi antenna

  WiFi.begin(ssid, password);  //begin connecting to WiFi, turn LED blue while connecting
  analogWrite(blueLED, 100);
  delay(1000);


  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();  //start NTP client running

  analogWrite(blueLED, 0);
  analogWrite(redLED, 20);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(100); // Pause

  // Clear the buffer.
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  display.clearDisplay();
  display.setCursor(0, 16);
  display.println("Locked");  //display "locked" on the mini LCD
  display.display();
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculating the distance
  distance = duration * 0.034 / 2;

  
  char customKey = customKeypad.getKey(); //read next keypress from keypad
  if (customKey == '*')  // "*" key ends current input and checks
  {

    totpCode = getTOTPCode(); //get new code from NTP server

    if (temp == totpCode)  //if code is correct, turn LED green, chirp buzzer, and unlock door for 5 seconds, then re-lock and turn LED red again
    {
      Serial.println(totpCode);
      Serial.println("Access Granted");
      display.clearDisplay();
      display.setCursor(0, 16);
      display.println("Unlocked"); //display lock status on LCD
      display.display();
      openDoor();
      chirp();
      analogWrite(redLED, 0);
      analogWrite(greenLED, 40);
      delay(5000);
      closeDoor();
      analogWrite(greenLED, 0);
      analogWrite(redLED, 20);
      display.clearDisplay();
      display.setCursor(0, 16);
      display.println("Locked");    //display lock status on LCD
      display.display();
      temp = "";

    }
    else
    {
      Serial.println();   //if wrong code is entered, buzz and blink red LED slightly brighter
      Serial.println("Code Entered: " + temp);
      temp = "";
      analogWrite(redLED, 200);
      chirpBad();
      analogWrite(redLED, 20);

    }
  }
  else if (customKey == '#') //pressing "#" clears current code buffer
  {
    Serial.println("Code cleared");
    temp = "";
  }
  else
  {
    Serial.print(customKey);
    temp += String(customKey);
  }
  if (digitalRead(interiorRelease) == 1 || distance < 6) { // also unlock if Ultrasonic sensor or interior release are pressed
    openDoor();
    display.clearDisplay();
    display.setCursor(0, 16);
    display.println("Unlocked");
    display.display();
    delay(5000);
    closeDoor();
    display.clearDisplay();
    display.setCursor(0, 16);
    display.println("Locked");
    display.display();
  }
}

int getTOTPCode() {

  timeClient.update();  //update from NTP server
  long GMT = timeClient.getEpochTime();  //get Epoch time (# of seconds since January 01, 1970)
  return int(totp.getCode(GMT));  //generate hash, convert to int, and return 6-digit code

}
void chirp() {
  analogWrite(buzzer, 255); //success chirp of buzzer
  delay(100);
  analogWrite(buzzer, 0);
}
void chirpBad() {
  analogWrite(buzzer, 255); //bad chirp of buzzer
  delay(300);
  analogWrite(buzzer, 0);
}
void openDoor() {
  digitalWrite(lock, HIGH); //activate solenoid chirp of buzzer
}
void closeDoor() {
  digitalWrite(lock, LOW); //release solenoid
}
