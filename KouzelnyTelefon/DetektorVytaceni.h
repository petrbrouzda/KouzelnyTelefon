#ifndef __DETEKTOR_VYTACENI__
#define __DETEKTOR_VYTACENI__

/*
 * Obsluhuje cely subsystém telefonu.
 
 Je napojený na tři piny:
 1) kontakt zvednutého sluchátka
 2) kontakt probíhajícího vytáčení
 3) impulzní kontakt pro jednotlivé impulzy

 Funkce process() je volána každou milisekundu z Tickeru, 
 kontroluje stav všech tří pinů a provozuje stavový automat.

 Nevyhodnocuje validitu čísla. Čeká, dokud uživatel nevytočí číslo o správné délce
 nebo do vypršení timeoutu. Po dosažení nastavené délky vytáčení končí.

 Funkce vratUdalost() vrací události ke zpracování uživatelskou aplikací.

 Omezení: V každý okamžik umí jen jednu délku vytáčeného čísla, tj. nejde mít víc čísel různých délek.
 */

#include "src/logging/AsyncLogger.h"

enum StavSluchatka {
  POLOZENE,
  ZVEDNUTE
};

enum StavDetektoru {
  NESPUSTENO,
  CEKAM_NA_CISLA,
  PRIJATE_CISLO,
  TIMEOUT
};

enum UdalostDetektoru {
  ZADNA_UDALOST,
  CEKAM_NA_VYTOCENI,
  UZIVATEL_VYTACI,
  VYTOCENE_CISLO,
  TIMEOUT_VYTACENI,
  ZAVESENO
};


enum InterniStav {
  NIC,
  ZACALO_VYTACENI
};

#define PHONE_NUMBER_MAX 100

class DetektorVytaceni 
{
  public:
    DetektorVytaceni( int pinSluchatko, int pinDetekce, int pinCiselnik, AsyncLogger * logger );
    void nastavDelkuCisla( int delka );

    /** volano periodicky per 1 ms */
    void process();

    UdalostDetektoru vratUdalost();
    char * vytoceneCislo();

  private:
    void pridejCifru();
    void zvednutyTelefon();
    void zavesenyTelefon();

    // konfiguracni parametry
    AsyncLogger * logger;
    int pinDetekce;
    int pinCiselnik;
    int pinSluchatko;
    int delkaCisla;

    // stav detektoru a vytaceni cisla jako celku
    StavDetektoru stav;
    UdalostDetektoru posledniReportovanyStav;
    StavSluchatka sluchatko;

    long zmenaSluchatka;
    long limitCasuNaVytoceni;
    char number[PHONE_NUMBER_MAX+1];
    
    // promenne pro zpracovani jedne cifry    
    InterniStav interniStav;
    long digitStart;
    int pulseCount;
    long firstPulseTime;
    int prevValue;
    long pulseStart;
};

#endif
