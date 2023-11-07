#pragma once

#include <iterator>
#include <cstdint>
#include "codes.hpp"

namespace codes
{

template<typename EncondedDataIterator, typename T = uint64_t>
class VariableBlocksDecoder : public CodeDecoder<EncondedDataIterator, T>
{
public:
	struct DecodeIterator: public CodeDecoder<EncondedDataIterator, T>::template DecodeIteratorBase<DecodeIterator>
	{
		using DecodeIteratorBase = typename CodeDecoder<EncondedDataIterator, T>::template DecodeIteratorBase<DecodeIterator>::DecodeIteratorBase;
		using DecodeIteratorBase::current_encoded_it;
		using DecodeIteratorBase::end_it;

		explicit DecodeIterator(EncondedDataIterator pos, EncondedDataIterator stop): DecodeIteratorBase(pos, stop) {}

		T operator*() const {return current_datum_decoded;}

		void alignNext()
		{
			// Align iterator to the last byte of a number
			while (current_encoded_it != end_it and *current_encoded_it & 0b10000000)
				++current_encoded_it;

			// Move to the begin of a new number, if any
			if(current_encoded_it != end_it)
				++current_encoded_it;
		}

		void parseCurrent()
		{
			current_datum_decoded = 0;

			// Save the next input offset
			auto next_input = this->current_encoded_it;

			unsigned i = 0;

			// Append the 7 useful bits to the number until I find the end byte or 
			// until I reach the end of the iterator
			do
				current_datum_decoded |= (uint64_t) (*next_input & ~0b10000000) << 7 * i++;
			while (next_input != end_it and *next_input++ & 0b10000000);
		}

	private:
		T current_datum_decoded;
	};

	VariableBlocksDecoder(EncondedDataIterator start, const EncondedDataIterator& end) :
			CodeDecoder<EncondedDataIterator, T>(start, end) {}

	DecodeIterator begin() const
	{
		auto begin = DecodeIterator(this->first_datum, this->data_right_boundary);
		begin.parseCurrent();
		return begin;
	}

	DecodeIterator end() const
	{
		return DecodeIterator(this->data_right_boundary, this->data_right_boundary);
	}
};

template<typename RawDataIterator>
class VariableBlocksEncoder: public CodeEncoder<RawDataIterator>
{
public:
	VariableBlocksEncoder(RawDataIterator start, RawDataIterator end):
			CodeEncoder<RawDataIterator>(start, end) {}

	struct EncodeIterator: public CodeEncoder<RawDataIterator>::EncodeIterator
	{
		explicit EncodeIterator(RawDataIterator current_it): current_it(current_it) {}

		uint8_t operator*()
		{
			if(not working)
				buffer = *current_it;

			// If number is not representable in 7 bits put 1 in the most significant bit
			// if not put 0
			return (buffer > 0b01111111ul ? 0b10000000 : 0) | (buffer & 0b01111111);
		}

		EncodeIterator& operator++()
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

		friend bool operator== (const EncodeIterator& a, const EncodeIterator& b) { return a.current_it == b.current_it; };
		friend bool operator!= (const EncodeIterator& a, const EncodeIterator& b) { return a.current_it != b.current_it; };

	private:
		RawDataIterator current_it;
		bool working = false;
		typename RawDataIterator::value_type buffer;
	};

	EncodeIterator begin() const {return EncodeIterator(this->start);}
	EncodeIterator end() const {return EncodeIterator(this->stop);}

};

struct VariableBytes
{
	uint8_t bytes[10] = {0};
	unsigned short used_bytes = 0;

	/**
	 * Takes a number and compresses it using the Variable Bytes method
	 * The compressed number is put in the variable `bytes[]`
	 */
	explicit inline VariableBytes(uint64_t number)
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