# Linux-project

Final project for subject "Programowanie w systemie Linux".

## Description

Project consist of 3 programs:
*monochord - program periodically sends Real-time signal with accompying value, which is value of sinus function.
Furthermore, program receives messages with UDP socket. The message changes program properties. It contains parameters for sinus function, PID of signal receiver or number of signal;
*rejestrator - program receives signals(sent by monochord by specified Real-time signal number(`dataSignalNumber`)) and writes values, which are delivered with signals, to text file and binary file.
The program is controlled by different Real-time signal('commandsSignalNumber'). The commands concern: start or stop data registration, and request information on the current state of the program.
*info_rejestrator - program is used to request information on the current state of the 'rejestrator' by sending proper command.
