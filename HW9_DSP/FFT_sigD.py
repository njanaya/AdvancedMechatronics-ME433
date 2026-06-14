import csv
import matplotlib.pyplot as plt
import numpy as np

# Pick the file
filename = 'sigD.csv'

# Read CSV
t = []
data = []

with open(filename) as f:
    reader = csv.reader(f)

    for row in reader:
        t.append(float(row[0]))
        data.append(float(row[1]))

# Calculate sample rate
dt = t[1] - t[0] # calculate time step from the first two time points, rather than hardcoding it, in case the time step changes in the future
Fs = 1.0 / dt #calculate sample rate rather than hardcoding it, in case the sample rate changes in the future

print("Sample rate =", Fs, "Hz")

# FFT
y = data # signal data
n = len(y) # length of the signal

k = np.arange(n) # array of sample indices
T = n / Fs # total time duration of the signal, calculated from the number of samples and the sample rate, rather than hardcoding it, in case the duration changes in the future

frq = k / T # two sides frequency range
frq = frq[range(int(n/2))] # one side frequency range

Y = np.fft.fft(y) / n # fft computing and normalization
Y = Y[range(int(n/2))] # one side fft range

# Plot signal and FFT
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 7))

ax1.plot(t, y) # plot the signal data against time
ax1.set_xlabel('Time [s]')
ax1.set_ylabel('Amplitude')
ax1.set_title(filename + ' Signal vs Time')
ax1.grid(True)

ax2.loglog(frq, abs(Y)) # plot the magnitude of the FFT against frequency on a log-log scale, which is useful for visualizing a wide range of frequencies and magnitudes
ax2.set_xlabel('Frequency [Hz]')
ax2.set_ylabel('|Y(freq)|')
ax2.set_title(filename + ' FFT')
ax2.grid(True)

plt.tight_layout()
plt.show()