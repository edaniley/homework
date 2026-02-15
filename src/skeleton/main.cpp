#include <iostream>


// #include <hw/type/NameTag.hpp>
// #include <hw/type/NamedType.hpp>
// #include <hw/type/TypeInfo.hpp>
// #include <hw/type/TypeList.hpp>
// #include <hw/type/beacon/Beacon.hpp>
// #include <hw/utility/Text.hpp>
// #include <hw/utility/Format.hpp>
// #include <hw/type/beacon/TypeTraits.hpp>
// #include <hw/type/beacon/Numeric.hpp>
// #include <hw/type/beacon/Enum.hpp>
// #include <hw/type/beacon/String.hpp>
// #include <hw/type/beacon/Opaque.hpp>
// #include <hw/utility/Buffer.hpp>
// #include <hw/utility/Allocator.hpp>
// #include <hw/utility/Time.hpp>
// #include <hw/utility/Clock.hpp>
// #include <hw/utility/cce/Counter.hpp>
// #include <hw/utility/cce/FastHashTable.hpp>
// // #include <hw/utility/cce/OrderBurstControl.hpp>
// #include <hw/utility/cce/OrderCounter.hpp>
// #include <hw/utility/cce/HashTable.hpp>
// #include <hw/utility/OrderBurstControl.hpp>

// #include <hw/utility/PriorityQueue.hpp>
// #include <hw/assembly/Timer.hpp>
// #include <hw/utility/CPU.hpp>
// #include <hw/assembly/Ether.hpp>
// #include <hw/assembly/Config.hpp>
// #include <hw/assembly/Context.hpp>
// #include <hw/assembly/Component.hpp>
// #include <hw/utility/EPoller.hpp>
// #include <hw/assembly/Dispatcher.hpp>
// #include <hw/utility/MMap.hpp>
#include <hw/assembly/Assembly.hpp>

// void test_burst_control() {
//   using namespace hw::utility;
//   using Timepoint = std::chrono::system_clock::time_point;
//   using BurstControl = OrderBurstControl<32>;
//   BurstControl ctl(std::chrono::milliseconds(10), 10,  std::chrono::milliseconds(5), 3);
//   const Timepoint zero = std::chrono::system_clock::now();
//   const auto start = zero.time_since_epoch().count();
//   const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1)).count();

//   ctl.evaluate(start);
//   for (int i = 0; i < 30; ++i) {
//     bool rc = ctl.evaluate(start + (delta * i));
//     BurstControl::State state = ctl.state();
//     std::chrono::system_clock::time_point winstart {
//         std::chrono::time_point_cast<std::chrono::system_clock::duration>(
//             std::chrono::system_clock::time_point(
//               std::chrono::nanoseconds(state.start_time)
//             )
//         )
//     };

//     std::cout << frmt::format("rc:{} mode:{} start:{:%T} count:{}",
//         rc, (state.mode == BurstControl::Mode::Normal ? "Normal" : "Cooldown"), winstart, state.total_count) << std::endl;
//   }
//   exit(0);

// }
// void test_utilities() {
//   using namespace hw::utility;
//   using namespace hw::utility::cce;
//   OrderCounter<10> oc(std::chrono::milliseconds(20), 500);


//   SwissTableHashmap<int, 1024> mm;
// }

// void test_types() {
//   using namespace hw::type;
//   std::cout << TypeName<char[6]>() << std::endl;
//   using L = type_list<char[6], double, long double, char, int>;
//   std::cout << TypeListToString<L>() << std::endl;
// }

// void test_text() {
//   using namespace hw::utility;
//   char buff[1024];
//   strcpy(buff, "7098709870987098709870979087 using namespace hw::utility;");
//   std::cout << frmt::format("{}", toHex(buff, 256)) << std::endl;
// }

int main() {
  // test_burst_control();
  // test_utilities();

  return 0;
}

