#include "../inc/utility.h"

int kv_node_compare(const void *a, const void *b, void *udata) {
    const struct KV_Node *kv_a = a;
    const struct KV_Node *kv_b = b;
    if (kv_a->key < kv_b->key)
        return 1;
    else if (kv_a->key > kv_b->key)
        return -1;
    return 0;
}

int gauss_rand_integer() {
    // Box-Muller方法产生正态分布随机整数
    static double U, V, y;
    static int phase = 0;
    double z;
    if (phase == 0) {
        U = rand() / (RAND_MAX + 1.0);
        V = rand() / (RAND_MAX + 1.0);
        z = sqrt(-2.0 * log(U)) * sin(2.0 * PI * V);
    } else {
        z = sqrt(-2.0 * log(U)) * cos(2.0 * PI * V);
    }
    phase = 1 - phase;
    y = RAND_SIGMA * z + RAND_MEAN;
    int result = (int)y;
    // 产生(INT_MIN, INT_MAX)之间的值
    if (result > INT_MIN && result < INT_MAX) {
        return result;
    }
    return gauss_rand_integer();
}

void quick_sort(int *arr, int l, int r) {
    if (l >= r)
        return;
    int mid = arr[(l + r) >> 1], i = l - 1, j = r + 1;
    while (i < j) {
        do
            i++;
        while (arr[i] < mid);
        do
            j--;
        while (arr[j] > mid);
        if (i < j) {
            int tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
        }
    }
    quick_sort(arr, l, j), quick_sort(arr, j + 1, r);
}

int *generate_sorted_arr(int n) {
    int *arr = malloc(n * sizeof(int));
    int arr_len = 0; // 初始的已填充的数字数量
    int *h = malloc((n + 10) * sizeof(int)); // 初始化哈希表和链表结构
    int *e = malloc(2 * n * sizeof(int));
    int *ne = malloc(2 * n * sizeof(int));
    const int mod = n + 3;
    int idx = 0;
    // memset(h, -1, sizeof h);
    for (int i = 0; i < n + 10; i++) {
        h[i] = -1;
    }
    while (arr_len < n) {
        int x = gauss_rand_integer(); // 随机产生一个整数
        int u = (x % mod + mod) % mod;
        bool flag = false;
        for (int i = h[u]; i != -1; i = ne[i]) {
            if (e[i] == x) {
                flag = true;
                break;
            }
        }
        if (!flag) { // 未找到则向哈希表中插入该数字并更新arr
            arr[arr_len++] = x;
            e[idx] = x;
            ne[idx] = h[u];
            h[u] = idx++;
        }
    }
    free(h);
    free(e);
    free(ne);
    quick_sort(arr, 0, n - 1); // 进行快速排序返回有序数组
    return arr;
}

void statistic_feature(int *arr, int n, double *avg, double *sigma) {
    double sum = 0.0;        // 总和
    double square_sum = 0.0; // 平方和

    for (int i = 0; i < n; i++) {
        sum += arr[i];
        square_sum += pow(arr[i], 2);
    }
    *avg = sum / n;
    *sigma = sqrt((square_sum - n * pow(*avg, 2)) / (n - 1));
}

void print_kv_node(const KV_Node *node) {
    if (node == NULL) {
        printf("<-----要打印的节点不存在----->\n");
        return;
    }
    printf("Key: %d   Value: %s\n", node->key, node->str);
}

double normal_distribution(double mean, double sigma, double x) {
    double tmp1 = exp(-pow((x - mean), 2.0) / (2.0 * sigma * sigma));
    double tmp2 = (sigma * sqrt(2.0 * PI));
    printf("tmp1: %lf, tmp2: %lf, tmp1 / tmp2: %lf", tmp1, tmp2, tmp1 / tmp2);
    return exp(-pow((x - mean), 2.0) / (2.0 * sigma * sigma)) /
           (sigma * sqrt(2.0 * PI));
}

double normal_cdf(double mean, double sigma, double x) {
    return 0.5 * (1.0 + erf((x - mean) / (sigma * sqrt(2))));
}

double normal_icdf(double mean, double sigma, double y) {
    assert(y >= 0.0 && y <= 1.0);
    // 开始二分查找
    double left = mean - 10.0 * sigma;  // 设置搜索的左边界
    double right = mean + 10.0 * sigma; // 设置搜索的右边界
    while (right - left > EPSILON) {
        double mid = (left + right) / 2.0;
        double cdf_val = normal_cdf(mean, sigma, mid);
        if (cdf_val > y) {
            right = mid;
        } else {
            left = mid;
        }
    }
    return (right + left) / 2.0;
}

void linear_fitting(double mean, double sigma, int left, int right, double *k,
                    double *b, int base) {
    // 取出一定的点数用于拟合CDF
    int point_num = right - left;
    point_num = (point_num > 500) ? 500 : point_num; // 取最多500点拟合
    double *x_val = (double *)malloc(point_num * sizeof(double));
    double *y_val = (double *)malloc(point_num * sizeof(double));
    double x = (double)left;
    double step = (double)(right - left) / (double)point_num;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    // 在取出CDF函数值的同时基于base进行归一化, 重映射到[0, base - 1]之间
    double y0 = normal_cdf(mean, sigma, (double)left);
    double y1 = normal_cdf(mean, sigma, (double)right);
    // double y2 = 0.0;
    double y3 = base - 1.0;
    for (int i = 0; i < point_num; i++) {
        x_val[i] = x;
        y_val[i] = normal_cdf(mean, sigma, x);
        y_val[i] = (y_val[i] - y0) * y3 / (y1 - y0);
        x += step;
        sum_x += x_val[i];
        sum_y += y_val[i];
        sum_xy += x_val[i] * y_val[i];
        sum_xx += x_val[i] * x_val[i];
    }
    // printf("本次拟合的首尾坐标: [%lf, %lf] [%lf, %lf]\n", x_val[0], y_val[0],
    // x_val[point_num - 1], y_val[point_num - 1]);
    double denominator = point_num * sum_xx - sum_x * sum_x;
    assert(denominator != 0.0);
    *k = (point_num * sum_xy - sum_x * sum_y) / denominator;
    *b = (sum_y - (*k) * sum_x) / point_num;
    free(x_val);
    free(y_val);
}

char **generate_str(int n, int *arr) {
    char **str = (char **)malloc(n * sizeof(char *));
    for (int i = 0; i < n; i++) {
        // 动态分配能够容纳字符串所需长度的内存
        int bufsize = 100; // 初始大小
        str[i] = (char *)malloc(bufsize * sizeof(char));
        // 构造字符串
        int length = snprintf(str[i], bufsize, "This is num %d!", arr[i]);
        // 检查是否溢出
        if (length >= bufsize) {
            bufsize = length + 1; // 为字符串分配新的大小
            str[i] = (char *)realloc(str[i], bufsize * sizeof(char)); // 重新分配内存
            snprintf(str[i], bufsize, "This is num %d!", arr[i]); // 重新构造字符串
        }
    }
    return str;
}

void data_free(int n, int *arr, char **str) {
    free(arr);
    arr = NULL;
    for (int i = 0; i < n; i++) {
        free(str[i]);
        str[i] = NULL;
    }
    free(str);
    str = NULL;
}