#ifndef ___APP_STATUS_H_
#define ___APP_STATUS_H_

/*
Sdílený stav celé aplikace.
Používá se, aby asynchronní akce ve webserveru měly přístup ke všemu, co potřebují.
*/

#define STATUS_TEXT_MAX_LEN 255

enum ProblemLevel {
    NONE,
    WARNING,
    ERROR
};

class AppState
{
    public:

        AppState();

        //--- evidence problemu, common funkcionalita

        ProblemLevel globalState;
        char problemDesc[STATUS_TEXT_MAX_LEN + 5];
        long problemTime;

        void setProblem( ProblemLevel appState, const char * text );
        void clearProblem();

        bool isProblem();

        //---- per-app funkcionalita
        char posledniVytoceneCislo[100];
        long casPoslednihoVytoceni;

        double accuVoltage;
};

#endif