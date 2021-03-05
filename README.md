# Linux-project

Final project for "Programowanie w systemie Linux" university course.

## Description

Project consist of 3 programs:
* monochord<br/>
**Usage:**<br/>
`./monochord <UDP_port_number>`<br/><br/>
Program periodically sends Real-time signal with accompying value, which is value of sinus function.
Furthermore, program receives messages with UDP socket. The message contains records terminated with `\n`. Information included in record is used to change program properties. Record should have format: `<parameter_name><spaces or tabs or char ':'><new_value_of_parameter>`. Record can also contain only one field that equals "raport". In this case program process other records in the message and then it sends back a new message containing information about current program properties.<br/>
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
Program receives signals(sent by monochord by specified Real-time signal number(`dataSignalNumber`)) and writes values, which are delivered with signals, to text file and binary file. Values are written with date and time, specfying when they arrived or, if the program is using reference point in that moment, only with time elapsed since time specified by reference point.
The program is controlled by values sent to it with different Real-time signal(`commandsSignalNumber`).<br/>
Meaning of received value:
  * 0 : stop writing data to files
  * 1 : start writing data to files
    * +0 : do not use reference point
    * +1 : set reference point to the present time and use it 
    * +2 : use previous reference point
    * +4 : write PID of process, which sent signal with data, to file
    * +8 : truncate text and binary files
  * 255 : send back information about current state of the program
* info_rejestrator<br/>
**Usage:**<br/>
`./info_rejestrator -c <commands_signal_number> <rejestrator_PID_number>`<br/><br/>
Program is used to request information on the current state of the 'rejestrator' by sending proper command.
