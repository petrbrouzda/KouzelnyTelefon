#ifndef ___MP3_PLAYER__H_
#define ___MP3_PLAYER__H_

/**
 * Obálka nad MP3 playerem. Odstiňuje aplikaci od chytání a obsluhy jednotlivých událostí.
 * Umožňuje automaticky opakovat přehrávaný soubor.
 * Umožňuje po spuštění přehrávání poslat do fronty jeden další soubor.
 */


/**
 *
Poznámky k MP3 playeru: 

Pouziva se knihovna:
https://github.com/DFRobot/DFRobotDFPlayerMini
  
Struktura karty:
- adresare s cisly 01 .. 99
- v nich soubory 001.mp3 ... 999.mp3

Popis modulu: https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299
Zdrojak knihovny: https://github.com/DFRobot/DFRobotDFPlayerMini/blob/master/DFRobotDFPlayerMini.h

Muj modul:
MP3-TF-16P V3.0

1) karta musi byt formatovana v MBR rezimu
https://forum.digikey.com/t/dfplayer-mini-communication-issue/18159/24

2) napajeni musi byt 5 V

3) 
playLargeFolder nefunguje
play funguje podle fyzickeho poradi na karte, ne podle jmena

4) playMp3Folder(1) funguje, jmenna konvence:
  adresar: mp3
  soubory: 0001.mp3 0002.mp3 atd

5) playFolder(1, 3) funguje, jmenna konvence:
  adresar: 01
  soubory: 001.mp3 002.mp3 atd

6)
Pokud se zada spatne cislo souboru, vrati se chyba:
DFPlayerError:Cannot Find File

7) readFileCountsInFolder asi nefunguje
readState asi taky ne

*/

#include <DFRobotDFPlayerMini.h>
#include "AppState.h"

#include "src/logging/LoggerInterface.h"


#define OPAKOVAT_NEUSTALE -1

enum PlayerStatus {
      /** 0 nic nedelam */
    IDLE,
      /** 1 pozadavek na prehrani */
    REQUEST_TO_PLAY,
      // TODO: v realu nikde nenastavuju 
      /** 2 poslano do prehravace  */
    PLAYING,
      /** 3 prehravani skoncilo */
    DONE,
      /** 4 soubor nenalezen */
    NOT_FOUND
};

class PlayerCommand
{
    public:
        int folder;
        int file;
        int repeatTimes;
};

class Mp3Player 
{
    public:
        Mp3Player( LoggerInterface * logger, AppState *appState );
        bool begin( HardwareSerial *hws );

        /**
         * Hlasitost 0-30
         */
        void setVolume( int volume );

        /** prehraje jednou, smaze zaznam o nasledujicim souboru k prehravani */
        void playFile( int folder, int file );

        /**
         * repeatTimes bud pocet opakovani, nebo OPAKOVAT_NEUSTALE.
         * smaze zaznam o nasledujicim souboru k prehravani
         */
        void playFile( int folder, int file, int repeatTimes );

        /**
         * Musi byt zavolano az po playFile() !
         */
        void setNextFile( int folder, int file, int repeatTimes );

        /**
         * Prerusi stavajici prehravani
         */
        void stop();

        /**
         * Odbavuje udalosti, musi byt volano z loop()
         */
        void process();

        DFRobotDFPlayerMini myDFPlayer;
        PlayerStatus status;

        /**
         * Vraci informaci, zda se prave ted prehrava
         */
        bool isPlaying();

    private:
        void playNextIfAvailable();

        AppState *appState;
        bool inited;

        LoggerInterface * logger;

        PlayerCommand command;

        bool isNextCommand;
        PlayerCommand nextCommand;

};


#endif
