#pragma once

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
class UnaryEncoder
{
	RawDataIterator raw_begin;
	RawDataIterator raw_end;

public:
	static_assert(std::is_integral_v<typename std::iterator_traits<RawDataIterator>::value_type>);

	UnaryEncoder(RawDataIterator begin, RawDataIterator end):
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
		RawDataIterator current_raw_it;
		RawDataIterator end_raw_it;
		uint64_t buffer = 0;
		uint8_t out_buffer = 0;
		bool eos = false;

		explicit iterator(RawDataIterator current_it, RawDataIterator end_raw_it, bool eos = false):
				current_raw_it(current_it), end_raw_it(end_raw_it), eos(eos)
		{
			if (not eos)
				build_out_buffer();
		}

		void build_out_buffer()
		{
			out_buffer = 0;
			for(uint8_t bit_mask = 0b00000001; bit_mask != 0; bit_mask <<= 1)
			{
				if (!buffer)
				{
					if (current_raw_it == end_raw_it)
						break;

					buffer =  *current_raw_it;
					++ current_raw_it;
				}

				out_buffer |= bit_mask & (buffer > 1 ? 0xff : 0);

				buffer -= 1;
			}
		}

	public:

		const uint8_t& operator*() const { return out_buffer; }
		const uint8_t* operator->() const { return &out_buffer; }

		iterator& operator++()
		{
			if(current_raw_it == end_raw_it)
				eos = true;

			build_out_buffer();
			return *this;
		}

		friend bool operator== (const iterator& a, const iterator& b)
		{
			// This generates simpler asm
			//return a.eos == b.eos;
			if(a.eos == b.eos)
				return true;

			// if we're at the last element, check if we've streamed the last byte before. Otherwise, we'd lose a byte
			return a.current_raw_it == b.current_raw_it and (a.current_raw_it != a.end_raw_it or a.eos or b.eos);
		};

		friend bool operator!= (const iterator& a, const iterator& b) { return not operator==(a,b); };
		friend UnaryEncoder;
	};

	iterator begin() const {return iterator(raw_begin, raw_end);}
	iterator end() const {return iterator(raw_begin, raw_end, true);}

};

}
