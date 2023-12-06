#pragma once

#include <iterator>
#include <cstdint>

namespace codes
{

template<typename RawDataIterator>
class CodeEncoder
{
protected:
	RawDataIterator start;
	RawDataIterator stop;

public:
	struct EncodeIterator
	{
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint8_t;
		using pointer = uint8_t*;  // or also value_type*
		using reference = uint8_t&;  // or also value_type&
	};

	explicit CodeEncoder(RawDataIterator start, RawDataIterator end):
			start(start), stop(end)
	{
		// @TODO use static_assert to check if it's of appropriate iterator_category!
	}
};

}
