#include <Arduino.h>
#include "DetektorVytaceni.h"

/*
Viz popis v DetektorVytaceni.h
*/

/** vytaceni jedne cifry nemuze byt kratsi nez X ms */
#define MINIMALNI_CAS_VYTACENI_MS 90 

/** vytaceni jedne cifry nemuze byt delsi nez X ms od prvniho do posledniho impulsu */
#define MAXIMALNI_CAS_VYTACENI_MS 1500

#define MAX_CAS_PRO_VYTOCENI_CELEHO_CISLA 60000
#define PO_VYTOCENI_CIFRY_PRODLOUZIT_O 10000

#define IMPULZ HIGH
#define MEZERA LOW

#define MIN_DELKA_PULSU 20
#define MAX_DELKA_PULSU 80

#define SLUCHATKO_ZVEDNUTE LOW
#define SLUCHATKO_POLOZENE HIGH
#define SLUCHATKO_MIN_CAS_ZMENY 10

DetektorVytaceni::DetektorVytaceni( int pinSluchatko, int pinDetekce, int pinCiselnik, AsyncLogger * logger )
{
    this->pinSluchatko = pinSluchatko;
    this->pinDetekce = pinDetekce;
    this->pinCiselnik = pinCiselnik;
    this->logger = logger;

    this->delkaCisla = 6;
    this->interniStav = NIC;
    this->number[0];
    this->posledniReportovanyStav = ZADNA_UDALOST;
    this->sluchatko = POLOZENE;
    this->zmenaSluchatka=0;

    pinMode( pinDetekce, INPUT_PULLUP );
    pinMode( pinCiselnik, INPUT_PULLUP );
    pinMode( pinSluchatko, INPUT_PULLUP );
}

void DetektorVytaceni::nastavDelkuCisla(int delka)
{
  this->delkaCisla = delka;
}

void DetektorVytaceni::zvednutyTelefon()
{
  this->logger->log( "zvednuto" );
  this->sluchatko=ZVEDNUTE;
  this->zmenaSluchatka=millis();  
  this->stav = CEKAM_NA_CISLA;
  this->number[0] = 0;
  this->interniStav = NIC;
  this->posledniReportovanyStav = ZADNA_UDALOST;
  this->limitCasuNaVytoceni = millis() + MAX_CAS_PRO_VYTOCENI_CELEHO_CISLA;
}

void DetektorVytaceni::zavesenyTelefon()
{
  this->logger->log( "zaveseno" );
  this->sluchatko=POLOZENE;
  this->zmenaSluchatka=millis();
  this->stav = NESPUSTENO;
}

void DetektorVytaceni::process()
{
  long now = millis();

  int stavPinuSluchatka=digitalRead(this->pinSluchatko);

  if( this->sluchatko==POLOZENE && now>(SLUCHATKO_MIN_CAS_ZMENY+this->zmenaSluchatka) && stavPinuSluchatka==SLUCHATKO_ZVEDNUTE ) {
    this->zvednutyTelefon();
    return;
  }

  if( this->sluchatko==ZVEDNUTE && now>(SLUCHATKO_MIN_CAS_ZMENY+this->zmenaSluchatka) && stavPinuSluchatka==SLUCHATKO_POLOZENE ) {   
    this->zavesenyTelefon();
    return;
  }  

  // pokud neni zvednuty telefon, neresime nic
  if( this->stav != CEKAM_NA_CISLA ) {
    return;
  }

  if( now > limitCasuNaVytoceni ) {
    this->logger->log( "Cislo nebylo vytoceno do %d sec po zvednuti sluchatka",  MAX_CAS_PRO_VYTOCENI_CELEHO_CISLA/1000 );
    this->stav = TIMEOUT;
    return;
  }

  if( this->interniStav==NIC && digitalRead(this->pinDetekce)==LOW ) {
    this->interniStav = ZACALO_VYTACENI;
    this->digitStart = now;
    this->pulseCount = 0;
    this->firstPulseTime = 0;
    this->prevValue = MEZERA;
    this->pulseStart = now;
    this->logger->log( "Vytaceni!" );
  }

  if( this->interniStav != ZACALO_VYTACENI ) {
    return;
  }

  // sem se dostaneme, pokud uz se toci cifernik

  // vytaceni jako celek
  if( now > (MINIMALNI_CAS_VYTACENI_MS+this->digitStart) ) {
      // driv to nemusime kontrolovat, abychom ignorovali zachvevy po zacatku pulzu
      if( digitalRead(this->pinDetekce)==HIGH ) {
          // skoncilo vytoceni jedne cifry
          this->interniStav=NIC;
          this->logger->log( "* Konec cifry, delka %d ms, pocet pulsu=%d", (now - this->digitStart), this->pulseCount );  
          this->pridejCifru();
          return;
      }
  }

  // vytaceni jako celek
  if( (firstPulseTime!=0) && now > (MAXIMALNI_CAS_VYTACENI_MS+firstPulseTime) ) {
      // vytaceni nemuze byt delsi nez N ms
      this->interniStav=NIC;
      this->logger->log( "* timeout pro cifru, koncim" );
      return;
  }
  
  int readVal = digitalRead(this->pinCiselnik);
  if( this->prevValue != readVal ) {
    long delkaPulsu = now - this->pulseStart;
    if( delkaPulsu<MIN_DELKA_PULSU || delkaPulsu>MAX_DELKA_PULSU) {
      this->logger->log( "(%d)", delkaPulsu );
    } else { 
      this->logger->log( "<%d, %d>", delkaPulsu, this->prevValue );
      if( this->prevValue==IMPULZ ) {
        this->pulseCount++;
        if( firstPulseTime==0 ) {
          this->firstPulseTime = now;
        }
      }
    }
    this->prevValue = readVal;
    this->pulseStart = now;
  }
}


UdalostDetektoru DetektorVytaceni::vratUdalost()
{
    switch( this->stav ) {
      
      case NESPUSTENO:
        if( this->posledniReportovanyStav!=ZADNA_UDALOST && this->posledniReportovanyStav!=ZAVESENO ) {
          this->posledniReportovanyStav = ZAVESENO;
          return ZAVESENO;
        }
        this->posledniReportovanyStav = ZADNA_UDALOST;
        return ZADNA_UDALOST;

      case CEKAM_NA_CISLA:
        if( this->posledniReportovanyStav == ZADNA_UDALOST || this->posledniReportovanyStav == ZAVESENO ) {
          this->posledniReportovanyStav = CEKAM_NA_VYTOCENI;
          return CEKAM_NA_VYTOCENI;
        }
        
        // zacalo neco chodit
        if( this->interniStav==ZACALO_VYTACENI || strlen(this->number)!=0 ) {
          if( this->posledniReportovanyStav == CEKAM_NA_VYTOCENI) {
            this->posledniReportovanyStav = UZIVATEL_VYTACI;
            return UZIVATEL_VYTACI;
          }
        }
        return ZADNA_UDALOST;
        
      case PRIJATE_CISLO:
        if( this->posledniReportovanyStav != VYTOCENE_CISLO ) {
            this->posledniReportovanyStav = VYTOCENE_CISLO;
            return VYTOCENE_CISLO;
        }
        return ZADNA_UDALOST;

      case TIMEOUT:
        if( this->posledniReportovanyStav != TIMEOUT_VYTACENI ) {
            this->posledniReportovanyStav = TIMEOUT_VYTACENI;
            return TIMEOUT_VYTACENI;
        }
        return ZADNA_UDALOST;
    } 

    return ZADNA_UDALOST;
}

char *DetektorVytaceni::vytoceneCislo()
{
  if( this->stav!=PRIJATE_CISLO ) {
    return NULL;
  }
  return number;
}

void DetektorVytaceni::pridejCifru()
{
  if( this->pulseCount==0 || this->pulseCount>10 ) {
    this->logger->log( "Spatny pocet pulsu %d", this->pulseCount );
    return;
  }
  int cifra = this->pulseCount % 10;
  int len = strlen(this->number);
  sprintf( this->number + len, "%d", cifra );
  if( len+1 == this->delkaCisla ) {
    this->logger->log( "Vytocene cislo: [%s]", this->number );
    this->stav = PRIJATE_CISLO;
  } else {
    this->logger->log( "prijata cifra %d, zatim mam [%s]", cifra, this->number );
    this->limitCasuNaVytoceni += PO_VYTOCENI_CIFRY_PRODLOUZIT_O;
  }
}
