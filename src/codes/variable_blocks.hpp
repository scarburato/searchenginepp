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
			// Align iterator to a number begin
			do
				++current_encoded_it;
			while (current_encoded_it != end_it and *current_encoded_it & 0b10000000);
		}

		void parseCurrent()
		{
			current_datum_decoded = 0;

			auto next_input = this->current_encoded_it;
			for (unsigned i = 0; i < sizeof(uint64_t) and next_input != end_it and (i == 0 || *next_input & 0b10000000); i++, ++next_input)
				current_datum_decoded |= (uint64_t) (*next_input & ~0b10000000) << 7 * i;
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

			// If first byte encoded (working is false), put most sign. bit to 0
			return (working ? 0b10000000 : 0) | (buffer & 0b01111111);
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

}