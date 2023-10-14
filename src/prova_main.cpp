#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <vector>
#include <list>

#include "codes/variable_blocks.hpp"

using namespace std;

int main() {
	const std::vector<uint64_t > test0_data{3, 67822l};
	const std::vector<uint8_t> real{0b00000011, 0b01101110, 0b10010001, 0b10000100};

	codes::VariableBlocksEncoder encoder(test0_data.begin(), test0_data.end());

	for(auto byte : encoder)
		std::cout << (uint32_t )(byte) << ' ';
	std::cout << std::endl;

	for(auto byte : real)
		std::cout << (uint32_t )(byte) << ' ';
	std::cout << std::endl;

	return 0;
}