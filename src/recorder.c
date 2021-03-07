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

#define MAX_PATH 128
#define TIMESTAMP_LENGTH_MAX 32
#define RECORD_LENGTH_MAX 64

struct RegisterParameters {
    bool stop;
    bool globalTimestamp;
    bool identifySource;
} registerParameters;

struct Descriptors {
    int textFile;
    int binaryFile;
} descriptors;

struct timespec referencePoint;

void parseArgs(int argc, char **argv, char **textFilePath, char **binaryFilePath, int *dataSignalNumber, int *commandsSignalNumber);
void setDefaultValues();
void commandsSignal_handl(siginfo_t * info, sigset_t *blockMask, int dataSignalNumber);
void openFiles(char *textFilePath, char *binaryFilePath);
void timestampFormat(char *buf, struct timespec *ts);
void writeDataToFiles(int value, pid_t pid);
void error(char *functionName);


int main(int argc, char **argv) {

    char *textFilePath = NULL;
    char *binaryFilePath = NULL;
    int dataSignalNumber;
    int commandsSignalNumber;
    sigset_t blockMask;
    siginfo_t info;
    int sigNumber;

    parseArgs(argc, argv, &textFilePath, &binaryFilePath, &dataSignalNumber, &commandsSignalNumber);
    
    setDefaultValues();

    openFiles(textFilePath, binaryFilePath);

    if(signal(dataSignalNumber, SIG_IGN) == SIG_ERR) // sygnał przesyłający dane jest ignorowany, aby nie przerywał działania programu, kiedy jest odblokowany
        error("signal");

    sigemptyset(&blockMask);
    sigaddset(&blockMask, commandsSignalNumber);

    if(sigprocmask(SIG_BLOCK, &blockMask, NULL) == -1)
        error("sigprocmask");

    while(1) {
        sigNumber = sigwaitinfo(&blockMask, &info);
        if(sigNumber == -1)
            error("sigwaitinfo");
        else if(sigNumber == commandsSignalNumber)
            commandsSignal_handl(&info, &blockMask, dataSignalNumber);
        else if(sigNumber == dataSignalNumber)
            writeDataToFiles(info.si_value.sival_int, info.si_pid);
    }

    exit(EXIT_SUCCESS);
}


void parseArgs(int argc, char **argv, char **textFilePath, char **binaryFilePath, int *dataSignalNumber, int *commandsSignalNumber) {
    int g;
    char *endptr;

    *dataSignalNumber = *commandsSignalNumber = -1;
    
    while ((g = getopt (argc, argv, "-b:t:d:c:")) != -1)
        switch(g) {
            case 'b':
                *binaryFilePath = (char*)malloc(MAX_PATH*sizeof(char));
                strcpy(*binaryFilePath, optarg);
                break;

            case 't':
                *textFilePath = (char*)malloc(MAX_PATH*sizeof(char));
                strcpy(*textFilePath, optarg);
                break;

            case 'd':
                errno = 0;
                *dataSignalNumber = strtol(optarg, &endptr, 0);
                if( !((*optarg != '\0') && (*endptr == '\0')) || *dataSignalNumber < SIGRTMIN || *dataSignalNumber > SIGRTMAX ) {
                    fprintf(stderr, "Niepoprawne argumenty.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            
            case 'c':
                errno = 0;
                *commandsSignalNumber = strtol(optarg, &endptr, 0);
                if( !((*optarg != '\0') && (*endptr == '\0')) || *commandsSignalNumber < SIGRTMIN || *commandsSignalNumber > SIGRTMAX ) {
                    fprintf(stderr, "Niepoprawne argumenty.\n");
                    exit(EXIT_FAILURE);
                }
                break;

            default:
                fprintf(stderr, "Niepoprawne argumenty.\n");
                exit(EXIT_FAILURE);
        }

    if(*dataSignalNumber == -1 || *commandsSignalNumber == -1) {
        fprintf(stderr, "Nalezy podac wartosci parametrow -d i -c.\n");
        exit(EXIT_FAILURE);
    }
}

void setDefaultValues() {
    descriptors.textFile = -1;
    descriptors.binaryFile = -1;

    referencePoint.tv_sec = 0;
    referencePoint.tv_nsec = 0;
    
    registerParameters.globalTimestamp = true;
    registerParameters.identifySource = false;
    registerParameters.stop = true;
}

bool isRegularFile(int fd) {
    struct stat status;

    if(fstat(fd, &status) == -1)
        error("fstat");

    if(S_ISREG(status.st_mode))
        return true;
    return false;
}

void timespecDifferenceFromReferencePoint(struct timespec *second, struct timespec *res) {
    if ((second->tv_nsec - referencePoint.tv_nsec) > 0) {
        res->tv_sec = second->tv_sec - referencePoint.tv_sec;
        res->tv_nsec = second->tv_nsec - referencePoint.tv_nsec;
    }
    else {
        res->tv_sec = second->tv_sec - referencePoint.tv_sec - 1;
        res->tv_nsec = second->tv_nsec - referencePoint.tv_nsec + 1000000000;
    }
}

void buildTextRecord(char *record, int value, pid_t pid) {
    char timestamp[TIMESTAMP_LENGTH_MAX];
    struct timespec now, res;
    
    unsigned char bytes[4] = {value&0xFF, (value>>8)&0xFF,
                                (value>>16)&0xFF, (value>>24)&0xFF}; // zamiana liczby typu int na tablicę bajtów
    float receivedValue = *(float*)bytes; // odczytanie liczby typu float na podstawie tablicy bajtów

    if(registerParameters.globalTimestamp) {
        if(clock_gettime(CLOCK_REALTIME, &res) == -1)
            perror("clock_gettime");
    }
    else {
        if(clock_gettime(CLOCK_MONOTONIC, &now) == -1)
            perror("clock_gettime");
        timespecDifferenceFromReferencePoint(&now, &res);
    }
    timestampFormat(timestamp, &res);
    sprintf(record, "%s ", timestamp);

    sprintf(record, "%s %f ", record, receivedValue);

    if(registerParameters.identifySource) {
        sprintf(record, "%s %d\n", record, pid);
    }
    else {
        sprintf(record, "%s\n", record);
    }
}

void writeDataToFiles(int value, pid_t pid) {
    char textRecord[RECORD_LENGTH_MAX];

    buildTextRecord(textRecord, value, pid);
    if(dprintf(descriptors.textFile, "%s", textRecord) < 0 ) // zapisanie rekordu do pliku tekstowego
        perror("dprintf");
    
    if(descriptors.binaryFile != -1) { //format binarny jest stosowany
        struct timespec now, res;

        if(registerParameters.globalTimestamp) {
            if(clock_gettime(CLOCK_REALTIME, &res) == -1)
                perror("clock_gettime");
        }
        else {
            if(clock_gettime(CLOCK_MONOTONIC, &now) == -1)
                perror("clock_gettime");
            timespecDifferenceFromReferencePoint(&now, &res);
        }
        
        if(write(descriptors.binaryFile, &res, sizeof(res)) == -1)
            error("write");
        if(write(descriptors.binaryFile, &value, sizeof(value)) == -1)
            error("write");
        if(write(descriptors.binaryFile, &pid, sizeof(pid)) == -1)
            error("write");
    }
}

void commandsSignal_handl(siginfo_t * info, sigset_t *blockMask, int dataSignalNumber) {
    int val;
    union sigval sv;

    if(info->si_value.sival_int == 0) { // stop
        registerParameters.stop = true;

        sigdelset(blockMask, dataSignalNumber); // usunięcie sygnału przesyłającego dane z maski spowoduje, że funkcja sigwaitinfo() nie będzie na niego czekać
        if(sigprocmask(SIG_SETMASK, blockMask, NULL) == -1) // odblokowanie sygnału przesyłającego dane spowoduje, że nie będą one kolejkowane w czasie, gdy program jest w stanie "stop"
            error("sigprocmask");
    }
    else if(info->si_value.sival_int == 255) { // info
        val = 0;

        // ustawianie bitów zmiennej "val"
        if(registerParameters.stop == false) // rejestracja działa
            val |= 1UL << 0;
        if(registerParameters.globalTimestamp == false) // używany jest punkt referencyjny
            val |= 1UL << 1;
        if(registerParameters.identifySource == true) // używana jest identyfikacja źródeł
            val |= 1UL << 2;
        if(descriptors.binaryFile != -1) // używany jest format binarny
            val |= 1UL << 3;

        sv.sival_int = val;
        if(sigqueue(info->si_pid, info->si_signo, sv) == -1) // odesłanie do nadawcy tego samego sygnału, ale z wartością informująca o bieżącym statusie programu
            perror("sigqueue");
    }
    else if(info->si_value.sival_int >= 1 && info->si_value.sival_int <= 16) { // start
        registerParameters.stop = false;
        
        registerParameters.identifySource = false;

        sigaddset(blockMask, dataSignalNumber);  // dodanie sygnału przesyłającego dane do maski spowoduje, że funkcja sigwaitinfo() będzie na niego czekać
        if(sigprocmask(SIG_SETMASK, blockMask, NULL) == -1) // blokuję sygnał przesyłający dane, aby mieć pewność, że zostanie on odebrany tylko wtedy, gdy wywołana zostanie funkcja sigwaitinfo()
            error("sigprocmask");

        info->si_value.sival_int -= 1; // zmniejszam wartość o 1, aby było łatwiej ją zinterpretować

        if((info->si_value.sival_int & 2) != 0 && (info->si_value.sival_int & 1) != 0) // wartości +1 i +2 nie mogą wystąpić jednocześnie
            return;
            
        if((info->si_value.sival_int & 2) != 0) { // poprzedni punkt referencyjny
            registerParameters.globalTimestamp = false;
            if(referencePoint.tv_sec == 0) // jeśli jeszcze żaden punkt referencyjny nie był określony
                if(clock_gettime(CLOCK_MONOTONIC, &referencePoint) == -1)
                    perror("clock_gettime");
        }
        else if((info->si_value.sival_int & 1) != 0) { // nowy punkt referencyjny
            registerParameters.globalTimestamp = false;
            if(clock_gettime(CLOCK_MONOTONIC, &referencePoint) == -1)
                perror("clock_gettime");
        }
        else { // bez punktu referencyjnego
            registerParameters.globalTimestamp = true;
        }

        if((info->si_value.sival_int & 4) != 0) { // użycie identyfikacji źródeł
            registerParameters.identifySource = true;
        }

        if((info->si_value.sival_int & 8) != 0) { // skrócenie plików do zera
            if(isRegularFile(descriptors.textFile))
                if(ftruncate(descriptors.textFile, 0) == -1) // usunięcie starej zawartości, jeżeli plik jest regularny
                    perror("ftruncate");

            if(isRegularFile(descriptors.binaryFile))
                if(ftruncate(descriptors.binaryFile, 0) == -1) // usunięcie starej zawartości, jeżeli plik jest regularny
                    perror("ftruncate");
        }
    }
}

void openFiles(char *textFilePath, char *binaryFilePath) {
    if(textFilePath == NULL) // ścieżka nie została podana, więc domyślnie jest to standardowe wyjście
        descriptors.textFile = 1;
    else if(strcmp(textFilePath, "-") == 0) // podanie "-" jako ścieżki oznacza standardowe wejśćie
        descriptors.textFile = 0;
    else {
        if((descriptors.textFile = open(textFilePath, O_WRONLY | O_CREAT, S_IRWXU)) == -1)
            error("open");
    }

    if(isRegularFile(descriptors.textFile))
        if(ftruncate(descriptors.textFile, 0) == -1) // usunięcie starej zawartości, jeżeli plik jest regularny
            perror("ftruncate");

    if(binaryFilePath == NULL) { // ścieżka nie została podana, więc format binarny nie jest stosowany
        descriptors.binaryFile = -1;
        return;
    }
    else if(strcmp(binaryFilePath, "-") == 0) // podanie "-" jako ścieżki oznacza standardowe wejśćie
        descriptors.binaryFile = 0;
    else {
        if((descriptors.binaryFile = open(binaryFilePath, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU)) == -1)
            error("open");
    }

    if(isRegularFile(descriptors.binaryFile))
        if(ftruncate(descriptors.binaryFile, 0) == -1) // usunięcie starej zawartości, jeżeli plik jest regularny
            perror("ftruncate");
}

void timestampFormat(char *buf, struct timespec *ts) {
    struct tm t;

    if(localtime_r(&(ts->tv_sec), &t) == NULL)
        perror("localtime_r");

    if(registerParameters.globalTimestamp)
        sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d.%03ld", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, ts->tv_nsec/1000000);
    else
        sprintf(buf, "0:%02d:%02d.%03ld", t.tm_min, t.tm_sec, ts->tv_nsec/1000000);
}

void error(char *functionName) {
    perror(functionName);
    exit(EXIT_FAILURE);
}
