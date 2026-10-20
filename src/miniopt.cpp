#include "miniopt.hpp"
#include <algorithm>
#include <charconv>
#include <deque>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace miniopt {
namespace {
const std::string ident = "[A-Za-z_][A-Za-z_0-9]*";
const std::string atom = "(-?[0-9]+|" + ident + ")";
Operand operand(const std::string &s) {
  Operand o;
  if (s[0] == '-' || (s[0] >= '0' && s[0] <= '9')) {
    o.literal = true;
    auto r = std::from_chars(s.data(), s.data() + s.size(), o.value);
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size())
      throw std::runtime_error("integer outside signed 32-bit range: " + s);
  } else
    o.name = s;
  return o;
}
std::int32_t wrap(std::uint32_t n) {
  if (n <= 2147483647U)
    return static_cast<std::int32_t>(n);
  return static_cast<std::int32_t>(static_cast<std::int64_t>(n) - 4294967296LL);
}
std::int32_t calc(const std::string &op, std::int32_t a, std::int32_t b) {
  auto x = static_cast<std::uint32_t>(a), y = static_cast<std::uint32_t>(b);
  if (op.empty())
    return a;
  if (op == "+")
    return wrap(x + y);
  if (op == "-")
    return wrap(x - y);
  if (op == "*")
    return wrap(x * y);
  if (op == "/") {
    if (b == 0)
      throw std::runtime_error("division_by_zero");
    if (a == std::numeric_limits<std::int32_t>::min() && b == -1)
      return a;
    return a / b;
  }
  if (op == "==")
    return a == b;
  if (op == "!=")
    return a != b;
  if (op == "<")
    return a < b;
  if (op == "<=")
    return a <= b;
  if (op == ">")
    return a > b;
  if (op == ">=")
    return a >= b;
  throw std::runtime_error("invalid_operator");
}
std::string show(const Operand &o) {
  return o.literal ? std::to_string(o.value) : o.name;
}
std::map<std::string, std::size_t> labels(const Program &p) {
  std::map<std::string, std::size_t> m;
  for (std::size_t n = 0; n < p.size(); ++n)
    if (p[n].kind == Kind::Label)
      m.emplace(p[n].name, n);
  return m;
}
using Constants = std::map<std::string, std::int32_t>;
std::optional<std::int32_t> known(const Operand &o, const Constants &env) {
  if (o.literal)
    return o.value;
  auto it = env.find(o.name);
  if (it != env.end())
    return it->second;
  return std::nullopt;
}
void transfer(const Instruction &i, Constants &env) {
  if (i.kind != Kind::Assign)
    return;
  auto a = known(i.a, env),
       b = i.op.empty() ? std::optional<std::int32_t>(0) : known(i.b, env);
  if (a && b && !(i.op == "/" && *b == 0))
    env[i.name] = calc(i.op, *a, *b);
  else
    env.erase(i.name);
}
Constants meet(const Constants &a, const Constants &b) {
  Constants r;
  for (const auto &[key, val] : a) {
    auto it = b.find(key);
    if (it != b.end() && it->second == val)
      r[key] = val;
  }
  return r;
}
std::set<std::string> intersection(const std::set<std::string> &a,
                                   const std::set<std::string> &b) {
  std::set<std::string> r;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                        std::inserter(r, r.end()));
  return r;
}
} // namespace
Program parse(std::istream &in) {
  const std::regex label("^\\s*(" + ident + "):\\s*$"),
      jump("^\\s*goto\\s+(" + ident + ")\\s*$"),
      branch("^\\s*if\\s+" + atom + "\\s+goto\\s+(" + ident + ")\\s*$"),
      ret("^\\s*return\\s+" + atom + "\\s*$"),
      assign("^\\s*(" + ident + ")\\s*=\\s*" + atom +
             "(?:\\s*(==|!=|<=|>=|[+*/<>-])\\s*" + atom + ")?\\s*$");
  Program p;
  std::string line;
  std::size_t n = 0;
  std::set<std::string> names;
  while (std::getline(in, line)) {
    ++n;
    auto comment = line.find('#');
    if (comment != std::string::npos)
      line.resize(comment);
    if (line.find_first_not_of(" \t\r\n") == std::string::npos)
      continue;
    std::smatch m;
    Instruction i;
    i.line = n;
    if (std::regex_match(line, m, label)) {
      i.kind = Kind::Label;
      i.name = m[1];
      if (!names.insert(i.name).second)
        throw std::runtime_error("duplicate label at line " +
                                 std::to_string(n));
    } else if (std::regex_match(line, m, jump)) {
      i.kind = Kind::Jump;
      i.target = m[1];
    } else if (std::regex_match(line, m, branch)) {
      i.kind = Kind::Branch;
      i.a = operand(m[1]);
      i.target = m[2];
    } else if (std::regex_match(line, m, ret)) {
      i.kind = Kind::Return;
      i.a = operand(m[1]);
    } else if (std::regex_match(line, m, assign)) {
      i.kind = Kind::Assign;
      i.name = m[1];
      i.a = operand(m[2]);
      i.op = m[3];
      if (!i.op.empty())
        i.b = operand(m[4]);
    } else
      throw std::runtime_error("malformed instruction at line " +
                               std::to_string(n));
    p.push_back(std::move(i));
  }
  if (p.empty())
    throw std::runtime_error("empty program");
  for (const auto &i : p)
    if ((i.kind == Kind::Jump || i.kind == Kind::Branch) &&
        !names.count(i.target))
      throw std::runtime_error("unknown label: " + i.target);
  return p;
}
std::string format(const Program &p) {
  std::ostringstream out;
  for (const auto &i : p) {
    if (i.kind == Kind::Label)
      out << i.name << ":";
    else if (i.kind == Kind::Assign) {
      out << "  " << i.name << " = " << show(i.a);
      if (!i.op.empty())
        out << " " << i.op << " " << show(i.b);
    } else if (i.kind == Kind::Jump)
      out << "  goto " << i.target;
    else if (i.kind == Kind::Branch)
      out << "  if " << show(i.a) << " goto " << i.target;
    else
      out << "  return " << show(i.a);
    out << '\n';
  }
  return out.str();
}
std::set<std::string> uses(const Instruction &i) {
  std::set<std::string> r;
  if (i.kind == Kind::Assign || i.kind == Kind::Branch ||
      i.kind == Kind::Return) {
    if (!i.a.literal)
      r.insert(i.a.name);
    if (i.kind == Kind::Assign && !i.op.empty() && !i.b.literal)
      r.insert(i.b.name);
  }
  return r;
}
std::vector<Block> cfg(const Program &p) {
  if (p.empty())
    return {};
  std::set<std::size_t> leaders{0};
  auto ls = labels(p);
  for (std::size_t n = 0; n < p.size(); ++n) {
    if (p[n].kind == Kind::Label)
      leaders.insert(n);
    if ((p[n].kind == Kind::Jump || p[n].kind == Kind::Branch ||
         p[n].kind == Kind::Return) &&
        n + 1 < p.size())
      leaders.insert(n + 1);
  }
  std::vector<std::size_t> starts(leaders.begin(), leaders.end()),
      owner(p.size());
  std::vector<Block> blocks;
  for (std::size_t b = 0; b < starts.size(); ++b) {
    auto end = b + 1 < starts.size() ? starts[b + 1] : p.size();
    blocks.push_back({starts[b], end, {}, {}, false});
    for (auto n = starts[b]; n < end; ++n)
      owner[n] = b;
  }
  for (std::size_t b = 0; b < blocks.size(); ++b) {
    const auto &last = p[blocks[b].end - 1];
    auto &s = blocks[b].successors;
    if (last.kind == Kind::Jump || last.kind == Kind::Branch)
      s.push_back(owner.at(ls.at(last.target)));
    if (last.kind != Kind::Jump && last.kind != Kind::Return &&
        b + 1 < blocks.size())
      s.push_back(b + 1);
    std::sort(s.begin(), s.end());
    s.erase(std::unique(s.begin(), s.end()), s.end());
    for (auto to : s)
      blocks[to].predecessors.push_back(b);
  }
  std::vector<std::size_t> stack{0};
  while (!stack.empty()) {
    auto b = stack.back();
    stack.pop_back();
    if (blocks[b].reachable)
      continue;
    blocks[b].reachable = true;
    for (auto s : blocks[b].successors)
      stack.push_back(s);
  }
  return blocks;
}
Outcome run(const Program &p, std::size_t limit) {
  auto ls = labels(p);
  std::map<std::string, std::int32_t> env;
  std::size_t pc = 0, steps = 0;
  auto read = [&](const Operand &o) {
    if (o.literal)
      return o.value;
    auto it = env.find(o.name);
    if (it == env.end())
      throw std::runtime_error("undefined_variable:" + o.name);
    return it->second;
  };
  try {
    while (pc < p.size()) {
      const auto &i = p[pc];
      if (i.kind == Kind::Label) {
        ++pc;
        continue;
      }
      if (steps == limit)
        return {"limit", 0, steps};
      ++steps;
      if (i.kind == Kind::Assign) {
        auto a = read(i.a);
        auto b = i.op.empty() ? 0 : read(i.b);
        env[i.name] = calc(i.op, a, b);
        ++pc;
      } else if (i.kind == Kind::Jump)
        pc = ls.at(i.target);
      else if (i.kind == Kind::Branch)
        pc = read(i.a) != 0 ? ls.at(i.target) : pc + 1;
      else
        return {"return", read(i.a), steps};
    }
    return {"fell_off", 0, steps};
  } catch (const std::runtime_error &e) {
    return {e.what(), 0, steps};
  }
}
Program optimize(Program p, OptimizationMetrics *metrics) {
  if (metrics)
    metrics->unreachable_blocks_removed = 0;
  for (;;) {
    auto before = format(p);
    auto blocks = cfg(p);
    const auto count = blocks.size();
    // Bottom = no path yet, top = arbitrary or undefined value; meet keeps
    // equal constants.
    std::vector<std::optional<Constants>> in(count), out(count);
    bool changed = true;
    while (changed) {
      changed = false;
      for (std::size_t b = 0; b < count; ++b) {
        std::optional<Constants> state;
        if (b == 0)
          state = Constants{};
        for (auto pred : blocks[b].predecessors)
          if (out[pred])
            state = state ? meet(*state, *out[pred]) : out[pred];
        if (!state)
          continue;
        if (in[b] != state) {
          in[b] = state;
          changed = true;
        }
        for (auto n = blocks[b].begin; n < blocks[b].end; ++n)
          transfer(p[n], *state);
        if (out[b] != state) {
          out[b] = state;
          changed = true;
        }
      }
    }
    Program folded;
    for (std::size_t b = 0; b < count; ++b) {
      if (!blocks[b].reachable) {
        if (metrics)
          ++metrics->unreachable_blocks_removed;
        continue;
      }
      auto env = in[b].value_or(Constants{});
      for (auto n = blocks[b].begin; n < blocks[b].end; ++n) {
        auto i = p[n];
        auto substitute = [&](Operand &o) {
          if (auto v = known(o, env)) {
            o.literal = true;
            o.value = *v;
            o.name.clear();
          }
        };
        if (i.kind == Kind::Assign || i.kind == Kind::Branch ||
            i.kind == Kind::Return)
          substitute(i.a);
        if (i.kind == Kind::Assign) {
          if (!i.op.empty())
            substitute(i.b);
          if (i.a.literal && (i.op.empty() || i.b.literal) &&
              !(i.op == "/" && i.b.value == 0)) {
            i.a.value = calc(i.op, i.a.value, i.b.value);
            i.op.clear();
            i.b = Operand{};
          }
          transfer(i, env);
        }
        if (i.kind == Kind::Branch && i.a.literal) {
          if (i.a.value == 0)
            continue;
          i.kind = Kind::Jump;
        }
        folded.push_back(std::move(i));
      }
    }
    p = std::move(folded);
    blocks = cfg(p);
    // Must-defined analysis preserves errors from otherwise-dead reads of
    // undefined variables.
    std::set<std::string> universe;
    for (const auto &i : p)
      if (i.kind == Kind::Assign)
        universe.insert(i.name);
    std::vector<std::set<std::string>> defined_in(blocks.size(), universe),
        defined_out(blocks.size(), universe);
    changed = true;
    while (changed) {
      changed = false;
      for (std::size_t b = 0; b < blocks.size(); ++b) {
        if (!blocks[b].reachable)
          continue;
        auto state = b == 0 ? std::set<std::string>{} : universe;
        for (auto pred : blocks[b].predecessors)
          if (blocks[pred].reachable)
            state = intersection(state, defined_out[pred]);
        if (defined_in[b] != state) {
          defined_in[b] = state;
          changed = true;
        }
        for (auto n = blocks[b].begin; n < blocks[b].end; ++n)
          if (p[n].kind == Kind::Assign)
            state.insert(p[n].name);
        if (defined_out[b] != state) {
          defined_out[b] = state;
          changed = true;
        }
      }
    }
    std::vector<bool> safe(p.size(), false);
    for (std::size_t b = 0; b < blocks.size(); ++b) {
      auto defined = defined_in[b];
      for (auto n = blocks[b].begin; n < blocks[b].end; ++n) {
        const auto &i = p[n];
        auto reads = uses(i);
        safe[n] = std::includes(defined.begin(), defined.end(), reads.begin(),
                                reads.end()) &&
                  (i.op != "/" || (i.b.literal && i.b.value != 0));
        if (i.kind == Kind::Assign)
          defined.insert(i.name);
      }
    }
    std::vector<std::set<std::string>> live_in(blocks.size()),
        live_out(blocks.size());
    changed = true;
    while (changed) {
      changed = false;
      for (std::size_t b = blocks.size(); b-- > 0;) {
        std::set<std::string> live;
        for (auto s : blocks[b].successors)
          live.insert(live_in[s].begin(), live_in[s].end());
        auto after = live;
        for (auto n = blocks[b].end; n-- > blocks[b].begin;) {
          const auto &i = p[n];
          if (i.kind == Kind::Assign)
            live.erase(i.name);
          auto reads = uses(i);
          live.insert(reads.begin(), reads.end());
        }
        if (live_in[b] != live || live_out[b] != after) {
          live_in[b] = std::move(live);
          live_out[b] = std::move(after);
          changed = true;
        }
      }
    }
    std::vector<bool> keep(p.size(), true);
    for (std::size_t b = 0; b < blocks.size(); ++b) {
      if (!blocks[b].reachable) {
        if (metrics)
          ++metrics->unreachable_blocks_removed;
        for (auto n = blocks[b].begin; n < blocks[b].end; ++n)
          keep[n] = false;
        continue;
      }
      auto live = live_out[b];
      for (auto n = blocks[b].end; n-- > blocks[b].begin;) {
        const auto &i = p[n];
        if (i.kind == Kind::Assign && !live.count(i.name) && safe[n]) {
          keep[n] = false;
          continue;
        }
        if (i.kind == Kind::Assign)
          live.erase(i.name);
        auto reads = uses(i);
        live.insert(reads.begin(), reads.end());
      }
    }
    Program result;
    for (std::size_t n = 0; n < p.size(); ++n)
      if (keep[n])
        result.push_back(p[n]);
    p = std::move(result);
    if (p.empty()) {
      Instruction empty;
      empty.kind = Kind::Label;
      empty.name = "__miniopt_empty";
      p.push_back(empty);
    }
    if (format(p) == before)
      return p;
  }
}
std::size_t instruction_count(const Program &p) {
  return static_cast<std::size_t>(std::count_if(
      p.begin(), p.end(), [](const auto &i) { return i.kind != Kind::Label; }));
}
} // namespace miniopt
