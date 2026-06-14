import csv
import matplotlib.pyplot as plt
import numpy as np

filename = 'sigA.csv'
X = 100   # number of points to average

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

# Moving average filter
filtered = []

for i in range(len(data)):
    if i < X:
        filtered.append(data[i])  # leave early points mostly unchanged
    else:
        avg = sum(data[i-X:i]) / X
        filtered.append(avg)

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

# Time plot
ax1.plot(t, data, 'k', label='Unfiltered')
ax1.plot(t, filtered, 'r', label='Filtered')
ax1.set_xlabel('Time [s]')
ax1.set_ylabel('Amplitude')
ax1.set_title(filename + ' Moving Average, X = ' + str(X))
ax1.grid(True)
ax1.legend()

# FFT plot
ax2.loglog(frq, abs(Y), 'k', label='Unfiltered FFT')
ax2.loglog(frq, abs(Yf), 'r', label='Filtered FFT')
ax2.set_xlabel('Frequency [Hz]')
ax2.set_ylabel('|Y(freq)|')
ax2.set_title(filename + ' FFT Before and After Moving Average')
ax2.grid(True)
ax2.legend()

plt.tight_layout()
plt.show()