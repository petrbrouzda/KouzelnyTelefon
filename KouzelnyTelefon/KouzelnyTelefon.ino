// FQBN: esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=80,FlashFreq=40,FlashMode=dio

/*
TODO:
- detekce napětí akumulátoru + vypsat ve webu + ukončit práci na konci baterky
*/


// zapojeni desky
#include "pinout.h"

/* AsyncLogger se pouziva pro logovani udalosti v asynchronnich aktivitach (detektor vyraceni, webserver...).
Uklada zaznamy do pole v pameti a ty se pak vypisou v loop() pomoci volani dumpTo(); */
#include "src/logging/AsyncLogger.h"
AsyncLogger asyncLogger;

#include "src/logging/SerialLogger.h"
SerialLogger serialLogger( &Serial );


/** Sdileny stav aplikace - objekt drzici napr. chybu inicializace MP3, aby se dala vypsat  */
#include "AppState.h"
AppState appState;


/* Asynchronni detektor vytaceni na telefonu  */
#include "DetektorVytaceni.h"
DetektorVytaceni detektor( SLUCHATKO, VYTACENI_AKTIVNI, CISELNIK, &asyncLogger );

/* periodicke spousteni nacitani z portu se dela pres Ticker,
binduje se v pripojDetektorKTickeru()
a callback jde pres detektorCallback() */
#include <Ticker.h>
Ticker milisecondTicker;


/*
Obsluha MP3 prehravace pres izolacni vrstvu zjednodusujici ovladani

Low-level se pouziva knihovna:
https://github.com/DFRobot/DFRobotDFPlayerMini
*/
#include "Mp3Player.h"
Mp3Player player( &serialLogger, &appState );

// 0-30
#define VOLUME 30

#define MP3_SLOZKA_TELEFONU 7
#define VYTACECI_TON 1
#define VYZVANECI_TON 2
#define CISLO_NEEXISTUJE 3

HardwareSerial hwSerial_1(1);


/*
WebServer - taky pres izolacni vrstvu

Pouzivaji se tyto dve knihovny:
- https://github.com/ESP32Async/ESPAsyncWebServer
- https://github.com/ESP32Async/AsyncTCP 
*/
#include "EasyWebServer.h"
EasyWebServer webserver( &asyncLogger );


/*
Konfigurace.
Je nutny alespon kousek filesystemu SPIFFS.
*/
#include "src/toolkit/BasicConfig.h"
#include "src/toolkit/ConfigProviderSpiffs.h"
BasicConfig config;
// může dostat serialLogger, protože poběží synchronně v loopu
ConfigProviderSpiffs configProvider( &serialLogger, &config, &appState);
bool saveConfigChange=false;


//----- callback detektoru vytaceni

// je volano kazdou ms z Tickeru
void detektorCallback() 
{
  detektor.process();
}




//---------- setup

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println( "Startuju!" );

  pinMode( LED, OUTPUT );
  digitalWrite( LED, LOW );

  configProvider.openFsAndLoadConfig();

  pripojDetektorKTickeru();
  detektor.nastavDelkuCisla( config.getLong( "NumberLen", 5 ) );

  digitalWrite( LED, LOW );
  hwSerial_1.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX );
  player.begin( &hwSerial_1 );
  player.setVolume( VOLUME );

  webserver.startApAndWebserver();

  digitalWrite( LED, HIGH );
}

void pripojDetektorKTickeru()
{
  milisecondTicker.attach_ms(1, detektorCallback );
}

/**
 * Zde nastavte routy pro svou aplikaci.
 * Je zavolano z webserveru v dobe volani webserver.startWifiAndWebserver()
 */
void userRoutes( AsyncWebServer * server )
{
  server->on("/", HTTP_GET, onRequestRoot );
  server->on("/nastaveni", HTTP_GET, onRequestNastaveni );
  server->on("/nastaveniA", HTTP_GET, onRequestNastaveniA );
  server->on("/nastaveniNr", HTTP_GET, onRequestNastaveniCislo );
}




// ----------- vlastni vykonna cast aplikace (loop)

PlayerCommand prehrat;

/**
 * Najde vytocene cislo. Bud vrati cislo PlayerCommand s cislem slozky a souboru, nebo NULL
 */
PlayerCommand * najdiTelefonniCislo( char * cislo )
{
  long items = config.getLong( "items", 3 );
  for( int i=0; i<items; i++ ) {
    char fieldName[20];
    sprintf( fieldName, "%d_nr", i );
    const char *nr = config.getString( fieldName, "" );
    if( strcmp(nr,cislo)!=0 ) {
      continue;
    }
    Serial.printf( "Tlf. cislo [%s] nalezeno na pozici %d.\n", cislo, i+1 );

    // sem se dostaneme jen pro spravne cislo
    sprintf( fieldName, "%d_folder", i );
    prehrat.folder = config.getLong( fieldName, -1 );
    sprintf( fieldName, "%d_file", i );
    prehrat.file = config.getLong( fieldName, -1 );
    return &prehrat;
  }

  Serial.printf( "Tlf. cislo [%s] neznam.\n", cislo );
  return NULL;
}





void zpracujUdalostiTelefonu()
{
  // pokud se neco stane, zde dostaneme udalost (kazdou jen jednou)
  UdalostDetektoru udalost = detektor.vratUdalost();
  if( udalost==CEKAM_NA_VYTOCENI ) {
    Serial.printf( "** UDALOST: Zvednute sluchatko, cekam na vytoceni.\n");
    digitalWrite( LED, LOW );
    // prehravat vytaceci ton
    player.playFile( MP3_SLOZKA_TELEFONU, VYTACECI_TON, OPAKOVAT_NEUSTALE );
  }
  
  if( udalost==UZIVATEL_VYTACI ) {
    Serial.printf( "** UDALOST: Uzivatel zacal vytacet.\n");
    // prestat prehravat vytaceci ton
    player.stop();
    
  }

  if( udalost==VYTOCENE_CISLO ) {
    Serial.printf( "** UDALOST: Vytocene cislo %s\n", detektor.vytoceneCislo() );

    strcpy( appState.posledniVytoceneCislo, detektor.vytoceneCislo() );
    appState.casPoslednihoVytoceni = millis();

    PlayerCommand * cmd = najdiTelefonniCislo( detektor.vytoceneCislo() );
    if( cmd != NULL ) {
      player.playFile( MP3_SLOZKA_TELEFONU, VYZVANECI_TON, 1 );
      player.setNextFile( cmd->folder, cmd->file, 1 );
    } else {
      player.playFile( MP3_SLOZKA_TELEFONU, CISLO_NEEXISTUJE, 3 );
    }

  }

  if( udalost==TIMEOUT_VYTACENI ) {
    Serial.printf( "** UDALOST: Uzivatel nevytocil cislo behem minuty.\n");
    // ton pro spatne cislo
    player.playFile( MP3_SLOZKA_TELEFONU, CISLO_NEEXISTUJE, 3 );
  }
  if( udalost==ZAVESENO ) {
    Serial.printf( "** UDALOST: Uzivatel zavesil sluchatko.\n");
    // prestat prehravat
    player.stop();
    digitalWrite( LED, HIGH );
  }
}



void onConfigChanged() 
{
    saveConfigChange=false;
    configProvider.saveConfig();
    
    // kdyby se zmenila delka cisla:
    detektor.nastavDelkuCisla( config.getLong( "NumberLen", 5 ) );
    
    // vypsat konfiguraci
    Serial.println( "--- zacatek konfigurace" );
    config.printTo(Serial);
    Serial.println( "--- konec konfigurace" );
}



void loop() {

  // vypiseme asynchronni log, pokud v nem neco je
  asyncLogger.dumpTo( &Serial );

  // udalosti z MP3 prehravace
  player.process();

  // odbavit DNS pozadavky
  webserver.processDNS();

  // udalosti z telefonu
  zpracujUdalostiTelefonu();

  // pokud uzivatel zmenil nastaveni, ulozit do souboru a promitnout, kde je potreba
  // nezavesujeme primo na config.isDirty(), protoze bychom mohli trefit nekonzistenci pri nastavovani asynchronne z webserveru
  if( saveConfigChange ) {
    onConfigChanged();
  }

}




// -------------- odsud dal je webserver

const char htmlHlavicka[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
  <head>
    <title>Kouzelný telefon</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html {font-family: Arial; display: inline-block; text-align: left;}
      h2 {font-size: 2.0rem;}
      h3 {font-size: 1.65rem; font-weight: 600}
      p {font-size: 1.4rem;}
      input {font-size: 1.4rem;}
      input#text {width: 100%;}
      select {font-size: 1.4rem;}
      form {font-size: 1.4rem;}
      body {max-width: 600px; margin:10px ; padding-bottom: 25px;}
    </style>
  </head>
  <body>
)rawliteral";

const char htmlPaticka[] PROGMEM = R"rawliteral(
  </body>
  </html>
)rawliteral";




const char htmlZacatek[] PROGMEM = R"rawliteral(
  <h1>Kouzelný telefon</h1>
  <form method="GET" action="/?">
  <input type="submit" name="obnov" value="Obnov stav" > 
  </form>
)rawliteral";


void vlozInformace( AsyncResponseStream *response )  {
  if( appState.isProblem() ) {
    response->printf( "<p><b>Problém! %s:</b> [%s] před %d sec.</p>",
      appState.globalState==ERROR ? "ERR" : "Warning",
      appState.problemDesc,
      (millis()-appState.problemTime) / 1000L
    );  
  }
  
  if( appState.casPoslednihoVytoceni!=0 ) {
    response->printf( "<p>Poslední vytočené číslo: <b>%s</b> před %d sec.</p>",
      appState.posledniVytoceneCislo,
      (millis()-appState.casPoslednihoVytoceni) / 1000L
    );
  }
}

void vlozUptime( AsyncResponseStream *response ) {
  response->printf( "<p>Čas od spuštění zařízení: %d min</p>",
    millis()/60000L
  );
}


/** 
 * Je lepe misto Serial.print pouzivat AsyncLogger.
 * Je volano z webserveru asynchronne.
 * Nevolat odsud dlouhotrvajici akce!
 * I logovani by melo byt pres asyncLogger!
 * 
 * Kazda funkce onRequest* musi byt zaregistrovana v userRoutes()
 */
void onRequestRoot(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req root" );

  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( htmlHlavicka );
  response->print( htmlZacatek );
  vlozInformace( response );
  response->print( "<p><a href=\"/nastaveni\">Nastavení systému</a>" );
  vlozUptime( response );
  response->print( htmlPaticka );
  request->send(response);

  // variantne: request->redirect("/#0");
}




const char htmlNastaveni1[] PROGMEM = R"rawliteral(
  <h1>Nastavení</h1>
  <p><a href="/">Zpět</a></p>
  <form action="/nastaveniA" method="GET">
)rawliteral";

void onRequestNastaveni(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req nastaveni" );

  long numberLen = config.getLong( "NumberLen", 5 );
  long items = config.getLong( "items", 3 );

  AsyncResponseStream *response = request->beginResponseStream(webserver.HTML_UTF8);
  response->print( htmlHlavicka );
  response->print( htmlNastaveni1 );
  response->printf( "Délka telefonního čísla:<br> <input type=\"number\" name=\"delka\" min=\"1\" max=\"5\" value=\"%d\"><br>",
      numberLen );
  response->printf( "Počet telefonních čísel:<br> <input type=\"number\" name=\"pocet\" min=\"1\" max=\"20\" value=\"%d\"><br>",
      items ); 
  response->print( "<input type=\"submit\" value=\"Změnit!\"></form><br><hr><br>" );
  
  for( int i=0; i<items; i++ ) {
    char fieldName[20];
    sprintf( fieldName, "%d_nr", i );
    const char *nr = config.getString( fieldName, "" );
    sprintf( fieldName, "%d_folder", i );
    int folder = config.getLong( fieldName, -1 );
    sprintf( fieldName, "%d_file", i );
    int file = config.getLong( fieldName, -1 );
    response->printf( "<a name=\"%d\"></a><form action=\"/nastaveniNr\" method=\"GET\"><input type=\"hidden\" name=\"id\" value=\"%d\">", 
      i, i );
    response->printf( "Tlf. číslo:<br> <input type=\"text\" name=\"nr\" maxlength=\"%d\" value=\"%s\"><br>",
      numberLen, nr );
    response->printf( "Složka:<br> <input type=\"number\" name=\"folder\" min=\"1\" max=\"99\" value=\"%d\"><br>",
      folder );
    response->printf( "Soubor:<br> <input type=\"number\" name=\"file\" min=\"1\" max=\"999\" value=\"%d\"><br>",
      file );
    response->print( "<input type=\"submit\" value=\"Nastavit!\"></form><br><hr><br>" );
  }

  response->print( "<p><a href=\"/\">Zpět</a></p>" );

  request->send(response);

  // variantne: request->redirect("/#0");
}



void onRequestNastaveniA(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req nastaveni-akce" );

  int numberLen = webserver.getQueryParamAsLong( request, "delka", 4 );
  int items = webserver.getQueryParamAsLong( request, "pocet", 3 );
  config.setValue( "NumberLen", numberLen );
  config.setValue( "items", items );

  saveConfigChange=true;

  request->redirect("/nastaveni");
}  


void onRequestNastaveniCislo(AsyncWebServerRequest *request){
  asyncLogger.log( "@ req nastaveni-cislo" );
  
  int id = webserver.getQueryParamAsLong( request, "id", -1 );
  if( id==-1 ) {
    asyncLogger.log( "ID nenastaveno" );
    request->redirect("/nastaveni");
    return;
  }
  const char * cislo = webserver.getQueryParamAsString( request, "nr", "" );
  long numberLen = config.getLong( "NumberLen", 5 );
  if( strlen(cislo)!=numberLen ) {
    asyncLogger.log( "Cislo nema spravnou delku [%s]", cislo );
    request->redirect("/nastaveni");
    return;
  }
  int folder = webserver.getQueryParamAsLong( request, "folder", -1 );
  int file = webserver.getQueryParamAsLong( request, "file", -1 );

  char fieldName[20];
  sprintf( fieldName, "%d_nr", id );
  config.setValue( fieldName, cislo );
  sprintf( fieldName, "%d_folder", id );
  config.setValue( fieldName, folder );
  sprintf( fieldName, "%d_file", id );
  config.setValue( fieldName, file );

  saveConfigChange=true;

  char url[50];
  sprintf( url, "/nastaveni#%d", id );
  request->redirect( url );
}



/**
ESP32 2.0.17

Using library Ticker at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\Ticker 
Using library DFRobotDFPlayerMini at version 1.0.6 in folder: E:\dev.moje\arduino\libraries\DFRobotDFPlayerMini 
Using library DNSServer at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\DNSServer 
Using library WiFi at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\WiFi 
Using library Async TCP at version 3.4.0 in folder: E:\dev.moje\arduino\libraries\Async_TCP 
Using library ESP Async WebServer at version 3.7.7 in folder: E:\dev.moje\arduino\libraries\ESP_Async_WebServer 
Using library FS at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\FS 
Using library SPIFFS at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\SPIFFS 

 */