import matplotlib.pyplot as plt
import numpy as np
data = np.loadtxt('benchmark_results.txt')
plt.figure(figsize=(10, 6))
plt.scatter(data[:,0], data[:,1])
plt.xlabel('File Size (bytes)')
plt.ylabel('Execution Time (seconds)')
plt.title('Client Performance: File Size vs Execution Time')
plt.grid(True)
plt.savefig('benchmark_plot.png')
plt.show()