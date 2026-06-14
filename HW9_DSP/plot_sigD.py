# This code reads the signal data from sigD.csv and plots it using matplotlib.
# The sequence is:
# Import libraries
# Read CSV into t and data
# Create the figure
# Plot the data
# Add grid
# Add labels and title
# Display the plot
import csv
import matplotlib.pyplot as plt

# Read the CSV file
t = []
data = []

with open('sigD.csv') as f:
    reader = csv.reader(f)

    for row in reader:
        t.append(float(row[0]))
        data.append(float(row[1]))

# Plot the signal
#initial code to plot the signal
# plt.plot(t, data)
# plt.xlabel('Time [s]')
# plt.ylabel('Signal')
# plt.title('sigD Signal vs Time')
# plt.show()

#new way to plot the signal with grid and figure size
plt.figure(figsize=(10,4))
plt.plot(t, data)
plt.grid(True)

plt.xlabel('Time [s]')
plt.ylabel('Amplitude')
plt.title('sigD Signal vs Time')

plt.show()
