import numpy as np
import matplotlib.pyplot as plt

# 数据
operations = ['Insert', 'Search', 'Update', 'Delete']
b_tree_times = [848000, 171000, 651000, 183000]
lr_tree_times = [824000, 144000, 613000, 174000]
fool_tree_times = [852000, 173000, 662000, 260000]
hash_tree_times = [776000, 159000, 663000, 244000]

bar_width = 0.2
index = np.arange(len(operations))

# 绘图
plt.bar(index, b_tree_times, bar_width, label='B Tree')
plt.bar(index + bar_width, lr_tree_times, bar_width, label='LR Tree')
plt.bar(index + 2*bar_width, fool_tree_times, bar_width, label='Fool Tree')
plt.bar(index + 3*bar_width, hash_tree_times, bar_width, label='Hash Tree')

plt.xlabel('Operations')
plt.ylabel('Time (microseconds)')
plt.title('Performance of Different Trees')
plt.xticks(index + 3*bar_width / 2, operations)
plt.legend()

plt.show()
