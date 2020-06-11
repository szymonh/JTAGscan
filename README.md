# JTAGscan

JTAGscan allows you to find JTAG ports without buying expensive hardware like the JTAGulator.
All you need is a Arduino compatible board and optionally a logic level shifter.

## How does it work?

JTAGscan iterates through configured set of pins to find the JTAG TMS, TCK, TDO and TDI lines
using two approaches:
- by reading ID CODE to find TMS, TCK and TDO, TDI is found by shifting bits in BYPASS mode
- by shifting bits in BYPASS mode to find TMS, TCK, TDO and TDI
The first approach is quicker and thus preferred.
Unfortunately not all chips support ID CODE retrieval so BYPASS mode scan will be needed for them.

## How to use it?

- hook up your Arduino to the target
- use a logic level shifer between Arduino and target if required
- adjust PIN_MASK and PIN_MAX for your setup
- select proper build target
- build the project and upload it to your board
- establish serial connection with baud rate of 115200
- ammend debug options if required
- use option _h_ for help
- use option _e_ to enumerate pins using available methods

## How does it look like?

- help
```
> h
e - enumerate JTAG pins in automatic mode
f - enumerate pins by reading ID CODE
g - enumerate lines using BYPASS mode
h - print this help
i - find TDI using BYPASS mode
d - set debug level
```
- automatic enumeration on a Kinetis based board
```
> e
tck:  4 | tms:  2 | tdo:  5 | tdi:  X | value: cba00477
tck:  4 | tms:  2 | tdo:  5 | tdi:  3 | value:     1f
```

- ID CODE enumeration of TCK, TMS and TDO lines with debug level 1
```
> d
Choose debug level 0-2 1
> f
tck:  2 | tms:  3 | tdo:  4 | tdi:  X | value:      0
tck:  2 | tms:  3 | tdo:  5 | tdi:  X | value:      0
tck:  2 | tms:  4 | tdo:  3 | tdi:  X | value:      0
tck:  2 | tms:  4 | tdo:  5 | tdi:  X | value:      0
tck:  2 | tms:  5 | tdo:  3 | tdi:  X | value:      0
tck:  2 | tms:  5 | tdo:  4 | tdi:  X | value:      0
tck:  3 | tms:  2 | tdo:  4 | tdi:  X | value:      0
tck:  3 | tms:  2 | tdo:  5 | tdi:  X | value:      0
tck:  3 | tms:  4 | tdo:  2 | tdi:  X | value:      0
tck:  3 | tms:  4 | tdo:  5 | tdi:  X | value:      0
tck:  3 | tms:  5 | tdo:  2 | tdi:  X | value:      0
tck:  3 | tms:  5 | tdo:  4 | tdi:  X | value:      0
tck:  4 | tms:  2 | tdo:  3 | tdi:  X | value:      0
tck:  4 | tms:  2 | tdo:  5 | tdi:  X | value:      0
tck:  4 | tms:  3 | tdo:  2 | tdi:  X | value:      0
tck:  4 | tms:  3 | tdo:  5 | tdi:  X | value:      0
tck:  4 | tms:  5 | tdo:  2 | tdi:  X | value: cba00477
```

- enumeration of TDI following a ID CODE scan
```
> i
tck:  4 | tms:  5 | tdo:  2 | tdi:  3 | value:     1f
```

- full identification of lines using BYPASS mode
```
> g
tck:  2 | tms:  3 | tdo:  4 | tdi:  5 | value:      0
tck:  2 | tms:  3 | tdo:  5 | tdi:  4 | value:      0
tck:  2 | tms:  4 | tdo:  3 | tdi:  5 | value:      0
tck:  2 | tms:  4 | tdo:  5 | tdi:  3 | value:      0
tck:  2 | tms:  5 | tdo:  3 | tdi:  4 | value:      0
tck:  2 | tms:  5 | tdo:  4 | tdi:  3 | value:      0
tck:  3 | tms:  2 | tdo:  4 | tdi:  5 | value:      0
tck:  3 | tms:  2 | tdo:  5 | tdi:  4 | value:      0
tck:  3 | tms:  4 | tdo:  2 | tdi:  5 | value:      0
tck:  3 | tms:  4 | tdo:  5 | tdi:  2 | value:      0
tck:  3 | tms:  5 | tdo:  2 | tdi:  4 | value:      0
tck:  3 | tms:  5 | tdo:  4 | tdi:  2 | value:      0
tck:  4 | tms:  2 | tdo:  3 | tdi:  5 | value:      0
tck:  4 | tms:  2 | tdo:  5 | tdi:  3 | value:      0
tck:  4 | tms:  3 | tdo:  2 | tdi:  5 | value:      0
tck:  4 | tms:  3 | tdo:  5 | tdi:  2 | value:      0
tck:  4 | tms:  5 | tdo:  2 | tdi:  3 | value:     1f
```

## No Platformio?

No problem, just copy the contents of src/main.cpp to a new Arduino project, remove the Arduino.h import from the first line and you're ready to go.
