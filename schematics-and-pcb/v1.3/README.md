
# Schematic v1.3 Changes Breakdown
This README.md contains a the changes since the last version of the schematic + Details about the PCB Layout.

- Added two bypass capacitors in parallel with the buck converter input and output capacitors.

# PCB Layout Notes

- 4 Layer PCB with the first inner layer being the GND plane and the second being the ESP_3V3 plane
- D+ and D- differential signal pair traces hand routed and matched to within 0.05 of a mm (+2 Vias)
- Power and GND lines are thicker
- No signal lines beneath the buck converter inductor
- Pins connected to the power planes via thermal pads