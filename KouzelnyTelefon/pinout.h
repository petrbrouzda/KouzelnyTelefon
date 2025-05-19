
// napojeni telefonu
#define VYTACENI_AKTIVNI 5
#define CISELNIK 6
#define SLUCHATKO 7

#define LED 8

// MP3 prehravac; RX a TX jsou z pohledu ESP32 (TX = my vysilame, MP3 prijimac prijima)
#define MP3_RX 4
#define MP3_TX 2

// merici delic na akumulatoru a hodnoty rezistoru v delici
#define ACCU 0
// horni odpor delice (pozor, nutno zapsat s desetinnou teckou!)
#define DELIC_R1 47000.0
// dolni odpor delice (pozor, nutno zapsat s desetinnou teckou!)
#define DELIC_R2 10000.0
// kolik to namerilo / kolik to melo byt
#define KALIBRACE (4.0/4.10)

#define LOW_BATTERY_LIMIT 3.5
