import requests
import matplotlib.pyplot as plt
from time import sleep

class Control():
    def __init__(self, ip_address="192.168.4.1"):
        self.ip_address = ip_address

    def set_values(self, Voltage1, Voltage2, Interpulse):
        status = requests.get(f"http://{self.ip_address}/submit?Voltage1={Voltage1}&Voltage2={Voltage2}&Interpulse={Interpulse}").status_code
        if status < 400:
            print("Submit command succesfully sent")
        else:
            print("Error occured sending command: status code " + status)

    def stimulate(self):
        status = requests.get(f"http://{self.ip_address}/stimulate").status_code
        if status < 400:
            print("Stimulation command succesfully sent")
        else:
            print("Error occured sending command: status code " + status)

    def get_data(self, make_plot=True):
        data = requests.get(f"http://{self.ip_address}/get_data").content.decode('ASCII')
        print(data)
        if make_plot is True:
            plt.plot(data.split(','))
            # plt.show()
            plt.savefig(r"./graphs/image.png")

def main():
    c = Control()
    Voltage1 = input("Enter voltage 1 [V]: ")
    Voltage2 = input("Enter voltage 2 [V]: ")
    Interpulse = input("Enter Interpulse duration [us]: ")
    c.set_values(Voltage1, Voltage2, Interpulse)
    sleep(2)
    c.stimulate()
    sleep(2)
    c.get_data(True)

if __name__ == '__main__':
    main()
