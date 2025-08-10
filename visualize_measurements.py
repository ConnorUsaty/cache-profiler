""" Gets the most recent .csv from measurements/ and creates a plot to visualize it """

from datetime import datetime
import matplotlib.pyplot as plt
import os


def get_most_recent_csv():
    csv_paths = [r"measurements/" + f for f in os.listdir(os.path.relpath(r"measurements/"))]
    most_recent = max(csv_paths, key = lambda x : os.path.getctime(x))
    return most_recent

def parse_csv(csv_path):
    data = [ [], [] ]
    with open(csv_path, 'r') as f:
        lines = f.read().splitlines()[1:]
        for l in lines:
            test_size, median_latency, _ = l.split(',')
            # this will already be in ascending order
            data[0].append(int(test_size))
            data[1].append(float(median_latency))
    return data

def plot_data(data, out_dir):
    if not os.path.exists(out_dir):
        os.mkdir(out_dir)

    timestamp = datetime.now().strftime("%d_%m_%Y-%H_%M_%S")
    out_path = out_dir + "graph_" + timestamp + ".png"
    
    x_min, x_max = min(data[0]), max(data[0])
    y_min, y_max = min(data[1]), max(data[1])

    plt.figure()
    plt.xlim(x_min, x_max)
    plt.ylim(y_min, y_max*1.05)
    plt.xlabel("Test Size (KB)")
    plt.ylabel("Median Latency (ns)")
    plt.title("Cache Profiler Results")
    plt.plot(data[0], data[1], marker='o')
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()

def main():
    out_dir = r"graphs/"

    csv_path = get_most_recent_csv()
    data = parse_csv(csv_path)
    plot_data(data, out_dir)


if __name__ == "__main__":
    main()
