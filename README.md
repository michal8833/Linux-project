# Linux-project

Final project for "Programowanie w systemie Linux" university course.

## Description

Project consist of 3 programs:
* monochord - program periodically sends Real-time signal with accompying value, which is value of sinus function.
Furthermore, program receives messages with UDP socket. The message contains records terminated with `\n`. Information included in record is used to change program properties. Record should have format: `<parameter_name><spaces or tabs or char ':'><new_value_of_parameter>`.
`parameter_name` can be:
  * `amp` - sine amplitude
  * `freq` - sine frequency
  * `probe` - sampling frequency
  * `period` - sampling period[s]; special values: 0 - infinitity, <0 - sampling is stopped
  * `pid` - receiver's PID
  * `rt` - Real-time signal number
 Record can also contain only one field equal to "raport". In this case program process other records in the message and then it sends back message containing information about current program properties.
* rejestrator - program receives signals(sent by monochord by specified Real-time signal number(`dataSignalNumber`)) and writes values, which are delivered with signals, to text file and binary file.
The program is controlled by different Real-time signal(`commandsSignalNumber`). The commands concern: start or stop data registration, and request information on the current state of the program;
* info_rejestrator - program is used to request information on the current state of the 'rejestrator' by sending proper command;
