# Phase Box main board (KiCad)

A two-layer carrier board for a Raspberry Pi Pico module, sized for a Hammond **1590BB** enclosure
(landscape, 119.5 × 94 mm). It follows the pinout in [`include/hal/pinout.hpp`](../../include/hal/pinout.hpp).

| File | What it is |
| :--- | :--- |
| `phase-box.kicad_pro` | KiCad 10 project (open this) |
| `phase-box.kicad_sch` | Schematic |
| `phase-box.kicad_pcb` | Board |
| `phase-box-schematic.pdf` | Schematic as a PDF |
| `render-top.png`, `render-bottom.png` | 3D renders of both sides |
| `fab/phase-box-gerbers.zip` | Gerbers + Excellon drill files, ready to upload to a fab |
| `fab/phase-box-bom.csv`, `fab/phase-box-pos.csv` | Bill of materials and pick-and-place positions |
| `fab/assembly-top.pdf`, `fab/assembly-bottom.pdf` | Assembly drawings (reference designators) |

Status: the schematic passes ERC with no violations. The board is fully routed (Freerouting 2.5.0)
with GND pours on both layers, and passes DRC with no violations, no unconnected items and no
schematic parity issues.

Design rules: 0.2 mm clearance, 0.25 mm signal tracks, 0.4 mm power tracks, 0.6/0.3 mm vias. Minimum
track width is 0.15 mm because the router narrows a few tracks to 0.19 mm between Pico pins. Standard
2-layer fab processes (0.127 mm minimum) handle this.

## What is on the board

- **U1, Raspberry Pi Pico** (on the back of the board). BOOTSEL and the USB socket face the open lid side. The board
  is powered from the Pico's USB.
- **ENC1**: Alps EC11E encoder with push switch (value, page and save). **ENC2**: EC11E without switch
  (expression).
- **J5, OLED**: a 0.96" 128×64 SSD1306 I²C module, soldered directly on its 4 pins (no socket, to keep it under
  the lid). Pin order **GND VCC SCL SDA**: some modules swap GND and VCC, so check yours. Build the firmware
  with `-DPHASEBOX_OLED_HEIGHT=64`.
- **MIDI out (DIN and TRS)**: a 74LVC2G34 buffers GP0 into one output per jack, so each current loop has its
  own driver. 3.3 V MIDI values: 33 Ω to the source pin, 10 Ω to the sink pin. **SW1** chooses the TRS
  standard: slider toward the "A" end = Type A (tip = sink), "B" end = Type B (tip = source).
- **Input conditioning** on every control line: 10 kΩ pull-up, series resistor and a capacitor at the GPIO.
  Encoders 10 kΩ / 10 nF (Bourns' recommended filter); switches 1 kΩ / 100 nF.
- **GP1** (MIDI in) is not connected. Adding MIDI in needs an optocoupler (6N138 or H11L1).

## Off-board parts and wiring (JST-XH connectors on the back)

| Connector | Pin 1 | Pin 2 | Pin 3 | Goes to |
| :--- | :--- | :--- | :--- | :--- |
| J1 FS1 TAP | signal | GND | | momentary SPST footswitch |
| J2 FS2 MUTATE | signal | GND | | momentary SPST footswitch |
| J3 MIDI DIN | DIN pin 4 (source) | DIN pin 2 (shield) | DIN pin 5 (sink) | panel-mount 5-pin DIN |
| J4 MIDI TRS | tip | ring | sleeve | panel-mount 3.5 mm TRS |

The back silkscreen reads in pad order as seen from the back, where pin 1 (square pad) is on the right:
`DIN 5-2-4` and `S-R-T` above J3 and J4, and `TRS B <-> A` under SW1.

The Pico's micro-USB needs a short panel-mount USB extension, for example USB-C (panel) to micro-USB.
If you use USB-C at the panel and want to power the pedal from USB-C to USB-C chargers, the panel end
needs 5.1 kΩ CC pull-downs (cheap pigtails often leave them out). USB-A to USB-C cables work without them.

## Power and intended setup

The board is powered only through the Pico's USB (no 9 V jack). It draws roughly 40–60 mA: Pico
about 25 mA, OLED 10–20 mA, and up to about 7 mA per MIDI output while sending.

The intended rig puts a **CME H2MIDI Pro** between the Phase Box and the Raspberry Pis:

```
9 V pedalboard supply ──> H2MIDI Pro (DC in, 5.5 × 2.1 mm, centre-negative)
Phase Box USB ──────────> H2MIDI Pro USB-A host port (power + USB-MIDI)
H2MIDI Pro USB-C ───────> Raspberry Pi (USB host)
H2MIDI Pro DIN OUT ─────> Raspberry Pi with a MIDI input
Phase Box DIN/TRS out ──> optional, another device directly
```

- The H2MIDI Pro's USB-A host port supplies up to 5 V / 500 mA in total, well above what the Phase Box
  needs. Its manual says to use a self-powered hub if the devices on that port need more.
- The Phase Box firmware is a class-compliant USB-MIDI device (TinyUSB), which is what that host port
  expects.
- Avoid USB power banks as the only supply: many switch off below 50–100 mA.
- If everything always goes through the H2MIDI Pro over USB, the MIDI out parts (U2, R17–R20, SW1, J3,
  J4) can be left unpopulated on the same board.
- The H2MIDI Pro's routing settings and how each Pi sees it have not been tried with this board.

## Parts list

| Ref | Value | Footprint |
| :--- | :--- | :--- |
| U1 | Raspberry Pi Pico (or Pico 2: same pinout) | Module:RaspberryPi_Pico_Common_THT |
| U2 | 74LVC2G34 | SOT-23-6 |
| ENC1 | Alps EC11E with switch, 20 mm shaft | EC11E-Switch_Vertical_H20mm |
| ENC2 | Alps EC11E, 20 mm shaft | EC11E_Vertical_H20mm |
| SW1 | C&K JS202011CQN DPDT slide | SW_CK_JS202011CQN_DPDT_Straight |
| J1, J2 | JST B2B-XH-A | 2-pin XH vertical |
| J3, J4 | JST B3B-XH-A | 3-pin XH vertical |
| J5 | (OLED module pins) | 1×4 2.54 mm |
| R1, R3, R5, R7, R9, R11, R13 | 10k | 0805 |
| R2, R4, R8, R10 | 10k | 0805 |
| R6, R12, R14 | 1k | 0805 |
| R15, R16 | 4.7k | 0805 |
| R17, R19 | 33 | 0805 |
| R18, R20 | 10 | 0805 |
| C1, C2, C4, C5 | 10nF | 0805 |
| C3, C6, C7, C8, C10 | 100nF | 0805 |
| C9 | 10uF | 0805 |

All passives are 0805, so they can be hand-soldered. Their reference designators are on the fab layer
(F.Fab), not the silkscreen; plot F.Fab for an assembly drawing.

## Panel drilling (enclosure coordinates)

Measured from the lid's top-left corner, landscape, looking at the front:

| Feature | x (mm) | y (mm) |
| :--- | :--- | :--- |
| ENC1 shaft (value) | 92.0 | 34.0 |
| ENC2 shaft (expression) | 59.75 | 72.0 |
| FS1 TAP footswitch | 30.0 | 72.0 |
| FS2 MUTATE footswitch | 89.5 | 72.0 |
| OLED module, header row centre | 34.0 | 19.0 |

The board sits under the lid with its top side facing the lid. The outline is a strip across the top
(y 14–58) with a tab down to the expression encoder, which keeps it clear of both footswitches.

## Not verified

- **Mechanical fit.** The board stand-off height, the OLED window position (it depends on the module) and
  where the jacks go on the wall have not been checked against a real 1590BB. Print the board 1:1 and
  check it against the enclosure before ordering.
- **Nothing has been built or tested.**
