#include <Arduino.h>

#include "Mp3Player.h"

/*
Viz popis v Mp3Player.h
*/

Mp3Player::Mp3Player( LoggerInterface * logger, AppState *appState ) 
{
    this->logger = logger;
    this->appState = appState;

    this->status = IDLE;
}

bool Mp3Player::begin( HardwareSerial * hws )
{
    this->logger->log("MP3: Initializing DFPlayer ... (May take 3~5 seconds)");
    delay(100);

    if (!myDFPlayer.begin(*hws, /*isACK = */true, /*doReset = */true)) 
    {  
        this->logger->log("MP3: Unable to begin: recheck the connection, insert the SD card!");
        this->appState->setProblem( ERROR, "Nelze inicializovat MP3 prehravac. Je v nem SD karta?" );
        this->inited = false;
        return false;
    }
    this->logger->log("MP3: DFPlayer Mini online.");
    this->inited = true;
    return true;
}

/**
 * Set volume value. From 0 to 30
 */
void Mp3Player::setVolume(int volume)
{
    if( !this->inited ) return;

    this->logger->log( "MP3: set volume %d", volume );
    myDFPlayer.volume(volume);  
}

void Mp3Player::stop()
{
    if( !this->inited ) return;

    this->logger->log( "MP3: stop" );

    // posilam 2x, protoze se mi jednou stalo, ze stop tesne po start ignoroval
    this->myDFPlayer.stop();
    delay(100);
    this->myDFPlayer.stop();

    this->command.folder = -1;
    this->isNextCommand = false;
    this->status = IDLE;
}

void Mp3Player::playFile( int folder, int file )
{
    this->playFile( folder, file, 1 );
}

void Mp3Player::playFile( int folder, int file, int repeatTimes )
{
    if( !this->inited ) return;

    this->command.folder = folder;
    this->command.file = file;
    this->command.repeatTimes = repeatTimes;
    
    this->isNextCommand = false;
    this->status = REQUEST_TO_PLAY;

    this->logger->log( "MP3: hraju folder %d, file %d, opakovani=%d", folder, file, repeatTimes );
    this->myDFPlayer.playFolder(folder, file);
}

void Mp3Player::setNextFile( int folder, int file, int repeatTimes )
{
    if( !this->inited ) return;

    this->nextCommand.folder = folder;
    this->nextCommand.file = file;
    this->nextCommand.repeatTimes = repeatTimes;
    
    this->isNextCommand = true;    
}

bool Mp3Player::isPlaying()
{
    return ( this->status==REQUEST_TO_PLAY || this->status==PLAYING );
}

void Mp3Player::playNextIfAvailable()
{

    if( this->command.folder == -1 ) {
        this->logger->log( "MP3: Doslo ke stopu!");
        return;
    }
    
    if( this->command.repeatTimes==-1 ) {
        bool isNext = this->isNextCommand;
        this->playFile( this->command.folder, this->command.file, this->command.repeatTimes );
        this->isNextCommand = isNext;
        return;
    }

    this->command.repeatTimes--;
    if( this->command.repeatTimes>0 ) {
        bool isNext = this->isNextCommand;
        this->playFile( this->command.folder, this->command.file, this->command.repeatTimes );
        this->isNextCommand = isNext;
        return;
    }

    if( this->isNextCommand ) {
        this->logger->log( "MP3: prepinam na nextCommand" );
        this->playFile( this->nextCommand.folder, this->nextCommand.file, this->nextCommand.repeatTimes );
    }
}

void Mp3Player::process()
{
    if( !this->inited ) return;

    if (!this->myDFPlayer.available()) {
        return;
    }
    int type = myDFPlayer.readType();
    int value = myDFPlayer.read(); 

    char buffer[100];

    switch (type) {
        
        case TimeOut:
            // tohle chodi po prvnim commandu play, ignorovat?
            this->logger->log("MP3: (Time Out)");
            break;

        case WrongStack:
            this->logger->log("MP3: Stack Wrong!");
            break;
        case DFPlayerCardInserted:
            this->logger->log("MP3: Card Inserted!");
            break;
        case DFPlayerCardRemoved:
            this->logger->log("MP3: Card Removed!");
            this->appState->setProblem( WARNING, "MP3: Vyndana SD karta." );
            break;
        case DFPlayerCardOnline:
            this->logger->log("MP3: Card Online!");
            break;
        case DFPlayerUSBInserted:
            this->logger->log("MP3: USB Inserted!");
            break;
        case DFPlayerUSBRemoved:
            this->logger->log("MP3: USB Removed!");
            break;
        
        case DFPlayerPlayFinished:
            // tohle chodi kdyz skonci prehravani
            this->logger->log( "MP3: Number %d - Play Finished", value);
            this->status=DONE;
            this->playNextIfAvailable();
            break;

        case DFPlayerError:
            this->logger->log("MP3: DFPlayerError: ");
            switch (value) {
                case Busy:
                    this->logger->log("Card not found");
                    this->appState->setProblem( WARNING, "MP3: SD karta nenalezena." );
                    break;
                case Sleeping:
                    this->logger->log("Sleeping");
                    this->appState->setProblem( WARNING, "MP3: Sleeping." );
                    break;
                case SerialWrongStack:
                    this->logger->log("Get Wrong Stack");
                    this->appState->setProblem( WARNING, "MP3: Get Wrong Stack." );
                    break;
                case CheckSumNotMatch:
                    this->logger->log("Check Sum Not Match");
                    break;

                case FileIndexOut:
                    // kdyz je pokyn na prehrani spatneho souboru
                    this->logger->log("File Index Out of Bound [folder=%d file=%d]",
                        this->command.folder,
                        this->command.file
                    );
                    sprintf( buffer, "MP3: Neplatne cislo souboru k prehrani (FileIndexOut) [folder=%d file=%d].", 
                        this->command.folder,
                        this->command.file
                    );
                    this->appState->setProblem( WARNING, buffer );
                    this->status=NOT_FOUND;
                    this->playNextIfAvailable();
                    break;

                case FileMismatch:
                    // kdyz je pokyn na prehrani spatneho souboru
                    this->logger->log("Cannot Find File  [folder=%d file=%d]", 
                        this->command.folder,
                        this->command.file
                    );
                    sprintf( buffer, "MP3: Neplatne cislo souboru k prehrani (FileMismatch) [folder=%d file=%d].", 
                        this->command.folder,
                        this->command.file
                    );
                    this->appState->setProblem( WARNING, buffer );
                    this->status=NOT_FOUND;
                    this->playNextIfAvailable();
                    break;

                case Advertise:
                    this->logger->log("In Advertise");
                    break;
                
                default:
                    this->logger->log("Neznama chyba %d", value );
                    break;
            }
            break;

        default:
            this->logger->log("Neznamy stav %d", type );
            break;
    }
  
}