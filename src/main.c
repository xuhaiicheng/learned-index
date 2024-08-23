#include "b_tree.c"
#include "fool_tree.c"
#include "hash_tree.c"
#include "lr_tree.c"
#include "utility.c"

int main(int argc, char *argv[]) {
    // -------------------参数设定---------------------
    srand(time(NULL));             // 初始化随机数种子
    int left_range = INT_MIN + 1;  // key值左边界
    int right_range = INT_MAX - 1; // key值右边界

    int n = 1e6;      // 统一划定操作次数
    int n_insert = n; // 删除操作测试次数
    int n_change = n; // 更改操作测试次数
    int n_erase = n;  // 删除操作测试次数
    int n_query = n;  // 查询操作测试次数
    int i = atoi(argv[1]), j = atoi(argv[2]);
    int leaf_num_1 = i; // 线性回归树每个root节点的叶子节点数量
    int leaf_num_2 = j; // 每个叶子节点包含的B树的数量
    int *arr = generate_sorted_arr(n_insert); // 生成n个插入测试用的key值
    double mean, sigma; // 计算出均值和方差估计值
    statistic_feature(arr, n_insert, &mean, &sigma);
    // printf("平均值: %lf  标准差: %lf\n", mean, sigma);
    clock_t start, end; // 每段程序的开始和结束时间点
    LR_Tree_Root *lr_tree = lr_tree_create(mean, sigma, leaf_num_1, leaf_num_2,
                                           left_range, right_range);
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        char *s;
        asprintf(&s, "This is num %d!!!", i);
        lr_tree_insert(lr_tree, arr[i], s);
        free(s);
    }
    for (int i = 0; i < n_query; i++) {
        KV_Node *node = lr_tree_query(lr_tree, arr[i]);
    }
    for (int i = 0; i < n_change; i++) {
        char *s;
        asprintf(&s, "Num %d has been changed!!!", i);
        lr_tree_insert(lr_tree, arr[i], s);
        free(s);
    }
    for (int i = 0; i < n_erase; i++) {
        lr_tree_erase(lr_tree, arr[i]);
    }
    end = clock();
    printf("LR树在叶子节点数量%d, B树子节点数量%d情况下所需总时间: %lf "
           "(微秒)\n",
           leaf_num_1, leaf_num_2,
           (((double)(end - start)) / CLOCKS_PER_SEC * 1000000));
    free(arr);
    lr_tree_free(lr_tree);

    /*
    for (int i = 500000; i <= 5000000; i += 500000) {
        int n = i;        // 统一划定操作次数
        int n_insert = n; // 删除操作测试次数
        int n_change = n; // 更改操作测试次数
        int n_erase = n;  // 删除操作测试次数
        int n_query = n;  // 查询操作测试次数
        int leaf_num_1 = 10; // 线性回归树每个root节点的叶子节点数量
        int leaf_num_2 = 10; // 每个叶子节点包含的B树的数量
        int *arr = generate_sorted_arr(n_insert); // 生成n个插入测试用的key值
        double mean, sigma; // 计算出均值和方差估计值
        statistic_feature(arr, n_insert, &mean, &sigma);
        // printf("平均值: %lf  标准差: %lf\n", mean, sigma);
        clock_t start, end; // 每段程序的开始和结束时间点
        LR_Tree_Root *lr_tree = lr_tree_create(
            mean, sigma, leaf_num_1, leaf_num_2, left_range, right_range);
        start = clock();
        for (int i = 0; i < n_insert; i++) {
            char *s;
            asprintf(&s, "This is num %d!!!", i);
            lr_tree_insert(lr_tree, arr[i], s);
            free(s);
        }
        end = clock();
        printf("LR树插入 %d 次单次时间: %lf (微秒)\n", n_insert,
               (((double)(end - start)) / CLOCKS_PER_SEC * 1000000) /
    (double)n);

        start = clock();
        for (int i = 0; i < n_query; i++) {
            KV_Node* node = lr_tree_query(lr_tree, arr[i]);
        }
        end = clock();
        printf("LR树查询 %d 次单次时间: %lf (微秒)\n", n_insert,
               (((double)(end - start)) / CLOCKS_PER_SEC * 1000000) /
    (double)n);

        start = clock();
        for (int i = 0; i < n_change; i++) {
            char *s;
            asprintf(&s, "Num %d has been changed!!!", i);
            lr_tree_insert(lr_tree, arr[i], s);
            free(s);
        }
        end = clock();
        printf("LR树更改 %d 次单次时间: %lf (微秒)\n", n_insert,
               (((double)(end - start)) / CLOCKS_PER_SEC * 1000000) /
    (double)n);

        start = clock();
        for (int i = 0; i < n_erase; i++) {
            lr_tree_erase(lr_tree, arr[i]);
        }
        end = clock();
        printf("LR树删除 %d 次单次时间: %lf (微秒)\n", n_insert,
               (((double)(end - start)) / CLOCKS_PER_SEC * 1000000) /
    (double)n); free(arr);
    }*/

    /*
    int n = 1e6;      // 统一划定操作次数
    int n_insert = n; // 删除操作测试次数
    int n_change = n; // 更改操作测试次数
    int n_erase = n;  // 删除操作测试次数
    int n_query = n;  // 查询操作测试次数

    int leaf_num_1 = 10; // 线性回归树每个root节点的叶子节点数量
    int leaf_num_2 = 10; // 每个叶子节点包含的B树的数量

    int *arr = generate_sorted_arr(n_insert); // 生成n个插入测试用的key值
    char **str = generate_str(n_insert, arr); // 对应的value值
    double mean, sigma; // 计算出均值和方差估计值
    statistic_feature(arr, n_insert, &mean, &sigma);
    printf("平均值: %lf  标准差: %lf\n", mean, sigma);
    clock_t start, end;  // 每段程序的开始和结束时间点
    printf("插入次数: %d, 更改次数: %d, 删除次数: %d, 查询次数: %d\n", n_insert,
    n_change, n_erase, n_query); FILE *file = fopen("../data/num.txt", "w");

    // -------------------树的构造--------------------
    struct B_Tree *b_tree = b_tree_create();
    LR_Tree_Root *lr_tree = lr_tree_create(mean, sigma, leaf_num_1, leaf_num_2,
                                           left_range, right_range);
    Fool_Tree_Root *fool_tree =
        fool_tree_create(left_range, right_range, leaf_num_1 * leaf_num_2);
    Hash_Tree_Root *hash_tree =
        hash_tree_create(left_range, right_range, leaf_num_1 * leaf_num_2);

    // -------------------插入测试--------------------
    // b tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        b_tree_insert(b_tree, arr[i], str[i]);
    }
    end = clock();
    printf("B树插入 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // lr tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        lr_tree_insert(lr_tree, arr[i], str[i]);
    }
    end = clock();
    printf("LR树插入 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // fool tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        fool_tree_insert(fool_tree, arr[i], str[i]);
    }
    end = clock();
    printf("FOOL树插入 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // hash tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        hash_tree_insert(hash_tree, arr[i], str[i]);
    }
    end = clock();
    printf("HASH树插入 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // -------------------查询测试--------------------
    // b tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        KV_Node* node = b_tree_query(b_tree, arr[i]);
    }
    end = clock();
    printf("B树查询 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // lr tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        KV_Node* node = lr_tree_query(lr_tree, arr[i]);
    }
    end = clock();
    printf("LR树查询 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // fool tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        KV_Node* node = fool_tree_query(fool_tree, arr[i]);
    }
    end = clock();
    printf("FOOL树查询 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // hash tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        KV_Node* node = hash_tree_query(hash_tree, arr[i]);
    }
    end = clock();
    printf("HASH树查询 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // -------------------更改测试--------------------
    // b tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        b_tree_insert(b_tree, arr[i], "This num has been changed!");
    }
    end = clock();
    printf("B树更改 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // lr tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        lr_tree_insert(lr_tree, arr[i], "This num has been changed!");
    }
    end = clock();
    printf("LR树更改 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // fool tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        fool_tree_insert(fool_tree, arr[i], "This num has been changed!");
    }
    end = clock();
    printf("FOOL树更改 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // hash tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        hash_tree_insert(hash_tree, arr[i], "This num has been changed!");
    }
    end = clock();
    printf("HASH树更改 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // -------------------删除测试--------------------
    // b tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        b_tree_erase(b_tree, arr[i]);
    }
    end = clock();
    printf("B树删除 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // lr tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        lr_tree_erase(lr_tree, arr[i]);
    }
    end = clock();
    printf("LR树删除 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // fool tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        fool_tree_erase(fool_tree, arr[i]);
    }
    end = clock();
    printf("FOOL树删除 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // hash tree
    start = clock();
    for (int i = 0; i < n_insert; i++) {
        hash_tree_erase(hash_tree, arr[i]);
    }
    end = clock();
    printf("HASH树删除 %d 次所需时间: %lf (微秒)\n", n_insert, ((double) (end -
    start)) / CLOCKS_PER_SEC * 1000000);

    // -------------------资源释放--------------------
    data_free(n_insert, arr, str);
    b_tree_free(b_tree);
    lr_tree_free(lr_tree);
    fool_tree_free(fool_tree);
    hash_tree_free(hash_tree);
    */
    /*
    int n = 100000;
    int *arr = generate_sorted_arr(n);
    double avg, sigma;
    statistic_feature(arr, n, &avg, &sigma);
    //printf("平均值: %lf  标准差: %lf", avg, sigma);
    free(arr);
    lr_tree_create(avg, sigma, 100, 100, INT_MIN + 1, INT_MAX - 1);
    LR_Tree_Root* lr_tree = lr_tree_create(avg, sigma, 100, 100, INT_MIN + 1,
    INT_MAX - 1); LARGE_INTEGER start, end, frequency;
    // 获取计数器的频率
    QueryPerformanceFrequency(&frequency);
    // 获取开始时间
    QueryPerformanceCounter(&start);
    for(int i = 0; i < n; i ++){
        char* s;
        asprintf(&s, "This is num %d!!!", i);
        lr_tree_insert(lr_tree, i, s);
        free(s);
    }
    // 获取结束时间
    QueryPerformanceCounter(&end);
    // 计算时间差
    double elapsed = (double)(end.QuadPart - start.QuadPart) /
    frequency.QuadPart;
    // 将秒转换为微秒
    double elapsedMicroseconds = elapsed * 1000000.0;
    printf("Elapsed time: %.6f microseconds\n", elapsedMicroseconds);


    struct B_Tree* b_tree = b_tree_create();
    // 获取开始时间
    QueryPerformanceCounter(&start);
    for(int i = 0; i < n; i ++){
        char* s;
        asprintf(&s, "This is num %d!!!", i);
        b_tree_insert(b_tree, i, s);
        free(s);
    }
    // 获取结束时间
    QueryPerformanceCounter(&end);
    // 计算时间差
    elapsed = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    // 将秒转换为微秒
    elapsedMicroseconds = elapsed * 1000000.0;
    printf("Elapsed time: %.6f microseconds\n", elapsedMicroseconds);
    //print_LR_Tree_Root(lr_tree, 1145);
    //lr_tree_erase(lr_tree, 1145);
    //print_LR_Tree_Root(lr_tree, 1145);
    */
    return 0;
}