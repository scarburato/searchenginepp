#include "utf8_utils.hpp"

namespace normalizer
{

inline size_t fix_utf8_encoded_latin1_chunk(uint8_t *buffer, size_t size)
{
	// If it's utf8-econded latin1, then, since latin1's maximum value is 0xff, it'll fit on 2 utf8 bytes
	// If it's not the case, the heuristic fails
	if (size < 2 or ((buffer[0] & 0b11100000) != 0b11000000 and (buffer[1] & 0b11000000) != 0b10000000))
		return size;

	buffer[0] = ((buffer[0] & 0b00011111) << 6) | (buffer[1] & 0b00111111);

	//memmove(buffer + 1, buffer + 2, size - 1);
	for(size_t i = 1; i < size; ++i)
		buffer[i] = buffer[i+1];

	return size - 1;
}

size_t fix_utf8_encoded_latin1(char *str, size_t size)
{
	for(size_t i = 0; i < size; ++i)
		size = i + fix_utf8_encoded_latin1_chunk(reinterpret_cast<uint8_t *>(str + i), size - i);

	return size;
}

size_t ms_marco_utf8_enconded_latin1_heuristc(const uint8_t *buffer, size_t size)
{
	for(size_t i = 0; i < size - 1; ++i)
		if(buffer[i] == 0xc2 and ((buffer[i+1] >= 0x80 and buffer[i+1] <= 0xa0) or buffer[i+1] == 0xad))
			return i*2;

	return size;
}

}