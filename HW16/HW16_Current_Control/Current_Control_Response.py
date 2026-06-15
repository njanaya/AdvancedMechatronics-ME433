import serial
import matplotlib.pyplot as plt

PORT = "COM9"
BAUD = 115200

ser = serial.Serial(PORT, BAUD)
ser.reset_input_buffer()

t = []
target = []
actual = []

plt.ion()
fig, ax = plt.subplots()

while True:
    try:
        line = ser.readline().decode().strip()

        vals = line.split(',')
        if len(vals) != 3:
            continue

        t_ms = float(vals[0])
        target_deg = float(vals[1])
        actual_deg = float(vals[2])

        t.append(t_ms/1000.0)
        target.append(target_deg)
        actual.append(actual_deg)

        if len(t) > 500:
            t.pop(0)
            target.pop(0)
            actual.pop(0)

        ax.clear()
        ax.plot(t, target, label="Target Angle")
        ax.plot(t, actual, label="Actual Angle")
        ax.set_xlabel("Time (s)")
        ax.set_ylabel("Angle (deg)")
        ax.legend()
        plt.pause(0.01)

    except Exception:
        pass