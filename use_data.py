import csv
import matplotlib.pyplot as plt

def main():
    with open("test_data.csv", newline='') as f:
        reader = csv.reader(f)
        data_s = list(reader)

    data = [int(e) for e in data_s[0]]
    print(data)

    plt.plot(data)
    plt.show()

if __name__ == '__main__':
    main()
