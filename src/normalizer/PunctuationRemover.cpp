#include <vector>
#include <string>
#include <cassert>
#include <hs/hs.h>
#include <iostream>

#include "PunctuationRemover.hpp"


namespace normalizer
{

/** Builds a regex rules that matches one of the symbols */
static inline std::string build_regex(const std::vector<std::string>& symbols);

// see https://en.wikipedia.org/wiki/Template:Punctuation_marks_in_Unicode
static const std::vector<std::string> pun_1_byte = {
		",", "\\.", ":", ";", "\\-", "_", "\\\"", "\\!", "\\n", "\\t",
		"#", "%", "&", "\\*", "\\/", "\\?", "@", "\\\\", "\\(", "\\)", "\\[",
		"\\]", "{", "}", "\\|", "\\=", "\\^", "$", "\\'"
};

static const std::vector<std::string> pun_2_byte = {
		"«", "»", "՚", "՛", "՜", "՝", "՞", "՟", "։", "؉", "؊", "¿", "·",
		"¶", "§", "¡", "£"
};

static const std::vector<std::string> pun_3_byte = {
		"‐", "‑", "‒", "–", "—", "―", "⸗", "⸺", "⸻", "⹀", "〜",
		"‟", "〰", "゠", "︱", "︲", "﹘", "﹣", "－", "‘", "’", "‛", "“", "”",
		"⸚", "‹", "›", "„", "‚", "⁅", "⁆", "〈", "〉", "⌈", "⌉", "⌊", "⌋",
		"＿"
};

// Object to store the shared stuff
static struct Hyperscan
{
	hs_database_t *hs_pun_db = nullptr;
	hs_scratch_t *common_scratch = nullptr;

	Hyperscan();
} hyperscan;

// Object to store one scratch per thread
static thread_local struct Scratch
{
	hs_scratch_t *scratch = nullptr;

	Scratch();
	~Scratch();
} thread_scratch;

/**
 * Helper functions that builds a regex that matches one (1) of the symbols
 * @param symbols the symbols to match eg [s1, s2, s3]
 * @return a regex like "[s1s2s3]"
 */
static inline std::string build_regex(const std::vector<std::string>& symbols)
{
	std::string regex;

	// Iterate through all symbols to match and create a OR rule
	// e.g. s1s2s3s4
	for(const auto& symbol : symbols)
		regex += symbol;

	// match one at the time: [s1s2s3]
	regex = "[" + regex + "]";

	return regex;
}

Hyperscan::Hyperscan()
{
	std::clog << "Init hyperscan db..." << std::endl;

	hs_compile_error_t *compile_err;

	// Build regexes
	const std::string pun_1_byte_reg = build_regex(pun_1_byte);
	const std::string pun_2_byte_reg = build_regex(pun_2_byte);
	const std::string pun_3_byte_reg = build_regex(pun_3_byte);

	// Flags: enable utf8, enable unicode mode, enable reporting start byte to the handler when matching
	constexpr unsigned int flags = HS_FLAG_UTF8 | HS_FLAG_UCP | HS_FLAG_SOM_LEFTMOST;

	//std::cout << pun_1_byte_reg << '\n';

	// Build arrays for the library
	const char* hs_rules[] = {pun_1_byte_reg.c_str(), pun_2_byte_reg.c_str(), pun_3_byte_reg.c_str()};
	const unsigned int hs_rules_flags[] = {flags, flags, flags};
	const unsigned int hs_rules_ids[] = {1,2,3};

	// Compile the three patters in the database.
	auto res = hs_compile_multi(
			hs_rules, hs_rules_flags, hs_rules_ids,
			3, HS_MODE_BLOCK, nullptr, &hs_pun_db, &compile_err
	);

	if (res != HS_SUCCESS)
	{
		std::clog
				<< "ERROR: Unable to compile pattern n (" << compile_err->expression
				<< "): " << compile_err->message << '\n';
		hs_free_compile_error(compile_err);
		abort();
	}

	// Init the common thread_scratch, it'll be cloned for each thread
	res = hs_alloc_scratch(hs_pun_db, &common_scratch);
	if(res != HS_SUCCESS)
	{
		std::clog << "ERROR: Unable to allocate common thread_scratch\n";
		abort();
	}
}

Scratch::Scratch():
	scratch(nullptr)
{
	assert(hyperscan.common_scratch != nullptr);
	std::clog << "building scratch..." << std::endl;

	// For each thread, clone the thread_scratch
	auto res = hs_clone_scratch(hyperscan.common_scratch, &scratch);
	if(res != HS_SUCCESS)
	{
		std::clog << "ERROR: Unable to allocate common thread_scratch" << std::endl;
		abort();
	}
}

Scratch::~Scratch()
{
	hs_free_scratch(scratch);
}

// Called by hs_scan everytime a Unicode char is matched
static int remove_punctuation_handler (
		unsigned int id,
		unsigned long long from, unsigned long long to,
		[[maybe_unused]] unsigned int flags, void *context)
{
	auto *str = (std::string*)context;

	// During debug we make sure that the proper number of utf8 bytes were captured. Eg. a 3 bytes rule cap. 3 bytes
	// We made s.t. the rule's id matches the number of bytes
	assert(to - from == id);
	//std::cout << id << '\t' << from << '\t' << to << '\t' << str->substr(from, to-from) <<'\n';

	// Replace the matched bytes with spaces
	str->replace(from, to - from, to - from ,' ');

	return 0;
}

void remove_punctuation(std::string &str)
{
	auto ret = hs_scan(
			hyperscan.hs_pun_db,
			str.c_str(), str.size(),
			0, thread_scratch.scratch, remove_punctuation_handler , &str);

	if(ret != HS_SUCCESS)
	{
		std::clog << "ERROR: while processing string `" << str << "\'\n";
		abort();
	}
}

} // normalizer