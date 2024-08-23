import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import norm
from scipy.optimize import curve_fit

# 定义正态分布的参数
mu = 0
sigma = 1e7

# 创建正态分布对象
dist = norm(mu, sigma)

# 定义绘图范围
x_min = dist.ppf(0.001)  # 留出一些余地
x_max = dist.ppf(0.999)
x = np.linspace(x_min, x_max, 1000)

# 计算CDF
cdf = dist.cdf(x)

# 选取CDF的一部分进行线性拟合
# 我们选取CDF的中间部分，例如从0.1到0.3
x_fit = x[(cdf >= 0.1) & (cdf <= 0.3)]
y_fit = cdf[(cdf >= 0.1) & (cdf <= 0.3)]

# 定义线性拟合函数
def linear(x, a, b):
    return a * x + b

# 进行线性拟合
params, _ = curve_fit(linear, x_fit, y_fit)

# 使用拟合参数计算拟合直线
fit_line = params[0] * x_fit + params[1]

# 绘制CDF和拟合直线
plt.figure(figsize=(12, 6))
plt.plot(x, cdf, label='CDF', color='blue')
plt.plot(x_fit, fit_line, label='Linear Fit', linestyle='--', color='red')

# 美化图像
plt.title('CDF of Normal Distribution and Linear Fit')
plt.xlabel('x')
plt.ylabel('Cumulative Probability')
plt.legend()
plt.grid(True)
plt.xlim(x_min, x_max)
plt.ylim(0, 1)
plt.tight_layout()

# 显示图形
plt.show()