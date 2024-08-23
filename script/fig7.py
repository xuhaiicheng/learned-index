import pandas as pd
import matplotlib.pyplot as plt

# Create data
data = {
    'Data Size': [500000, 1000000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000, 4500000, 5000000],
    'Insert Time': [500000, 1054000, 1571000, 2343000, 3357000, 4083000, 5500000, 7106000, 9865000, 15383000],
    'Query Time': [57000, 118000, 181000, 290000, 385000, 404000, 459000, 526000, 587000, 682000],
    'Update Time': [485000, 986000, 1564000, 2397000, 3134000, 4029000, 5649000, 7038000, 9876000, 15187000],
    'Delete Time': [60000, 124000, 208000, 353000, 340000, 443000, 581000, 676000, 838000, 942000]
}

# Create DataFrame
df = pd.DataFrame(data)

# Set plot style
plt.style.use('seaborn-darkgrid')

# Create the plot
plt.figure(figsize=(14, 8))

# Plot each operation with different styles
plt.plot(df['Data Size'], df['Insert Time'], marker='o', color='b', linestyle='-', linewidth=2, markersize=8, label='Insert Time')
plt.plot(df['Data Size'], df['Query Time'], marker='s', color='g', linestyle='--', linewidth=2, markersize=8, label='Query Time')
plt.plot(df['Data Size'], df['Update Time'], marker='^', color='r', linestyle='-.', linewidth=2, markersize=8, label='Update Time')
plt.plot(df['Data Size'], df['Delete Time'], marker='D', color='purple', linestyle=':', linewidth=2, markersize=8, label='Delete Time')

# Add legend
plt.legend(loc='upper left')

# Add title and labels
plt.title('LR Tree Operation Times vs. Data Size', fontsize=16, fontweight='bold')
plt.xlabel('Data Size', fontsize=14)
plt.ylabel('Time (Microseconds)', fontsize=14)

# Customize tick parameters
plt.xticks(fontsize=12)
plt.yticks(fontsize=12)

# Add grid
plt.grid(True, linestyle='--', alpha=0.7)

# Show plot
plt.tight_layout()  # Adjust layout to make room for labels
plt.show()
