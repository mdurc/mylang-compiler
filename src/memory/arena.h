#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
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

  /* use a span to easily allow vector-like allocations that don't
     need to be destructured to free any heap memory */
  template <typename T>
  std::span<T> allocate_array(size_t count) {
    if (count == 0) return std::span<T>();
    void* memory = allocate(count * sizeof(T), alignof(T));
    return std::span<T>(static_cast<T*>(memory), count);
  }

  /* helper to create new strings in the arena */
  std::string_view make_string(const std::string& str);

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
