import numpy as np
import matplotlib.pyplot as plt

# 读取文件
with open('S:\\Workspace\\VSCode\\RMI_v2\\data\\num.txt', 'r') as file:
    numbers = [int(line.strip()) for line in file]

# 计算数据的范围和集中趋势
data_range = max(numbers) - min(numbers)
mean_value = np.mean(numbers)
std_value = np.std(numbers)

# 绘制直方图
bin_width = 5000
bins = np.arange(min(numbers), max(numbers) + bin_width, bin_width)
plt.hist(numbers, bins=bins, edgecolor='black', color='skyblue')

# 添加标注
plt.axvline(mean_value, color='r', linestyle='--', linewidth=2, label='Mean')
plt.axvline(mean_value - std_value, color='orange', linestyle='--', linewidth=2, label='Mean - Std')
plt.axvline(mean_value + std_value, color='orange', linestyle='--', linewidth=2, label='Mean + Std')
plt.xlabel('Value')
plt.ylabel('Frequency')
plt.title('Distribution of Integers')

# 根据数据范围设置x轴范围
plt.xlim(min(numbers), max(numbers))

# 显示图例
plt.legend()

# 展示图
plt.show()
