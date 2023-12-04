#pragma once

#include <cstdint>
#include <utility>
#include <cstddef>
#include <string>

class memory_area
{
public:
	virtual ~memory_area() = default;
	virtual std::pair<uint8_t *, size_t> get() const = 0;
};

class memory_mmap: public memory_area
{
	uint8_t *buff;
	size_t buff_size;
public:
	explicit memory_mmap(const std::string& filename);
	memory_mmap(memory_mmap&&);
	~memory_mmap() override;
	std::pair<uint8_t *, size_t> get() const override;
};

class memory_buffer: public memory_area
{
	uint8_t *buff;
	size_t buff_size;
public:
	memory_buffer(uint8_t *buff, size_t buff_size):
		buff(buff), buff_size(buff_size) {}
	~memory_buffer() override {};
	std::pair<uint8_t *, size_t> get() const override;
};
