#include <iostream>


#include <hw/type/NameTag.hpp>
#include <hw/type/NamedType.hpp>
#include <hw/type/TypeInfo.hpp>
#include <hw/type/TypeList.hpp>
#include <hw/type/beacon/Beacon.hpp>
#include <hw/utility/Text.hpp>
#include <hw/utility/Format.hpp>
#include <hw/type/beacon/TypeTraits.hpp>
#include <hw/type/beacon/Numeric.hpp>
#include <hw/type/beacon/Enum.hpp>
#include <hw/type/beacon/String.hpp>
#include <hw/type/beacon/Opaque.hpp>
#include <hw/utility/Buffer.hpp>
#include <hw/utility/Allocator.hpp>
#include <hw/utility/Time.hpp>
#include <hw/utility/Clock.hpp>
#include <hw/utility/cce/Counter.hpp>
#include <hw/utility/cce/FastHashTable.hpp>
// #include <hw/utility/cce/OrderBurstControl.hpp>
#include <hw/utility/cce/OrderCounter.hpp>
#include <hw/utility/cce/HashTable.hpp>
#include <hw/utility/OrderBurstControl.hpp>

void test_burst_control() {
  using namespace hw::utility;
  OrderBurstControl ctl(std::chrono::milliseconds(10), 10,  std::chrono::milliseconds(5), 3);
  const auto start = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1)).count();

  ctl.evaluate(start);
  for (int i = 0; i < 30; ++i) {
    ctl.evaluate(start + (delta * i));
  }
  exit(0);

}
void test_utilities() {
  using namespace hw::utility;
  using namespace hw::utility::cce;
  OrderCounter<10> oc(std::chrono::milliseconds(20), 500);


  SwissTableHashmap<int, 1024> mm;
}

void test_types() {
  using namespace hw::type;
  std::cout << TypeName<char[6]>() << std::endl;
  using L = type_list<char[6], double, long double, char, int>;
  std::cout << TypeListToString<L>() << std::endl;
}

void test_text() {
  using namespace hw::utility;
  char buff[1024];
  strcpy(buff, "7098709870987098709870979087 using namespace hw::utility;");
  std::cout << frmt::format("{}", toHex(buff, 256)) << std::endl;
}

int main() {
  test_burst_control();
  test_utilities();

  return 0;
}

