# Schematic v1.0 Detailed Breakdown
This README.md contains a detailed breakdown of each labeled block
within the schematic given in the PDF (Schematic v1.0).

## Table of Contents

[ESP32-S3-WROOM1-N4 SoM Config](#esp32-s3-wroom1-n4-som-config)
- [ESP32 MCU](#esp32-mcu)
- [Reset Switch, Boot/Flash Switch, RGB Led, Battery Connector](#reset-switch-bootflash-switch-rgb-led-battery-connector)
- [USB Connector](#usb-connector)
- [LDO Regulator](#ldo-regulator)
- [Buck Converter](#buck-converter)
- [USB2UART](#usb2uart)

[Sensor & Motor Connectors](#sensor--motor-connectors)
- [ToF Sensor Connectors & Mux](#tof-sensor-connectors--mux)  
- [IR Tx/Rx Connectors & Mux](#ir-txrx-connectors--mux)  
- [Stepper Motor Driver Connectors](#stepper-motor-driver-connectors)

[Changes](#changes)

## ESP32-S3-WROOM1-N4 SoM Config

The blocks on the left side of the schematic are all part of the ESP32
SoM configuration. Most of these blocks are either based on or copied
from the schematic of the **ESP32-S3-DevKitC-1** development board by
Espressif.

### ESP32 MCU

The "brains" of the project is the **ESP32-S3-WROOM1** System on Module
chip, which provides the bare MCU chip + the necessities such as the
oscillator and the RF module within the SoM.

The schematic features two decoupling capacitors that are physically
laid out near the 3.3v pin of the SoM as well as a 0Ohm resistor that
may or may not be used to jump over a trace within the PCB layout, which
I haven't started yet.

- 10uF bulk capacitor to handle voltage sags.

- 0.1uF bypass capacitor to handle higher frequency noise.

Pin descriptions:

- Reset & Boot/Flash

  - The EN Pin enables the SoM when pulled high. The RESET net goes to
    the reset switch

  - The GPIO0 configures the boot mode of the MCU, when high, the ESP32
    boots from the flash memory, when low, it is in download mode. It
    has a "weak pullup" on the inside of the MCU, as stated by the
    datasheet.

- U0RXD & U0TXD

  - These are nets that come from the USB2UART Module into the ESP32,
    they are the pins that are used for programming the MCU in download
    mode.

  - The ESP32-S3 chip also has a USB interface within it which would
    allow you to program it without a USB2UART chip, but I decided to go
    with the USB2UART anyways since according to sources on the
    internet, it is more reliable (as well as the fact that I had a
    schematic from the Espressif devboard)

- SDA/SCL

  - I2C Pins used for controlling the I2C Multiplexer that connects to
    the ToF distance sensors used by the robot.

- Step1-3 & Dir 1-3

  - Pins connected to the connectors that go to the stepper motor
    drivers which drive the stepper motors of the robots.

- RGB

  - Pin that controls the external RGB LED connected to the SoM, which
    will be used for early programming/debugging tests & as a status
    LED.

- IR Mux S1-3, IR Tx, IR Rx

  - IR Mux S1-3 are pins used to address the specific IR modules
    connected to the analog multiplexers within the same block.

  - IR Tx is the pin connected to the IR LEDs, driven using the ESP32
    Remote Control Peripheral (RMT)

  - IR Rx is the pin connected to the IR Receivers, driven using the
    ESP32 Remote Control Peripheral (RMT)

### Reset Switch, Boot/Flash Switch, RGB Led, Battery Connector

Mostly copied from the Espressif schematic.

- The reset switch circuit features a 10KOhm pullup resistor connected
  to the 3.3v terminal of the ESP, a button that shorts it to ground,
  and a 0.1uF capacitor that acts as a debouncer.

- The Boot/Flash switch circuit is identical to the reset switch
  circuit, just without the pullup resistor, since it features one
  internally as per the datasheet.

- The **XL-3535RGBC-WS2812B** RGB LED was added to be used as a) an easy
  way to test whether the schematic of the MCU works and b) a status
  LED. It has 3 pins, namely the power pins and a control signal pin.

- The circuit will be fed by either 2 or 3 LiPo cells (Not decided yet),
  which will be connected to this connector.

### USB Connector

Copied from the Espressif schematic.

A Micro-USB connector that has its standard 4 pins: VBUS, GND, D+ and
D-.

The VBUS, D+, and D- pins all have transient voltage suppressor (TVS)
diodes connecting them to ground to protect the USB source from
transient voltage events, as well as to help signal integrity.

The TVS Diodes are **LESD5D5.0CT1G** diodes which offers a 5V reverse
standoff voltage, 5.6V breakdown voltage, and 18.6V.

The VBUS pin has a **1N5819HW-7-F** Schottky barrier diode connecting it
to the input of the LDO regulator, with a 10uF decoupling capacitor at
the negative end of the diode as an extra transient voltage event
protector. As well as this, there is a net leaving VBUS that goes to the
USB2UART block.

### LDO Regulator

The **SGM2212-3.3XKC3G/TR** LDO regulator can handle input voltages from
2.7V to 3.3V. This chip has a fixed output of 3.3V with a maximum of
610mV dropout voltage, making it a good fit to turn 5V into 3V.

The circuit features 10uF and 0.1uF decoupling capacitors on each end of
the LDO as well as an extra TVS diode on the input to protect the
regulator from transient voltage events related to driving the stepper
motors (since they will be powered by the battery that eventually ends
up at the regulator)

This TVS is the **SMBJ12A** which has a clamping voltage of 19.9V (Just
below the max rated voltage of the LDO), a 13.3V breakdown voltage and a
12V reverse stand-off voltage.

The circuit also features a power-on LED.

### Buck Converter

The buck converter is used to take the battery voltage and step it down
to 5Vs which is later fed into the LDO regulator. The reason that the
battery isn't fed straight into the LDO is that the buck converter has a
much higher efficiency (the LDO keeps a constant voltage on its ends,
dissipating the excess energy as heat).

The circuit uses an **MP1584EN** buck converter chip. The circuit is
copied from the Typical Applications section of the **MP1584EN**
datasheet.

### USB2UART

The circuit uses the **CP2102N-A02-GQFN28** USB2UART Chip. This block is
mostly copied from the Espressif schematic, minus the DTR and RTS
signals, which are used for the auto programming feature on the
development board (which I don't use).

## Sensor & Motor Connectors

The sensor and motor modules are separated from the PCB for ease of
component swapping. They are instead placed as connectors on the PCB,
which are multiplexed because we don't have enough pins to control so
many sensors.

### ToF Sensor Connectors & Mux

For the Time-of-Flight sensors, I use a **TCA9548****A** I2C Multiplexer
breakout board, which makes it possible to control up to 8 I2C devices
with the same address at once (In this case only 6 outputs are used).

It has a configurable I2C address (A0-2 pins), with which you can
connect up to 8 of these multiplexers at once, in this case these pins
are pulled low since we use only 1 mux.

It's reset pin is pulled up and the SDA/SCL pins are connected to the
sensor connectors, to which the power lines are also connected to.

The ToF sensors have software configurable I2C addresses as well, but
they require you to hold all the other sensors in reset state while you
configure them, which requires you to use the XSHUT and GPIO1 pins of
the sensor, which makes it much simpler to use a multiplexer instead of
configuring the I2C addresses on each boot.

### IR Tx/Rx Connectors & Mux

Each IR Module consists of an **IR-333** LED and a **TSOP38238** IR
receiver. In total the module is driven by 2 power pins, an Rx pin and a
Tx pin, which are organized into a single 4-pin connector per module.

The connectors are multiplexed using 2 **CD74HC4051PWR** analog
multiplexers, one for the receivers, and one for the LEDs. The address
pins of the multiplexers are connected, since each output corresponds to
a single IR module.

Each multiplexer also features a decoupling capacitor at its VCC line,
as well as an active low enable pin which is pulled down.

### Stepper Motor Driver Connectors

For the stepper motors driving the robots, I use the **A4988** stepper
motor drivers, which are separate from the PCB.

The PCB itself offers 4-pin connectors which give the driver its logic
level power, the Dir signal, and the Step signal.

The power for the motors will come straight from the battery, as it's
better not to pass high amounts of current through PCB traces.

# Changes

# Schematic v1.1 Changes Breakdown
This README.md contains a the changes since the last version of the schematic.

## IR Tx/Rx Connectors & Mux

For this version, the Multiplexers for the IR Transceiver modules were changed from the **CD74HC4051-EP Analog Multiplexer and Demultiplexer** to the **SN74CB3Q3251 1-OF-8 FET MULTIPLEXER/DEMULTIPLEXER**.  
The reason this was done was to reduce the input resistance and parasitic capacitance introduced by the multiplexer.

-   **ON-State resistance** for the **SN74CB3Q3251** is **4 Ohms**, as opposed to about **50 Ohms** of the **CD74HC4051-EP**, which means that the signal will be attenuated much less.
-   The **SN74CB3Q3251** has a typical **input capacitance of 4pF**, as opposed to about **12pF** of the **CD74HC4051-EP**, which means that the propagation delay will be smaller, even if it can be neglected at the frequency of the **NEC IR protocol (38kHz carrier)**.
-   Both the **SN74CB3Q3251** and the **CD74HC4051-EP** are bidirectional, meaning they can handle signals in both directions, suitable for the IR transceiver setup.


# Schematic v1.2 Changes Breakdown
This README.md contains a the changes since the last version of the schematic.

- Added 190 Ohm resistors to IR LED connectors
- Changed all of the SMT connectors to THT connectors, as well as buttons.
- Corrected buck converter feedback resistor mistake and changed inductance value to match datasheet typical application.


# Schematic v1.3 Changes Breakdown
This README.md contains a the changes since the last version of the schematic + Details about the PCB Layout.

- Added two bypass capacitors in parallel with the buck converter input and output capacitors.

# PCB Layout Notes

- 4 Layer PCB with the first inner layer being the GND plane and the second being the ESP_3V3 plane
- D+ and D- differential signal pair traces hand routed and matched to within 0.05 of a mm (+2 Vias)
- Power and GND lines are thicker
- No signal lines beneath the buck converter inductor
- Pins connected to the power planes via thermal pads


# v1.4 Changes Breakdown
This README.md contains a the changes since the last version of the schematic and PCB Layout.

## Schematic

- Added a 4-Pin connector that connects to pins IO12,IO13,IO14, and IO21 of th ESP32 SoM, in case they are needed later.

# PCB Layout

- Added vias beneath the thermal pads of the USB2UART and Buck Converter chips for better heat transfer.

# v1.5 Changes Breakdown
This README.md contains a the changes since the last version of the schematic and PCB Layout.

## Schematic

- Fixed UART Tx/Rx mismatch mistake
- Moved around some traces and added labels for the buck converter cap voltage ratings

# PCB Layout

- Added PCB Art