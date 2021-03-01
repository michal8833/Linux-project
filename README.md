# Linux-project

Final project for "Programowanie w systemie Linux" university course.

## Description

Project consist of 3 programs:
* monochord<br/>
**Usage:**<br/>
`./monochord <UDP_port_number>`<br/><br/>
Program periodically sends Real-time signal with accompying value, which is value of sinus function.
Furthermore, program receives messages with UDP socket. The message contains records terminated with `\n`. Information included in record is used to change program properties. Record should have format: `<parameter_name><spaces or tabs or char ':'><new_value_of_parameter>`. Record can also contain only one field that equals "raport". In this case program process other records in the message and then it sends back a new message containing information about current program properties.
`parameter_name` can be:
  * `amp` - sine amplitude
  * `freq` - sine frequency
  * `probe` - sampling frequency
  * `period` - sampling period[seconds]; special values: 0 - infinitity, <0 - sampling is stopped
  * `pid` - receiver's PID
  * `rt` - Real-time signal number
* rejestrator<br/>
**Usage:**<br/>
`./rejestrator [OPTION] [VALUE]...`<br/>
OPTION can be:<br/>
`-b`[optional] - binary file path<br/>
`-t` - text file path<br/>
`-d` - number of signal transmitting data<br/>
`-c` - number of signal transmitting commands<br/><br/>
Program receives signals(sent by monochord by specified Real-time signal number(`dataSignalNumber`)) and writes values, which are delivered with signals, to text file and binary file.
The program is controlled by different Real-time signal(`commandsSignalNumber`). The commands concern: start or stop data registration, and request information on the current state of the program;
* info_rejestrator<br/>
**Usage:**<br/>
`./info_rejestrator -c <signal_number> <PID_number>`<br/><br/>
Program is used to request information on the current state of the 'rejestrator' by sending proper command;
