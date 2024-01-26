#ifndef CO2_Gadget_Improv_h
#define CO2_Gadget_Improv_h

// clang-format off
/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                          SETUP IMPROV FUNCTIONALITY                               *********/
/*********                                                                                   *********/
/*****************************************************************************************************/
/*****************************************************************************************************/
// clang-format on

#include "ImprovWiFiLibrary.h"
ImprovWiFi improvSerial(&Serial);

char linebuf[80];
int charcount = 0;

void blink_led(int d, int times) {
    //   for (int j = 0; j < times; j++)
    //   {
    //     digitalWrite(LED_BUILTIN, HIGH);
    //     delay(d);
    //     digitalWrite(LED_BUILTIN, LOW);
    //     delay(d);
    //   }
}

void onImprovWiFiErrorCb(ImprovTypes::Error err) {
    server.end();
    Serial.println("-->[IMPR] Error: " + String(err));
    blink_led(2000, 3);
}

void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
    // Save ssid and password here
    //   server.begin();
    Serial.println("-->[IMPR] Connected to: " + String(ssid));
    blink_led(100, 3);
}

// bool connectWifi(const char *ssid, const char *password) {
//     WiFi.begin(ssid, password);

//     while (!improvSerial.isConnected()) {
//         blink_led(500, 1);
//     }

//     return true;
// }

void initImprov() {
    char version[100];  // Ajusta el tamaño según tus necesidades
    sprintf(version, "CO2 Gadget Version: %s%s Flavour: %s\n", CO2_GADGET_VERSION, CO2_GADGET_REV, FLAVOUR);

    //   pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("-->[IMPR] Init Improv");
    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "CO2-Gadget-Beta-Desarrollo", version, "CO2-Gadget", "http://{LOCAL_IPV4}/preferences.html");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
    //   improvSerial.setCustomConnectWiFi(connectWifi);  // Optional

    blink_led(100, 5);
}

void improvLoopNew() {
    if (!inMenu) {
        if (Serial.peek() != -1 && Serial.peek() != 0x2A) {
            improvSerial.handleSerial();
        }
    }
}

void improvLoop() {
    if (!inMenu) improvSerial.handleSerial();
}

#endif  // CO2_Gadget_Improv_h