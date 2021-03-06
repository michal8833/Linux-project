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

void parseArgs(int argc, char **argv, int *pid, int *signalNumber, int *value);
void registerSignalHandler(int signalNumber);
void decodeAndPrintReceivedInformation();

volatile sig_atomic_t receivedValue = -1;

int main(int argc, char **argv) {

    int signalNumber;
    int pid;
    int value;
    struct timespec twoSecondsWait = {.tv_sec = 2, .tv_nsec = 0};
    union sigval sv;

    parseArgs(argc, argv,  &pid, &signalNumber, &value);

    registerSignalHandler(signalNumber);

    sv.sival_int = value;
    sigqueue(pid, signalNumber, sv);

    if(value == 255) { // 'recorder' will send back information about current state of the program
        nanosleep(&twoSecondsWait, NULL);

        if(receivedValue != -1)
            decodeAndPrintReceivedInformation();
        else {
            fprintf(stderr, "Odpowiedz nie nadeszla.\n");
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
}


void parseArgs(int argc, char **argv, int *pid, int *signalNumber, int *value) {

    char *endptr;

    if(argc != 4) {
        fprintf(stderr, "Niepoprawne argumenty.\n");
        exit(EXIT_FAILURE);
    }    
    
    *pid = strtol(argv[1], &endptr, 0);
    if( !((*argv[1] != '\0') && (*endptr == '\0')) || *pid < 0 ) {
        fprintf(stderr, "Niepoprawne argumenty.\n");
        exit(EXIT_FAILURE);
    }

    *signalNumber = strtol(argv[2], &endptr, 0);
    if( !((*argv[2] != '\0') && (*endptr == '\0')) || *signalNumber < SIGRTMIN || *signalNumber > SIGRTMAX ) {
        fprintf(stderr, "Niepoprawne argumenty.\n");
        exit(EXIT_FAILURE);
    }

    *value = strtol(argv[3], &endptr, 0);
    if( !((*argv[3] != '\0') && (*endptr == '\0')) ) {
        fprintf(stderr, "Niepoprawne argumenty.\n");
        exit(EXIT_FAILURE);
    }
}

void signal_handl( int sig, siginfo_t * info, void * data ) {
    receivedValue = info->si_value.sival_int;
}

void registerSignalHandler(int signalNumber) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handl;
    if(sigaction(signalNumber, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void decodeAndPrintReceivedInformation() {
    if((receivedValue & 1) != 0) {
        printf("Rejestracja dziala.\n");

        if((receivedValue & 2) != 0)
            printf("Uzywany jest punkt referencyjny.\n");
        if((receivedValue & 4) != 0)
            printf("Uzywana jest identyfikacja zrodel.\n");
    }
    else
        printf("Rejestracja nie dziala.\n");

    if((receivedValue & 8) != 0)
        printf("Uzywany jest format binarny.\n");
}

