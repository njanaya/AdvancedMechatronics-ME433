import serial
import time
import numpy as np
import matplotlib.pyplot as plt

PORT = "COM9"   # Change this
BAUD = 115200
NUM_SAMPLES = 1000

ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(2)

# Clear anything already sitting in the serial buffer
ser.reset_input_buffer()

# Send the number of samples to the Pico
ser.write(f"{NUM_SAMPLES}\n".encode())

time_ms = []
raw = []
filtered = []

print("Reading data...")
while len(time_ms) < NUM_SAMPLES:
    line = ser.readline().decode("utf-8", errors="ignore").strip()

    if not line:
        continue

    print(line)

    # Skip header or prompt lines
    if line.startswith("Enter") or line.startswith("time") or "," not in line:
        continue

    parts = line.split(",")

    if len(parts) == 3:
        try:
            t = float(parts[0])
            r = float(parts[1])
            f = float(parts[2])

            time_ms.append(t)
            raw.append(r)
            filtered.append(f)

        except ValueError:
            pass

ser.close()

time_ms = np.array(time_ms)
raw = np.array(raw)
filtered = np.array(filtered)

time_s = (time_ms - time_ms[0]) / 1000.0

dt = np.mean(np.diff(time_s))
fs = 1.0 / dt

print(f"Collected {len(raw)} samples")
print(f"Sample rate: {fs:.2f} Hz")

# Plot raw and filtered data
plt.figure()
plt.plot(time_s, raw, label="Raw")
plt.plot(time_s, filtered, label="IIR filtered")
plt.xlabel("Time (s)")
plt.ylabel("HX711 value")
plt.title("Force Sensor Data")
plt.legend()
plt.grid(True)
plt.show()

# FFT of raw signal
raw_centered = raw - np.mean(raw)

fft_values = np.fft.rfft(raw_centered)
fft_freqs = np.fft.rfftfreq(len(raw_centered), d=dt)
fft_mag = np.abs(fft_values)

dominant_index = np.argmax(fft_mag[1:]) + 1
dominant_freq = fft_freqs[dominant_index]

print(f"Dominant frequency: {dominant_freq:.2f} Hz")

plt.figure()
plt.plot(fft_freqs, fft_mag)
plt.xlabel("Frequency (Hz)")
plt.ylabel("Magnitude")
plt.title("FFT of Raw Force Sensor Data")
plt.grid(True)
plt.show()