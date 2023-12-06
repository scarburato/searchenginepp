#include "gtest/gtest.h"
#include "codes/unary.hpp"

TEST(UnaryCode, decode)
{
	const std::vector<uint8_t> test0_data{0b00000011, 0b01101110, 0b10010001, 0b01110101};
	const std::vector<unsigned> test0_parsed{
		3, 1, 1, 1, 1, 1, 1, 4, 3, 2, 1, 1, 2, 1, 3,  2, 4
	};

	codes::UnaryDecoder decoder(test0_data.begin(), test0_data.end());
	std::vector<unsigned> result(decoder.begin(), decoder.end());

	ASSERT_EQ(decoder.begin().get_bit_offset(), 0);
	ASSERT_EQ(result.size(), test0_parsed.size());

	auto res_it = result.begin();
	auto test_it = test0_parsed.begin();

	for(; res_it != result.end(); ++res_it, ++test_it)
		ASSERT_EQ(*res_it, *test_it) << " at index " << (test_it - test0_parsed.begin());

	// Test offseted data
	codes::UnaryDecoder decoder_offseted(test0_data.begin(), test0_data.end(), 3);
	std::vector<unsigned> result_offseted(decoder_offseted.begin(), decoder_offseted.end());

	ASSERT_EQ(decoder_offseted.begin().get_bit_offset(), 3);
	ASSERT_EQ(result_offseted.size(), test0_parsed.size() - 1);

	res_it = result_offseted.begin();
	test_it = test0_parsed.begin() + 1;

	for(; res_it != result_offseted.end(); ++res_it, ++test_it)
		ASSERT_EQ(*res_it, *test_it) << " at index " << (test_it - test0_parsed.begin());
}

TEST(UnaryCode, encode)
{
	const std::vector<uint8_t> test0_data{0b00000011, 0b01101110, 0b10010001, 0b01110101};
	const std::vector<uint64_t > test0_parsed{
			3, 1, 1, 1, 1, 1, 1, 4, 3, 2, 1, 1, 2, 1, 3,  2, 4
	};
	std::vector<uint8_t> result;

	codes::UnaryEncoder encoder(test0_parsed.begin(), test0_parsed.end());

	for (auto encoded : encoder)
		result.push_back(encoded);

	ASSERT_EQ(result.size(), test0_data.size());

	auto res_it = result.begin();
	auto test_it = test0_data.begin();

	for(; res_it != result.end(); ++res_it, ++test_it)
		ASSERT_EQ(*res_it, *test_it) << " at byte " << (test_it - test0_data.begin());
}

TEST(UnaryCode, encode_decode)
{
	const std::vector<unsigned long> data_to_encode{10, 20, 10, 1,1,1,1, 8, 23, 1, 5, 1, 1};

	constexpr size_t buff_size = 256;
	uint8_t buffer[buff_size] = {0};

	codes::UnaryEncoder encoder(data_to_encode.begin(), data_to_encode.end());

	auto buffer_it = buffer;
	for(auto byte : encoder)
	{
		*buffer_it = byte;
		++buffer_it;
	}

	codes::UnaryDecoder decoder(buffer, buffer_it);

	std::vector<unsigned long> data_decoded(decoder.begin(), decoder.end());

	ASSERT_LE(data_to_encode.size(), data_decoded.size());

	for(size_t i = 0; i < data_to_encode.size(); i++)
		ASSERT_EQ(data_to_encode[i], data_decoded[i]);
}
