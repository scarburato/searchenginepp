#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace normalizer
{

/**
 * Like fix_utf8_encoded_latin1 but only on the first two bytes
 * @param buffer
 * @param size
 * @return
 */
size_t fix_utf8_encoded_latin1_chunk(uint8_t *buffer, size_t size);

/**
 * This function fixes a utf8 string that was wrongly and forcefully transformed from latin1 to utf8
 * for example a proper UNICODE char í is wrongfully transformed into Ã and ­
 * @param str A string to parse
 * @param size str's length in bytes
 * @return str's new length
 */
size_t fix_utf8_encoded_latin1(char *str, size_t size);

/**
 * This directly operates on STL's strings
 * @see size_t fix_utf8_encoded_latin1(char *str, size_t size);
 */
inline size_t fix_utf8_encoded_latin1(std::string& str)
{
	auto new_size = fix_utf8_encoded_latin1(str.data(), str.size());
	str.resize(new_size);
	return new_size;
}

size_t ms_marco_utf8_enconded_latin1_heuristc(const uint8_t *buffer, size_t size);

/**
 * Tries to evaulate if a utf8 string contains chars that were wrongly and forcefully transformed from latin1 to utf8
 * This can't cover all cases since many utf8-latin1-utf8 chars are indistinguishable from proper utf8 chars
 * @param buffer
 * @param size
 * @return the position of the first likely wrong char
 */
inline size_t ms_marco_utf8_enconded_latin1_heuristc(const char *buffer, size_t size)
{
	return ms_marco_utf8_enconded_latin1_heuristc((const uint8_t*)buffer, size);
};

inline size_t ms_marco_utf8_enconded_latin1_heuristc(const std::string& str)
{
	return ms_marco_utf8_enconded_latin1_heuristc(str.c_str(), str.size());
}


}