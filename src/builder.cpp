#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <thread>
#include "index/types.hpp"
#include "normalizer/WordNormalizer.hpp"
#include "indexBuilder/IndexBuilder.hpp"

typedef std::pair<sindex::docno_t, std::string> doc_tuple_t;

// Chunks' sizes
constexpr size_t CHUNK_SIZE = 2'000'000;

/*
 * The uncompressed file contains one document per line.
 * Each line has the following format:
 * <pid>\t<text>\n
 * where <pid> is the docno and <text> is the document content
 */
static void process_chunk(std::unique_ptr<std::vector<doc_tuple_t>> chunk, sindex::docid_t base_id, const std::filesystem::path& out_dir) {
	using namespace std::chrono_literals;

	normalizer::WordNormalizer wn;
	sindex::IndexBuilder indexBuilder(CHUNK_SIZE, base_id);
	sindex::docid_t docid = base_id;

	const auto start_time = std::chrono::steady_clock::now();

	// Process all docs (lines) in a chunk
    for (const auto &line : *chunk)
    {
		// Extract all tokens and their freqs
		std::unordered_map<std::string, unsigned> term_freqs;
		auto terms = wn.normalize(line.second);

		while(true)
		{
			const auto& term = terms.next();
			if(term.empty())
				break;

			term_freqs[term]++;
		}

		// Add to index
		indexBuilder.add_to_doc(
				docid,
				{.docno = line.first, .lenght = (sindex::doclen_t)(term_freqs.size())}
				);

		for(const auto& [term, freq] : term_freqs)
			indexBuilder.add_to_post(term, docid, freq);

		// Increment docid
		docid += 1;
    }

	const auto stop_time_proc = std::chrono::steady_clock::now();

	// Write stuff to disk
	std::string base_name = "db_" + std::to_string(base_id / CHUNK_SIZE);
	if(not std::filesystem::exists(out_dir/base_name))
		std::filesystem::create_directory(out_dir/base_name);

	auto pl_docids = std::ofstream(out_dir/base_name/"posting_lists_docids", std::ios_base::binary);
	auto pl_freqs = std::ofstream(out_dir/base_name/"posting_lists_freqs", std::ios_base::binary);
	auto lexicon = std::ofstream(out_dir/base_name/"lexicon", std::ios_base::binary);
	auto doc_index = std::ofstream(out_dir/base_name/"document_index", std::ios_base::binary);

	indexBuilder.write_to_disk(pl_docids, pl_freqs, lexicon, doc_index);

	const auto stop_time = std::chrono::steady_clock::now();
	std::cout
		<< "Chunk " << (base_id / CHUNK_SIZE)
		<< " (thread " << std::hex << std::this_thread::get_id() << std::dec << " )"
		<< " processed in " << (stop_time_proc - start_time) / 1.0s << "s"
		<< " and written in " << (stop_time - stop_time_proc) / 1.0s << "s"
		<< " ( " << (stop_time - start_time)/1s << "s elapsed)" << std::endl;
}


int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

    std::string pid_str, doc;
    size_t line_count = 1;

	// This is where we'll store the output stuff
	const std::filesystem::path out_dir = argc > 1 ? argv[1] : "data";
	if(std::filesystem::exists(out_dir))
		std::filesystem::remove_all(out_dir);

	std::filesystem::create_directory(out_dir);

	// VROOOOOM STANDARD IO
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(nullptr);

	// A chunk contains a partition of lines read from stdin to be processed
	// We store them in an array of <docno, doc> that will be deleted by
	// process_chunk
	auto chunk = std::make_unique<std::vector<doc_tuple_t>>();
	size_t chunk_n = 0;

	// bench stuff
	const auto start_time = std::chrono::steady_clock::now();

	// Iterate through all lines from STDIN
    while (std::getline(std::cin, pid_str, '\t') and std::getline(std::cin, doc))
    {
        chunk->push_back({pid_str, doc});

        if (line_count % CHUNK_SIZE == 0)
        {
            process_chunk(std::move(chunk), chunk_n * CHUNK_SIZE + 1, out_dir);
			chunk_n += 1;

			// Allocate new chunk for next round
            chunk = std::make_unique<std::vector<doc_tuple_t>>();
        }
		line_count++;
	}

    // Process the remaining lines which are less than CHUNK_SIZE
	if (not chunk->empty())
		process_chunk(std::move(chunk), chunk_n * CHUNK_SIZE + 1, out_dir);

	const auto stop_time = std::chrono::steady_clock::now();
	std::cout << "Processed " << line_count << " documents in " << (stop_time - start_time) / 1.0s << "s\n";

    return 0;
}