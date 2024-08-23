import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import norm

# 定义正态分布的参数
mu = 0
sigma = 1e7

# 创建正态分布对象
dist = norm(mu, sigma)

# 计算分位数，这些分位数将把分布分为10个相等概率的部分
quantiles = [dist.ppf(q / 10) for q in range(1, 10)]

# 定义绘图范围
x_min = dist.ppf(0.01)  # 留出一些余地
x_max = dist.ppf(0.99)
x = np.linspace(x_min, x_max, 1000)

# 定义颜色列表
colors = plt.cm.tab10(np.linspace(0, 1, 10))

# 绘制概率密度函数
plt.figure(figsize=(12, 6))
plt.plot(x, dist.pdf(x), label='Probability Density Function', color='black')

# 填充各个区域
for i in range(len(quantiles) + 1):
    if i == 0:
        left_bound = x_min
    else:
        left_bound = quantiles[i - 1]
        
    if i == len(quantiles):
        right_bound = x_max
    else:
        right_bound = quantiles[i]
    
    plt.fill_between(x[(x >= left_bound) & (x <= right_bound)], 
                     dist.pdf(x[(x >= left_bound) & (x <= right_bound)]),
                     color=colors[i], alpha=0.5)

# 添加图例
plt.legend()

# 设置轴标签
plt.xlabel('x')
plt.ylabel('Density')

# 显示图形
plt.show() 