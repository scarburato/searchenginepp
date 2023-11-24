#include <cstdint>
#include <sstream>
#include <fstream>
#include <random>
#include "gtest/gtest.h"
#include <vector>
#include "codes/diskmap/diskmap.hpp"

constexpr size_t V_SIZE = 5;
using Value = std::array<uint64_t, V_SIZE>;
using VariableValue = std::vector<uint64_t>;

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

static VariableValue random_variable_data()
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<size_t> distrib_size(1, 10);
	
	size_t size = distrib_size(gen);
	VariableValue v(size);
	
	for(size_t i = 0; i < size; ++i)
		v[i] = distrib_prob(gen) <= 0.333 ? distrib_vals(gen) : distrib_vals(gen) & 0b01111111;
	
	return v;
}

constexpr size_t test_page_size = 0x100;
constexpr size_t test_cardinality = 8'500;

struct DiskTest: public testing::Test
{
	std::map<std::string, Value> test_data;
	std::unique_ptr<codes::disk_map<Value, test_page_size>> map = nullptr;
	std::unique_ptr<memory_mmap> filefr = nullptr;
	std::vector<std::pair<std::string, Value>> test_search = {
			{"corea", {1,2,3,4,5}}, {"zorro", {3,4,5,99, 10}},
			{"kkkkkkk", {50, 0xcafebabe, 0, 0, 0xefff}},
			{"pechino", {0, 0, 0, 0, 0}}
			};

	DiskTest() {
		// Generate random stuff
		for(size_t i = 0; i < test_cardinality; ++i)
			test_data[random_string()] = random_data();

		// Search test
		for (const auto& t : test_search)
			test_data.insert(t);

		auto it = test_data.find("autocisterna");
		if(it != test_data.end())
			test_data.erase(it);

		// Write to disk
		auto filename = testing::TempDir() + "disk_map_test0";
		std::ofstream file(filename, std::ios::binary | std::ios::trunc);
		codes::disk_map_writer<Value, test_page_size> map_w(file);

		for(auto const& p : test_data)
			map_w.add(p);

		map_w.finalize();
		file.close();

		filefr = std::make_unique<memory_mmap>(filename);
		map = std::make_unique<codes::disk_map<Value, test_page_size>>(*filefr);
	}

};

TEST_F(DiskTest, data_integrity)
{
	ASSERT_EQ(map->size(), test_data.size());

	auto it =  map->begin();
	size_t index = 0;
	for(auto it_test = test_data.begin(); it_test != test_data.end(); ++it, ++it_test, ++index)
	{
		ASSERT_EQ(it_test->first, it->first) << "index " << index << " at offset " << std::hex << it.memory_offset() << std::dec;
		ASSERT_EQ(it_test->second, it->second) << "index " << index << "at offset " << std::hex << it.memory_offset() << std::dec;
	}

	ASSERT_EQ(it, map->end()) << it.memory_offset();
}

TEST_F(DiskTest, data_search)
{
	auto it1 = map->find("autocisterna");
	ASSERT_EQ(it1, map->end());

	for (const auto& t : test_search)
	{
		auto it2 = map->find(t.first);
		ASSERT_NE(it2, map->end());
		ASSERT_EQ(it2->first, t.first);
		ASSERT_EQ(it2->second, t.second);
	}
}
struct ss
{
	static constexpr size_t serialize_size = 0;

	VariableValue data;

	std::vector<uint64_t> serialize() const
	{
		return data;
	}

	static ss deserialize(const std::vector<uint64_t>& ser)
	{
		return {ser};
	}
};

struct DiskMapVariable: public testing::Test
{
	std::map<std::string, VariableValue> test_data;
	std::unique_ptr<memory_mmap> filefr = nullptr;
	std::unique_ptr<codes::disk_map<ss, test_page_size>> map = nullptr;
	std::vector<std::pair<std::string, VariableValue>> test_search = {
			{"corea", {1,2,3}}, {"zorro", {5,99, 10}},
			{"kkkkkkk", {50, 0xcafebabe, 0, 0xefff}},
			{"pechino", {0, 0, 0, 0, 0, 0, 0, 0}},
			{"treno", {}}
			};

	DiskMapVariable()
	{
		// Generate random stuff
		for(size_t i = 0; i < test_cardinality; ++i)
			test_data[random_string()] = random_variable_data();

		// Search test
		for (const auto& t : test_search)
			test_data.insert(t);

		auto it = test_data.find("autocisterna");
		if(it != test_data.end())
			test_data.erase(it);

		// Write to disk
		auto filename = testing::TempDir() + "disk_map_test1";
		std::ofstream file(filename, std::ios::binary | std::ios::trunc);
		codes::disk_map_writer<ss, test_page_size> map_w(file);

		for(auto const& p : test_data)
			map_w.add(p.first, {p.second});

		map_w.finalize();
		file.close();

		filefr = std::make_unique<memory_mmap>(filename);
		map = std::make_unique<codes::disk_map<ss, test_page_size>>(*filefr);
	}
	
};

TEST_F(DiskMapVariable, data_integrity)
{
	ASSERT_EQ(map->size(), test_data.size());

	auto it =  map->begin();
	size_t index = 0;
	for(auto it_test = test_data.begin(); it_test != test_data.end(); ++it, ++it_test, ++index)
	{
		ASSERT_EQ(it_test->first, it->first) << "index " << index << " at offset " << std::hex << it.memory_offset() << std::dec;
		ASSERT_EQ(it_test->second, it->second.data) << "index " << index << "at offset " << std::hex << it.memory_offset() << std::dec;
	}

	ASSERT_EQ(it, map->end()) << it.memory_offset();
}

TEST_F(DiskMapVariable, variable_data)
{
	
	auto it1 = map->find("autocisterna");
	ASSERT_EQ(it1, map->end());

	for (const auto& t : test_search)
	{
		auto it2 = map->find(t.first);
		ASSERT_NE(it2, map->end());
		ASSERT_EQ(it2->first, t.first);
		ASSERT_EQ(it2->second.data, t.second);
	}

}

struct Merger: public testing::Test
{
	std::map<std::string, int> map1 = {
			{"corea", 1},
			{"zorro", 5},
			{"kkkkkkk", 50},
			{"pechino", 0},
			{"cisterna", 100}
	};

	std::map<std::string, int> map2 = {
			{"corea", 4},
			{"banano", 5},
			{"ewew", 50},
			{"pacone", 0},
			{"pechino", 69},
			{"cisterna", 150}
	};

	std::map<std::string, int> map3 = {
			{"corea", 5},
			{"banano", 5},
			{"ewew", 50},
			{"pacone", 0},
			{"pechino", 69},
			{"ewew", 50},
			{"cisterna", 250}, {"zorro", 5}, {"kkkkkkk", 50}
	};

	std::string filename1 = testing::TempDir() + "disk_map_test2_a";
	std::string filename2 = testing::TempDir() + "disk_map_test2_b";

	Merger()
	{
		std::ofstream file1(filename1, std::ios::binary | std::ios::trunc);
		std::ofstream file2(filename2, std::ios::binary | std::ios::trunc);

		codes::disk_map_writer<uint64_t ,test_page_size> diskMapWriter1(file1);
		codes::disk_map_writer<uint64_t ,test_page_size> diskMapWriter2(file2);

		for(const auto &p : map1)
			diskMapWriter1.add(p);

		for(const auto &p : map2)
			diskMapWriter2.add(p);

		diskMapWriter1.finalize();
		diskMapWriter2.finalize();
	}
};

TEST_F(Merger, merge_test)
{
	memory_mmap file1mem(filename1);
	memory_mmap file2mem(filename2);

	codes::disk_map<uint64_t ,test_page_size> diskMap1(file1mem);
	codes::disk_map<uint64_t ,test_page_size> diskMap2(file2mem);

	auto filename3 = testing::TempDir() + "disk_map_test2_3";
	std::ofstream file3(filename3, std::ios::binary | std::ios::trunc);

	auto pol = []([[maybe_unused]] const std::string& key, const std::vector<uint64_t >& vals)
	{
		return std::accumulate(vals.begin(), vals.end(), 0llu);
	};

	codes::merge<uint64_t, test_page_size>(file3,{diskMap1, diskMap2}, pol);
	file3.close();

	memory_mmap file3mem(filename3);
	codes::disk_map<int,test_page_size> diskMap3(file3mem);

	ASSERT_EQ(diskMap3.size(), map3.size());

	auto it =  diskMap3.begin();
	size_t index = 0;
	for(auto it_test = map3.begin(); it_test != map3.end(); ++it, ++it_test, ++index)
	{
		ASSERT_EQ(it_test->first, it->first) << "index " << index << " at offset " << std::hex << it.memory_offset() << std::dec;
		ASSERT_EQ(it_test->second, it->second) << "index " << index << "at offset " << std::hex << it.memory_offset() << std::dec;
	}

	ASSERT_EQ(it, diskMap3.end()) << it.memory_offset();
}

