# BigFileTopK
100GB url 文件，使用 1GB 内存计算出出现次数 top100 的 url 和出现的次数。

## 设计思路
假设一个特定的硬件环境：单机+单核+1G。
如果是多核，可以考虑把数据按照核数等分，然后多线程执行，然后汇总结果。

### 1. 拆分文件
把大文件分成若干小文件，并保持相同url在同一个文件中。每个url放在哪个文件是通过hash算出来的，我们假设拆分后的大部分文件在500M~1G之间，按照平均500M一个文件的大小，100G/500M计算出文件个数(不是1G因为内存只有1G，需要确保大部分文件都不超过1G)。某些情况下部分url重复较多，会超过1G, 这种情况，可以按照同样的方法再进行拆分成更小的文件，极限情况下一个URL重复都超过1G，做特殊处理无需再拆分。这里没有细化后面两种特殊情况，只是考虑相对正常的情况，思路是相同。

### 2. 统计分析
- 使用hash_map，统计每一个文件中重复URL次数；
- 为每一个hash_map创建对应的一个最小堆，堆大小保持topk+1大小，保持topk+1是为了预防删除了最小的但是在topk范围url，最终结果只取topk。后一次生成的堆和前一次的堆进行合并，保持一个最大统计的堆。

## 实现
可以直接看文件：topk.cpp

```
/*
 * 100GB url 文件，使用 1GB 内存计算出出现次数 top100 的 url 和出现的次数。
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include<string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <vector>
#include <hash_map>
#include <queue>

// <url, count>
typedef __gnu_cxx::hash_map<std::string, unsigned long long> UrlHashMap;
// <url, count>
typedef std::pair<std::string, unsigned long long> UrlPair;
typedef std::vector<UrlPair> UrlVector;

struct url_cmp {
    bool operator()(const UrlPair &a, const UrlPair &b) {
         return a.second > b.second;
    }
};

typedef std::priority_queue<UrlPair, UrlVector, url_cmp> UrlMinHeap;

static bool get_line(unsigned char* input_addr, unsigned long long &offset, std::string &url);
static void split_big_file();
static void statistics_url();

class SplitFile {
public:
	SplitFile() : _mmap_ptr(NULL), _offset(0), _split_size(0) {
	
	}
	~SplitFile() {
		munmap(_mmap_ptr, _split_size);
	}

	bool open(const std::string &path, const unsigned long long size);
	bool readLine(std::string &line);
	bool writeLine(const std::string &line);
	bool reset();

private:
	std::string _path;
	unsigned char* _mmap_ptr;
	unsigned long long _offset;
	unsigned long long _split_size;
};

typedef std::map<unsigned int, SplitFile*> FileMap;

typedef struct
{
	std::string big_file;
	unsigned long long file_size;
	unsigned int topk;
	unsigned long long split_size;
	FileMap split_file;
	UrlMinHeap url_heap;
} GlobalInfo;

GlobalInfo global_info;

namespace __gnu_cxx 
{  
    template<> struct hash<const std::string> {  
        size_t operator()(const std::string& s) const { 
        	return hash<const char*>()(s.c_str());
        }  
    };

    template<> struct hash<std::string> {
        size_t operator()(const std::string& s) const {
        	return hash<const char*>()(s.c_str());
        }
    };  
}

static bool get_line(unsigned char* input_addr, unsigned long long &offset, std::string &url) {
	unsigned long long beg_offset = offset;

	// on linux oneline end with '\n'
	while (offset < global_info.file_size) {
		if (*(input_addr + offset) == '\n') {
			break;
		}

		offset++;
	}

	url.assign((const char*)input_addr, offset - beg_offset);

	// skip '\n'
	offset++;

	if (beg_offset + 1 == offset)
		return false;

	return true;
}

bool SplitFile::open(const std::string &path, const unsigned long long size) {
    int fd = open(path.c_str(), O_CREAT|O_RDWR|O_TRUNC);
    if (fd == -1) {
    	std::cout << path << " open failed." << std::endl;
    	return false;
	}

    _mmap_ptr = (unsigned char*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);

	_path = path;
	_split_size = size;

	return true;		
}

bool SplitFile::readLine(std::string &line) {
	if (_mmap_ptr == NULL) {
		return false;
	}

	return get_line(_mmap_ptr, _offset, line);
}

bool SplitFile::writeLine(const std::string &line) {
	if (_mmap_ptr == NULL) {
		return false;
	}

	memcpy(_mmap_ptr, line.c_str(), line.length());
	_offset += line.length();
	_mmap_ptr[_offset++] = '\n';
}

bool SplitFile::reset() {
	_offset = 0;
}

/*
 * 把大文件分成若干小文件，并保持相同url在同一个文件中。
 */
static void split_big_file() {
	// IO优化，使用mmap映射文件
	// mmap打开文件，可以大大提高IO效率，减少内核态到用户态的一次copy。
    int fd = open(global_info.big_file.c_str(), O_RDONLY);
    if (fd == -1) {
    	std::cout << global_info.big_file << " isn't exist." << std::endl;
		exit(1);
	}

	struct stat file_info;
	fstat(fd, &file_info);
	global_info.file_size = file_info.st_size;

    unsigned char* map_ptr = (unsigned char*)mmap(NULL, global_info.file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	// 假设平均每个文件500M，计算出文件个数，即使部分url重复较多，
	// 也不太容易超过1G, 最大文件1G。 
	// 如果url重复的分布差距较大，可能会导致部分文件超过1G，
	// 这种情况，可以按照同样的方法再进行拆分成更小的文件，
	// 这里没有细化，只是考虑相对比较平均的情况。
	unsigned int count = global_info.file_size / 1024 * 1024 * 1024 + 1;
	global_info.split_size = 1024 * 1024 * 1024;

	unsigned long long offset = 0;
	std::string url;

	while (get_line(map_ptr, offset, url)) {
		unsigned int file_no = std::hash<const char*>()(url.c_str()) % count;
		std::string path = std::string("temp_") + std::to_string(file_no);
		SplitFile *file = NULL;

		FileMap::iterator iter = global_info.split_file.find(file_no);
		if (iter == global_info.split_file.end()) {
			file = new SplitFile();
			if (!file->open(path, global_info.split_size)) {
				exit(1);				
			}

			global_info.split_file[file_no] = file;
		} else {
			file = iter->second;
		}

		file->writeLine(url);
	}

	// 解除映射区域
	munmap(map_ptr, global_info.file_size);

	// reset for read
	FileMap::iterator iter = global_info.split_file.begin();
	for (; iter != global_info.split_file.end(); iter++) {
		iter->second->reset();
	}
}

static void merge_heap(UrlMinHeap *final_url_heap, UrlMinHeap *new_url_heap) {
	while (!new_url_heap->empty()) {
		if (final_url_heap->size() > global_info.topk) {
			final_url_heap->pop();
		}

		final_url_heap->push(new_url_heap->top());
		new_url_heap->pop();
	}
}

/*
 * 1. 使用hash_map，统计每一个文件中重复URL次数
 * 2. 为每一个hash_map，创建对应的一个最小堆，堆大小保持topk+1大小
 *    保持topk+1是为了预防删除了一个有效的topk范围内统计，最终结果只取topk。
 * 3. 后一次生成的堆和前一次的堆进行合并，保持一个最大统计的堆。
 */
static void statistics_url() {
	UrlMinHeap *final_url_heap = &global_info.url_heap;
	UrlMinHeap new_url_heap;
	UrlMinHeap *curr_url_heap = &new_url_heap;
	
	FileMap::iterator iter = global_info.split_file.begin();
	for (; iter != global_info.split_file.end(); iter++) {
		SplitFile *file = iter->second;
		std::string url;
		UrlHashMap url_map;

		while (file->readLine(url)) {
			UrlHashMap::iterator iter = url_map.find(url);
			if (iter != url_map.end()) {
				iter->second++;
			} else {
				url_map[url] = 1;
			}
		}

		if (iter == global_info.split_file.begin()) {
			curr_url_heap = final_url_heap;
		}

		for (UrlHashMap::iterator iter = url_map.begin(); iter != url_map.end(); iter++) {
			if (curr_url_heap->size() > global_info.topk) {
				curr_url_heap->pop();
			}

			curr_url_heap->push(std::make_pair(iter->first, iter->second));
		}

		if (iter != global_info.split_file.begin()) {
			merge_heap(final_url_heap, curr_url_heap);
		}
	}
}
/*
 * 设计思路：假设一个特定的硬件环境：单机+单核+1G
 * 如果是多核，可以考虑把数据按照核数等分，然后多线程执行，然后汇总结果。
 */
int main(int argc,char **argv) {
	// 获取文件路径
	if (argc != 3) {
		std::cout << "Usage: topk big_file_path top_num" << std::endl;
		exit(1);
	}

	global_info.big_file = std::string(argv[1]);
	global_info.topk = std::stol(argv[2]);

	// 拆分文件
	split_big_file();

	// 分别统计文件，并归并到一个最小堆里
	statistics_url();

	// 释放文件
	for (FileMap::iterator iter = global_info.split_file.begin();
		iter != global_info.split_file.end(); iter++) {
		delete iter->second;
	}	

	// 输出 topk
	UrlVector topk_vec;
	UrlMinHeap &topk_heap = global_info.url_heap;
	while (!topk_heap.empty()) {
		topk_vec.push_back(topk_heap.top());
		topk_heap.pop();
	}

	reverse(topk_vec.begin(), topk_vec.end());

	for (UrlVector::iterator iter = topk_vec.begin(); iter != topk_vec.end() - 1; iter++) {
		std::cout << iter->second << ", url : " << iter->first << std::endl;
	}

	return 0;
}

```
