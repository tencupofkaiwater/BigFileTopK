# BigFileTopK
> 2019-3-29 应PingCap Shirley雪邀请。

100GB url 文件，使用 1GB 内存计算出出现次数 top100 的 url 和出现的次数。

## 设计思路
1. Linux下使用mmap映射待分析的URL文件。
2. 循环逐行读入URL。
3. 对读入的URL做hash_map统计。
4. 当读入的文件大小超过输入指定的每次读入的长度后，结束这次hash_map统计。
5. 如果是第一个分片，看下一步。如果不是第一个分片，那么topk_vec有内容，把它和当前的hash_map合并统计，不过这里不是++，而是+n，合并后清空topk_vec。
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
- 1G URL文件， top10：
```
[root@localhost BigFileTopK]# ./topk url 10 1000
TOP1 146880 http://www.1ting.com/album_30703.html
TOP2 146880 http://v.pps.tv/play_323QCL.html
TOP3 146880 http://www.aipai.com/c1/PTk4NiAhbiFoLSc.html
TOP4 146880 http://v.ifeng.com/special/jiaodianzhiji/maliqiang/index.shtml#8694d4be-0f8e-48fd-ab50-d23944b87d42
TOP5 146880 http://www.56.com/w55/play_album-aid-3497210_vid-OTk2NjMwOA.html
TOP6 146880 http://news.replays.net/page/20090113/1410454.html
TOP7 146880 http://tv.tom.com/App_User_Video.php%3Fvideo_id=2386
TOP8 146880 http://v.v1.cn/vodplayer/content/303392.shtml
TOP9 146880 http://www.joy.cn/sport/videoSport?resourceId=60236937
TOP10 146880 http://movie.mtime.com/80706/trailer/22259.html
The run time is:4.66s

[root@localhost BigFileTopK]# ./topk url 10 500
TOP1 146880 http://www.1ting.com/album_30703.html
TOP2 146880 http://v.pps.tv/play_323QCL.html
TOP3 146880 http://www.aipai.com/c1/PTk4NiAhbiFoLSc.html
TOP4 146880 http://v.ifeng.com/special/jiaodianzhiji/maliqiang/index.shtml#8694d4be-0f8e-48fd-ab50-d23944b87d42
TOP5 146880 http://www.56.com/w55/play_album-aid-3497210_vid-OTk2NjMwOA.html
TOP6 146880 http://news.replays.net/page/20090113/1410454.html
TOP7 146880 http://tv.tom.com/App_User_Video.php%3Fvideo_id=2386
TOP8 146880 http://v.v1.cn/vodplayer/content/303392.shtml
TOP9 146880 http://www.joy.cn/sport/videoSport?resourceId=60236937
TOP10 146880 http://movie.mtime.com/80706/trailer/22259.html
The run time is:4.99s

[root@localhost BigFileTopK]# ./topk url 10 50
TOP1 146880 http://www.1ting.com/album_30703.html
TOP2 146880 http://v.pps.tv/play_323QCL.html
TOP3 146880 http://www.aipai.com/c1/PTk4NiAhbiFoLSc.html
TOP4 146880 http://v.ifeng.com/special/jiaodianzhiji/maliqiang/index.shtml#8694d4be-0f8e-48fd-ab50-d23944b87d42
TOP5 146880 http://www.56.com/w55/play_album-aid-3497210_vid-OTk2NjMwOA.html
TOP6 146880 http://news.replays.net/page/20090113/1410454.html
TOP7 146880 http://tv.tom.com/App_User_Video.php%3Fvideo_id=2386
TOP8 146880 http://v.v1.cn/vodplayer/content/303392.shtml
TOP9 146880 http://www.joy.cn/sport/videoSport?resourceId=60236937
TOP10 146880 http://movie.mtime.com/80706/trailer/22259.html
The run time is:4.86s

[root@localhost BigFileTopK]# ./topk url 10 1
TOP1 99535 http://tv.esl.eu/de/vod/view/21184
TOP2 99089 http://news.replays.net/page/20090113/1410454.html
TOP3 96269 http://weibo.com/p/230444275371ca2a5d0b4799dfc23ab3a98d61
TOP4 66140 http://search.cctv.com/playVideo.php?qtext=%E7%9B%B8%E5%A3%B0%E5%A4%A7%E8%B5%9B&pid=p25822&title=%E7%AC%AC%E4%B8%83%E5%B1%8A%E5%85%A8%E5%9B%BD%E7%94%B5%E8%A7%86%E7%9B%B8%E5%A3%B0%E5%A4%A7%E8%B5%9B
TOP5 42393 http://movie.mtime.com/80706/trailer/22259.html
TOP6 39869 http://v.v1.cn/vodplayer/content/303392.shtml
TOP7 38237 http://podcast.tvscn.com/podcast/podcastplay-335459.htm
TOP8 22207 http://www.joy.cn/sport/videoSport?resourceId=60236937
TOP9 10928 http://www.jsbc.com/v/c/ws/rj/201001/t20100126_38292.shtml
TOP10 6327 http://tv.tom.com/App_User_Video.php%3Fvideo_id=2386
The run time is:4.99s
```

10G URL文件， top10：
```
[root@localhost BigFileTopK]# ./topk url 10 500
TOP1 587520 http://www.1ting.com/album_30703.html
TOP2 587520 http://v.pps.tv/play_323QCL.html
TOP3 587520 http://www.aipai.com/c1/PTk4NiAhbiFoLSc.html
TOP4 587520 http://v.ifeng.com/special/jiaodianzhiji/maliqiang/index.shtml#8694d4be-0f8e-48fd-ab50-d23944b87d42
TOP5 587520 http://www.56.com/w55/play_album-aid-3497210_vid-OTk2NjMwOA.html
TOP6 587520 http://news.replays.net/page/20090113/1410454.html
TOP7 587520 http://tv.tom.com/App_User_Video.php%3Fvideo_id=2386
TOP8 587520 http://v.v1.cn/vodplayer/content/303392.shtml
TOP9 587520 http://www.joy.cn/sport/videoSport?resourceId=60236937
TOP10 587520 http://movie.mtime.com/80706/trailer/22259.html
The run time is:21.75s
```
