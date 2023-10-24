#include "IndexBuilder.hpp"
#include "../index/types.hpp"
#include <cstdint>
#include <ranges>
#include <iostream>
#include <bits/stdc++.h> 

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
    // Write to the teletype the posting list and its relative entry in the lexicon
    for(const auto& [term, array] : inverted_index)
    {
        // Iterate through all docids
        auto iter_docid = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.first;});

        // Encode the posting lists of the relative term using the variable bytes algorithm 
        codes::VariableBlocksEncoder doc_encoder(iter_docid.begin(), iter_docid.end());

        // Save starting offset of the posting list
        const auto docid_start_offset = docid_teletype.tellp();

        // Save docid posting list
        for(uint8_t byte : doc_encoder)
            docid_teletype.put(byte);

        // Save ending offset of the posting list
        const auto docid_end_offset = docid_teletype.tellp();

        // Iterate through all docids
        auto iter_freq = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.second;});

        // Encode the posting lists of the relative term using the variable bytes algorithm 
        codes::VariableBlocksEncoder freq_encoder(iter_freq.begin(), iter_freq.end());

        // Save starting offset of the posting list
        const auto freq_start_offset = freq_teletype.tellp();

        // Save freq posting list
        for(uint8_t byte : freq_encoder)
            freq_teletype.put(byte);

        // Save starting offset of the posting list
        const auto freq_end_offset = freq_teletype.tellp();
            
        // @TODO write offset in the lexicon
        lexicon_teletype.write(term.c_str(), term.size());
        lexicon_teletype.write((char*)&docid_start_offset, sizeof(docid_start_offset));
        lexicon_teletype.write((char*)&docid_end_offset, sizeof(docid_end_offset));
        lexicon_teletype.write((char*)&freq_start_offset, sizeof(freq_start_offset));
        lexicon_teletype.write((char*)&freq_end_offset, sizeof(freq_end_offset));
    }

    // Document index part
    for(const auto& [docid, document] : document_index)
    {
        document_index_teletype.write((char*)&docid, sizeof(docid));
        document_index_teletype.write((char*)&document, sizeof(DocumentInfo));
    }
}

}

