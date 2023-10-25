#pragma once
#include <libstemmer.h>
#include <string>
#include <pcrecpp.h>
#include <vector>
#include <optional>
#include "PunctuationRemover.hpp"

namespace normalizer
{
class WordNormalizer
{
private:
	sb_stemmer *stemmer;
	//pcrecpp::RE punctuation;

public:
	WordNormalizer();
	~WordNormalizer();

	class TokenStream
	{
	private:
		std::string token;
		const char* stem_token;
		std::istringstream isstream;
		WordNormalizer& wn;

	public:
		/**
		 * @return The next token from the document, "" if EOS
		 */
		const std::string& next();

		explicit TokenStream(const std::string &str, WordNormalizer &wn);
	};

	/**
	 * Takes a string and creates a list of word tokens
	 * @param str
	 * @return
	 */
	TokenStream normalize(std::string str);
};
}