#pragma once

#include <cstddef>
#include <unordered_map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <iterator>
#include "../index/types.hpp"
#include "../index/Index.hpp"
#include "../codes/variable_blocks.hpp"
#include <ranges>

namespace sindex
{

class IndexBuilder
{
private:
	const docid_t base_docid;
	const docid_t n_docs;

	/*
	This structure 'PostingList' represents a set of information associated with a specific term in the index.
	- 'docids' is a vector containing compressed representations of document IDs where the term occurs.
	- 'freqs' is a vector containing compressed representations of frequencies corresponding to the document IDs.
	- 'n_docs' denotes the count of documents where this term occurs (initialized to zero).
	*/
    struct PostingList
    {
        std::vector<uint8_t> docids;
        std::vector<uint8_t> freqs;
        freq_t n_docs = 0;
    };

    // For each term in the lexicon we save two vectors: the first contains the compressed docIDs
    // the second one the compressed frequencies
    // term -> <compressedDocIDs[], compressedFreqs[]>
    std::map<std::string,PostingList> inverted_index;
    std::vector<DocumentInfo> document_index;

public:
	explicit IndexBuilder(docid_t n_docs, docid_t base = 0):
		base_docid(base), n_docs(n_docs), document_index(n_docs) {}

    /**
    This function 'add_to_post' inserts a document ID and its associated term frequency into the inverted index.
    It is assumed to recieve strictly increasing docIDs
	Steps:
	1. Compress the document ID ('id') and the frequency ('occurrences') using Variable Byte Encoding.
	2. Access the entry in the inverted index for the given 'term'.
	3. Append the compressed representation of the document ID and frequency to the respective vectors in the entry.
	4. Increment the count of documents ('n_docs') where the term occurs within the entry.

    * @param term index of the inverted index
    * @param id docid
    * @param occurrences number of occurrences of the term
    */
    void add_to_post(const std::string& term, docid_t id, freq_t occurrences)
    {
        // Compute compressed representation of the docID and the frequency
        auto cDocId = codes::VariableBytes(id);
        auto cFreqs = codes::VariableBytes(occurrences);

        // Insert the compressed representation of the docID and the frequency in the inverted index
		auto& entry = inverted_index[term];
		entry.docids.insert(entry.docids.end(), cDocId.bytes, cDocId.bytes + cDocId.used_bytes);
		entry.freqs.insert(entry.freqs.end(), cFreqs.bytes, cFreqs.bytes + cFreqs.used_bytes);
		entry.n_docs += 1;
    }

    /**
    * This function 'add_to_doc' inserts document information into the document index at a specific document ID.
    *
    * Steps:
    * 1. The function checks if the given 'docid' is greater than or equal to the 'base_docid' to ensure it's within the expected range.
    * 2. It updates the 'document_index' at the position 'docid - base_docid' (offset from the base document ID) with the provided 'doc' information.
    * This stores the document information at the respective position in the 'document_index' vector.
    * @param docid index of the document
    * @param doc struct containing the doc number and its length
    */

    void add_to_doc(const docid_t docid, const DocumentInfo& doc)
    {
		assert(docid >= base_docid);
        document_index[docid - base_docid] = doc;
    }

    void write_to_disk(std::ostream& docid_teletype, std::ostream& freq_teletype, std::ostream& lexicon_teletype, std::ostream& document_index_teletype);

	/*
	This function 'get_n_docs_view' creates a view to obtain the number of documents 'n_i' associated with each term in the 'IndexBuilder'.

	Steps:
	1. The function uses std::views::transform to create a transformed view of 'inverted_index'.
	2. For each entry (pair) in 'inverted_index', the lambda function captures the key-value pair.
	3. The lambda returns a pair containing the term (first element of the pair) and the corresponding 'n_docs' count (second element of the pair) from 'PostingList'.
	4. The returned transformed view provides a sequence of pairs with each term mapped to its respective 'n_docs' count.

	NB: This view allows accessing 'n_docs' counts associated with each term without directly accessing the 'inverted_index' structure.
	*/
	auto get_n_docs_view()
	{
		return std::views::transform(inverted_index, [](const auto &pair) {
			return std::make_pair(pair.first, pair.second.n_docs);
		});
	}
};

}
