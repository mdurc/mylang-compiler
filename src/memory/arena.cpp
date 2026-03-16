#include "arena.h"
#include <cstdlib>
#include <algorithm>

ArenaAllocator::ArenaAllocator(size_t chunk_size) : m_chunk_size(chunk_size) {
  allocate_new_chunk();
}

ArenaAllocator::~ArenaAllocator() {
  for (Chunk& chunk : m_chunks) {
    std::free(chunk.data);
  }
}

void ArenaAllocator::allocate_new_chunk() {
  Chunk chunk;
  chunk.size = m_chunk_size;
  chunk.used = 0;
  chunk.data = static_cast<std::uint8_t*>(std::malloc(m_chunk_size));
  m_chunks.push_back(chunk);
}

void* ArenaAllocator::allocate(size_t size, size_t align) {
  Chunk& current_chunk = m_chunks.back();

  // calculate the padding needed to satisfy the alignment requirement
  size_t current_addr = reinterpret_cast<size_t>(current_chunk.data + current_chunk.used);
  size_t padding = (align - (current_addr % align)) % align;

  if (current_chunk.used + padding + size > current_chunk.size) {
    m_chunk_size = std::max(m_chunk_size, size);
    allocate_new_chunk();
    return allocate(size, align); // try again in the new chunk
  }

  // apply padding
  size_t aligned_offset = current_chunk.used + padding;
  void* result = current_chunk.data + aligned_offset;

  // bump the ptr
  current_chunk.used = aligned_offset + size;

  return result;
}

void ArenaAllocator::reset() {
  for (Chunk& chunk : m_chunks) {
    chunk.used = 0;
  }
}
