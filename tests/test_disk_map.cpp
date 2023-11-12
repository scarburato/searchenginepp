#include "gtest/gtest.h"
#include "codes/diskmap/reader.hpp"

TEST(DiskTest, test0)
{
	codes::disk_map<int> test("ciao.txt");
}
