#ifndef UTILITY_H_
#define UTILITY_H_
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
// ---------------------宏定义--------------------
#define PI 3.14159265358         // 圆周率
#define RAND_MEAN 0              // 随机数均值
#define RAND_SIGMA 10000000.0    // 随机数标准差
#define EPSILON 1e-8             // 误差精度
#define LEFT_EDGE (INT_MIN + 1)  // 范围左边界
#define RIGHT_EDGE (INT_MAX - 1) // 范围右边界

// --------------------结构体定义------------------
// 键值对结构体, 存储int - string关系对
typedef struct KV_Node {
    int key;
    char *str;
} KV_Node;

// ---------------------函数原型-------------------
// B树中KV_Node元素的比较规则
int kv_node_compare(const void *a, const void *b, void *udata);
// 产生均值为RAND_MEAN, 标准差为RAND_SIGMA的随机整数
int gauss_rand_integer(void);
// 快速排序辅助算法
void quick_sort(int *arr, int l, int r);
// 创建服从正态分布的n个不重合的整数
int *generate_sorted_arr(int n);
// 基于样本给出统计特征(均值和方差)
void statistic_feature(int *arr, int n, double *avg, double *sigma);
// 打印一个KV_Node节点中的键值对信息
void print_kv_node(const KV_Node *node);
// 计算正态分布的概率密度函数
double normal_distribution(double mean, double sigma, double x);
// 基于erf函数计算累积分布函数y
double normal_cdf(double mean, double sigma, double x);
// 基于累积分布函数的值y求自变量x
double normal_icdf(double mean, double sigma, double y);
// 对[left, right]段的key值进行线性拟合
void linear_fitting(double mean, double sigma, int left, int right, double *k,
                    double *b, int base);
// 生成n组测试数据
char** generate_str(int n, int* arr);
// 释放n组测试数据内存
void data_free(int n, int *arr, char** str);
#endif // UTILITY_H_