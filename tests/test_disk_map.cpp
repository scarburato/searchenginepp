#include "gtest/gtest.h"
#include "codes/diskmap/reader.hpp"

TEST(DiskTest, test0)
{
	memory_mmap area("ciao.txt");
	codes::disk_map<int> test(area);

	for(auto it = test.begin(); it != test.end(); ++it)
		std::cout << it->first << '\n';

	auto it = test.find("banano");

}
