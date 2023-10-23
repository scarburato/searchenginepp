#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include "index/types.hpp"

using namespace index;

typedef std::pair<docno_t, std::string> doc_tuple_t;

constexpr size_t CHUNK_SIZE = 50'000; // number of documents to read

/*
 * The uncompressed file contains one document per line.
 * Each line has the following format:
 * <pid>\t<text>\n
 * where <pid> is the docno and <text> is the document content
 */
static void process_chunk(std::unique_ptr<std::vector<doc_tuple_t >> chunk, docid_t base_id) {
    for (const auto &line : *chunk) {
		// esempio stampa docno e text
		asm("nop");
		//std::cout << "docno: " << line.first << ", text: " << line.second << std::endl;
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