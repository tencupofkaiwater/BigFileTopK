# BigFileTopK
> 2019-3-29 应PingCap Shirley雪邀请。

100GB url 文件，使用 1GB 内存计算出出现次数 top100 的 url 和出现的次数。

## 设计思路
1. Linux下使用mmap映射待分析的URL文件。
2. 循环逐行读入URL。
3. 对读入的URL做hash_map统计。
4. 当读入的文件大小超过输入指定的每次读入的长度后，结束这次hash_map统计。
5. 如果是第一个分片，看下一步。如果不是第一个分片，那么topk_vec有内容，把它和当前的hash_map合并统计，不过这里不是++，而是+n。
6. 把hash_map统计的结果建立大小为topk+1的最小堆，统计URL出现的次数为比较键值。
7. 把最小堆导出到一个vector容器中，假设叫topk_vec，也是升序。
8. 如果文件内容没有读完，跳到第2步。否则结束循环。
9. 对topk_vec逆序，使之降序排列。
10. 输出topk_vec。

假设一个特定的硬件环境：单机+单核+1G。如果是多核，可以考虑把数据按照核数等分，然后多线程执行，然后汇总结果。

### 编译(Linux x86-64)
```
# g++ topk.cpp -O3 -Wno-deprecated -std=c++11 -o topk
```
### 执行
Usage: topk url_file top_num once_size(MB)
假设输入的URL文件名为url, 统计前100个, 一次读出500M。
```
# ./topk url 100 500
```

### 测试结果
