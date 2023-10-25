#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <iostream>
#include "WordNormalizer.hpp"
#include "stop_words.hpp"
#include "utf8_utils.hpp"

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
#ifdef FIX_MSMARCO_LATIN1
	// Fix enconding error
	if(ms_marco_utf8_enconded_latin1_heuristc(str) < str.size())
		fix_utf8_encoded_latin1(str);
#endif

	remove_punctuation(str);
	return TokenStream(str, *this);
}

WordNormalizer::TokenStream::TokenStream(const std::string &str, WordNormalizer &wn) :
		stem_token(nullptr), isstream(str), wn(wn)
{

}

const std::string& WordNormalizer::TokenStream::next()
{
	// Read tokens until a non stop-word token is read.
	// If such option is disabled, we'll effectively read one token per function call
	while (isstream >> token)
	{
		// @FIXME This stuff works only for ASCII ofc
		// Make string lowercase
		std::transform(token.begin(), token.end(), token.begin(), [](char c) { return std::tolower(c); });

#ifdef SEARCHENGINECPP_STEMMER_ENABLE
		// Skip stop words
		if (token.empty() or stop_words.contains(token))
			continue;

		// Library stemmer
		stem_token = (const char*) sb_stemmer_stem(
				wn.stemmer,
				reinterpret_cast<const sb_symbol *>(token.c_str()),
				token.size());

		token = stem_token;
#endif

		return token;
	}

	// We use empty string to signal EOS to the consumer
	token = "";
	return token;
}

}