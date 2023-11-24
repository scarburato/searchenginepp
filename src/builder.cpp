#include <atomic>
#include <cstddef>
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
#include "util/thread_pool.hpp"
#include "codes/diskmap/diskmap.hpp"

typedef std::pair<sindex::docno_t, std::string> doc_tuple_t;

// Chunks' sizes
constexpr size_t MAX_CHUNK_SPACE = 675'000'000;

std::map<std::string, sindex::freq_t> global_lexicon;
std::atomic<sindex::doclen_t> global_doc_len_sum = 0;

// At most one thread should write on disk at a given time
std::mutex disk_writer_mutex;

/*
 * The uncompressed file contains one document per line.
 * Each line has the following format:
 * <pid>\t<text>\n
 * where <pid> is the docno and <text> is the document content
 */

static void process_chunk(std::shared_ptr<std::vector<doc_tuple_t>> chunk, sindex::docid_t base_id, size_t chunk_n, const std::filesystem::path& out_dir)
{
	using namespace std::chrono_literals;

	normalizer::WordNormalizer wn;
	sindex::IndexBuilder indexBuilder(chunk->size(), base_id);
	sindex::docid_t docid = base_id;
	sindex::doclen_t doc_len_sum = 0;

	const auto start_time = std::chrono::steady_clock::now();

	// Process all docs (lines) in a chunk
    for (const auto &line : *chunk)
	{

		// Extract all tokens and their freqs
		std::unordered_map<std::string, sindex::freq_t> term_freqs;
		auto terms = wn.normalize(line.second);

		while (true)
		{
			const auto &term = terms.next();
			if (term.empty())
				break;

			term_freqs[term]++;
		}

		auto doc_len = std::accumulate(term_freqs.begin(), 
												term_freqs.end(), (sindex::doclen_t)0,
												[](sindex::freq_t sum, const auto& pair) {
													return sum + pair.second;
					});
		// Add to index
		indexBuilder.add_to_doc(
				docid,
				{.docno = line.first, .lenght = doc_len}
		);

		doc_len_sum += doc_len;

		for (const auto &[term, freq]: term_freqs)
			indexBuilder.add_to_post(term, docid, freq);

		// Increment docid
		docid += 1;
	}


	// Retrieve n_docs_view from IndexBuilder
	// Wait for exclusive access to disk
	std::lock_guard<std::mutex> guard(disk_writer_mutex);

	for (const auto &[term, n]: indexBuilder.get_n_docs_view())
		global_lexicon[term] += n;

	global_doc_len_sum += doc_len_sum;
	const auto stop_time_proc = std::chrono::steady_clock::now();

	// Write stuff to disk
	std::string base_name = "db_" + std::to_string(chunk_n);
	if(not std::filesystem::exists(out_dir/base_name))
		std::filesystem::create_directory(out_dir/base_name);

	auto pl_docids = std::ofstream(out_dir/base_name/"posting_lists_docids", std::ios_base::binary);
	auto pl_freqs = std::ofstream(out_dir/base_name/"posting_lists_freqs", std::ios_base::binary);
	auto lexicon = std::ofstream(out_dir/base_name/"lexicon", std::ios_base::binary);
	auto doc_index = std::ofstream(out_dir/base_name/"document_index", std::ios_base::binary);

	indexBuilder.write_to_disk(pl_docids, pl_freqs, lexicon, doc_index);

	const auto stop_time = std::chrono::steady_clock::now();
	std::cout
		<< "Chunk " << chunk_n
		<< " (thread " << std::hex << std::this_thread::get_id() << std::dec << " )"
		<< " processed in " << (stop_time_proc - start_time) / 1.0s << "s"
		<< " and written in " << (stop_time - stop_time_proc) / 1.0s << "s"
		<< " ( " << (stop_time - start_time)/1s << "s elapsed)" << std::endl;
}

void write_global_lexicon_to_disk_map(const std::filesystem::path& out_dir) {
	std::ofstream lexicon_teletype(out_dir / "global_lexicon", std::ios::binary);
	codes::disk_map_writer<sindex::freq_t> lexicon_writer(lexicon_teletype);

	for (const auto& pair : global_lexicon)
		lexicon_writer.add(pair);

	lexicon_writer.finalize();
}

void write_metadata(const std::filesystem::path& out_dir, const size_t ndocs) {
	std::ofstream metadata(out_dir / "metadata", std::ios::binary);
	metadata.write((char*)&global_doc_len_sum, sizeof(sindex::doclen_t));
	metadata.write((char*)&ndocs, sizeof(size_t));
}

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	size_t space_count = 0;

	std::string pid_str, doc;
    size_t line_count = 1;
	sindex::docid_t docid_start = 1;

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
	auto chunk = std::make_shared<std::vector<doc_tuple_t>>();
	size_t chunk_n = 0;

	// bench stuff
	const auto start_time = std::chrono::steady_clock::now();

	// Multi-thread
	thread_pool pool(4);

	// Iterate through all lines from STDIN
    while (std::getline(std::cin, pid_str, '\t') and std::getline(std::cin, doc))
    {
        chunk->emplace_back(pid_str, doc);

		// Compute the space occupied by doc and pid_str
		space_count += pid_str.size() + doc.size();
		// Send the chunk only if overcomes the space's threshold
		if (space_count >= MAX_CHUNK_SPACE)
		{
			pool.add_job([chunk = std::move(chunk), docid_start, chunk_n, out_dir] {
				process_chunk(chunk, docid_start, chunk_n, out_dir);
			});
			chunk_n += 1;
			docid_start = line_count + 1;

			// Allocate new chunk for next round
			chunk = std::make_shared<std::vector<doc_tuple_t>>();

			// Reset space
			space_count = 0;

			// Wait for a spot before we continue
			pool.wait_for_free_worker();
		}
/*
 * COSA CIERA PRIMA
        if (line_count % CHUNK_SIZE == 0)
        {
			pool.add_job([chunk = std::move(chunk), chunk_n, out_dir] {
				process_chunk(chunk, chunk_n * CHUNK_SIZE + 1, out_dir);
			});
			chunk_n += 1;

			// Allocate new chunk for next round
            chunk = std::make_shared<std::vector<doc_tuple_t>>();

			// Wait for a spot before we continue
			pool.wait_for_free_worker();
        }
*/
		line_count++;
	}

    // Process the remaining lines which are less than CHUNK_SIZE
	if (not chunk->empty())
	{
		pool.add_job([chunk = std::move(chunk), docid_start, chunk_n, out_dir]() {
			process_chunk(chunk, docid_start, chunk_n, out_dir);
		});
		chunk_n += 1;
	}

	pool.wait_all_jobs();


	// Write global_lexicon using disk_map_writer
	write_global_lexicon_to_disk_map(out_dir);
	write_metadata(out_dir, line_count - 1);

	const auto stop_time = std::chrono::steady_clock::now();
	std::cout << "Processed " << (line_count - 1) << " documents in " << (stop_time - start_time) / 1.0s << "s"
		<< " " << (chunk_n) << " indices generated\n";

    return 0;
}
