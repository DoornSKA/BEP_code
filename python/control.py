import socket
import requests
import matplotlib.pyplot as plt
import csv
from time import sleep
from data_accuracy import plot_data as p_data

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

def read_chunked_response(sock):
    response = b""

    while True:
        # Read the chunk size
        chunk_size_str = b""
        while True:
            char = sock.recv(1)
            if char == b"\r":
                # Expecting '\n' next
                sock.recv(1)  # Read '\n'
                break
            chunk_size_str += char

        # Convert chunk size from hex to int
        chunk_size = int(chunk_size_str.decode('utf-8'), 16)
        if chunk_size == 0:
            break  # End of chunks

        # Read the chunk data
        print(chunk_size)
        chunk_data = sock.recv(chunk_size)
        response += chunk_data

        # Read the trailing \r\n after the chunk
        sock.recv(2)

    return response

def chunked_response():
    # Example usage:
    host = '192.168.4.1'
    # path = '/read_test_2'
    path = '/test_read'

    # Create a socket connection
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, 80))

    # Send an HTTP GET request
    request = f"GET {path} HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n"
    sock.sendall(request.encode('utf-8'))

    # Read the response headers
    headers = b""
    while True:
        header = sock.recv(1)
        headers += header
        if headers.endswith(b"\r\n\r\n"):
            break

    # Read the chunked response body
    body = read_chunked_response(sock)

    # Close the socket
    sock.close()

    # Print the response (headers + body)
    print('headers:')
    print(headers.decode('utf-8'))
    # print(body.decode('utf-8')) # misschien ascii

    chunk_size = 4
    integer_list = []

    data = body.decode('ascii')
    for i in range(0, len(data), chunk_size):
        chunk = data[i:i + chunk_size]
        integer_list.append(int(chunk))

    with open("test_data.csv", 'w', newline='') as f:
        wr = csv.writer(f, quoting=csv.QUOTE_ALL)
        wr.writerow(integer_list)


    # print(integer_list)
    plt.plot(integer_list)
    plt.show()
    # plt.savefig(r"./graphs/image.png")


def main():
    c = Control()
    # Voltage1 = input("Enter voltage 1 [V]: ")
    # Voltage2 = input("Enter voltage 2 [V]: ")
    # Interpulse = input("Enter Interpulse duration [us]: ")
    # c.set_values(Voltage1, Voltage2, Interpulse)
    # sleep(2)
    chunked_response()

    # c.stimulate()
    # sleep(2)
    # c.get_data(True)

if __name__ == '__main__':
    main()
