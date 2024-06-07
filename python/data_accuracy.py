from numpy.fft import fft, ifft
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import csv

class plot_data():
    def __init__(self, f_pass, f_stop):
        self.f_sample = 8000
        self.f_pass = f_pass
        self.f_stop = f_stop
        self.fs = 0.5
        self.ADC_RESOLUTION = 4095
        self.VRef = 1.114
        self.a, self.b = self.create_filter()

    def create_filter(self):
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

    def filter_data(self, data):
        response = signal.filtfilt(self.b, self.a, data)
        return response
    
    def results_to_voltage(self, data, response):
        voltages_data = [reading / self.ADC_RESOLUTION * self.VRef for reading in data]
        voltages_response = [reading / self.ADC_RESOLUTION * self.VRef for reading in response]
        return voltages_data, voltages_response
    
    def compare_data(self, data, response):
        average_filter = sum(response)/len(response)
        average_orig = sum(data)/len(data)

        difference_filter = sum([abs(average_filter - response[i])/self.ADC_RESOLUTION for i in range(len(response))])/len(response)
        difference_orig = sum([abs(average_orig - data[i])/self.ADC_RESOLUTION for i in range(len(data))])/len(data)

        print(f"average_filter: {average_filter}\n average_orig: {average_orig}\n difference_filter: {difference_filter}\n difference_orig: {difference_orig}")
    
    @staticmethod
    def get_data():
        with open("test_data.csv", newline='') as f:
            reader = csv.reader(f)
            data_s = list(reader)

        data = [int(e) for e in data_s[0]]
        return data
    
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

    @staticmethod
    def plot_data(data, response):
        plt.plot(data)
        plt.plot(response)
        plt.show()

if __name__ == '__main__':
    f = plot_data(500,1500)
    data = f.get_data()
    f.plot_data(data, f.filter_data(data))
    v_data, v_response = f.results_to_voltage(data, f.filter_data(data))
    f.plot_data(v_data, v_response)