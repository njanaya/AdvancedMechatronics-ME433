import csv

t = []
data = []

with open('sigB.csv') as f:
    reader = csv.reader(f)

    for row in reader:
        t.append(float(row[0]))
        data.append(float(row[1]))

num_samples = len(t)
total_time = t[-1]

Fs = num_samples / total_time
print("sigB Sample Rate Calculation")
print("Number of samples =", num_samples)
print("Total time =", total_time, "seconds")
print("Last time stamp Sample rate =", Fs, "Hz")

dt = t[1] - t[0]
Fs = 1.0 / dt

print("dt =", dt)
print("DeltaSample rate =", Fs, "Hz")