#include <iostream>
#include <numeric>
#include <cstring>

#include "codes/variable_blocks.hpp"
#include "normalizer/utf8_utils.hpp"

using namespace std;

char const *const strs[] = {
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

		if(normalizer::ms_marco_utf8_enconded_latin1_heuristc(reinterpret_cast<uint8_t *>(str), size) == size)
			continue;

		std::cout << "latin1!\n";

		normalizer::fix_utf8_encoded_latin1(str, size);

		std::cout << str << std::endl;
	}

	return 0;
}