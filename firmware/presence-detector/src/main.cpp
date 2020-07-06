#include <Arduino.h>
#include "sys/time.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <esp_sleep.h>
#include <string>

#define GPIO_DEEP_SLEEP_DURATION     2  // sleep x seconds and then wake up
#define LED_BUILTIN 2

RTC_DATA_ATTR static time_t last;        // remember last boot in RTC Memory
RTC_DATA_ATTR static uint32_t bootcount; // remember number of boots in RTC Memory

#ifdef __cplusplus
extern "C" {
#endif

uint8_t temprature_sens_read();

#ifdef __cplusplus
}
#endif

BLEAdvertising *pAdvertising;
BLEScan* pBLEScan;
struct timeval now;

#define BEACON_UUID     "bdd07587-83e9-4733-a118-8148419a4813" // This UUID was generated with https://www.uuidgenerator.net/
const char* dev_uuid =  "bdd0758783e94733a1188148419a4813";

const int scanTime = 5;
const int rssid_threshold = -70; // RSSI APROX - SUSANA DISTANCIA

using namespace std;

class ScanCallback: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveManufacturerData()) {
        String address = advertisedDevice.getAddress().toString().c_str();
        const char* manufacturer_data_pointer = BLEUtils::buildHexData(nullptr, (uint8_t*)advertisedDevice.getManufacturerData().data(), advertisedDevice.getManufacturerData().length());
        String manufacturer_data = String(manufacturer_data_pointer);
        String ad_uuid = manufacturer_data.substring(8, strlen(dev_uuid) + 8);
        if (ad_uuid.equals(dev_uuid)) {
          int ad_rssid = advertisedDevice.getRSSI();
          Serial.println("FAKE BEACON FOUND");
          Serial.printf("Address: %s", address.c_str());
          Serial.printf("\tUUID: 0x%s", ad_uuid.c_str());
          Serial.printf("\tRSSI: %d\n", ad_rssid);
          if (ad_rssid > rssid_threshold) {
            Serial.println("Near device");
            digitalWrite(LED_BUILTIN, HIGH);
          }
        }
      }
    }
};

void printDeviceAddress() {
  uint64_t chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
	Serial.printf("ESP32 Chip ID = %04X",(uint16_t)(chipid>>32));//print High 2 bytes
	Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.
	Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", (uint8_t)(chipid), (uint8_t)(chipid >> 8), (uint8_t)(chipid >> 16), (uint8_t)(chipid >> 24), (uint8_t)(chipid >> 32), (uint8_t)(chipid >> 40));

}

void setBeacon() {

  BLEBeacon oBeacon = BLEBeacon();
  oBeacon.setManufacturerId(0x4C00); // fake Apple 0x004C LSB (ENDIAN_CHANGE_U16!)
  BLEUUID bleUUID = BLEUUID(BEACON_UUID);
  bleUUID = bleUUID.to128();
  oBeacon.setProximityUUID(BLEUUID(bleUUID.getNative()->uuid.uuid128, 16, true));
  oBeacon.setMajor((bootcount & 0xFFFF0000) >> 16);
  oBeacon.setMinor(bootcount&0xFFFF);
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
  
  oAdvertisementData.setFlags(0x04); // BR_EDR_NOT_SUPPORTED 0x04
  
  std::string strServiceData = "";
  
  strServiceData += (char)26;     // Len
  strServiceData += (char)0xFF;   // Type
  strServiceData += oBeacon.getData(); 
  oAdvertisementData.addData(strServiceData);
  
  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setScanResponseData(oScanResponseData);

}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  gettimeofday(&now, NULL);

  Serial.printf("start ESP32 %d\n",bootcount++);
  Serial.printf("deep sleep (%lds since last reset, %lds since last boot)\n",now.tv_sec,now.tv_sec-last);

  Serial.println("*********************************");
  printDeviceAddress();
  Serial.println("*********************************");

  last = now.tv_sec;

  // Create the BLE Device
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new ScanCallback());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  
  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();
  setBeacon();
   // Start advertising
  pAdvertising->start();
  Serial.println("Advertizing started...");
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Scanning...");
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("Devices found: ");
  Serial.println(foundDevices.getCount());
  Serial.println("Scan done!");
  pBLEScan->clearResults();
}