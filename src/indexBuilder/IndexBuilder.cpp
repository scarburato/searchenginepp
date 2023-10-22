#include "IndexBuilder.hpp"
#include "../index/types.hpp"
#include <cstdint>
#include <ranges>
#include <iostream>

namespace index 
{

// Function that writes to disk the inverted index
void IndexBuilder::write_to_disk(std::ostream& inverted_indices_teletype, std::ostream& lexicon_teletype)
{
    for(const auto& [term, array] : term_to_inverted_index)
    {
        auto iter_docid = array | std::ranges::views::transform([](std::pair<docid_t, freq_t> p) {return p.first;});

        codes::VariableBlocksEncoder encoder(iter_docid.begin(), iter_docid.end());

        for(uint8_t byte : encoder)
            inverted_indices_teletype.put(byte);

    }
}

}

