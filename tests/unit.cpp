#include "miniopt.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
void check(bool b) {
  if (!b)
    throw std::runtime_error("unit assertion failed");
}
miniopt::Program parse(const std::string &s) {
  std::istringstream in(s);
  return miniopt::parse(in);
}
int main() {
  auto p = parse("entry:\nx = 1\nif x goto yes\nreturn 0\nyes:\nreturn x\n");
  auto blocks = miniopt::cfg(p);
  check(blocks.size() == 3);
  check(blocks[0].successors.size() == 2);
  check(blocks[2].predecessors.size() == 1);
  auto q = miniopt::optimize(p);
  check(miniopt::run(q).value == 1);
  check(miniopt::instruction_count(q) == 2);
  check(miniopt::format(miniopt::optimize(q)) == miniopt::format(q));
  auto trap =
      miniopt::run(miniopt::optimize(parse("x = missing + 1\nreturn 0\n")));
  check(trap.status == "undefined_variable:missing");
  check(miniopt::run(parse("x = 2147483647 + 1\nreturn x\n")).value ==
        -2147483647 - 1);
  for (const auto &s : {"x = 2147483648\n", "goto absent\n",
                        "a:\na:\nreturn 0\n", "x = 1 +\n"}) {
    bool rejected = false;
    try {
      (void)parse(s);
    } catch (const std::exception &) {
      rejected = true;
    }
    check(rejected);
  }
  std::cout << "unit assertions passed\n";
}
