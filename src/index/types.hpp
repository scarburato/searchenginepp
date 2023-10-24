#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include "../codes/variable_blocks.hpp"

namespace sindex
{
typedef uint32_t docid_t;
typedef uint32_t docno_t;
typedef size_t freq_t;

template<typename EncondedDataIterator>
using IndexDecoder = codes::VariableBlocksDecoder<EncondedDataIterator, docid_t>;

template<typename RawDataIterator>
using IndexEncoder = codes::VariableBlocksEncoder<RawDataIterator>;

}