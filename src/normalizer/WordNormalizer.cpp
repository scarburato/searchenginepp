#include <cstdlib>
#include <sstream>
#include <algorithm>
#include "WordNormalizer.hpp"

namespace normalizer
{
WordNormalizer::WordNormalizer() :
		stemmer(sb_stemmer_new("english", nullptr)),
		punctuation(R"(,|\.|:|;|-|_|'|"|!|\n)", pcrecpp::UTF8())
{
	if (stemmer == nullptr)
		abort();
}

WordNormalizer::~WordNormalizer()
{
	sb_stemmer_delete(stemmer);
}

std::vector<std::string> WordNormalizer::normalize(std::string str)
{
	std::vector<std::string> tokens;

	punctuation.GlobalReplace(" ", &str);

	std::istringstream iss(str);
	std::string token;


	while (iss >> token)
	{
		std::transform(token.begin(), token.end(), token.begin(), [](char c) { return std::tolower(c); });

#ifdef SEARCHENGINECPP_STEMMER_ENABLE
		if (token == "and") // @TODO stopword fr fr no cap
			continue;

		token = std::string((char *) sb_stemmer_stem(
				stemmer,
				reinterpret_cast<const sb_symbol *>(token.c_str()),
				token.size()));
#endif

		tokens.push_back(token);
	}


	return tokens;
}
}