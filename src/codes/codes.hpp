#pragma once

#include <iterator>
#include <cstdint>

namespace codes
{

template<typename EncondedDataIterator, typename T = uint64_t>
class CodeDecoder
{
protected:
	EncondedDataIterator first_datum;
	EncondedDataIterator data_right_boundary;

public:
	CodeDecoder(EncondedDataIterator start, EncondedDataIterator end):
			first_datum(start), data_right_boundary(end)
	{
		// @TODO use static_assert to check if it's of appropriate iterator_category!
	}

protected:
	// static polymorphism see https://stackoverflow.com/questions/4525984
	template<typename Derived>
	struct DecodeIteratorBase
	{
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;  // or also value_type*
		using reference = T&;  // or also value_type&

		DecodeIteratorBase& operator++()
		{
			static_cast<Derived *>(this)->alignNext();
			static_cast<Derived*>(this)->parseCurrent();
			return *this;
		}

		friend bool operator== (const DecodeIteratorBase& a, const DecodeIteratorBase& b) { return a.current_encoded_it == b.current_encoded_it; };
		friend bool operator!= (const DecodeIteratorBase& a, const DecodeIteratorBase& b) { return a.current_encoded_it != b.current_encoded_it; };

		EncondedDataIterator current_encoded_it;
		const EncondedDataIterator& end_it;
		explicit DecodeIteratorBase(EncondedDataIterator pos, const EncondedDataIterator& end_it): current_encoded_it(pos), end_it(end_it) {}
	};
};

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
