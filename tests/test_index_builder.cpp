#include "gtest/gtest.h"
#include "indexBuilder/IndexBuilder.hpp"
#include "codes/variable_blocks.hpp"
#include "codes/unary.hpp"

TEST(IndexBuilder, write_to_disk)
{
    sindex::IndexBuilder builder;

	builder.add_to_doc(1, {.docno = 0xcafe, .lenght = 1});
	builder.add_to_doc(2, {.docno = 0xbabe, .lenght = 2});
	builder.add_to_doc(3, {.docno = 0xbeaf, .lenght = 1});

	builder.add_to_post("banano", 1, 1);
	builder.add_to_post("banano", 2, 2);
	builder.add_to_post("banano", 3, 1);

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

	ASSERT_EQ(docid_teletype_stream.str(), "\x1\x2\x3");
	ASSERT_EQ(freq_teletype_stream.str(), "\x2"); //0b00000010 = 0x02
	// The first 8 bytes is the number of buckets, we don't care about it
	//ASSERT_EQ(lexicon_teletype_stream.str().substr(sizeof(uint64_t)), "banano\000\000\000\000\000\000\000\000\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000");
	//ASSERT_EQ(document_index_teletype_stream.str(), ""); // @TODO
}