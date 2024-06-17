from numpy.fft import fft, ifft
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import csv
from time import time as t
import os

# This class is used to filter data and plot the results
class plot_data():
    def __init__(self, f_pass, f_stop, filter_type):
        self.f_sample = 8000
        self.f_pass = f_pass
        self.f_stop = f_stop
        self.fs = 0.5
        self.ADC_RESOLUTION = 4095
        self.VRef = 3.08 # 3.15 - 0.07
        self.rootpath = "C:\\Users\\mgroe\\Coding\\Github\\BEP_code\\python\\"
        if filter_type == "butter":
            self.a, self.b = self.create_butter()
        elif filter_type == "ellip":
            self.a, self.b = self.create_ellip()
        else:
            raise ValueError("Invalid filter type")

    # This function creates a Butterworth filter
    def create_butter(self):
        wp = self.f_pass/(self.f_sample/2)
        ws = self.f_stop/(self.f_sample/2)
        g_pass = 0.5
        g_stop = 40

        # Td = len(data)/self.f_sample
        # omega_p = (2/Td)*np.tan(self.wp/2)
        # omega_s = (2/Td)*np.tan(self.ws/2)

        N, Wn = signal.buttord(wp, ws, g_pass, g_stop)
        print("Order of the Filter=", N)
        print("Cut-off frequency= {:.3f} rad/s ".format(Wn))
        b, a = signal.butter(N, Wn, 'low')
        return a, b
    
    # This function creates an Elliptic filter
    def create_ellip(self):
        wp = self.f_pass / (self.f_sample / 2)
        ws = self.f_stop / (self.f_sample / 2)
        g_pass = 0.5
        g_stop = 40

        N, Wn = signal.ellipord(wp, ws, g_pass, g_stop)
        print("Order of the Filter=", N)
        print("Normalized passband frequency= {:.3f} rad/s ".format(Wn))

        b, a = signal.ellip(N, g_pass, g_stop, Wn, 'low')
        return a, b

    # This function filters the data
    def filter_data(self, data):
        response = signal.filtfilt(self.b, self.a, data)
        return response

    # This function converts the data to voltage
    def results_to_voltage(self, data):
        voltages_data = [reading / self.ADC_RESOLUTION * self.VRef for reading in data]
        return voltages_data

    # This function compares the data
    def compare_data(self, data, response):
        average_filter = sum(response)/len(response)
        average_orig = sum(data)/len(data)

        difference_filter = sum([abs(average_filter - response[i])/self.ADC_RESOLUTION for i in range(len(response))])/len(response)
        difference_orig = sum([abs(average_orig - data[i])/self.ADC_RESOLUTION for i in range(len(data))])/len(data)

        print(f"average_filter: {average_filter}\n average_orig: {average_orig}\n difference_filter: {difference_filter}\n difference_orig: {difference_orig}")

    # This function gets the data from a file
    def get_data(self, file):
        with open(os.path.join(self.rootpath, file), newline='') as f:
            reader = csv.reader(f)
            data_s = list(reader)

        data = [int(e) for e in data_s[0]]
        return data

    # This function plots the magnitude response
    def plot_magnitude_response(self):
        w, h = signal.freqz(self.b, self.a, worN=8000)
        frequencies = w * self.f_sample / (2*np.pi)
        magnitude = 20 * np.log10(abs(h))
        plt.plot(frequencies, magnitude, 'b')
        plt.title('Frequency Response')
        plt.xlabel('Frequency [Hz]')
        plt.ylabel('Magnitude [dB]')
        plt.grid(True)
        plt.show()

    def measure_error(self, data):
        mean = sum(data) / len(data)
        mean_error = sum([abs(x - mean) for x in data]) / len(data)
        mean_square_error = sum([(x - mean_error) ** 2 for x in data]) / len(data)
        print(f"Mean: {mean}")
        print(f"Mean Error: {mean_error}")
        print(f"Mean Square Error: {mean_square_error}")

    # This function plots the data
    @staticmethod
    def plot_data(data, response):
        time_array = [i/8000 for i in range(len(data))]
        plt.plot(time_array, data)
        plt.plot(time_array, response)
        plt.legend(["Data", "Sine wave"])
        plt.title("Data and Sine wave")
        plt.ylabel("Voltage [V]")
        plt.xlabel("Time [s]")
        plt.show()


if __name__ == '__main__':
    f = plot_data(500,1500, "butter") # f_pass, f_stop, filter_type
    start_time = t() # If you want to measure the time it takes to run the code, comment out plt.show() and uncomment the print statement
    data = f.get_data("DC_mono_test.csv") # file
    response = f.filter_data(data)
    # f.plot_data(data, f.filter_data(data)) # data, response

    # frequency = 200       # Frequency of the sine wave in Hz
    # sample_rate = 8000  # Sample rate in samples per second (Hz)
    # duration = 640/8000       # Duration of the sine wave in seconds
    # t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False)
    # sine_wave = 4096/2 * np.sin(2 * np.pi * frequency * t + np.pi - 0.05) + 4096/2
    # sine_wave_list = sine_wave.tolist()
    # v_data, v_response = f.results_to_voltage(data), f.results_to_voltage(sine_wave) # data, sine_wave

    v_data, v_response = f.results_to_voltage(data), f.results_to_voltage(response) # data, response
    f.measure_error(v_data)
    f.measure_error(v_response)
    
    f.plot_data(v_data, v_response) # data, response (In voltage)
    stop_time = t() - start_time
    # print(f"Time: {stop_time}")
    # magnitude_response = f.plot_magnitude_response() # plot magnitude response
    