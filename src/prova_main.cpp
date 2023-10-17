#include <iostream>
#include <numeric>
#include <vector>
#include <libstemmer.h>
#include <cassert>
#include <cstring>
#include <pcrecpp.h>
#include <unicode/unistr.h>

#include "codes/variable_blocks.hpp"
#include <simdutf.h>

using namespace std;

size_t fix_utf8_enconded_latin1(uint8_t *buffer, size_t size)
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

size_t ms_marco_utf8_enconded_latin1_heuristc(uint8_t *buffer, size_t size)
{
	for(size_t i = 0; i < size - 1; ++i)
		if(buffer[i] == 0xc2 and ((buffer[i+1] >= 0x80 and buffer[i+1] <= 0xa0) or buffer[i+1] == 0xad))
			return i*2;

	return size;
}

char *strs[] = {
		R"(AntonÃ­n DvorÃ¡k)",
		"good luck finding that cohort of â\u0080\u009CnaÃ¯veâ\u0080\u009D participants",
		"Home My MBTIÂ® Personality Type Â® Basics.",
		"AntonÃ\u00ADn DvorÃ¡k (1841â\u0080\u00931904) Antonin Dvorak was a son of butcher,",
		"Wikipedia â\u0080\u0093 one of the worldâ\u0080\u0099s great examples of transparent collective behaviour â\u0080\u0093 states",
		"1 Most Tiny creatures canâ\u0080\u0099t attack",
		"uk â\u0080\u008B us â\u0080\u008B. â\u0080º HR, WORKPLACE, MANAGEMENT",
		"Ленин È³²³¼¼ non ò caratteri malati ðe ÐE"
};

int main()
{
	for(auto& str_ : strs)
	{
		char str[300] = {0};
		std::strcpy(str, str_);

		std::cout << str << std::endl;

		size_t size = std::strlen(str);

		if(ms_marco_utf8_enconded_latin1_heuristc(reinterpret_cast<uint8_t *>(str), size) == size)
			continue;

		std::cout << "latin1!\n";
		for(size_t i = 0; i < size; ++i)
			size = fix_utf8_enconded_latin1(reinterpret_cast<uint8_t *>(str + i), size - i);

		std::cout << str << std::endl;
	}

	return 0;
}