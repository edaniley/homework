#pragma once

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cassert>

namespace hw {

template<size_t SIZE>
struct CBuffer {
  static constexpr size_t PAGE_SIZE = 4*1024;
  static_assert(!(SIZE & (SIZE-1))); // must be power of 2
  static_assert(SIZE >= PAGE_SIZE);

  char *m_buff = nullptr;
  char *m_read = nullptr;
  char *m_write = nullptr;
  size_t m_size = 0;
  size_t m_total_size = 0;
  CBuffer(const char *name) {
    int fd = shm_open(name, O_RDWR|O_CREAT, 0600);
    ftruncate(fd, 4096*2);
    m_buff = (char *)mmap(NULL, SIZE, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
    char *rem = (char *)mmap(m_buff + SIZE, SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_LOCKED|MAP_FIXED, fd, 0);
    close(fd);
    rem[0] = 'X';
    assert(m_buff[0] == rem[0]);
    m_read = m_write = m_buff;
  }

  size_t Capacity() const { return SIZE; }
  size_t Size() const { return m_size; }
  size_t Available() const { return Capacity() - Size(); }

  void AdvancePtr(char *&ptr, size_t size) {
    ptr += size;
    size_t d = ptr - m_buff;
    if (d >= SIZE) {
      ptr -= SIZE;
    }
    assert(*ptr == *(ptr+SIZE));
  }

  char *BeginWrite() const  { return m_write; }
  char *BeginRead() const  { return m_read; }
  void CommitWrite(size_t size) {
    AdvancePtr(m_write, size);
    m_size += size;
    m_total_size += size;
  }
  void CommitRead(size_t size) {
    AdvancePtr(m_read, size);
    m_size -= size;
  }
};

}

#ifdef HW_UNIT_TEST
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string_view>

using TestBuffer = hw::CBuffer<4096>;
enum OPT {READ, WRITE};

static void test_Write(TestBuffer &buffer) {
  const size_t sz = (uint16_t)std::rand() % 128;
  if (sz + sizeof(uint16_t) <= buffer.Available()) {
    const char chr = 'A' + std::rand() % 26;
    uint16_t *hrd =  (uint16_t*)buffer.BeginWrite();
    *hrd = (uint16_t)sz;
    memset(hrd+1, chr, sz);
    buffer.CommitWrite(sizeof(uint16_t)+sz);
    std::cout << "test_Write: success : " << sz << " [" << chr << ']' << std::endl;
  }
  else {
    std::cout << "test_Write: failed : " << sz << std::endl;
  }
}
static void test_Read(TestBuffer &buffer) {
  if (buffer.Size()) {
    uint16_t *psz = (uint16_t*)buffer.BeginRead();
    buffer.CommitRead(sizeof(uint16_t)+*psz);
    char *chr = (char*)(psz+1);
    std::cout << "test_Read: " << *psz << '"' << std::string_view(chr, *psz) << std::endl;
  }
  else {
    std::cout << "test_Read: empty" << std::endl;
  }
}

void PrintState(const TestBuffer &buffer) {
  const ptrdiff_t rd =  buffer.m_read - buffer.m_buff;
  const ptrdiff_t wr =  buffer.m_write - buffer.m_buff;
  std::cout << "wr:" << wr << " rd:" << rd << " sz:" << buffer.Size()
    << " tsz:" << buffer.m_total_size << std::endl;
}

inline
void test_CBuffer () {
  std::srand(std::time(nullptr));
  static bool opts[] = {READ, READ, WRITE, WRITE, WRITE };
  TestBuffer buffer("unit-test");
  for (int i = 0; i < 100000; ++i) {
    const auto opt = opts[std::rand() % (sizeof(opts)/sizeof(opts[0]))];
    if (opt == WRITE) {
      test_Write(buffer);
    }
    else {
      test_Read(buffer);
    }
    PrintState(buffer);
  }
  std::cout << "-------------------------------------" << std::endl;
  while (buffer.Size()) {
    test_Read(buffer);
    PrintState(buffer);
  }
}
#endif
