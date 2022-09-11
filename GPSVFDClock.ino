#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <LiquidCrystal.h>
#include <RTClib.h>

#define ubxRX 16
#define ubxTX 17

#define vfdRS 23
#define vfdEN 25
#define vfdD4 26
#define vfdD5 27
#define vfdD6 32
#define vfdD7 33

RTC_DS3231 rtc;
SFE_UBLOX_GNSS ubxGNSS;
LiquidCrystal vfd(vfdRS, vfdEN, vfdD4, vfdD5, vfdD6, vfdD7);

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int initializeVFD() {
  Serial.println("VFD: initializing");
  vfd.begin(40, 2);
  vfd.setCursor(11, 0);
  vfd.print("VFD Initialized!");
  delay(1000);
  vfd.clear();
  vfd.setCursor(0, 0);
  return 0;
}

int initalizeGNSS() {
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

int initalizeRTC() {
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
    Serial.println("RTC: initalized successfully!");
  } else {
    return -1;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC: power loss detected, time set to default");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  return 0;
}


void setup() {
  Serial.begin(115200);
  Serial.println("GPS VFD Clock Mk1");
  Serial.printf("VFD: init status %d\n", initializeVFD());
  Serial.printf("GNSS: init status %d\n", initalizeGNSS());
  Serial.printf("RTC: init status %d\n", initalizeRTC());

}

void loop() {

}
