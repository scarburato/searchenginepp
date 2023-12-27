#pragma once

#include <iterator>
#include <cstdint>

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

		/**
		* Function 'parse_and_align()':
		* Parses and aligns encoded data to retrieve the decoded datum.
		*
		* The function performs the following operations:
		* - Initializes 'current_datum_decoded' to 0.
		* - Saves the next input offset in 'next_input'.
		* - Appends the 7 useful bits to 'current_datum_decoded' until reaching the end byte or the end of the iterator.
		* - Sets 'next_encoded_it' to the next input offset after parsing and aligning.
		*/
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

		/**
		 * Prefix increment operator 'operator++()':
		 * Moves the iterator to the next position in the encoded data sequence and parses the next datum.
		 *
		 * The function performs the following operations:
		 * - Assigns 'next_encoded_it' to 'current_encoded_it'.
		 * - Checks if the current iterator position is not the end position.
		 *   - If true, invokes 'parse_and_align()' to parse the next datum.
		 * - Returns a reference to the updated iterator.
		 */
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

	/**
	 * Function 'begin()':
	 * Initializes an iterator pointing to the beginning of the encoded data sequence and aligns the parsed datum.
	 *
	 * The function performs the following operations:
	 * - Creates an iterator 'begin' initialized with 'encoded_begin' and 'encoded_end'.
	 * - Invokes 'parse_and_align()' to align and parse the datum at the beginning position.
	 * - Returns the initialized iterator pointing to the start of the encoded data sequence.
	 */
	iterator begin() const
	{
		auto begin = iterator(encoded_begin, encoded_end);
		begin.parse_and_align();
		return begin;
	}
	/**
	 * Function 'end()':
	 * Returns an iterator pointing to the end position of the encoded data sequence.
	 *
	 * The function performs the following operations:
	 * - Returns an iterator initialized with 'encoded_end' as both start and end positions.
	 *   This effectively represents the end of the encoded data sequence.
	 * - Provides an iterator pointing to the position after the last element.
	 */
	iterator end() const
	{
		return iterator(encoded_end, encoded_end);
	}
	/**
 * Function 'at()':
 * Returns an iterator pointing to the specified offset position in the encoded data sequence, aligned with parsed data.
 *
 * The function performs the following operations:
 * - Calculates the iterator 'begin' by adding the specified 'off' offset to 'encoded_begin'.
 * - Creates an iterator 'begin' initialized with the calculated position and 'encoded_end'.
 * - Invokes 'parse_and_align()' to align and parse the datum at the specified offset position.
 * - Returns the initialized iterator pointing to the specified offset position in the sequence.
 */
	iterator at(size_t off) const
	{
		auto begin = iterator(encoded_begin + off, encoded_end);
		begin.parse_and_align();
		return begin;
	}
};

template<typename RawDataIterator>
class VariableBlocksEncoder
{
	RawDataIterator raw_begin;
	RawDataIterator raw_end;

public:
	/**
	 * Static assertion ensuring that the value type of the provided RawDataIterator is integral.
	 *
	 * The static_assert performs compile-time validation to ensure that the value type associated with the RawDataIterator
	 * is an integral type (e.g., int, char, long, etc.).
	 *
	 * Explanation:
	 * - 'std::iterator_traits' provides a way to obtain information about iterators.
	 * - 'std::iterator_traits<RawDataIterator>::value_type' retrieves the value type associated with 'RawDataIterator'.
	 * - 'std::is_integral_v' is a type trait that checks if a given type is integral (i.e., a built-in integer type).
	 * - The 'static_assert' generates a compile-time error if the value type is not integral.
	 */
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
		/**
		 * Dereference operator:
		 * Retrieves the current value pointed to by the iterator.
		 *
		 * The function performs the following operations:
		 * - Checks if 'working' is false, indicating the need to update 'buffer' from 'current_it'.
		 * - Determines whether the number is representable in 7 bits:
		 *   - If the number is not representable in 7 bits, sets the most significant bit to 1.
		 *   - If the number is representable, sets the MSB to 0.
		 * @Returns the processed value that aligns and represents the current value pointed to by the iterator.
		 */
		uint8_t operator*()
		{
			if(not working)
				buffer = *current_it;

			// If number is not representable in 7 bits put 1 in the most significant bit
			// if not put 0
			return (buffer > 0b01111111ul ? 0b10000000 : 0) | (buffer & 0b01111111);
		}

		/**
		 * Prefix increment operator:
		 * Advances the iterator to the next position and updates the buffer for the next value.
		 *
		 * The function performs the following operations:
		 * - Shifts the 'buffer' right by 7 bits, preparing for the next value.
		 * - Checks if 'buffer' still contains a value:
		 *   - If 'buffer' is not empty, sets 'working' to true, indicating that the iterator is still processing.
		 *     Then, returns the updated iterator.
		 * - If 'buffer' is empty:
		 *   - Increments 'current_it' to move to the next element.
		 *   - Resets 'working' to false, indicating that a new buffer needs to be loaded.
		 *   - Loads the next value into 'buffer'.
		 *  @Returns a reference to the updated iterator.
		 */
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
	 * Constructor for the VariableBytes struct, using Variable Byte Encoding to compress a number.
	 *
	 * The constructor performs Variable Byte Encoding on the provided 'number' and stores the compressed bytes.
	 * - 'number': The input number to be compressed using Variable Byte Encoding. Default value is 0.
	 *
	 * The function performs the following operations:
	 * - Uses a loop to compress the 'number' into a series of bytes using Variable Byte Encoding.
	 * - Iterates until 'number' becomes 0:
	 *    - Extracts 7 bits from the 'number', stores them in a byte with the MSB set to 1 (0b10000000).
	 *    - Appends the created byte to the 'bytes' array and increments 'used_bytes'.
	 * - Adds a terminator byte (0b01111111) to mark the end of the compressed bytes if 'used_bytes' is non-zero.
	 *   - If 'used_bytes' is 0, sets 'used_bytes' to 1.
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
	 * Parses a sequence of bytes using Variable Byte Decoding to reconstruct the original number.
	 *
	 * The function performs Variable Byte Decoding on the input 'bytes' to reconstruct the original number.
	 * - 'bytes': A pointer to the sequence of bytes to be parsed.
	 *
	 * The function iterates through the 'bytes' array:
	 * - Iterates up to a maximum of 10 times or until encountering the end marker in the bytes.
	 * - For each byte in the sequence:
	 *    - Retrieves the 7 bits from the byte (excluding the most significant bit) and adds it to 'number'.
	 *    - Moves the bits to the correct position in 'number' using bitwise left shifts (<<).
	 *    - Checks the most significant bit to determine if there are more bytes to read ('more' flag).
	 *  @Returns a pair containing the reconstructed 'number' and the count of bytes read (i).
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
