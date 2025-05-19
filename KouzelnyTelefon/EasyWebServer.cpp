#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>

#include "EasyWebServer.h"
#include "EasyWebServer_config.h"

#define CAPTIVE_PORTAL_REDIRECT_URL "http://" MY_NAME CAPTIVE_PORTAL_REDIRECT_REL

EasyWebServer::EasyWebServer( AsyncLogger * logger ) 
{
    this->logger = logger;
    this->server = new AsyncWebServer(WEB_PORT);
    this->mode = EWS_NONE;
}

void EasyWebServer::startApAndWebserver()
{
    this->mode = EWS_AP;
    
    Serial.printf( "Startuji wifi AP '%s'\n", SSID );
    WiFi.softAPConfig(local_ip, gateway, subnet);
    // musi tady byt; kdyz se AP spusti _hned_, obcas to padne
    delay(100);
    WiFi.softAP(SSID, PASSWORD, 1, false );

    if( FIX_POWER_FOR_ESP32C3MICRO ) {
        Serial.println( "++ Snizeni vysilaciho vykonu o 8.5 dBm" );
        // fix for https://github.com/espressif/arduino-esp32/issues/6767
        WiFi.setTxPower( WIFI_POWER_8_5dBm );
    }

    Serial.println("Wait 500 ms for AP_START...");
    // musi tady byt; kdyz se operace spusti _hned_, obcas to padne
    delay(500);
    WiFi.softAPsetHostname(SSID);
    
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    Serial.println("EasyWebServer start (AP mode)");
    this->begin();

    this->dnsServer.start(53, "*", WiFi.softAPIP());
}


void EasyWebServer::startWebserverClientMode()
{
    this->mode = EWS_CLIENT;
    Serial.println("EasyWebServer start (client mode)");
    this->begin();
}


/**
 * Musi byt volano z loop() !
 */
void EasyWebServer::processDNS()
{
    if( this->mode == EWS_AP ) {
        this->dnsServer.processNextRequest();
    }
}

const char *EasyWebServer::getQueryParamAsString(AsyncWebServerRequest *request, const char *paramName, const char *defaultValue)
{
    if(request->hasParam(paramName) ) { 
        return request->getParam(paramName)->value().c_str();
    } else {
        return defaultValue;
    }
}

long EasyWebServer::getQueryParamAsLong(AsyncWebServerRequest *request, const char *paramName, long defaultValue)
{
    if(request->hasParam(paramName) ) { 
        return atol( request->getParam(paramName)->value().c_str() );
    } else {
        return defaultValue;
    }
}

/**
 * Captive portal; nedela nic jineho nez ze presmerovava na urcene URL
*/
class CaptiveRequestHandler : public AsyncWebHandler {
    public:
        CaptiveRequestHandler( AsyncLogger * logger ) { 
            this->logger = logger;
        }
        virtual ~CaptiveRequestHandler() {}
        
        bool canHandle(__unused AsyncWebServerRequest *request) const override {
            // da se kontrolovat treba request->addInterestingHeader("ANY");

            if( request->host()==NULL ) {
                return true;
            }
            // prevezmeme vsechny pozadavky na servery, co nejsme zrovna my
            if( strcmp(request->host().c_str(),MY_NAME)!=0 ) {
                return true;
            }
            // pokud je pozadavek na nase jmeno, timto filtrem ho neresim, aby fungovalo 404
            return false;
        }

        void handleRequest(AsyncWebServerRequest *request) {
            this->logger->log( "  > %s%s", request->host().c_str(), request->url().c_str() );
            request->redirect( CAPTIVE_PORTAL_REDIRECT_URL );
        }

    private:
        AsyncLogger * logger;

};



/**
 * Konfigurace webserveru; konfiguraci podle aplikace nastavte v userRoutes() !
 */
void EasyWebServer::begin() 
{
    userRoutes( this->server );

    this->server->addHandler( new CaptiveRequestHandler(this->logger) );
                    // .setFilter(ON_AP_FILTER);  //only when requested from AP

    this->server->begin();
}





