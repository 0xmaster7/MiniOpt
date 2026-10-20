#pragma once
#include <cstdint>
#include <istream>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace miniopt {
struct Operand { bool literal = false; std::int32_t value = 0; std::string name; };
enum class Kind { Label, Assign, Jump, Branch, Return };
struct Instruction {
    Kind kind = Kind::Label;
    std::string name, op, target;
    Operand a, b;
    std::size_t line = 0;
};
using Program = std::vector<Instruction>;
struct Block { std::size_t begin, end; std::vector<std::size_t> successors, predecessors; bool reachable = false; };
struct Outcome { std::string status; std::int32_t value = 0; std::size_t steps = 0; };
Program parse(std::istream& input);
std::string format(const Program& p);
std::vector<Block> cfg(const Program& p);
Outcome run(const Program& p, std::size_t limit = 100000);
struct OptimizationMetrics { std::size_t unreachable_blocks_removed = 0; };
Program optimize(Program p, OptimizationMetrics* metrics = nullptr);
std::size_t instruction_count(const Program& p);
std::set<std::string> uses(const Instruction& i);
}
