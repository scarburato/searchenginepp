#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include "index/types.hpp"
#include "normalizer/WordNormalizer.hpp"
#include "indexBuilder/IndexBuilder.hpp"

typedef std::pair<sindex::docno_t, std::string> doc_tuple_t;

constexpr size_t CHUNK_SIZE = 50'000; // number of documents to read

/*
 * The uncompressed file contains one document per line.
 * Each line has the following format:
 * <pid>\t<text>\n
 * where <pid> is the docno and <text> is the document content
 */
static void process_chunk(std::unique_ptr<std::vector<doc_tuple_t>> chunk, sindex::docid_t base_id) {
	normalizer::WordNormalizer wn;
	sindex::IndexBuilder indexBuilder;

    for (const auto &line : *chunk) {
		std::unordered_map<std::string, unsigned> term_freqs;
		auto terms = wn.normalize(line.second);

		while(true)
		{
			const auto& term = terms.next();
			if(term.empty())
				break;

			term_freqs[term]++;
		}

		for(const auto& [term, freq] : term_freqs)
			indexBuilder.add(term, line.first, freq);
    }
}


int main()
{
    std::string pid_str, doc;
    size_t line_count = 1;

	std::ios_base::sync_with_stdio(false);
	std::cin.tie(nullptr);

	auto chunk = std::make_unique<std::vector<doc_tuple_t>>();

    while (std::getline(std::cin, pid_str, '\t') and std::getline(std::cin, doc)) {
        chunk->push_back({std::stoull(pid_str), doc});

        if (line_count % CHUNK_SIZE == 0) {
            process_chunk(std::move(chunk), line_count);
            chunk = std::make_unique<std::vector<doc_tuple_t>>();
        }
		line_count++;
	}

    // Process the remaining lines which are less than CHUNK_SIZE
    if (not chunk->empty()) {
        process_chunk(std::move(chunk), line_count);
    }

    return 0;
}