#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <TimeLib.h>

#define ubxRX 16
#define ubxTX 17

#define ubxPPS 18
#define rtcPPS 19

#define vfdRS 23
#define vfdEN 25
#define vfdD4 26
#define vfdD5 27
#define vfdD6 32
#define vfdD7 33

RTC_DS3231 rtc;
SFE_UBLOX_GNSS ubxGNSS;
LiquidCrystal vfd(vfdRS, vfdEN, vfdD4, vfdD5, vfdD6, vfdD7);

volatile time_t currentTime;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char monthsOfTheYear[12][12] = {"January", "Febuary", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

bool usingGpsPPS = false;

int initializeVFD() {
  Serial.println("VFD: initializing");
  vfd.begin(40, 2);
  return 0;
}

int initializeGNSS() {
  int initCount = 0;
  Serial.println("GNSS: initializing U-Blox GNSS module");
  while (initCount < 5) {
    Serial.println("GNSS: trying 115200 baud");
    Serial2.begin(115200, SERIAL_8N1, ubxRX, ubxTX);
    if (ubxGNSS.begin(Serial2) == true) break;
    
    delay(100);
    Serial.println("GNSS: trying 9600 baud");
    Serial2.begin(9600, SERIAL_8N1, ubxRX, ubxTX);
     if (ubxGNSS.begin(Serial2) == true) {
      Serial.println("GNSS: connected at 9600 baud, switching to 115200");
      ubxGNSS.setSerialRate(115200);
      delay(100);
     } else {
      Serial.println("GNSS: Failed to initialize, retrying");
      initCount++;
      delay(1000);
     }
  }
  if (initCount < 5) {
    Serial.println("GNSS: connected to UBX module at 115200 baud");
    ubxGNSS.setUART1Output(COM_TYPE_UBX);
    ubxGNSS.saveConfiguration();
    return 0;
  } else {
    Serial.println("GNSS: UBX module initialization failed!");
    return -1;
  }
}

int initializeRTC() {
  int initCount = 0;
  Serial.println("RTC: initializing DS3231");
  while (initCount < 5) {
    if (!rtc.begin()) {
      Serial.println("RTC: initialization failed, retrying");
      initCount++;
      delay(1000);
    } else {
      break;
    }
  }
  if (initCount < 5) {
    Serial.println("RTC: initialized successfully!");
  } else {
    return -1;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC: power loss detected, time set to default");
    vfd.print("RTC: power loss detected, please set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);

  return 0;
}

void IRAM_ATTR gpsTimepulse() {
  currentTime++;
}

void IRAM_ATTR rtcTimepulse() {
  currentTime++;
}

int initializeInterrupts(bool useGps) {
  pinMode(ubxPPS, INPUT);
  pinMode(rtcPPS, INPUT_PULLUP);

  if (useGps == true) {
    detachInterrupt(rtcPPS);
    attachInterrupt(ubxPPS, gpsTimepulse, RISING);
    Serial.println("SYS: using GPS timepulses");
  } else {
    detachInterrupt(ubxPPS);
    attachInterrupt(rtcPPS, rtcTimepulse, FALLING);
    Serial.println("SYS: using RTC timepulses");
  }

  return 0;
}

const char *formattedTimeString() {
  char *amPm;
  if (isPM(currentTime)) {
    amPm = "PM";
  } else {
    amPm = "AM";
  }

  int numBytes = snprintf(NULL, 0, "%s %s %2d, %4d %02d:%02d:%02d %s", daysOfTheWeek[weekday(currentTime)], monthsOfTheYear[month(currentTime) - 1], day(currentTime), year(currentTime), hourFormat12(currentTime), minute(currentTime), second(currentTime), amPm) + 1;
  char *timeString = (char*)malloc(numBytes);
  snprintf(timeString, numBytes, "%s %s %2d, %4d %02d:%02d:%02d %s", daysOfTheWeek[weekday(currentTime)], monthsOfTheYear[month(currentTime) - 1], day(currentTime), year(currentTime), hourFormat12(currentTime), minute(currentTime), second(currentTime), amPm);

  return timeString;
}

void setup() {
  Serial.begin(115200);
  Serial.println("SYS: VFD GPS Clock initializing...");
  Serial.printf("VFD: init status %d\n", initializeVFD());
  Serial.printf("GNSS: init status %d\n", initializeGNSS());
  Serial.printf("RTC: init status %d\n", initializeRTC());

  Serial.println("SYS: Setting current time from RTC");
  currentTime = rtc.now().unixtime();
  initializeInterrupts(false);
  Serial.printf("SYS: time is now %s, RTC interrupts enabled\n", formattedTimeString());
}

void loop() {
  char *timeSource;

  vfd.setCursor(0, 0);
  vfd.print(formattedTimeString());
  vfd.setCursor(0, 1);

  if((ubxGNSS.getFixType() == 2 || ubxGNSS.getFixType() == 3) && !usingGpsPPS && ubxGNSS.getTimeValid()) {
    timeSource = "GPS";
    currentTime = ubxGNSS.getUnixEpoch();
    rtc.adjust(DateTime(currentTime));
    initializeInterrupts(true);
  } else {
    timeSource = "RTC";
    initializeInterrupts(false);
  }

  vfd.printf("Fix Type: %d SIV: %d Time Source: %s", ubxGNSS.getFixType(), ubxGNSS.getSIV(), timeSource);

  delay(100);
}
