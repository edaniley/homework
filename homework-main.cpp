
#include <hw/CBuffer.h>
#include <iostream>
#include <bitset>
#include <memory>

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>

#ifndef HW_UNIT_TEST
#define HW_UNIT_TEST
#endif

#include <TimeUtil.h>
#include <hw/CBuffer.h>
#include <OrderStateManagment.h>

extern int rbc_main(int argc, char **argv);
//extern int tcpd_ds_efvi_ct_rx_main(int argc, char* argv[]);
//extern int tcpd_ds_efvi_main(int argc, char* argv[]);

//constexpr uint64_t foo(uint64_t val) {
//  return __builtin_bswap64(val);
//}

using namespace std;
constexpr size_t SIZE_BITS = 22;
constexpr size_t BUFF_SIZE = 1 << 22;
constexpr size_t OFFSET_MASK = BUFF_SIZE-1;
constexpr size_t BASE_MASK = ~OFFSET_MASK;

template <typename T>
void Print(T val) {
    cout << bitset<sizeof(size_t)*8>((size_t)val) << " val:"<< ((size_t)val) << endl;
}
const size_t PAGE_SIZE = getpagesize();


void test() {
  hw::CBuffer<4096> buff("test");

  Print(SIZE_BITS);
  Print(BUFF_SIZE);
  Print(BASE_MASK);
  Print(OFFSET_MASK);
  Print(PAGE_SIZE);
  cout << "----------------------" << endl;
  const size_t sz =(BUFF_SIZE / PAGE_SIZE) * (PAGE_SIZE + 1);
  Print(sz);

//    const auto fd = fileno(tmpfile ());
//    ftruncate(fd, sz);
//    [[maybe_unused]]
//     char *buff = (char *)mmap(NULL, 2 * sz, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

}

int main(int argc, char **argv) {
  //test_OrderStateManagment();
  test_CBuffer();
//  exit(0);
//  constexpr size_t BITS=1<<9;
//  uint64_t x = foo(123);
//  std::cout << "x:" << x << std::endl;
//  uint64_t y = foo(hw::rdtsc());
//  std::cout << "y:" << y  << std::endl;
//  std::cout << std::bitset<sizeof(size_t)>(BITS) << std::endl;
//
////return rbc_main(argc, argv);
//  std::cout << "rdtsc:" << hw::rdtsc() << std::endl;
  return 0;
}
