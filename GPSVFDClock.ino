#include <DFRobot_VEML7700.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <CU40025_VFD.h>
#include <RTClib.h>
#include <Wire.h>

// ESP32 GPIO Pin Definitions:
#define ubxRX 16
#define ubxTX 17

#define ubxPPS 18

#define vfdRS 23
#define vfdEN 25
#define vfdD4 26
#define vfdD5 27
#define vfdD6 32
#define vfdD7 33

// Peripheral Handles:
RTC_DS3231 rtc;
DFRobot_VEML7700 als;
SFE_UBLOX_GNSS ubxGNSS;
CU40025_VFD vfd(vfdRS, vfdEN, vfdD4, vfdD5, vfdD6, vfdD7);

const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char monthsOfTheYear[12][12] = {"January", "Febuary", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

char displayBuffer[2][41];
uint8_t displayBrightness = 4;
SemaphoreHandle_t displayAccess;

uint8_t gnssFixType = 0;
SemaphoreHandle_t gnssDataLock;

volatile bool nextSecond = false;

DateTime currentTime;

// Initialization Code:

int8_t initializeVFD(void) {
  Serial.println("VFD: initializing");
  vfd.begin(40, 2);
  return 0;
}

int8_t initializeGNSS(void) {
  uint8_t initCount = 0;
  Serial.println("GNSS: initializing U-blox GNSS module");
  while (initCount < 5) {
    Serial2.begin(115200, SERIAL_8N1, ubxRX, ubxTX);
    if (ubxGNSS.begin(Serial2) == true) break;
    
    delay(100);
    Serial2.begin(9600, SERIAL_8N1, ubxRX, ubxTX);
     if (ubxGNSS.begin(Serial2) == true) {
      Serial.println("GNSS: connected at 9600 baud, switching to 115200");
      ubxGNSS.setSerialRate(115200);
      delay(100);
     } else {
      Serial.println("GNSS: Failed to initialize, retrying");
      initCount++;
      delay(200);
     }
  }
  if (initCount < 5) {
    Serial.println("GNSS: UBX initialized at 115.2k");
    ubxGNSS.setUART1Output(COM_TYPE_UBX);
    ubxGNSS.setI2COutput(COM_TYPE_UBX);
    ubxGNSS.saveConfiguration();
    return 0;
  } else {
    Serial.println("GNSS: UBX module initialization failed!");
    return -1;
  }
}

int8_t initializeRTC(void) {
  uint8_t initCount = 0;
  Serial.println("RTC: initializing");
  while (initCount < 5) {
    if (!rtc.begin()) {
      Serial.println("RTC: initialization failed, retrying");
      initCount++;
      delay(200);
    } else {
      break;
    }
  }
  if (initCount < 5) {
    Serial.println("RTC: initialized successfully");
  } else {
    return -1;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC: power loss detected, time set to default.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  return 0;
}

int8_t initializeALS(void) {
  Serial.println("ALS: initializing");
  als.begin();

  return 0;
}

// Interrupts and FreeRTOS Tasks:

void ppsInterrupt(void) {
  nextSecond = true;
}

void updateDisplay(void *thing) {

  char oldDisplayValue[2][41];

  while(true) {

    if (displayAccess != NULL) {

      if (xSemaphoreTake(displayAccess, 0) == pdTRUE) {

        if (strncmp(displayBuffer[0], oldDisplayValue[0], sizeof(oldDisplayValue[0])) != 0) {

          vfd.setCursor(0, 0);
          vfd.write(displayBuffer[0]);
          strncpy(oldDisplayValue[0], displayBuffer[0], sizeof(oldDisplayValue[0]));
          
        }

        if (strncmp(displayBuffer[1], oldDisplayValue[1], sizeof(oldDisplayValue[1])) != 0) {

          vfd.setCursor(0, 1);
          vfd.write(displayBuffer[1]);
          strncpy(oldDisplayValue[1], displayBuffer[1], sizeof(oldDisplayValue[1]));

        } 

        xSemaphoreGive(displayAccess);

      }

      vTaskDelay(30 / portTICK_PERIOD_MS);

    }
  }
}

void updateTime(void *thing) {
  uint8_t prevFixType = 0;
  
  while(true) {
    
    if (gnssFixType < 3) {
      
      if (prevFixType > 2) {
        detachInterrupt(ubxPPS);
        prevFixType = gnssFixType; 
      }

      currentTime = rtc.now();

      vTaskDelay(10 / portTICK_PERIOD_MS);

    } else {

      if (prevFixType < 3) {
        attachInterrupt(ubxPPS, ppsInterrupt, RISING);
        prevFixType = gnssFixType;
      }

      if (nextSecond) {
        currentTime = DateTime(ubxGNSS.getUnixEpoch() + 1);
        nextSecond = false;
      }
    }

  }
}

void generateDisplay(void *thing) {
  
  while(true) { 

    if (displayAccess != NULL) {

       if (xSemaphoreTake(displayAccess, 0) == pdTRUE) {

         uint8_t numBytes = snprintf(NULL, 0, "%u/%u/%u %02u:%02u:%02u", currentTime.month(), currentTime.day(), currentTime.year(), currentTime.hour(), currentTime.minute(), currentTime.second()) + 1;
         char *timeString = (char*)malloc(numBytes);
         snprintf(timeString, numBytes, "%u/%u/%u %02u:%02u:%02u", currentTime.month(), currentTime.day(), currentTime.year(), currentTime.hour(), currentTime.minute(), currentTime.second());
         strncpy(displayBuffer[0], timeString, sizeof(displayBuffer[0]));

         free(timeString);

         xSemaphoreGive(displayAccess);

       }

       vTaskDelay(100 / portTICK_PERIOD_MS);

    }

  }

}

void updateGnss(void *thing) {

}

void setup() {
  Serial.begin(115200);

  pinMode(ubxPPS, INPUT);
  
  Serial.println("SYS: VFD GPS Clock initializing...");
  
  if (initializeVFD() + initializeGNSS() + initializeRTC() + initializeALS()) {
    Serial.println("SYS: hardware initialization failed, hanging now");
    vfd.print("Hardware Initialization Failure!");
    while(true);
  } else {
    Serial.println("SYS: hardware initialization complete");
  }

  displayAccess = xSemaphoreCreateMutex();
  gnssDataLock = xSemaphoreCreateMutex();

  currentTime = rtc.now();

  xTaskCreate(updateDisplay, "Refresh Display Contents", 1000, NULL, 1, NULL);
  xTaskCreate(updateTime, "Monitor and Sync System Time", 4096, NULL, 1, NULL);
  xTaskCreate(generateDisplay, "Generate Strings for Display Output", 1000, NULL, 1, NULL);

}

void loop() {
}
