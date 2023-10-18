#pragma once
#include <libstemmer.h>
#include <string>
#include <pcrecpp.h>
#include <vector>

namespace normalizer
{
class WordNormalizer
{
private:
	sb_stemmer *stemmer;
	pcrecpp::RE punctuation;

public:
	WordNormalizer();
	~WordNormalizer();

	/**
	 * Takes a string and creates a list of word tokens
	 * @param str
	 * @return
	 */
	std::vector<std::string> normalize(std::string str);
};
}