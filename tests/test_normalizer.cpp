#include "gtest/gtest.h"
#include "normalizer/WordNormalizer.hpp"

TEST(WordNormalizerTest, test0)
{
	std::string str = "Twinkle, twinkle, little bat! "
					  "How I wonder what you're at! "
					  "Up above the world you fly, "
					  "Like a tea-tray in the sky.";

	std::vector<std::string> str_result = {
#ifdef SEARCHENGINECPP_STEMMER_ENABLE
			"twinkl", "twinkl", "littl", "bat", "wonder", "world", "like", "tea", "trai", "sky"
#else
			"twinkle", "twinkle", "little", "bat", "how", "i", "wonder", "what", "you", "re",
			"at", "up", "above", "the", "world", "you", "fly", "like", "a", "tea", "tray",
			"in", "the", "sky"
#endif
	};

	normalizer::WordNormalizer wn;

	ASSERT_EQ(wn.normalize(str), str_result);
}