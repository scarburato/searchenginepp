#include <iostream>
#include <numeric>
#include <vector>
#include <libstemmer.h>
#include <cassert>
#include <cstring>

#include "codes/variable_blocks.hpp"

using namespace std;

int main() {
	//auto stemmer_names = sb_stemmer_list();
	//for(auto stemmer_name = stemmer_names; *stemmer_name != nullptr; ++stemmer_name)
	//	std::cout << *stemmer_name << '\n';

	auto stemmer = sb_stemmer_new("english" , nullptr);

	assert(stemmer != nullptr);

	const char* words[] = {"cared","university","fairly","easily","singing",
					  "sings","sung","singer","sportingly", "è", "bourgeois",
					  "bourgeoisie", "PROFESSOR", "ᣡᣡᣨ"
	};

	for(auto it = words; it != words + sizeof(words); ++it)
	{
		const auto stemmed_word = sb_stemmer_stem(stemmer, reinterpret_cast<const sb_symbol *>(*it), std::strlen(*it));

		std::cout << stemmed_word << '\n';
	}

	sb_stemmer_delete(stemmer);
	return 0;
}