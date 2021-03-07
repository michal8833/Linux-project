# Monochord

## Intro

Final project for "Programowanie w systemie Linux" university course.<br/>
In the project i used:
* sockets,
* Linux signals sending and handling,
* POSIX timers,
* File I/O

## Build

```
mkdir build && cd build
cmake ..
make
```
or
```
// go to 'src' directory
gcc monochord.c -o monochord -lm -lrt
gcc recorder.c -o recorder
gcc info_recorder.c -o info_recorder
```

## Description

Project consist of 3 programs:
* monochord<br/>
**Usage:**<br/>
`./monochord <UDP_port_number>`<br/><br/>
Program periodically sends Real-time signal with accompying value, which is value of sinus function.
Furthermore, program receives messages with UDP socket. The message contains records terminated with `\n`. Information included in record is used to change program properties. Record should have format: `<parameter_name><spaces or tabs or char ':'><new_value_of_parameter>`. Record can also contain only one field that equals "raport". In this case program process other records in the message and then it sends back a new message containing information about current program properties.<br/>
**NOTE:** Because of the default values, sampling is stopped at the beginning of the program. To start it, you have to properly set `period`, `pid` and `rt`.<br/><br/>
`parameter_name` can be:
  * `amp` - sine amplitude
  * `freq` - sine frequency
  * `probe` - sampling frequency
  * `period` - sampling period[seconds]; special values: 0 - infinity, <0 - sampling is stopped
  * `pid` - receiver's PID
  * `rt` - Real-time signal number
* recorder<br/>
**Usage:**<br/>
`./recorder [OPTION] [VALUE]...`<br/>
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
    * +4 : write PID of process, which sent signal with data, to files
    * +8 : truncate text and binary files
  * 255 : send back information about current state of the program
* info_recorder<br/>
**Usage:**<br/>
`./info_recorder <recorder_PID_number> <commands_signal_number> <command_value>`<br/><br/>
Program is used to send command to 'recorder'.<br/><br/>

## Example
```
./monochord 8888
./recorder -b "binary" -t "text" -d 34 -c 35
ps -ax // to check PID of 'recorder' (in my case "9074")

// you can use 'nc' to send message to 'monochord'
nc -u 127.0.0.1 8888 <<<"pid 9074" // change pid
nc -u 127.0.0.1 8888 <<<"rt 34" // change Real-time signal number used to send data
nc -u 127.0.0.1 8888 <<<"period 0" // change sampling period to infinity
./info_recorder 9074 35 5 // start 'recorder' with writing PID to files not using reference point
./info_recorder 9074 35 6 // start 'recorder' with writing PID to files using just set reference point
nc -u 127.0.0.1 8888 <<<"amp 4" // change sine amplitude to 4
```
After executing the above commands, content of "text" file in my case is:<br/>
```
2021-03-06 00:10:30.510  -0.807507  9062
2021-03-06 00:10:31.510  0.188917  9062
2021-03-06 00:10:32.510  -0.710339  9062
2021-03-06 00:10:33.510  0.513651  9062
.
.
.
0:00:25.928  -0.805691  9062
0:00:26.928  0.194331  9062
0:00:27.928  0.478126  9062
0:00:28.928  -3.998338  9062
0:00:29.928  3.589485  9062
.
.
.
```


