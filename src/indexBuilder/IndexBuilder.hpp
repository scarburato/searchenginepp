#pragma once

#include <unordered_map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include "../index/types.hpp"
#include "../index/Index.hpp"
#include "../codes/variable_blocks.hpp"

namespace sindex
{

class IndexBuilder
{
private:

    std::unordered_map<std::string, std::vector<std::pair<docid_t, freq_t>>> inverted_index;

    std::unordered_map<docid_t, DocumentInfo> document_index;

public:

    /**
    * Function that adds a frequency of a term associated to a documentID
    * It is assumed to recieve strictly increasing docIDs 
    * @param term index of the inverted index
    * @param id docid
    * @param occurrences number of occurrences of the term
    */
    void add_to_post(const std::string& term, docid_t id, freq_t occurrences)
    {
        // Insert the <docID,freq> pair inside the vector associated to the term
        inverted_index[term].push_back(std::make_pair(id, occurrences));
    }

    /**
    * Inserts the document length inside the document index
    * @param docid index of the document
    * @param doc struct containing the doc number and its length
    */
    void add_to_doc(const docid_t docid, const DocumentInfo& doc)
    {
        document_index[docid] = doc;
    }

    void write_to_disk(std::ostream&, std::ostream&, std::ostream&, std::ostream&);
};

}