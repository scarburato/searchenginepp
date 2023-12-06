#pragma once

#include "codes.hpp"

namespace codes
{

template<typename EncondedDataIterator>
class UnaryDecoder
{
	EncondedDataIterator begin_it;
	EncondedDataIterator end_it;
public:
	/**
	 * 0 will be decoded to 1, as a side effect, padding bits in the last byte will be decoded as 1s!
	 *
	 * @param start Where to start reading
	 * @param end where to stop
	 */
	explicit UnaryDecoder(EncondedDataIterator start, const EncondedDataIterator &end) :
			begin_it(start), end_it(end) {}


	class iterator
	{
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint64_t;
		using pointer = uint64_t*;  // or also value_type*
		using reference = uint64_t&;  // or also value_type&
	private:
		EncondedDataIterator current_encoded_it;
		EncondedDataIterator end_encoded_it;
		uint64_t current_datum_decoded = 0;
		uint8_t bit_mask = 0b00000001;

		explicit iterator(EncondedDataIterator pos, EncondedDataIterator end, unsigned bit_off = 0):
			current_encoded_it(pos), end_encoded_it(end), bit_mask(0b00000001u << bit_off) {}

		/**
		* Move the current selected bit in the current byte by one. If the end of the byte is reached, move
		* to the next byte in stream.
		*/
		inline void next_bit()
		{
			if (bit_mask == 0b10000000)
			{
				bit_mask = 0b00000001;
				++current_encoded_it;
				return;
			}

			bit_mask = bit_mask << 1;
		};

		void parse_current()
		{
			current_datum_decoded = 1;
			for(;*current_encoded_it & bit_mask; next_bit())
				current_datum_decoded += 1;
		}

	public:
		const uint64_t& operator*() const { return current_datum_decoded; }
		const uint64_t* operator->() const { return &current_datum_decoded; }

		iterator& operator++()
		{
			next_bit();
			if(current_encoded_it != end_encoded_it)
				parse_current();

			return *this;
		}

		bool operator==(const iterator& b) const
		{
			return bit_mask == b.bit_mask and current_encoded_it == b.current_encoded_it;
		}

		bool operator!=(const iterator& b) const { return not operator==(b); }

		friend UnaryDecoder;
	private:
	};

	iterator begin() const
	{
		auto begin = iterator(begin_it, end_it);
		begin.parse_current();
		return begin;
	}

	iterator end() const
	{
		return iterator(end_it, end_it);
	}
};

template<typename RawDataIterator>
class UnaryEncoder: public CodeEncoder<RawDataIterator>
{
public:
	UnaryEncoder(RawDataIterator start, RawDataIterator end):
			CodeEncoder<RawDataIterator>(start, end) {}

	struct EncodeIterator: public CodeEncoder<RawDataIterator>::EncodeIterator
	{
		explicit EncodeIterator(RawDataIterator current_it, const UnaryEncoder& encoder, bool eos = false):
			encoder(encoder), current_it(current_it), eos(eos)
		{
			if (not eos)
				build_out_buffer();
		}

		uint8_t operator*()
		{
			return out_buffer;
		}

		EncodeIterator& operator++()
		{
			if(current_it == encoder.stop)
				eos = true;

			build_out_buffer();
			return *this;
		}

		friend bool operator== (const EncodeIterator& a, const EncodeIterator& b)
		{
			// This generates simpler asm
			return a.eos == b.eos;

			// if we're at the last element, check if we've streamed the last byte before. Otherwise, we'd lose a byte
			//return a.current_it == b.current_it and (a.current_it != a.encoder.stop or a.eos or b.eos);
		};

		friend bool operator!= (const EncodeIterator& a, const EncodeIterator& b) { return not operator==(a,b); };

	private:
		const UnaryEncoder& encoder;
		RawDataIterator current_it;
		uint64_t buffer = 0;
		uint8_t out_buffer = 0;
		bool eos = false;

		void build_out_buffer()
		{
			out_buffer = 0;
			for(uint8_t bit_mask = 0b00000001; bit_mask != 0; bit_mask <<= 1)
			{
				if (!buffer)
				{
					if (current_it == encoder.stop)
						break;

					buffer =  *current_it;
					++ current_it;
				}

				out_buffer |= bit_mask & (buffer > 1 ? 0xff : 0);

				buffer -= 1;
			}
		}
	};

	EncodeIterator begin() const {return EncodeIterator(this->start, *this);}
	EncodeIterator end() const {return EncodeIterator(this->stop, *this, true);}

};

}
