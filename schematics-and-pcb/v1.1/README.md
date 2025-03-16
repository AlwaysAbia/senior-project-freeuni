
# Schematic v1.1 Changes Breakdown
This README.md contains a the changes since the last version of the schematic.

## IR Tx/Rx Connectors & Mux

For this version, the Multiplexers for the IR Transceiver modules were changed from the **CD74HC4051-EP Analog Multiplexer and Demultiplexer** to the **SN74CB3Q3251 1-OF-8 FET MULTIPLEXER/DEMULTIPLEXER**.  
The reason this was done was to reduce the input resistance and parasitic capacitance introduced by the multiplexer.

-   **ON-State resistance** for the **SN74CB3Q3251** is **4 Ohms**, as opposed to about **50 Ohms** of the **CD74HC4051-EP**, which means that the signal will be attenuated much less.
-   The **SN74CB3Q3251** has a typical **input capacitance of 4pF**, as opposed to about **12pF** of the **CD74HC4051-EP**, which means that the propagation delay will be smaller, even if it can be neglected at the frequency of the **NEC IR protocol (38kHz carrier)**.
-   Both the **SN74CB3Q3251** and the **CD74HC4051-EP** are bidirectional, meaning they can handle signals in both directions, suitable for the IR transceiver setup.

