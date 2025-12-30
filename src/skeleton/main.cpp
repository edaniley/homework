#include <iostream>


#include <hw/type/NameTag.hpp>
#include <hw/type/NamedType.hpp>
#include <hw/type/TypeInfo.hpp>
#include <hw/type/TypeList.hpp>

void test_types() {
  using namespace hw::type;
  std::cout << TypeName<char[6]>() << std::endl;
  using L = type_list<char[6], double, long double, char, int>;
  std::cout << TypeListToString<L>() << std::endl;
}

int main() {
  test_types();

  return 0;
}

