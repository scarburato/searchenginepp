#include <cstdlib>
#include <sstream>
#include <algorithm>
#include "WordNormalizer.hpp"

namespace normalizer
{
WordNormalizer::WordNormalizer() :
		stemmer(sb_stemmer_new("english", nullptr))
		//punctuation(R"([,\.:;\-_\'\"\!\n\t#%&\*\/\?@\\\(\)\[\]{}\|\=\^$£])", pcrecpp::UTF8())
{
	if (stemmer == nullptr)
		abort();
}

WordNormalizer::~WordNormalizer()
{
	sb_stemmer_delete(stemmer);
}

WordNormalizer::TokenStream WordNormalizer::normalize(std::string str)
{
	//punctuation.GlobalReplace(" ", &str);
	remove_punctuation(str);
	return TokenStream(str, *this);
}

WordNormalizer::TokenStream::TokenStream(const std::string &str, WordNormalizer &wn) :
		stem_token(nullptr), isstream(str), wn(wn)
{

}

const std::string& WordNormalizer::TokenStream::next()
{
	while (isstream >> token)
	{
		std::transform(token.begin(), token.end(), token.begin(), [](char c) { return std::tolower(c); });

#ifdef SEARCHENGINECPP_STEMMER_ENABLE
		if (token.empty() or token == "and") // @TODO stopword fr fr no cap
			continue;

		stem_token = (const char*) sb_stemmer_stem(
				wn.stemmer,
				reinterpret_cast<const sb_symbol *>(token.c_str()),
				token.size());

		token = stem_token;
#endif

		return token;
	}

	token = "";
	return token;
}

}