# HW17_Force_Enc_Py

This project combines:

- AS5600 magnetic encoder angle reading over I2C
- HX711 force sensor raw reading
- IIR filtered force value
- CSV streaming over USB serial
- Python graphics showing encoder angle and force history

## Pico wiring

| Device | Signal | Pico pin |
|---|---:|---:|
| AS5600 | SDA | GP8 |
| AS5600 | SCL | GP9 |
| HX711 | SCK | GP2 |
| HX711 | DT | GP3 |

## Serial output format

```csv
time_ms,angle_deg,force_raw,force_filtered
```

## Python

Install:

```bash
pip install pyserial matplotlib
```

Run, replacing `COM9` with your Pico COM port:

```bash
python hw17_graphics.py COM9
```
