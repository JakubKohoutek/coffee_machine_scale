#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <Wire.h>

#ifndef OTA_H
#define OTA_H

class OTA {
    public:
        static void initialize (const char* deviceName);
        static void handle     ();
};

#endif /* OTA_H */
