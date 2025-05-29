# KouzelnyTelefon

Oživení klasického mechanického telefonu s rotačním číselníkem pomocí ESP32 a MP3 playeru.

![](/doc/osazeni.jpg) 

Po zvednutí sluchátka se ozve vytáčecí tón,
po vytočení čísla to nejdříve zvoní, a pak to přehraje nahrávku (pro různá čísla různé nahrávky).
Pro špatné číslo se ozve zvuk pro neplatné číslo.

Konfiguraci správných čísel a kontrolu stavu zařízení lze dělat přes webový prohlížeč přes spuštěné WiFi AP.

* Wifi AP: Olsanska2681
* heslo: Hoffmann2

Webový server pak běží na http://192.168.1.1/ , ale WiFi by si mělo samo otevřít prohlížeč po připojení (captive portál).

Aby spolehlivě fungovalo připojení na webserver, doporučuju:
* na mobilce vypnout data,
* na telefonech Xiaomi následně potvrdit, že má zůstat připojený na WiFi, i když v ní není připojení k internetu.


## SD karta v přehrávači

Doporučuji SD kartu naformátovat na FAT32.

Na SD kartě musí být adresář 07 se soubory 001.mp3 až 004.mp3 (viz [adresář sdcard](/sdcard) ).
A pak tam jsou potřeba nějaké další adresáře pojmenované 01-99 se soubory 001.mp3-999.mp3, které se budou přehrávat dle telefonních čísel - mapování se nastaví přes webserver.

Pokud karta nefunguje, je třeba zkontrolovat, že je formátovaná v MBR rezimu (s GPT partition table to nefunguje) - zmíněno zde 
https://forum.digikey.com/t/dfplayer-mini-communication-issue/18159/24 a popis, jak to rozlišit, je zde: https://www.tenforums.com/tutorials/84888-check-if-disk-mbr-gpt-windows.html



## Zapojení, použité součástky a konfigurace

Verze desky ESP32 v Arduino IDE **musí** být 2.0.x (nyní 2.0.17). Na 3.0.x to fungovat nebude.

Použité moduly:
* ESP32-C3 supermini https://s.click.aliexpress.com/e/_oCdelcb
* MP3 přehrávač https://www.laskakit.cz/audio-mini-mp3-prehravac/ - popisek na modulu je "MP3-TF-16P V3.0"

Schema je v [adresáři doc](/doc/schema.svg).

Konfigurace desky:
* deska: ESP32C3 Dev module
* flash mode: DIO (jinak tahle deska neběží!)
* CDC On Boot: enabled
* CPU speed: 80 MHz
* flash speed: 40 MHz
* partition scheme: default 4 MB with SPIFFS (1.2 MB APP/1.5 MB SPIFFS)

Knihovny:
* MP3 přehrávač: https://github.com/DFRobot/DFRobotDFPlayerMini
* Web server: https://github.com/ESP32Async/ESPAsyncWebServer a  https://github.com/ESP32Async/AsyncTCP 
* Tasker: https://github.com/joysfera/arduino-tasker

Detailní výpis z kompilace:
```
FQBN: esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=80,FlashFreq=40,FlashMode=dio 
Using library Ticker at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\Ticker 
Using library DFRobotDFPlayerMini at version 1.0.6 in folder: E:\dev.moje\arduino\libraries\DFRobotDFPlayerMini 
Using library DNSServer at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\DNSServer 
Using library WiFi at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\WiFi 
Using library Async TCP at version 3.4.0 in folder: E:\dev.moje\arduino\libraries\Async_TCP 
Using library ESP Async WebServer at version 3.7.7 in folder: E:\dev.moje\arduino\libraries\ESP_Async_WebServer 
Using library FS at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\FS 
Using library ESP32AnalogRead at version 0.3.0 in folder: E:\dev.moje\arduino\libraries\ESP32AnalogRead 
Using library Tasker at version 2.0.3 in folder: E:\dev.moje\arduino\libraries\Tasker 
Using library SPIFFS at version 2.0.0 in folder: C:\Users\brouzda\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.17\libraries\SPIFFS 
```

