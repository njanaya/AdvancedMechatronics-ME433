# HW17 Python graphics reader
# Reads CSV streamed by the Pico:
# time_ms,angle_deg,force_raw,force_filtered
#
# Install dependencies if needed:
#   pip install pyserial matplotlib
#
# Run:
#   py hw17_graphics.py COM9
# Replace COM9 with your Pico USB serial port.

import sys
import math
from collections import deque

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


PORT = sys.argv[1] if len(sys.argv) > 1 else "COM5"
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=0.05)

times = deque(maxlen=200)
angles = deque(maxlen=200)
forces = deque(maxlen=200)

force_zero = None

fig, (ax_arm, ax_force) = plt.subplots(2, 1, figsize=(7, 7))

def read_latest_lines():
    """Read all currently available serial lines and return the newest valid sample."""
    global force_zero

    newest = None

    while ser.in_waiting:
        line = ser.readline().decode(errors="ignore").strip()

        if not line or line.startswith("time_ms"):
            continue

        parts = line.split(",")
        if len(parts) != 4:
            continue

        try:
            time_ms = int(parts[0])
            angle_deg = float(parts[1])
            force_raw = int(parts[2])
            force_filtered = float(parts[3])
        except ValueError:
            continue

        if force_zero is None:
            force_zero = force_filtered

        newest = (time_ms, angle_deg, force_raw, force_filtered - force_zero)

    return newest

def update(_frame):
    sample = read_latest_lines()

    if sample is not None:
        time_ms, angle_deg, force_raw, force_relative = sample
        times.append(time_ms / 1000.0)
        angles.append(angle_deg)
        forces.append(force_relative)

    ax_arm.clear()
    ax_force.clear()

    # Draw encoder angle as an arm.
    if angles:
        theta = math.radians(angles[-1])

        # Rotate the graphic 90 degrees clockwise
        x = math.sin(theta)
        y = -math.cos(theta)

        ax_arm.plot([0, x], [0, y], linewidth=4)
        ax_arm.scatter([0, x], [0, y])
        ax_arm.text(-1.1, 1.15, f"Angle: {angles[-1]:.1f} deg")
        ax_arm.text(-1.1, 1.00, f"Relative force: {forces[-1]:.0f} counts")

    ax_arm.set_title("HW17 Encoder Arm")
    ax_arm.set_xlim(-1.25, 1.25)
    ax_arm.set_ylim(-1.25, 1.25)
    ax_arm.set_aspect("equal", adjustable="box")
    ax_arm.grid(True)

    # Draw force history.
    if times:
        t0 = times[0]
        plot_times = [t - t0 for t in times]
        ax_force.plot(plot_times, list(forces))

    ax_force.set_title("HX711 Relative Force")
    ax_force.set_xlabel("Time (s)")
    ax_force.set_ylabel("Force sensor counts, zeroed at start")
    ax_force.grid(True)

    plt.tight_layout()

ani = FuncAnimation(fig, update, interval=50)
plt.show()

ser.close()
