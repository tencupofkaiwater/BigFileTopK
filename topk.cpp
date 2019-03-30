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
#include <hash_map>

static unsigned long long input_file_size = 0;

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

unsigned char* mmap_big_file(const std::string& file_path) {
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd == -1) {
    	std::cout << file_path << " isn't exist." << std::endl;
		exit(1);
	}

	struct stat file_info;
	fstat(fd, &file_info);
	input_file_size = file_info.st_size;

    unsigned char* map_ptr = (unsigned char*)mmap(NULL, input_file_size, PROT_READ, MAP_PRIVATE, fd, 0);

    close(fd);

    return map_ptr;
}

bool get_next_url(unsigned char* input_addr, unsigned &offset, std::string &url) {
	unsigned long long beg = offset;

	// linux下每行以'\n'结束
	while (offset < input_file_size) {
		if (*(input_addr + offset) == '\n') {
			break;
		}

		offset++;
	}

	url.assign(input_addr, offset - beg);

	if (beg == offset)
		return false;

	return true;
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

	std::string input_file_path = std::string(argv[1]);
	unsigned top_num = std::stol(argv[2]);

	// 第一步：IO优化，使用mmap映射文件
	// mmap打开文件，可以大大提高IO效率，减少内核态到用户态的一次copy。
	unsigned char* input_addr = mmap_big_file(input_file_path);

	// 第二步：分文件
	__gnu_cxx::hash_map<std::string, int> urls_hash;

	std::string url;
	unsigned offset = 0;

	// 2.1 通过hash统计，对于重复量大时可以大大减少内存使用
	// 每次对1G内容进行hash统计，然后更加统计出现数量进行排序
	// hash_map对URL进行统计
	while (1) {
		if (get_next_url(input_addr, offset, url)) {
			urls_hash[url]++;
		}
	}

	// 2.2 堆排序, 找出top_num
	

	// 解除映射区域
	munmap(input_addr, input_file_size);

	return 0;
}
