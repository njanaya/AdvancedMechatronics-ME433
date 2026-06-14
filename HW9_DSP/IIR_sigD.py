import csv
import matplotlib.pyplot as plt
import numpy as np

filename = 'sigD.csv'

A = 0.75
B = 0.25

# Read CSV
t = []
data = []

with open(filename) as f:
    reader = csv.reader(f)
    for row in reader:
        t.append(float(row[0]))
        data.append(float(row[1]))

# Sample rate
dt = t[1] - t[0]
Fs = 1.0 / dt

# IIR low-pass filter
filtered = []

filtered.append(data[0])

for i in range(1, len(data)):
    new_average = A * filtered[i-1] + B * data[i]
    filtered.append(new_average)

# FFT of unfiltered data
y = np.array(data)
n = len(y)
k = np.arange(n)
T = n / Fs

frq = k / T
frq = frq[range(int(n/2))]

Y = np.fft.fft(y) / n
Y = Y[range(int(n/2))]

# FFT of filtered data
yf = np.array(filtered)

Yf = np.fft.fft(yf) / n
Yf = Yf[range(int(n/2))]

# Plot signal and FFT
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 7))

ax1.plot(t, data, 'k', label='Unfiltered')
ax1.plot(t, filtered, 'r', label='Filtered')
ax1.set_xlabel('Time [s]')
ax1.set_ylabel('Amplitude')
ax1.set_title(filename + ' IIR Filter, A = ' + str(A) + ', B = ' + str(B))
ax1.grid(True)
ax1.legend()

ax2.loglog(frq, abs(Y), 'k', label='Unfiltered FFT')
ax2.loglog(frq, abs(Yf), 'r', label='Filtered FFT')
ax2.set_xlabel('Frequency [Hz]')
ax2.set_ylabel('|Y(freq)|')
ax2.set_title(filename + ' FFT Before and After IIR')
ax2.grid(True)
ax2.legend()

plt.tight_layout()
plt.show()