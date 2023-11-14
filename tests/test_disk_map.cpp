#include <sstream>
#include <fstream>
#include <random>
#include "gtest/gtest.h"
#include "codes/diskmap/diskmap.hpp"

constexpr size_t V_SIZE = 5;
using Value = std::array<uint64_t, V_SIZE>;

static std::mt19937 gen(0xcafebabe); // mersenne_twister_engine seeded with rd()
static std::uniform_int_distribution<char> distrib_chars('a', 'z');
static std::uniform_int_distribution<uint64_t> distrib_vals(0, UINT64_MAX / 8);
static std::uniform_real_distribution distrib_prob(0.0, 1.0);

static std::string random_string()
{
	std::string str;
	do
		str += distrib_chars(gen);
	while(distrib_prob(gen) <= 0.90);
	return str;
}

static Value random_data()
{
	Value v;
	for(size_t i = 0; i < V_SIZE; ++i)
		v[i] = distrib_prob(gen) <= 0.333 ? distrib_vals(gen) : distrib_vals(gen) & 0b01111111;
	return v;
}

constexpr size_t test_page_size = 0x400;
constexpr size_t test_cardinality = 4'500;

TEST(DiskTest, test0)
{
	// Generate random stuff
	std::map<std::string, Value> test_data;
	for(size_t i = 0; i < test_cardinality; ++i)
		test_data[random_string()] = random_data();

	auto filename = testing::TempDir() + "disk_map_test0";
	std::ofstream file(filename, std::ios::binary | std::ios::trunc);
	codes::disk_map_writer<Value, test_page_size> map_w(file);

	for(auto const& p : test_data)
		map_w.add(p);

	map_w.finalize();
	file.close();

	memory_mmap filefr(filename);
	codes::disk_map<Value, test_page_size> map(filefr);

	ASSERT_EQ(map.size(), test_data.size());

	auto it =  map.begin();
	size_t index = 0;
	for(auto it_test = test_data.begin(); it_test != test_data.end(); ++it, ++it_test, ++index)
	{
		ASSERT_EQ(it_test->first, it->first) << "index " << index << " at offset " << std::hex << it.memory_offset() << std::dec;
		ASSERT_EQ(it_test->second, it->second) << "index " << index << "at offset " << std::hex << it.memory_offset() << std::dec;
	}

	ASSERT_EQ(it, map.end()) << it.memory_offset();
}
