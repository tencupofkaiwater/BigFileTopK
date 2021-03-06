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

unsigned long long url_file_size;

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

	// end file
	if (offset >= url_file_size)
		return false;	

	// on linux oneline end with '\n', windows '\r\n'
	while (offset < url_file_size) {
		if (*(input_addr + offset) == '\n' || *(input_addr + offset) == '\r') {
			break;
		}

		offset++;
	}

	// skip begin blank space
	while (*(input_addr + beg_offset) == ' ') {
		beg_offset++;
	}

	// skip end blank space
	unsigned long long end_offset = offset;
	while (*(input_addr + (end_offset - 1)) == ' ') {
		end_offset--;
	}

	url.assign((const char*)(input_addr + beg_offset), end_offset - beg_offset);

	// skip '\r'
	if (*(input_addr + offset) == '\r')
		offset++;

	// skip '\n'
	offset++;

	return true;
}

static unsigned char* open_file(const std::string &path) {
	// IO优化，使用mmap映射文件
	// mmap打开文件，可以大大提高IO效率，减少内核态到用户态的一次copy。
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
    	std::cout << path << " isn't exist." << std::endl;
		exit(1);
	}

	struct stat file_info;
	fstat(fd, &file_info);
	url_file_size = file_info.st_size;

    unsigned char* map_ptr = (unsigned char*)mmap(NULL, url_file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_ptr == MAP_FAILED) {
    	std::cout << "mmap fail." << std::endl;
    	close(fd);
    	exit(1);
    }
     
	close(fd);

	return map_ptr;
}

/*
 * 执行步骤：
 * 1. Linux下使用mmap映射待分析的URL文件。
 * 2. 循环逐行读入URL。
 * 3. 对读入的URL做hash_map统计。
 * 4. 当读入的文件大小超过输入指定的每次读入的长度后，结束这次hash_map统计。
 * 5. 如果是第一个分片，看下一步。如果不是第一个分片，那么topk_vec有内容，把它和当前的hash_map合并统计，不过这里不是++，而是+n，合并后清空topk_vec。
 * 6. 把hash_map统计的结果建立大小为topk+1的最小堆，统计URL出现的次数为比较键值。
 * 7. 把最小堆导出到一个vector容器中，假设叫topk_vec，也是升序。 
 * 8. 如果文件内容没有读完，跳到第2步。否则结束循环。
 * 9. 对topk_vec逆序，使之降序排列。 
 * 10. 输出topk_vec。
 */
int main(int argc,char **argv) {
	if (argc != 4) {
		std::cout << "Usage: topk url_file top_num once_size(MB)" << std::endl;
		exit(1);
	}

	std::string url_file = std::string(argv[1]);
	unsigned int topk = std::stol(argv[2]);
	unsigned long long once_size = std::stol(argv[3]) * 1024 * 1024;

	unsigned char* map_ptr = open_file(url_file);
	unsigned long long offset = 0;
	
	bool skip_out = true;
	UrlVector topk_vec;
	// 预设长度，长度固定为topk + 1
	topk_vec.reserve(topk + 1);
	UrlMinHeap url_heap;

	while (1) {
		skip_out = true;
		UrlHashMap url_map;
		UrlHashMap::iterator hiter;
		std::string url;
		
		while (get_line(map_ptr, offset, url)) {
			if (url.empty())
				continue;

			hiter = url_map.find(url);
			if (hiter != url_map.end()) {
				hiter->second++;
			} else {
				url_map[url] = 1;
			}

			if (offset % once_size == 0) {
				skip_out = false;
				break;			
			}
		}

		if (!topk_vec.empty()) {
			for (UrlVector::iterator viter = topk_vec.begin(); viter != topk_vec.end() - 1; viter++) {
				hiter = url_map.find(viter->first);
				if (hiter != url_map.end()) {
					hiter->second += viter->second;
				}			
			}

			topk_vec.clear();
		}

		for (hiter = url_map.begin(); hiter != url_map.end(); hiter++) {
			if (url_heap.size() > topk) {
				url_heap.pop();
			}

			url_heap.push(std::make_pair(hiter->first, hiter->second));
		}

		while (!url_heap.empty()) {
			topk_vec.push_back(url_heap.top());
			url_heap.pop();
		}

		if (skip_out)
			break;
	}

	// 解除映射区域
	munmap(map_ptr, url_file_size);

	reverse(topk_vec.begin(), topk_vec.end());

	int i = 1;
	for (int i = 0; i < topk; i++) {
		std::cout << "TOP" << i + 1 << " " << topk_vec[i].second << " " << topk_vec[i].first << std::endl;
	}

	std::cout << "The run time is:" << (double)clock() /CLOCKS_PER_SEC<< "s" << std::endl;

	return 0;
}
