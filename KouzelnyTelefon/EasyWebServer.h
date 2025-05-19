#ifndef __WEB_SERVER___H_
#define __WEB_SERVER___H_

/*

Zjednodušení kombinace WiFi AP + DNS server + webserver pro zařízení, která vystavují konfigurační interface přes web.

Pouzivaji se tyto dve knihovny:
- https://github.com/ESP32Async/ESPAsyncWebServer
- https://github.com/ESP32Async/AsyncTCP 
*/

#include <Arduino.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "src/logging/AsyncLogger.h"

// takovahle funkce musi byt v INO kodu aplikace
void userRoutes( AsyncWebServer * server );


enum EwsMode {
    EWS_NONE,
    EWS_CLIENT,
    EWS_AP
};


class EasyWebServer
{
    public:
        EasyWebServer( AsyncLogger * logger );

        void startApAndWebserver();
        void startWebserverClientMode();

        /** 
         * Odbavuje DNS pozadavky, musi byt volano z loop()
         */
        void processDNS();

        const char * getQueryParamAsString( AsyncWebServerRequest *request, const char * paramName, const char * defaultValue );
        long getQueryParamAsLong( AsyncWebServerRequest *request, const char * paramName, long defaultValue );

        const char* HTML_UTF8 = "text/html; charset=utf-8";

    private:
        AsyncLogger * logger;
        AsyncWebServer * server;
        DNSServer dnsServer;

        EwsMode mode;

        /** start webserveru */
        void begin();
};


#endif
