#include "gtest/gtest.h"
#include "normalizer/WordNormalizer.hpp"
#include "normalizer/PunctuationRemover.hpp"

TEST(PunctuationRemover, test0)
{
	const std::string original = "Ei fu. Siccome immobile,, dato il mortal sospiro ¶ paragrafo ⸻ inciso lungo ⸻ fine.";
	const std::string expected = "Ei fu  Siccome immobile   dato il mortal sospiro    paragrafo     inciso lungo     fine ";
	std::string str = original;

	normalizer::remove_punctuation(str);

	ASSERT_EQ(original.size(), str.size());
	ASSERT_EQ(expected, str);
}


TEST(WordNormalizerTest, test1)
{
	std::string str = "Twinkle, twinkle, little bat! "
					  "How I wonder what you're at! "
					  "Up above the world you fly, "
					  "Like a tea-tray in the sky.";

	std::vector<std::string> expected_result = {
#ifdef SEARCHENGINECPP_STEMMER_ENABLE
			"twinkl", "twinkl", "littl", "bat", "wonder", "world", "like", "tea", "tray", "sky"
#else
			"twinkle", "twinkle", "little", "bat", "how", "i", "wonder", "what", "you", "re",
			"at", "up", "above", "the", "world", "you", "fly", "like", "a", "tea", "tray",
			"in", "the", "sky"
#endif
	};

	normalizer::WordNormalizer wn;
	auto norm_tokens = wn.normalize(str);
	std::vector<std::string> result;

	while(true)
	{
		const auto& token = norm_tokens.next();
		if(token.empty())
			break;

		result.push_back(token);
	}

	ASSERT_EQ(result, expected_result);
}
