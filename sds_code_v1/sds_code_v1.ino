// include the library code:
#include <LiquidCrystal.h>
#include <Servo.h>

// declare functions:
String getTimeSinceReset();
String formatTimeHHHMM(long timeMs);
String padNumber(int digit, int numDigits);

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
const int solenoidPin = 4;
const int tiltPin = 5;
const int panPin = 6;
const int fsrPin = A0;

// FSR initialization
float fsrEMA = 600.0; // start value for the moving average
const float fsrAlpha = 0.01; // How much weighting is applied to latest value
const int fsrThreshold = 20; // How much above the current fsrEMA to trigger


// For the Servo control:
Servo panServo;
Servo tiltServo;
const int minTilt = 105;
const int maxTilt = 125;
const int minPan = 105;
const int maxPan = 125;
int curTilt = minTilt;
int curPan = minPan;
bool increaseTilt = true;
bool increasePan = true;


// These are for error state checking:
const int maxOnTime_secs = 60;
const int maxTimesInPeriod = 10;
const int maxTimePeriodToCheck_secs = 300; // 5 minutes
const int timeToResetErrState_secs = 3600; // 1 hour

const int clockCycle_ms = 100; // time to delay in the main loop

// globals:
bool isOn = false;
int runTime = 0;
long lastOn = 0;
long earliestOn = 0; // used for 5 minute check
int countIn5Minutes = 0;
int totalCount = 0;
bool isInErrState = false;
long errStatusStartTime_ms = 0;

void setup() {
  initializeLcd();
  initializeServos();
  turnWaterOff();
  Serial.begin(9600);
}

/**
* Main Loop for the program
**/
void loop() {

  // should I make this a global?
  int val = analogRead(fsrPin);
  printFSRVal(val);

  checkFsr(val);

  if(isOn) {
    updateRunTime();
    runServoPattern();
  }

  checkErrStatus();
  checkErrResetStatus(val);

  updateLcd();

  delay(clockCycle_ms);
}


/****** FSR Functions ******/
void checkFsr(int val) {
  calculateEMA(val);
  int threshold = fsrThreshold + static_cast<int>(fsrEMA);
  
  if(val >= threshold && !isOn && !isInErrState) {
    turnWaterOn();
  } else if(val < threshold && isOn) {
    turnWaterOff();
  }
}

void calculateEMA(int val) {
   float fVal = static_cast<float>(val);
   fsrEMA = (fsrAlpha * fVal) + ((1.0-fsrAlpha) * fsrEMA);
   Serial.print(fVal);
   Serial.print("  ");
   Serial.println(fsrEMA);
}

/****** CONTROL FUNCTIONS *******/

void turnWaterOn() {
  isOn = true;
  runTime = 0;
  lastOn = millis(); // record the time this started
  // Not sure about this: 5 minutes should be a running thing maybe?
  if(earliestOn == 0 || earliestOn < ((lastOn / 1000) - maxTimePeriodToCheck_secs)) {
    // reset these for now; may rethink that later:
    earliestOn = lastOn / 1000;
    countIn5Minutes = 0;
  }
  totalCount++;
  countIn5Minutes++; 
  digitalWrite(solenoidPin, HIGH); // Actually turn the water on
}


void turnWaterOff() {
  isOn = false;
  digitalWrite(solenoidPin, LOW);
}

/***** ERROR STATUS HANDLING *****/

void checkErrStatus() {
  if(isOn && runTime > maxOnTime_secs) {
    setErrorStatus();
  }
  if(isOn && countIn5Minutes > maxTimesInPeriod) {
    setErrorStatus();
  }
}

// TODO: This logic may need some help
void checkErrResetStatus(int fsrVal) {
  if(isInErrState) {
    long timeSinceErr_secs = (millis() - errStatusStartTime_ms) / 1000;
    if(fsrVal < fsrThreshold && timeSinceErr_secs > timeToResetErrState_secs) {
      resetErrorStatus();
    }
  }
}

void setErrorStatus() {
  turnWaterOff();
  isInErrState = true;
  errStatusStartTime_ms = millis();
}

void resetErrorStatus() {
  isInErrState = false;
}

/****** SERVO FUNCTIONS ****/
void initializeServos() {
  panServo.attach(panPin);
  tiltServo.attach(tiltPin);
  curTilt = minTilt;
  curPan = minPan;
  increaseTilt = true;
  increasePan = true;
}

void runServoPattern() {
    // determine positional change
  bool doTilt = false;
  if(!increasePan && curPan == minPan) {
    increasePan = true;
    doTilt = true;
  }
  if(increasePan && curPan == maxPan) {
    increasePan = false;
    doTilt = true;
  }

  if(doTilt) {
    if(!increaseTilt && curTilt == minTilt) {
      increaseTilt = true;
    }
    if(increaseTilt && curTilt == maxTilt) {
      increaseTilt = false;
    }
    if(increaseTilt) {
      curTilt += 5;
    } else {
      curTilt -= 5;
    }
 }
 
  if(increasePan) {
    curPan += 5;
  } else {
    curPan -= 5;
  }

  panServo.write(curPan);
  tiltServo.write(curTilt);

}


/***** LCD FUNCTIONS ****/

void initializeLcd() {
  lcd.begin(16,2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("SQUIRREL DEFENSE");
  lcd.setCursor(0,1);
  lcd.print("ALPHA v1.0");
  delay(1000);
  lcd.clear();
}

void updateRunTime() {
  runTime = (millis() - lastOn) / 1000;
}

// FSR has to update separately as it is not global:
void updateLcd() {
  printRunTime();
  printSinceLastSquirrel();
  printState();
  printSquirrelCount();
  printLastRunTime();
}


void printLastRunTime() {
  lcd.setCursor(14,0);
  lcd.print(padNumber(runTime, 2));
}

void printRunTime() {
  lcd.setCursor(0,0);
  //Serial.println(millis() / 1000);
  //Serial.println(getTimeSinceReset());
  lcd.print(getTimeSinceReset());
}


void printSinceLastSquirrel() {
  lcd.setCursor(7, 0);
  if(lastOn > 0) {
    long timeSinceLastOn = millis() - lastOn;
    lcd.print(formatTimeHHHMM(timeSinceLastOn));
  } else {
    lcd.print("000:00");
  }
}


void printFSRVal(int val) {
  //Serial.println(val);
  lcd.setCursor(0,1);
  lcd.print(padNumber(val, 3));
  lcd.setCursor(5,1);
  lcd.print(padNumber(static_cast<int>(fsrEMA),3));
}

void printSquirrelCount() {
  lcd.setCursor(13,1);
  lcd.print(padNumber(totalCount, 3));
}

void printState() {
  lcd.setCursor(9,1);
  if(isInErrState) {
    lcd.print("ERR");
  } else if(isOn) {
    lcd.print("ON ");
  } else {
    lcd.print("OFF");
  }
}

/******** STRING FUNCTIONS **********/

String padNumber(int digit, int numDigits) {
    String digitString = String(digit, DEC);
    while (digitString.length() < numDigits) {
        digitString = "0" + digitString;
    }
    return digitString;
}

String getTimeSinceReset() {
  return formatTimeHHHMM(millis());
}

String formatTimeHHHMM(long timeMs) {
    long timeSecs = timeMs / 1000;
    int timeHours = timeSecs / 3600;
    int remainingSecs = timeSecs % 3600;
    int remainingMins = remainingSecs / 60;
    //Serial.print(timeSecs);
    //Serial.print ("secs; ");
    //Serial.print(remainingSecs);
    //Serial.print("remainSecs; ");
    //Serial.print(remainingMins);
    //Serial.println("remainMins");
    char stringBuffer[7];
    snprintf(stringBuffer, sizeof(stringBuffer), "%03d:%02d", timeHours, remainingMins);
    return stringBuffer;
}
