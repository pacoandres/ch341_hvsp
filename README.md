# About
As the name says, this is an application for programming AVR chips with HVSP protocol using a CH341.

This library is based on the module contained in CH341PAR_LINUX.ZIP published by WCH:

http://www.wch.cn/download/CH341PAR_LINUX_ZIP.html

The application has been tested with a YSUMA01-341A module, and I think it must work with any EPP/I2C/SPI module based on the CH341 chip (and maybe any CH34X)

# Todo
## Hardware
In the folder [Hardware](hardware/ "hardware") you can find an schematic and a Kicad project.

The project is based on a board with access to all the GPIO ([Models](https://github.com/stahir/CH341-Store "https://github.com/stahir/CH341-Store")) and a MT3608 voltage booster for getting the required 12V in the AVR reset pin.

The GPIO assignment is as follows, but you can change it (don't forget to change it in code too)

|CH341 GPIO  |AVR HVSP   |
|--- |--- |
|D0|RST|
|--- |--- |
|D1|SCI|
|--- |--- |
|D2|SDO|
|--- |--- |
|D3|SII|
|--- |--- |
|D4|SDI|
|--- |--- |
|D5|Vcc control|
|--- |--- |

## Software

For linking the project need to link libusb-1.0.

If you want to use the application as a non root user copy the file _rules/99-ch341-gpio.rules_ to the _/etc/udev/rules.d/_ directory.

If you need to change the pin assignments in hardware, you also need to change them in _sofware/ch341i.cpp_:
```C
//CH341-HVSP pinout
#define  RST  D0// Output !RESET from npn transistor to Pin 1 (PB5) on ATTiny85
#define  CLKOUT  D1    // Connect to Serial Clock Input (SCI) Pin 2 (PB3) on ATTiny85
#define  DATAIN  D2    // Connect to Serial Data Output (SDO) Pin 7 (PB2) on ATTiny85
#define  INSTOUT D3    // Connect to Serial Instruction Input (SII) Pin 6 (PB1) on ATTiny85
#define  DATAOUT D4     // Connect to Serial Data Input (SDI) Pin 5  (PB0) on ATTiny85
#define  VCC     D5     // Connect to !VCC through pnp transistor
```

# Usage
The syntax is similar to _avrdude_:
* _-e_: erases the chip.
* _-U_: memory operation with the same syntax as _avrdude_

Only intel hex format is allowed by now.

# License
This application is under GPL v3, and greater, license. A copy of it is included.
