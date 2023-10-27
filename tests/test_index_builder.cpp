#include "gtest/gtest.h"
#include "indexBuilder/IndexBuilder.hpp"
#include "indexBuilder/IndexBuilder.cpp"

TEST(IndexBuilder, write_to_disk)
{
    sindex::IndexBuilder builder;

    // Create a stringsteam for every teletype
    std::ostringstream docid_teletype_stream;
    std::ostringstream freq_teletype_stream;
    std::ostringstream lexicon_teletype_stream;
    std::ostringstream document_index_teletype_stream;


    builder.write_to_disk(docid_teletype_stream, freq_teletype_stream, lexicon_teletype_stream, document_index_teletype_stream);

    // Saving data into strings
    std::string docid_teletype_data = docid_teletype_stream.str();
    std::string freq_teletype_data = freq_teletype_stream.str();
    std::string lexicon_teletype_data = lexicon_teletype_stream.str();
    std::string document_index_teletype_data = document_index_teletype_stream.str();

}