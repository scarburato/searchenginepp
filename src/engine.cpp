#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <set>
#include "normalizer/WordNormalizer.hpp"


int main()
{
	std::string query;
	normalizer::WordNormalizer wn;

	// Leggi righe da stdin fintanto EOF
	while (std::getline(std::cin, query))
	{
		// Tokenizza la query
		std::set<std::string> tokens;
		auto terms = wn.normalize(query);
		while(true)
		{
			const auto& term = terms.next();
			if(term.empty())
				break;
			tokens.insert(term);

		}
		for(auto const &term : tokens)
			std::cout << term << std::endl;

	}
	return 0;
}
