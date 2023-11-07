#include "gtest/gtest.h"
#include "codes/variable_blocks.hpp"

TEST(VariableCode, decode)
{
	const std::vector<uint8_t> test0_data{0b00000011, 0b11101110, 0b10010001, 0b00000100};

	codes::VariableBlocksDecoder decoder(test0_data.begin(), test0_data.end());

	auto it = decoder.begin();

	ASSERT_EQ(*it, 3);
	++it;
	ASSERT_EQ(*it, 67822l);
}

TEST(VariableCode, encode_decode)
{
	const std::vector<unsigned long> data_to_encode{10, 100, 1000, 10000, 0xcafebabel, 12345, 0xdeadbeef, 0xdeadbeef};

	constexpr size_t buff_size = 256;
	uint8_t buffer[buff_size] = {0};

	codes::VariableBlocksEncoder encoder(data_to_encode.begin(), data_to_encode.end());

	auto buffer_it = buffer;
	for(auto byte : encoder)
	{
		*buffer_it = byte;
		++buffer_it;
	}

	codes::VariableBlocksDecoder decoder(buffer, buffer_it);

	std::vector<unsigned long> data_decoded(decoder.begin(), decoder.end());

	ASSERT_EQ(data_to_encode.size(), data_decoded.size());

	for(size_t i = 0; i < data_to_encode.size(); i++)
		ASSERT_EQ(data_to_encode[i], data_decoded[i]);
}