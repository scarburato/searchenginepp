#pragma once

#include <iterator>
#include <cstdint>
#include "codes.hpp"

namespace codes
{

template<typename EncondedDataIterator>
class VariableBlocksDecoder
{
	EncondedDataIterator encoded_begin, encoded_end;
public:
	class iterator
	{
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint64_t;
		using pointer = uint64_t*;
		using reference = uint64_t&;

	private:
		uint64_t current_datum_decoded = 0;
		EncondedDataIterator current_encoded_it;
		EncondedDataIterator next_encoded_it;
		EncondedDataIterator end_encoded_it;

		explicit iterator(EncondedDataIterator p, EncondedDataIterator s):
			current_encoded_it(p), end_encoded_it(s) {}

		void parse_and_align()
		{
			current_datum_decoded = 0;

			// Save the next input offset
			auto next_input = current_encoded_it;

			unsigned i = 0;

			// Append the 7 useful bits to the number until I find the end byte or
			// until I reach the end of the iterator
			do
				current_datum_decoded |= (uint64_t) (*next_input & ~0b10000000) << 7 * i++;
			while (*next_input++ & 0b10000000);

			next_encoded_it = next_input;
		}

	public:
		const uint64_t& operator*() const {return current_datum_decoded;}
		const uint64_t* operator->() const {return &current_datum_decoded;}

		iterator& operator++()
		{
			current_encoded_it = next_encoded_it;
			if(current_encoded_it != end_encoded_it)
				parse_and_align();

			return *this;
		}
		const EncondedDataIterator& get_raw_iterator() const {return current_encoded_it;}

		bool operator==(const iterator& other) const {return this->current_encoded_it == other.current_encoded_it;}
		bool operator!=(const iterator& other) const {return this->current_encoded_it != other.current_encoded_it;}

		friend VariableBlocksDecoder;
	};

	VariableBlocksDecoder(EncondedDataIterator start, const EncondedDataIterator& end) :
			encoded_begin(start), encoded_end(end) {}

	iterator begin() const
	{
		auto begin = iterator(encoded_begin, encoded_end);
		begin.parse_and_align();
		return begin;
	}

	iterator end() const
	{
		return iterator(encoded_end, encoded_end);
	}
};

template<typename RawDataIterator>
class VariableBlocksEncoder
{
	RawDataIterator raw_begin;
	RawDataIterator raw_end;

public:
	static_assert(std::is_integral_v<typename std::iterator_traits<RawDataIterator>::value_type>);

	VariableBlocksEncoder(RawDataIterator begin, RawDataIterator end):
			raw_begin(begin), raw_end(end) {}

	class iterator
	{
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint8_t;
		using pointer = uint8_t*;
		using reference = uint8_t&;

	private:
		RawDataIterator current_it;
		bool working = false;
		typename RawDataIterator::value_type buffer;

		explicit iterator(RawDataIterator current_it): current_it(current_it) {}
	public:
		uint8_t operator*()
		{
			if(not working)
				buffer = *current_it;

			// If number is not representable in 7 bits put 1 in the most significant bit
			// if not put 0
			return (buffer > 0b01111111ul ? 0b10000000 : 0) | (buffer & 0b01111111);
		}

		iterator& operator++()
		{
			buffer = buffer >> 7;

			if(buffer)
			{
				working = true;
				return *this;
			}

			// empty buffer, we need to load another one
			++current_it;
			working = false;
			buffer = *current_it;

			return *this;
		}

		friend bool operator== (const iterator& a, const iterator& b) { return a.current_it == b.current_it; };
		friend bool operator!= (const iterator& a, const iterator& b) { return a.current_it != b.current_it; };
		friend class VariableBlocksEncoder;
	};

	iterator begin() const {return iterator(raw_begin);}
	iterator end() const {return iterator(raw_end);}

};

struct VariableBytes
{
	uint8_t bytes[10] = {0};
	unsigned short used_bytes = 0;

	/**
	 * Takes a number and compresses it using the Variable Bytes method
	 * The compressed number is put in the variable `bytes[]`
	 */
	explicit inline VariableBytes(uint64_t number = 0)
	{
		for(; number; number = number >> 7, ++used_bytes)
			bytes[used_bytes] = (number & 0b01111111) | 0b10000000;

		// Last byte terminator
		if(used_bytes)
			bytes[used_bytes - 1] &= 0b01111111;
		else
			used_bytes = 1;
	}

	/**
	 * Parses a variable-length number and returns it uncompressed
	 * @returns the parsed bytes and the number of bytes read
	 */
	static inline std::pair<uint64_t, unsigned> parse(uint8_t *bytes)
	{
		bool more = true;
		unsigned i;
		uint64_t number = 0;

		// Parsing stuff
		for(i = 0; i < 10 and more; ++i)
		{
			number |= (uint64_t)(bytes[i] & 0b01111111) << 7*i;
			more = bytes[i] & 0b10000000;
		}

		return {number, i};
	}
};

}
