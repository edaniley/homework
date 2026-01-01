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
  test_text();

  return 0;
}

