#pragma once
#include <bit>

namespace codes
{

/**
 * Function to serialize an offset and a bit offset into a single uint64_t.
 * This function 'serialize_bit_offset' combines an 'offset' and a 'bit_offset' into a single uint64_t value.
 * It ensures that the 'bit_offset' value is less than 8, allowing it to fit within a single byte (8 bits).
 * The combination is achieved by left-shifting the 'offset' bits by 3 positions (effectively multiplying 'offset' by 8)
 * and then using bitwise OR to merge these shifted bits with the 'bit_offset'.
 * The resulting uint64_t value represents the serialized combination of 'offset' and 'bit_offset'.
 * @param offset
 * @param bit_offset
 * @return
 */
inline uint64_t serialize_bit_offset(size_t offset, unsigned bit_offset)
{
	assert(bit_offset < 8);
	return (offset << 3) | bit_offset;
}

/**
 * Function to deserialize an offset and a bit offset from a single uint64_t.
 * The function 'deserialize_bit_offset' reverses the process of serialization,
 * extracting the original 'offset' and 'bit_offset' values from a serialized uint64_t value.
 * It takes a single uint64_t 'serialized' value as input.
 *
 * To obtain the 'offset', it performs a right shift by 3 positions on the 'serialized' value,
 * undoing the left shift performed during serialization (essentially dividing 'offset' by 8).
 *
 * For the 'bit_offset', the function uses a bitwise AND operation with 0b111 (binary 111),
 * preserving the rightmost 3 bits, which represents the 'bit_offset' in the serialized value.
 *
 * The extracted 'offset' and 'bit_offset' are returned as a std::pair<size_t, unsigned>.
 * @param offset
 * @param bit_offset
 * @return
 */
inline std::pair<size_t, unsigned> deserialize_bit_offset(uint64_t serialized)
{
	return {serialized >> 3, serialized & 0b111};
}
// UnaryDecoder class for decoding encoded unary data
template<typename EncondedDataIterator>
class UnaryDecoder
{
	EncondedDataIterator begin_it;
	EncondedDataIterator end_it;
	unsigned start_off = 0;
public:
	/**
	 * Constructor for UnaryDecoder
	 * 0 will be decoded to 1, as a side effect, padding bits in the last byte will be decoded as 1s!
	 *
	 * @param start Where to start reading
	 * @param end where to stop
	 */
	explicit UnaryDecoder(EncondedDataIterator start, const EncondedDataIterator &end, unsigned start_off = 0) :
			begin_it(start), end_it(end), start_off(start_off)
	{
		assert(start_off < 8);
	}
	// Iterator class within UnaryDecoder for decoding unary-encoded data
	class iterator
	{
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint64_t;
		using pointer = uint64_t*;  // or also value_type*
		using reference = uint64_t&;  // or also value_type&
	private:
		// Iterator member variables
		EncondedDataIterator current_encoded_it; // Iterator pointing to current position in encoded data
		EncondedDataIterator end_encoded_it; // Iterator pointing to the end of encoded data
		uint64_t current_datum_decoded = 0; // Current decoded unary value
		uint8_t bit_mask = 0b00000001; // Bit mask for selecting bits in a byte
		uint8_t current_datum_start_bit_mask = 0b00000001;  // Bit mask for the start of current datum
		// Iterator constructor

		/**
		 * Explicit constructor for the iterator class.
		 * Initializes the iterator object with iterators pointing to the current and end positions in the encoded data,
		 * along with an optional bit offset (defaulted to 0).
		 *
		 * @param pos Iterator pointing to the current position in the encoded data
		 * @param end Iterator pointing to the end of the encoded data
		 * @param bit_off Bit offset value (optional, default: 0)
		 */
		explicit iterator(EncondedDataIterator pos, EncondedDataIterator end, unsigned bit_off = 0):
			current_encoded_it(pos), end_encoded_it(end), bit_mask(0b00000001u << bit_off),
			current_datum_start_bit_mask(0b00000001u << bit_off) {}

		/**
		* Move the current selected bit in the current byte by one. If the end of the byte is reached, move
		* to the next byte in stream.
		*/
		inline void next_bit()
		{
			if (bit_mask == 0b10000000)
			{
				bit_mask = 0b00000001; // If end of byte reached, reset bit mask and move to next byte
				++current_encoded_it;
				return;
			}

			bit_mask = bit_mask << 1; // Move to the next bit in the current byte
		};
		// Function to parse the current bits and decode the unary value
		void parse_current()
		{
			current_datum_start_bit_mask = bit_mask; // Store the starting bit mask
			current_datum_decoded = 1; // Initialize the decoded value to 1

			// Loop to count consecutive 1-bits in the encoded data
			for(;*current_encoded_it & bit_mask; next_bit())
				current_datum_decoded += 1;  // Increment the decoded value for each consecutive 1-bit
		}

	public:
		// Overloaded operators and member functions for the iterator
		const uint64_t& operator*() const { return current_datum_decoded; } // Dereferencing operator
		const uint64_t* operator->() const { return &current_datum_decoded; } // Member access operator

		iterator& operator++()
		{
			next_bit(); // Move to the next bit
			if(current_encoded_it != end_encoded_it)
				parse_current(); // If not at the end, parse the current bits

			return *this;
		}

		const EncondedDataIterator& get_raw_iterator() const {return current_encoded_it;} // Access raw iterator
		unsigned get_bit_offset() const {return std::countr_zero(current_datum_start_bit_mask);} // Get bit offset

		bool operator==(const iterator& b) const
		{
			return bit_mask == b.bit_mask and current_encoded_it == b.current_encoded_it; // Comparison operator
		}

		bool operator!=(const iterator& b) const { return not operator==(b); } // Inequality operator

		friend UnaryDecoder; // Allowing UnaryDecoder class to access private members
	private:
	};
	/**
	 * Returns an iterator object pointing to the beginning of the encoded data
	 * after initializing it with 'begin_it', 'end_it', and 'start_off' values.
	 *
	 * @return An iterator object pointing to the beginning of the encoded data.
	 */
	iterator begin() const
	{
		auto begin = iterator(begin_it, end_it, start_off); // Initialize iterator 'begin' with 'begin_it', 'end_it', and 'start_off'
		begin.parse_current(); // Parse the current bits to decode the initial value
		return begin; // Return the initialized iterator
	}

	/**
	 * Returns an iterator object pointing to the end of the encoded data.
	 * The iterator is initialized with 'end_it' (indicating the end of the encoded data).
	 *
	 * @return An iterator object pointing to the end of the encoded data.
	 */
	iterator end() const
	{
		return iterator(end_it, end_it);
	}

	/**
	 * Returns an iterator object pointing to a specific position within the encoded data.
	 * The iterator is initialized with an offset 'off' from 'begin_it' and 'end_it' iterators.
	 * The 'bit_off' parameter indicates the starting bit offset (defaulted to 0 if not provided).
	 *
	 * @param off The offset indicating the position within the encoded data.
	 * @param bit_off Bit offset value indicating the starting bit (default: 0).
	 * @return An iterator object pointing to the specified position within the encoded data.
	 */
	iterator at(size_t off, unsigned bit_off = 0) const
	{
		auto it = iterator(begin_it + off, end_it, bit_off);
		it.parse_current();
		return it;
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
		/**
		 * Initializes the iterator object with the current iterator, end iterator, and an end-of-sequence flag.
		 * If 'eos' is false (indicating not the end of the sequence), it builds the output buffer.
		 *
		 * @param current_it Iterator pointing to the current position in the raw data
		 * @param end_raw_it Iterator pointing to the end of the raw data
		 * @param eos Flag indicating if it's the end of the sequence (default: false)
		 */
		explicit iterator(RawDataIterator current_it, RawDataIterator end_raw_it, bool eos = false):
				current_raw_it(current_it), end_raw_it(end_raw_it), eos(eos)
		{
			if (not eos)
				build_out_buffer();
		}

		/**
		 * Function build_out_buffer():
		 * Constructs the output buffer by processing the raw data buffer.
		 * It iterates through the bits of the buffer, populating the 'out_buffer' with unary-encoded values.
		 *
		 * The function operates as follows:
		 * - Initializes 'out_buffer' to 0.
		 * - Iterates through each bit using 'bit_mask'.
		 * - Checks if 'buffer' is empty.
		 *   - If empty, retrieves a new value from 'current_raw_it' and increments the iterator.
		 * - Constructs 'out_buffer' by applying unary encoding logic:
		 *   - Uses a bitwise OR operation to set specific bits based on the value of 'buffer'.
		 * - Decrements 'buffer' by 1 for each bit processed.
		 */
		void build_out_buffer()
		{
			out_buffer = 0; // Initialize the output buffer to 0
			for(uint8_t bit_mask = 0b00000001; bit_mask != 0; bit_mask <<= 1)
			{
				if (!buffer)
				{
					if (current_raw_it == end_raw_it)
						break;

					buffer =  *current_raw_it;  // Fetch a new value from 'current_raw_it'
					++ current_raw_it;			// Move to the next element in the raw data
				}
				// Constructs 'out_buffer' by setting specific bits based on the value of 'buffer'
				out_buffer |= bit_mask & (buffer > 1 ? 0xff : 0);

				buffer -= 1; // Decrease 'buffer' value by 1
			}
		}

	public:

		const uint8_t& operator*() const { return out_buffer; }
		const uint8_t* operator->() const { return &out_buffer; }

		/**
		 * Prefix increment operator 'operator++()':
		 * Moves the iterator to the next position in the raw data sequence.
		 *
		 * The function performs the following operations:
		 * - Checks if the iterator has reached the end of the sequence.
		 *   - If true, sets the 'eos' flag to true indicating the end of the sequence.
		 * - Calls 'build_out_buffer()' to construct the output buffer for the next position.
		 * - Increments the iterator to the next position in the raw data sequence.
		 *
		 * @return Reference to the updated iterator object.
		 */
		iterator& operator++()
		{
			if(current_raw_it == end_raw_it)
				eos = true; // Set 'eos' flag to true if the iterator reaches the end of the sequence

			build_out_buffer(); // Construct the output buffer for the next position
			return *this; // Return reference to the updated iterator object
		}

		/**
		 * Comparison operator 'operator==':
		 * Checks for equality between two iterator objects.
		 *
		 * The function performs the following equality check:
		 * - Compares the 'eos' flags of both iterators.
		 *   - If equal, returns true indicating equality.
		 * - If 'eos' flags are different:
		 *   - Compares the current positions of the iterators.
		 *   - Ensures that the iterators have not reached the end position or are at the last element.
		 *   - Returns true if both conditions are met; otherwise, returns false.
		 *
		 * @param a The first iterator for comparison
		 * @param b The second iterator for comparison
		 * @return True if the iterators are equal; otherwise, false
		 */
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
