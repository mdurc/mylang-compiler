#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class ArenaAllocator {
public:
  /* 8 Mb per chunk by default */
  explicit ArenaAllocator(size_t chunk_size = 8 * 1024 * 1024);

  /* frees all chunks */
  ~ArenaAllocator();

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  void* allocate(size_t size, size_t align = alignof(std::max_align_t));

  template <typename T, typename... Args>
  T* make(Args&&... args) {
    void* memory = allocate(sizeof(T), alignof(T));
    return new (memory) T(std::forward<Args>(args)...);
  }

  /* free up the memory without releasing chunks to OS */
  void reset();

private:
  struct Chunk {
    std::uint8_t* data;
    size_t size;
    size_t used;
  };

  size_t m_chunk_size;
  std::vector<Chunk> m_chunks;

  void allocate_new_chunk();
};

#endif // MEMORY_ARENA_H
