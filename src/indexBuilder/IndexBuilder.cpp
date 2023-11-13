#include <cstdint>
#include <ranges>
#include <iostream>
#include <bits/stdc++.h> 
#include <unordered_map>
#include "IndexBuilder.hpp"
#include "../index/types.hpp"
#include "../codes/unary.hpp"
#include "../codes/variable_blocks.hpp"

namespace sindex
{

/**
* Function that writes to disk the inverted index
* @param docid_teletype stream in which the docids are saved
* @param freq_teletype stream in which the frequencies are saved
* @param lexicon_teletype stream in which the lexicon is saved
* @param document_index_teletype stream in which the document index is saved
*/
void IndexBuilder::write_to_disk(std::ostream& docid_teletype, std::ostream& freq_teletype, std::ostream& lexicon_teletype, std::ostream& document_index_teletype)
{
	// Write down all terms to disk, first we write the number of buckets necessary for the hashmap
	for(const auto& [term, posting_list] : inverted_index)
    {
		lexicon_teletype.write(term.c_str(), term.size() + 1);
        lexicon_teletype.write((char *)&posting_list.n_docs, sizeof(freq_t));
    }

	lexicon_teletype.flush();

    // Write to the teletype the posting list and its relative entry in the lexicon
    for(const auto& [term, posting_list] : inverted_index)
    {

        // Write starting offset of the docids
		const uint64_t start_pos = docid_teletype.tellp();
		lexicon_teletype.write((char*)&start_pos, sizeof(uint64_t));

        // Save docid posting list
        for(uint8_t byte : posting_list.docids)
            docid_teletype.put(byte);

        // Save ending offset of the docids
		const uint64_t end_pos = docid_teletype.tellp();
		lexicon_teletype.write((char*)&end_pos, sizeof(uint64_t));
    }

	// Write to disk all the id postings
	docid_teletype.flush();

    for(const auto& [term, posting_list] : inverted_index)
    {
        // Decode Variable Bytes encoded frequencies
		codes::VariableBlocksDecoder iter_freqs(posting_list.freqs.begin(), posting_list.freqs.end());

        // Encode the posting lists of the relative term using the unary algorithm 
        codes::UnaryEncoder freq_encoder(iter_freqs.begin(), iter_freqs.end());

		// Write starting offset of the docids
		const uint64_t start_pos = freq_teletype.tellp();
		lexicon_teletype.write((char*)&start_pos, sizeof(uint64_t));

        // Save frequencies
        for(uint8_t byte : freq_encoder)
            freq_teletype.put(byte);

		// Save ending offset of the docids
		const uint64_t end_pos = freq_teletype.tellp();
		lexicon_teletype.write((char*)&end_pos, sizeof(uint64_t));
    }

	// flush freqs 'n lexicon
	freq_teletype.flush();
	lexicon_teletype.flush();

	// We use this variable as a offsets to the string section of the document index
	size_t current_string_offset = 0;

    // Document index part, first we write the raw_data
	document_index_teletype.write((char*)&base_docid, sizeof(docid_t));
	for(size_t i = 0; i < document_index.size(); ++i)
    {
		docid_t docid = base_docid + i;

		// Write to disk
        document_index_teletype.write((char*)&docid, sizeof(docid));
        document_index_teletype.write((char*)&current_string_offset, sizeof(size_t));

		// Increase offset
		current_string_offset += document_index[i].docno.size() + 1;
    }

	// We write the strings' section
	for(const auto& docinfo : document_index)
		document_index_teletype.write(docinfo.docno.c_str(), docinfo.docno.size() + 1);

	// final flush
	document_index_teletype.flush();
}

}

