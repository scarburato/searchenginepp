#pragma once

#include "codes.hpp"

namespace codes
{

template<typename EncondedDataIterator, typename T = uint64_t>
class UnaryDecoder : public CodeDecoder<EncondedDataIterator, T>
{
public:
	/**
	 * 0 will be decoded to 1, as a side effect, padding bits in the last byte will be decoded as 1s!
	 *
	 * @param start Where to start reading
	 * @param end where to stop
	 */
	explicit UnaryDecoder(EncondedDataIterator start, const EncondedDataIterator &end) :
			CodeDecoder<EncondedDataIterator, T>(start, end)
	{}


	struct DecodeIterator : public CodeDecoder<EncondedDataIterator, T>::template DecodeIteratorBase<DecodeIterator>
	{
		using DecodeIteratorBase = CodeDecoder<EncondedDataIterator, T>::template DecodeIteratorBase<DecodeIterator>::DecodeIteratorBase;
		using DecodeIteratorBase::current_encoded_it;
		using DecodeIteratorBase::end_it;

		explicit DecodeIterator(EncondedDataIterator pos, EncondedDataIterator stop) :
			DecodeIteratorBase(pos, stop)
		{}

		T operator*() const { return current_datum_decoded; }

		void alignNext()
		{
			if (current_encoded_it != end_it)
				next_bit();
		}

		void parseCurrent()
		{
			current_datum_decoded = 1;
			for(; current_encoded_it != end_it and *current_encoded_it & bit_mask; next_bit())
				current_datum_decoded += 1;
		}

	private:
		T current_datum_decoded = 0;
		uint8_t bit_mask = 0b00000001;

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
	};

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