#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Index.hpp"

namespace sindex
{

/**
 * This procedure mmaps a file in memory
 * @param filename
 * @return
 */
static std::pair<void*, size_t> mmap_helper(char const *const filename)
{
	auto fd = open(filename, O_RDONLY);

	// get file's size
	struct stat st{};
	auto ret_stat= stat(filename, &st);

	if(ret_stat == -1)
		abort();

	// map file in memory
	void* mapped = (uint8_t *)mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

	close(fd);

	return {mapped, st.st_size};
}

Index::Index(const std::string &map_filename, const std::string &inverted_indices_filename)
{
	auto inverted_indices_minfo = mmap_helper(inverted_indices_filename.c_str());
	inverted_indices = (uint8_t*)inverted_indices_minfo.first;
	inverted_indices_length = inverted_indices_minfo.second;
}

Index::~Index()
{
	munmap(inverted_indices, inverted_indices_length);
}

}
