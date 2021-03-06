#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <math.h>

#define MESSAGE_LENGTH_MAX 1024
#define PARAMETER_LENGTH_MAX 128
#define PI 3.14

struct Parameters {
    float amp;
    float freq;
    float probe;
    float period;
    short pid;
    short rt;
} parameters;

struct StateInfo {
    bool endOfPeriod;
    bool wrongPidOrNoPermission;
    bool wrongPid; // zmienna utworzona tylko na potrzeby raportu, wysyłanego przez program
    bool wrongSignalNumber;
    bool timerStopped;
} stateInfo;

struct TimeInfo {
    timer_t timer;
    long referencePoint; // [ns]
    long periodStartPoint; // [ns]
} timeInfo;

void parseArgs(int argc, char **argv, short *port);
void setDefaultValues();
void registerSocket(int *serverSocket, short port);
void registerSignalHandler(); // rejestracja obsługi sygnału USR1, wysyłanego przez timer
void setTimer(bool armed); // parametr "armed" określa, czy funkcja ma nastawić czy rozbroić budzik
void startSimulation();
void interpretMessage(char *message, int serverSocket, struct sockaddr_in *clientAddr, socklen_t clientAddrLength);
void error(char *functionName);

int main(int argc, char **argv) {
    int serverSocket; 
    char messageBuf[MESSAGE_LENGTH_MAX]; 
    short port;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLength;
    int received;
    sigset_t set;

    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddrLength = sizeof(clientAddr);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    parseArgs(argc, argv, &port);
    setDefaultValues();
      
    registerSocket(&serverSocket, port);
    registerSignalHandler();

    startSimulation();

    struct sigevent se = {.sigev_notify=SIGEV_SIGNAL, .sigev_signo=SIGUSR1};
    if(timer_create(CLOCK_REALTIME, &se, &timeInfo.timer) == -1)
        error("timer_create");

    while(1) {
        do {
            errno = 0;
            if((received = recvfrom(serverSocket, (char*)messageBuf, sizeof(messageBuf), 0, ( struct sockaddr *) &clientAddr, &clientAddrLength)) == -1 && errno != EINTR)
                error("recvfrom");
        } while(received == -1);

        messageBuf[received] = '\0';

        if(sigprocmask(SIG_BLOCK, &set, NULL) == -1) // zablokowanie sygnału USR1 na czas operowania na zmiennych współdzielonych
            error("sigprocmask");

        interpretMessage(messageBuf, serverSocket, &clientAddr, clientAddrLength);

        if(sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) // odblokowanie sygnału USR1
            error("sigprocmask");
    }

    exit(EXIT_SUCCESS); 
}


void parseArgs(int argc, char **argv, short *port) {
    char *endptr;
    long val;

    if(argc != 2) {
        fprintf(stderr, "Niepoprawne argumenty.\n");
        exit(EXIT_FAILURE);
    }

    errno = 0;
    val = strtol(argv[1], &endptr, 0);
    if( !((*argv[1] != '\0') && (*endptr == '\0')) || val < 0 || val > SHRT_MAX || errno != 0 ) {
        fprintf(stderr, "Niepoprawne argumenty.\n");
        exit(EXIT_FAILURE);
    }

    *port = val;
}

void setDefaultValues() {
    parameters.amp = 1.0;
    parameters.freq = 0.25;
    parameters.probe = 1;
    parameters.period = -1;
    parameters.pid = 1;
    parameters.rt = 0;

    stateInfo.endOfPeriod = false;
    stateInfo.wrongPidOrNoPermission = false;
    stateInfo.wrongPid = false;
    stateInfo.wrongSignalNumber = true;
    stateInfo.timerStopped = true;

    timeInfo.referencePoint = 0;
    timeInfo.periodStartPoint = 0;
}

void registerSocket(int *serverSocket, short port) {
    uint32_t addr;
    struct sockaddr_in serverAddress;

    memset(&serverAddress, 0, sizeof(serverAddress)); 

    if((*serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        error("socket"); 
    
    inet_pton(AF_INET, "127.0.0.1", &addr);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = addr; 
    serverAddress.sin_port = htons(port); 
    
    if(bind(*serverSocket, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) 
        error("bind"); 
}

void floatToBytes(unsigned char bytes[4], float floatVal) {
    union {
        float floatNumber;
        unsigned char bytes[4];
    } res;

    res.floatNumber = floatVal;

    memcpy(bytes, res.bytes, 4);
}

void SIGUSR1_handl( int sig, siginfo_t * info, void * data ) {
    struct timespec now;
    unsigned char bytes[4];
    long elapsedPeriodTime; // czas[ns], jaki upłynął od rozpoczęcia próbkowania
    long elapsedTime; // czas[ns], jaki upłynął od punktu referencyjnego
    double sinVal;
    union sigval sv;

    clock_gettime(CLOCK_REALTIME, &now);
    elapsedPeriodTime = (now.tv_sec*1000000000 + now.tv_nsec) - timeInfo.periodStartPoint;
    elapsedTime = (now.tv_sec*1000000000 + now.tv_nsec) - timeInfo.referencePoint;

    if(parameters.period > 0 && elapsedPeriodTime >= parameters.period*1000000000/*[ns]*/) { // zatrzymanie próbkowania - czas próbkowania minął
        setTimer(false);
        stateInfo.endOfPeriod = true;
        return;
    }

    sinVal = parameters.amp * sin(2 * PI * parameters.freq * elapsedTime);

    floatToBytes(bytes, (float)sinVal); // zamieniam liczbę zmiennoprzecinkową na tablicę bajtów
    sv.sival_int=*(int*)bytes; // tablica bajtów jest zamieniana na liczbę typu int, z której odbiorca sygnału będzie mógł odczytać liczbę typu float

    errno = 0;
    if(sigqueue(parameters.pid, parameters.rt, sv) == -1 && (errno == EPERM || errno == ESRCH)) { // jeżeli wysłanie nie jest możliwe (nieaktualny PID, brak uprawnień), to próbkowanie zostaje wstrzymane aż do czasu zmiany stosownego parametru
        setTimer(false);
        stateInfo.wrongPidOrNoPermission = true;
        if(errno == ESRCH)
            stateInfo.wrongPid = true;
    }
}

void registerSignalHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SIGUSR1_handl;
    if(sigaction(SIGUSR1, &sa, NULL) == -1)
        error("sigaction");
}

void setTimer(bool armed) { // parametr "armed" określa, czy funkcja ma nastawić czy rozbroić budzik
    struct timespec ts;
    struct itimerspec tspec;

    if(armed) { // nastawienie budzika
        ts.tv_sec=parameters.probe;
        ts.tv_nsec=(long)(parameters.probe*1000000000)%1000000000;
        tspec.it_interval=ts;
        tspec.it_value=ts;

        stateInfo.timerStopped = false;
    }
    else { // rozbrojenie budzika
        ts.tv_sec=0L;
        ts.tv_nsec=0L;
        tspec.it_interval=ts;
        tspec.it_value=ts;

        stateInfo.timerStopped = true;
    }
    
    timer_settime(timeInfo.timer, 0, &tspec, NULL);
}

void startSimulation() {
    struct timespec ts;
    if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
        error("clock_gettime");
    timeInfo.referencePoint = ts.tv_sec*1000000000 + ts.tv_nsec; // aktualizacja punktu referencyjnego
}

void sendReport(int serverSocket, struct sockaddr_in *clientAddr, socklen_t clientAddrLength) {
    char *messageToSender = (char*)malloc(MESSAGE_LENGTH_MAX*sizeof(char));
    char record[64];
    int len;

    if(messageToSender == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(EXIT_FAILURE);
    }

    len = sprintf(record, "amp %f\n", parameters.amp);
    strncat(messageToSender, record, len);

    len = sprintf(record, "freq %f\n", parameters.freq);
    strncat(messageToSender, record, len);

    len = sprintf(record, "probe %f\n", parameters.probe);
    strncat(messageToSender, record, len);

    if(parameters.period < 0) {
        len = sprintf(record, "period %f stopped\n", parameters.period);
    }
    else if(parameters.period > 0) {
        if(stateInfo.timerStopped)
            len = sprintf(record, "period %f suspended\n", parameters.period);
        else
            len = sprintf(record, "period %f\n", parameters.period);
    }
    else {
        if(stateInfo.timerStopped)
            len = sprintf(record, "period %f suspended\n", parameters.period);
        else
            len = sprintf(record, "period %f non-stop\n", parameters.period);
    }
    strncat(messageToSender, record, len);

    if(stateInfo.wrongPid)
        len = sprintf(record, "pid %d nie istnieje\n", parameters.pid);
    else
        len = sprintf(record, "pid %d istnieje\n", parameters.pid);
    strncat(messageToSender, record, len);

    if(stateInfo.wrongSignalNumber)
        len = sprintf(record, "rt %d nieakceptowalna\n", parameters.rt);
    else
        len = sprintf(record, "rt %d akceptowalna\n", parameters.rt);
    strncat(messageToSender, record, len);

    if(sendto(serverSocket, messageToSender, strlen(messageToSender), 0, (const struct sockaddr *) clientAddr, clientAddrLength) == -1)
        error("sendto");

    free(messageToSender);
}

bool readRecord(char **message, char *parameterName, char *parameterValue, bool *raport) {
    char parameterName_temp[PARAMETER_LENGTH_MAX];
    char parameterValue_temp[PARAMETER_LENGTH_MAX];
    bool colonOccurred = false;
    short wordLength = 0;

    while(**message != ' ' && **message != '\t' && **message != ':') {
        if(**message == '\0')
            return false;
        if(**message == '\n') {
            parameterName_temp[wordLength] = '\0';

            if(strcmp(parameterName_temp, "raport") == 0) {
                strcpy(parameterName, parameterName_temp);
                *raport = true;
                (*message)++;
                return true;
            }
            else {
                (*message)++;
                return false;
            }
        }

        parameterName_temp[wordLength++] = **message;
        (*message)++;

        if(wordLength >= PARAMETER_LENGTH_MAX)
            return false;
    }

    parameterName_temp[wordLength] = '\0';

    while(**message == ' ' || **message == '\t' || **message == ':') {
        if(**message == ':' && !colonOccurred)
            colonOccurred = true;
        else if(**message == ':' && colonOccurred) // między polami jest dopuszczalne umieszczenie tylko jednego znaku ':'
            return false;
        
        (*message)++;
    }
        
    wordLength = 0;

    while(**message != '\n') {
        if(**message == '\0')
            return false;
        
        parameterValue_temp[wordLength++] = **message;
        (*message)++;

        if(wordLength >= PARAMETER_LENGTH_MAX)
            return false;
    }

    parameterValue_temp[wordLength] = '\0';

    strcpy(parameterName, parameterName_temp);
    strcpy(parameterValue, parameterValue_temp);

    (*message)++;

    return true;
}

void interpretMessage(char *message, int serverSocket, struct sockaddr_in *clientAddr, socklen_t clientAddrLength) {
    char parametersNames[16][PARAMETER_LENGTH_MAX];
    char parametersValues[16][PARAMETER_LENGTH_MAX];
    short parametersNumber = 0;
    char *endptr;
    float float_value;
    long short_value;
    bool raport = false; // informacja, czy komunikat zawierał rekord z prośbą o raport

    while(*message != '\0') {
        if(readRecord(&message, parametersNames[parametersNumber], parametersValues[parametersNumber], &raport)) { // wczytywanie kolejnych rekordów, zawartych w odebranym komunikacie
            parametersNumber++;
        }
        else {
            fprintf(stderr, "Niepoprawny format odebranego komunikatu.\n");
            return;
        }
    }

    for(short parameter=0; parameter<parametersNumber; parameter++) { // interpretowanie kolejnych rekordów, zawartych w odebranym komunikacie
        if(strcmp(parametersNames[parameter], "amp") == 0) {
            errno = 0;
            float_value = strtof(parametersValues[parameter], &endptr);
            if( !((*parametersValues[parameter] != '\0') && (*endptr == '\0')) || errno != 0 ) {
                fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"amp\": niepoprawna wartosc.\n");
            }
            else {
                if(parameters.amp != float_value) {
                    parameters.amp = float_value;
                    startSimulation(); // zmiana parametru związanego z układem powoduje ponowne rozpoczęcie symulacji
                }
            }
        }
        else if(strcmp(parametersNames[parameter], "freq") == 0) {
            errno = 0;
            float_value = strtof(parametersValues[parameter], &endptr);
            if( !((*parametersValues[parameter] != '\0') && (*endptr == '\0')) || float_value < 0 || errno != 0 ) {
                fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"freq\": niepoprawna wartosc.\n");
            }
            else {
                if(parameters.freq != float_value) {
                    parameters.freq = float_value;
                    startSimulation(); // zmiana parametru związanego z układem powoduje ponowne rozpoczęcie symulacji
                }
            }   
        }
        else if(strcmp(parametersNames[parameter], "probe") == 0) {
            errno = 0;
            float_value = strtof(parametersValues[parameter], &endptr);
            if( !((*parametersValues[parameter] != '\0') && (*endptr == '\0')) || float_value <= 0 || errno != 0 ) {
                fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"probe\": niepoprawna wartosc.\n");
            }
            else {
                if(parameters.probe != float_value) { // zmienia się częstotliwość próbkowania, więc uruchamiany jest timer z nowym interwałem
                    parameters.probe = float_value;
                    if(parameters.period >= 0 && !stateInfo.endOfPeriod && !stateInfo.wrongPidOrNoPermission && !stateInfo.wrongSignalNumber)
                        setTimer(true);
                }
            }
        }
        else if(strcmp(parametersNames[parameter], "period") == 0) {
            errno = 0;
            float_value = strtof(parametersValues[parameter], &endptr);
            if( !((*parametersValues[parameter] != '\0') && (*endptr == '\0')) || errno != 0 ) {
                fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"period\": niepoprawna wartosc.\n");
            }
            else {
                parameters.period = float_value;
                stateInfo.endOfPeriod = false;
                if(parameters.period > 0) {
                    struct timespec ts;
                    if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
                        error("clock_gettime");
                    timeInfo.periodStartPoint = ts.tv_sec*1000000000 + ts.tv_nsec; // ustawienie momentu rozpoczęcia odliczania do końca próbkowania
                }
                else if(parameters.period < 0)
                    setTimer(false);

                if(parameters.period >= 0 && !stateInfo.wrongPidOrNoPermission && !stateInfo.wrongSignalNumber)
                    setTimer(true);
            }
        }
        else if(strcmp(parametersNames[parameter], "pid") == 0) {
            errno = 0;
            short_value = strtol(parametersValues[parameter], &endptr, 0);
            if( !((*parametersValues[parameter] != '\0') && (*endptr == '\0')) || short_value < SHRT_MIN || short_value > SHRT_MAX || errno != 0 ) {
                fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"pid\": niepoprawna wartosc.\n");
            }
            else {
                parameters.pid = short_value;

                if(stateInfo.wrongPidOrNoPermission == true) { // jeżeli poprzedni pid był nieaktualny lub było brak odpowiednich uprawnień, by go wysłać(próbkowanie zostało wstrzymane), zmiana PIDu wznawia próbkowanie
                    stateInfo.wrongPidOrNoPermission = stateInfo.wrongPid = false;
                    if(parameters.period >= 0 && !stateInfo.endOfPeriod && !stateInfo.wrongSignalNumber)
                        setTimer(true);
                }
            }
        }
        else if(strcmp(parametersNames[parameter], "rt") == 0) {
            errno = 0;
            short_value = strtol(parametersValues[parameter], &endptr, 0);
            if( !((*parametersValues[parameter] != '\0') && (*endptr == '\0')) || short_value < SHRT_MIN || short_value > SHRT_MAX || errno != 0 ) {
                fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"rt\": niepoprawna wartosc.\n");
            }
            else {
                parameters.rt = short_value;

                if(parameters.rt < SIGRTMIN || parameters.rt > SIGRTMAX) { // parametr "rt" nie mieści się w zakresie sygnałów Real-Time
                    stateInfo.wrongSignalNumber = true;
                    setTimer(false);
                }
                else {
                    if(stateInfo.wrongSignalNumber == true) { // jeżeli poprzedni numer sygnału nie był w zakresie sygnałów Real-Time(próbkowanie zostało wstrzymane), zmiana numeru sygnału na numer mieszczący się w zakresie, wznawia próbkowanie
                        stateInfo.wrongSignalNumber = false;
                        if(parameters.period >= 0 && !stateInfo.endOfPeriod && !stateInfo.wrongPidOrNoPermission)
                            setTimer(true);
                    }
                }
            }
        }
        else if(strcmp(parametersNames[parameter], "raport") == 0) {
            continue;
        }
        else {
            fprintf(stderr, "Nie udalo sie zmienic wartosci parametru \"%s\": brak parametru o takiej nazwie.\n", parametersNames[parameter]);
        }
    }

    if(raport) {
        sendReport(serverSocket, clientAddr, clientAddrLength); // proces odsyła do nadawcy informację o bieżącej konfiguracji
    }
}

void error(char *functionName) {
    perror(functionName);
    exit(EXIT_FAILURE);
}
