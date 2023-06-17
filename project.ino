#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Servo
Servo myServo;
#define SERVOPIN 12
#define BUTTON 11

//Ultrasonic
#define TRIGPIN 9
#define ECHOPIN 10
#define LEDINDICATOR 2

//Color
#define LIGHT 3
#define sensorOut 8
#define S0 4
#define S1 5
#define S2 6
#define S3 7
int colorSampleCount = 3;


//password
String password[] = {"red", "green", "blue"};
int passwordSize = 3;
int passwordCheckDuration = 2000;//collect color samples for this many milliseconds to verify it is a password color or if it is indecipherable


//display text
int lastAccess = 0;
bool hasBeenAccessed = false;

bool isWelcomed = false;

String lcdRow0;
String lcdRow1;

//lock servo
int locked = true;
int lockTimeout = 0;
int lockTimeoutDuration = 30000;

int distSampleSize = 20;
int distSamples[20];
int distSampleIdx = 0;
int lastDistCheckTime = 0;
int distSamplingRate = 200;
bool lastUserPresent = false;

int displayTimer = 0;
int displayDuration = 2000;
int lastTimeDisp = millis();
bool displayFree = true;


int buttonPressDuration = 500;
//backlight
bool backlightOn = false;

int redMin = 20;
int greenMin = 25;
int blueMin = 20;

int redMax = 204;
int greenMax = 210;
int blueMax = 200;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  setupLCD();
  setupUltrasonicSensor();
  setupColorSensor();
  setupServo();
  pinMode(LEDINDICATOR, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  lockSafe();
}








void loop() {
  // put your main code here, to run repeatedly:
  //Serial.println(getCardColor());
  if (millis()-lastTimeDisp>displayDuration) {
    displayFree = true;
    lastTimeDisp = millis();
  }
  
  if (userPresent()) {
    if (!isWelcomed) {
      if (displayFree){
        clearLCD();
        display("Welcome, User.", 0, 0);
        displayFree = false;
        lastTimeDisp = millis();
      }
      isWelcomed = true;
    }
    if (buttonClicked()) {
      if (locked){
        attemptEntry();
      }else {
        lockSafe();
      }
    }else {
      displayLastAccess();
    }
  }else {
    isWelcomed = false;
    lockSafe();
    delay(displayDuration);
    clearLCD();
    turnOffBacklight();
  }

}

void attemptEntry() {
  if (!locked) {
    unlockSafe();
    return;
  }
  clearLCD();
  display("Attempting entry...", 0, 0);
  digitalWrite(LIGHT, HIGH);
  bool success = true;
  for (int i=0; i<passwordSize; i++) {
    String expectedColor = password[i];
    success = success && verifyColor(expectedColor, i);
  }
  digitalWrite(LIGHT, LOW);
  
  clearLCD();
  if (success) {
    unlockSafe();
    display("Success.", 0, 0);
  }else {
    display("Wrong Password.", 0, 0);
  }
  displayFree = false;
  lastTimeDisp = millis();
  
  return;
}

bool verifyColor(String expected, int displayCount) {
  clearLCD();
  display((1+displayCount)+"", 0, 2*displayCount);
  String color = getPasswordColorDisplayed();
  String colorShort;
  if (color.equals("RED")) {
    colorShort = "R";
  }else if (color.equals("GREEN")) {
    colorShort = "G";
  }if (color.equals("BLUE")) {
    colorShort = "B";
  }else {
    colorShort = "X";
  }
  display(colorShort, 1, 2*displayCount);
  return color.equals(expected);
}

String getPasswordColorDisplayed() {
  int redCount = 0;
  int grnCount = 0;
  int bluCount = 0;
  int otherCount = 0;

  int startTime = millis();
  int displayDelay = 100;
  int lastDisplay = 0;
  while (millis()-startTime<passwordCheckDuration) {
    String color = getCardColor();
    if (color.equals("RED")) {
      redCount++;
    }else if (color.equals("GREEN")) {
      grnCount++;
    }else if (color.equals("BLUE")) {
      bluCount++;
    }else {
      otherCount++;
    }

    
    String colorShort;
    if (color.equals("RED")) {
      colorShort = "R";
    }else if (color.equals("GREEN")) {
      colorShort = "G";
    }if (color.equals("BLUE")) {
      colorShort = "B";
    }else {
      colorShort = "X";
    }
    if (millis()-lastDisplay>displayDelay) {
      lastDisplay = millis();
      display(colorShort, 0, 15);
    }
  }
  int mult = 5;
  if (redCount>mult*grnCount && redCount>mult*bluCount) {
    return "RED";
  }else if (grnCount>mult*redCount && grnCount>mult*bluCount) {
    return "GREEN";
  }else if (bluCount>mult*grnCount && bluCount>mult*redCount) {
    return "BLUE";
  }else {
    return "INVALID";
  }
}

String getCardColor() {
  // Read Red value
  delay(200);
  int redPW = getRedPW();
  // Map to value from 0-255
  int red = map(redPW, redMin,redMax,255,0);
  // Delay to stabilize sensor
  delay(200);
  
  // Read Green value
  int greenPW = getGreenPW();
  // Map to value from 0-255
  int grn = map(greenPW, greenMin,greenMax,255,0);
  // Delay to stabilize sensor
  delay(200);
  
  // Read Blue value
  int bluePW = getBluePW();
  // Map to value from 0-255
  int blu = map(bluePW, blueMin,blueMax,255,0);

  // Print output to Serial Monitor
//  Serial.print("Red = ");
//  Serial.print(red);
//  Serial.print(" - Green = ");
//  Serial.print(grn);
//  Serial.print(" - Blue = ");
//  Serial.print(blu);

  int mult = 1.7;
  int blackCutoff = 100;

  String color;
  if (red<blackCutoff && grn<blackCutoff && blu<blackCutoff)  color = "BLACK";
  else if (red>blackCutoff && red>mult*grn && red>mult*blu)   color = "RED";
  else if (grn>blackCutoff && grn>mult*red && grn>mult*blu)   color = "GREEN";
  else if (red>blackCutoff && grn>blackCutoff && red>0.7*blu*mult && grn>mult*blu)   color = "YELLOW";
  else if (blu>blackCutoff && blu>mult*red && blu>mult*grn)   color = "BLUE";
  else  color = "NO_COLOR";
  return color;
}

//ultrasonic tools
bool userPresent() {
  int sampleSize = 100;
  int sum = 0;
  for (int i=0; i<sampleSize; i++) {
    sum=sum+getDistance();
  }
  int distance = sum/sampleSize;
  return distance < 70;

}

int getDistance() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);

  int echoDuration = pulseIn(ECHOPIN, HIGH);
  int distance = echoDuration*0.034/2;
  return distance;
}


//LCD tools
void display(String msg, int row, int col) {
  if (!backlightOn) {
    turnOnBacklight();
  }
  if (row==0) {
    if (lcdRow0.equals(msg)) {
      return;
    }
    lcdRow0 = msg;
  } else if (row==1) {
    if (lcdRow1.equals(msg)) {
      return;
    }
    lcdRow1 = msg;
  }
  lcd.setCursor(col, row);
  lcd.print(msg);
}

void turnOnBacklight(){
  if (!backlightOn){
    lcd.backlight();
    backlightOn = true;
  }
}

void turnOffBacklight(){
  if (backlightOn){
    lcd.noBacklight();
    backlightOn = false;
  }
}

void clearLCD() {
  lcdRow0 = "";
  lcdRow1 = "";
  lcd.clear();
}

//servo tools
void lockSafe() {
  if (!locked) {
    myServo.write(180);
    locked = true;
    indicatorOff();
    clearLCD();
    display("Locked", 0, 0);
    displayFree = false;
    lastTimeDisp = millis();
  }
}

void unlockSafe() {
  if (locked) {
    myServo.write(90);
    locked = false;
    setLastAccessToNow();
    indicatorOn();
    clearLCD();
    display("Unlocked", 0, 0);
    displayFree = false;
    lastTimeDisp = millis();
  }
}


//SETUP
void setupLCD() {
  lcd.init();
}
void setupUltrasonicSensor() {
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
}
void setupColorSensor() {
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  
  // Setting frequency-scaling to 20%
  digitalWrite(S0,HIGH);
  digitalWrite(S1,LOW);
  
  pinMode(LIGHT, OUTPUT);
}
void setupServo() {
  myServo.attach(SERVOPIN);
}


// last access display
void displayLastAccess() {
  String lastAccessStr;
  if (hasBeenAccessed) {
    lastAccessStr = getDurationSince(lastAccess);
  } else {
    lastAccessStr = "none";
  }
  if (lcdRow0.equals("Last Access:") && lcdRow1.equals(lastAccessStr)) {
    return;
  }
  if (!displayFree) return;
  clearLCD();
  display("Last Access:", 0, 0);
  display(lastAccessStr, 1, 0);
  displayFree = false;
  lastTimeDisp = millis();
}

void setLastAccessToNow() {
  lastAccess = millis();
  hasBeenAccessed = true;
}

String getDurationSince(int prevTime) {
  int seconds = (millis()-prevTime)/1000;
  int minutes = seconds/60;
  int hours = minutes/60;
  int days = hours/24;
  int weeks = days/7;
  int months = weeks/4;
  int year = months/12;
  String str = "";
  char tmpStr[60] = "";
  
  if (months!=0 || year!=0 || days!=0){
    sprintf(tmpStr, "%i day", days%24);
    str+=tmpStr;
    if (days%24!=1) {
      str+="s";
    }
    str+=" ";
  }

  if (months!=0 || year!=0 || days!=0 || hours!=0){
    sprintf(tmpStr, "%i hr", hours%24);
    str+=tmpStr;
    if (hours%24!=1) {
      str+="s";
    }
    str+=" ";
  }
  sprintf(tmpStr, "%i min", minutes%24);
  str+=tmpStr;
  if (minutes%24!=1) {
    str+="s";
  }
  str+=" ";
//  if (months!=0 || weeks!=0 || days!=0 || hours!=0 || minutes!=0 || seconds!=0){
//    sprintf(tmpStr, "%i", seconds%60);
//  }
//  str+=tmpStr;
  str += "ago";
  return str;
}

//LED indicator/button
int buttonClicked() {
  int stuff = digitalRead(BUTTON);
  if (stuff==HIGH) return 0;
  int lastTime = millis();
  while(millis()-lastTime<buttonPressDuration) {
    if (digitalRead(BUTTON)==HIGH) return 1;
  }
  return 0;
}
void indicatorOn() {
  digitalWrite(LEDINDICATOR, HIGH);
}
void indicatorOff() {
  digitalWrite(LEDINDICATOR, LOW);
}


 
// Function to read Red Pulse Widths
int getRedPW() {
 
  // Set sensor to read Red only
  digitalWrite(S2,LOW);
  digitalWrite(S3,LOW);
  // Define integer to represent Pulse Width
  int PW;
  // Read the output Pulse Width
  PW = pulseIn(sensorOut, LOW);
  // Return the value
  return PW;
 
}
 
// Function to read Green Pulse Widths
int getGreenPW() {
 
  // Set sensor to read Green only
  digitalWrite(S2,HIGH);
  digitalWrite(S3,HIGH);
  // Define integer to represent Pulse Width
  int PW;
  // Read the output Pulse Width
  PW = pulseIn(sensorOut, LOW);
  // Return the value
  return PW;
 
}
 
// Function to read Blue Pulse Widths
int getBluePW() {
 
  // Set sensor to read Blue only
  digitalWrite(S2,LOW);
  digitalWrite(S3,HIGH);
  // Define integer to represent Pulse Width
  int PW;
  // Read the output Pulse Width
  PW = pulseIn(sensorOut, LOW);
  // Return the value
  return PW;
 
}
