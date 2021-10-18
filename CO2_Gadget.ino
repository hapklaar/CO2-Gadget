// Library Button2 by Lennart Hennigs https://github.com/LennartHennigs/Button2

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                   GENERAL GLOBAL DEFINITIONS AND OPTIONS                          *********/
/*********                                                                                   *********/
/*****************************************************************************************************/

#define SUPPORT_BLE
// #define SUPPORT_WIFI           // HTTP SERVER NOT WORKING CURRENTLY. AWAITING FIX
// #define SUPPORT_MQTT           // Needs SUPPORT_WIFI
// #define SUPPORT_OTA            // Needs SUPPORT_WIFI - CURRENTLY NOT WORKING AWAITING FIX
// #define SUPPORT_OLED
#define SUPPORT_TFT
#define SUPPORT_ARDUINOMENU
#define ALTERNATIVE_I2C_PINS   // For the compact build as shown at https://emariete.com/medidor-co2-display-tft-color-ttgo-t-display-sensirion-scd30/

#define UNITHOSTNAME "TEST"

#include <Wire.h>
#include "soc/soc.h"           // disable brownout problems
#include "soc/rtc_cntl_reg.h"  // disable brownout problems

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                              SETUP CO2 SENSOR SCD30                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#include "SparkFun_SCD30_Arduino_Library.h"
SCD30 airSensor;

#ifdef ALTERNATIVE_I2C_PINS
#define I2C_SDA 22
#define I2C_SCL 21
#else
#define I2C_SDA 21
#define I2C_SCL 22
#endif

bool pendingCalibration = false;
uint16_t calibrationValue = 415;
uint16_t customCalibrationValue = 415;
bool pendingAmbientPressure = false;
uint16_t ambientPressureValue = 0;
uint16_t altidudeMeters = 600;
bool autoSelfCalibration = false;

uint16_t co2, co2Previous = 0;
float temp, hum, tempPrevious, humPrevious  = 0;

uint16_t  co2OrangeRange = 700;
uint16_t  co2RedRange = 1000;

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                              SETUP BLE FUNCTIONALITY                              *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#ifdef SUPPORT_BLE
#include "Sensirion_GadgetBle_Lib.h"
GadgetBle gadgetBle = GadgetBle(GadgetBle::DataType::T_RH_CO2);
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                           INCLUDE WIFI FUNCTIONALITY                              *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#ifdef SUPPORT_WIFI
#include <CO2_Gadget_WIFI>
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                            SETUP MQTT FUNCTIONALITY                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_MQTT
#include "CO2_Gadget_MQTT.h"
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                             SETUP OTA FUNCTIONALITY                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_OTA
#include <AsyncElegantOTA.h>
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                           INCLUDE BATTERY FUNCTIONALITY                           *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#define ADC_PIN 34
int vref = 1100;
#include "CO2_Gadget_Battery.h"

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                        INCLUDE OLED DISPLAY FUNCTIONALITY                         *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_OLED
#include <CO2_Gadget_OLED.h>
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                      INCLUDE TFT DISPLAY FUNCTIONALITY                            *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_TFT
#include "CO2_Gadget_TFT.h"
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                         INCLUDE MENU FUNCIONALITY                                 *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#if defined SUPPORT_ARDUINOMENU
#define DEBUG_ARDUINOMENU
#include <CO2_Gadget_Menu.h>
#endif

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                      SETUP PUSH BUTTONS FUNCTIONALITY                             *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
#include "Button2.h"
// Button2 button;
// #define BUTTON_PIN  35 // Menu button (Press > 1500ms for calibration, press > 500ms to show battery voltage)
// void longpress(Button2& btn);
#define LONGCLICK_MS 300 // https://github.com/LennartHennigs/Button2/issues/10
#define BTN_UP 35 // Pinnumber for button for up/previous and select / enter actions
#define BTN_DWN 0 // Pinnumber for button for down/next and back / exit actions
Button2 btnUp(BTN_UP); // Initialize the up button
Button2 btnDwn(BTN_DWN); // Initialize the down button

void button_init()
{
#if defined SUPPORT_ARDUINOMENU
    btnUp.setLongClickHandler([](Button2 & b) {
        // Old way without #define LONGCLICK_MS 300
        // unsigned int time = b.wasPressedFor();
        // if (time >= 300) {
        //   nav.doNav(enterCmd);
        // }
        nav.doNav(enterCmd);
    });
    
    btnUp.setClickHandler([](Button2 & b) {
       // Up
       nav.doNav(downCmd); // It's called downCmd because it decreases the index of an array. Visually that would mean the selector goes upwards.
    });

    btnDwn.setLongClickHandler([](Button2 & b) {
        // // Exit
        // unsigned int time = b.wasPressedFor();
        // if (time >= 300) {
        //   nav.doNav(escCmd);
        // }
        nav.doNav(escCmd);
    });
    
    btnDwn.setClickHandler([](Button2 & b) {
        // Down
        nav.doNav(upCmd); // It's called upCmd because it increases the index of an array. Visually that would mean the selector goes downwards.
    });
#endif
}

void button_loop()
{
    // Check for button presses
    btnUp.loop();
    btnDwn.loop();
}

/*****************************************************************************************************/

static int64_t lastMmntTime = 0;
static int startCheckingAfterUs = 1900000;

void initMQTT()
{
#ifdef SUPPORT_MQTT
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  rootTopic = UNITHOSTNAME;
#endif
}

void processPendingCommands()
{
  if (pendingCalibration == true) {
    if (calibrationValue != 0) {
      printf("Calibrating SCD30 sensor at %d PPM\n", calibrationValue);
      pendingCalibration = false;
      airSensor.setForcedRecalibrationFactor(calibrationValue);
    }
    else
    {
      printf("Avoiding calibrating SCD30 sensor with invalid value at %d PPM\n", calibrationValue);
      pendingCalibration = false;
    }
  }

  if (pendingAmbientPressure == true) {
    if (ambientPressureValue != 0) {
      printf("Setting AmbientPressure for SCD30 sensor at %d mbar\n", ambientPressureValue);
      pendingAmbientPressure = false;
      airSensor.setAmbientPressure(ambientPressureValue);
    }
    else
    {
      printf("Avoiding setting AmbientPressure for SCD30 sensor with invalid value at %d mbar\n", ambientPressureValue);
      pendingAmbientPressure = false;
    }
  }
}

void setup()
{
  uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG); //save WatchDog register
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");

#if defined SUPPORT_OLED
  delay(100);
  initDisplayOLED();
  delay(1000);
#endif

#if defined SUPPORT_TFT
  initDisplayTFT();  
  displaySplashScreenTFT(); // Display init and splash screen  
  delay(2000); // Enjoy the splash screen for 2 seconds
  // tft.fillScreen(Black);
  tft.setTextSize(2);
#endif

#if defined SUPPORT_BLE && defined SUPPORT_WIFI
  // Initialize the GadgetBle Library
  gadgetBle.enableWifiSetupSettings(onWifiSettingsChanged);
  gadgetBle.setCurrentWifiSsid(WIFI_SSID_CREDENTIALS);
#endif

#ifdef SUPPORT_WIFI
  WiFiMulti.addAP(WIFI_SSID_CREDENTIALS, WIFI_PW_CREDENTIALS);
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(UNITHOSTNAME)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.print("mDNS responder started. CO2 monitor web interface at: http://");
  Serial.print(UNITHOSTNAME);
  Serial.println(".local");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "CO2: " + String(co2) + " PPM");
    //  server.on("/", handleRoot);      //This is display page
    //  server.on("/readADC", handleADC);//To get update of ADC Value only
  });

#ifdef SUPPORT_OTA
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  Serial.println("OTA ready");
#endif

  server.begin();
  Serial.println("HTTP server started");
#endif

#ifdef SUPPORT_BLE
  // Initialize the GadgetBle Library
  gadgetBle.begin();
  Serial.print("Sensirion GadgetBle Lib initialized with deviceId = ");
  Serial.println(gadgetBle.getDeviceIdString());
#endif

  // Initialize the SCD30 driver
  Wire.begin(I2C_SDA, I2C_SCL);
  if (airSensor.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    //    while (1)
    //      ;
  }
  else
  {
    airSensor.setAutoSelfCalibration(autoSelfCalibration);
  }

  initMQTT();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brown_reg_temp); //enable brownout detector

  readBatteryVoltage();
  button_init();
  menu_init();

  Serial.println("Ready.");
}

void loop()
{
#ifdef SUPPORT_MQTT
  mqttClient.loop();
#endif

  processPendingCommands();

  if (esp_timer_get_time() - lastMmntTime >= startCheckingAfterUs) {
    if (airSensor.dataAvailable()) {
      co2Previous = co2;
      tempPrevious = temp;
      humPrevious = hum;
      co2  = airSensor.getCO2();
      temp = airSensor.getTemperature();
      hum  = airSensor.getHumidity();
      if (co2Previous != co2) {
        nav.idleChanged=true;
      }
      #if defined SUPPORT_OLED
      showValuesOLED(String(co2));
      #endif
      #if defined SUPPORT_TFT
      #ifndef SUPPORT_ARDUINOMENU
      showValuesTFT(co2);
      #endif
      #endif

      #ifdef SUPPORT_BLE
      gadgetBle.writeCO2(co2);
      gadgetBle.writeTemperature(temp);
      gadgetBle.writeHumidity(hum);
      gadgetBle.commit();
      #endif
      lastMmntTime = esp_timer_get_time();

      // Provide the sensor values for Tools -> Serial Monitor or Serial Plotter
      Serial.print("CO2[ppm]:");
      Serial.print(co2);
      Serial.print("\t");
      Serial.print("Temperature[℃]:");
      Serial.print(temp);
      Serial.print("\t");
      Serial.print("Humidity[%]:");
      Serial.println(hum);

//      Serial.print("Free heap: ");
//      Serial.println(ESP.getFreeHeap());



      #ifdef SUPPORT_WIFI
      if (WiFiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
      } else {
        Serial.print("WiFi connected - IP = ");
        Serial.println(WiFi.localIP());
        if (!mqttClient.connected())
        {
          mqttReconnect();
        }
      }
      #endif

      #if defined SUPPORT_MQTT && defined SUPPORT_WIFI
      if (WiFiMulti.run() == WL_CONNECTED) {
        publishIntMQTT("/co2", co2);
        publishFloatMQTT("/temp", temp);
        publishFloatMQTT("/humi", hum);
      }
      #endif
    }
  }

#ifdef SUPPORT_BLE
  gadgetBle.handleEvents();
  delay(3);
#endif

#ifdef SUPPORT_OTA
  AsyncElegantOTA.loop();
#endif

  button_loop();
#ifdef SUPPORT_ARDUINOMENU  
  nav.poll();//this device only draws when needed
#endif  
}