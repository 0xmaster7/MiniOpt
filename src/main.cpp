#include "miniopt.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) {
  try {
    if (argc < 3 || argc > 4)
      throw std::runtime_error("usage: miniopt optimize|analyze|run|stats "
                               "input.ir [execution-limit]");
    std::ifstream input(argv[2]);
    if (!input)
      throw std::runtime_error("cannot open input");
    auto p = miniopt::parse(input);
    std::string command = argv[1];
    if (command == "optimize")
      std::cout << miniopt::format(miniopt::optimize(p));
    else if (command == "run") {
      std::size_t limit = 100000;
      if (argc == 4) {
        std::string s = argv[3];
        if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
          throw std::runtime_error("invalid execution limit");
        limit = std::stoull(s);
      }
      auto r = miniopt::run(p, limit);
      std::cout << "{\"status\":\"" << r.status << "\",\"value\":" << r.value
                << ",\"steps\":" << r.steps << "}\n";
    } else if (command == "analyze") {
      auto blocks = miniopt::cfg(p);
      for (std::size_t b = 0; b < blocks.size(); ++b) {
        const auto &block = blocks[b];
        std::cout << "block " << b << " instructions [" << block.begin << ","
                  << block.end << ") reachable=" << block.reachable
                  << " predecessors:";
        for (auto v : block.predecessors)
          std::cout << " " << v;
        std::cout << " successors:";
        for (auto v : block.successors)
          std::cout << " " << v;
        std::cout << '\n';
      }
    } else if (command == "stats") {
      miniopt::OptimizationMetrics metrics;
      auto q = miniopt::optimize(p, &metrics);
      std::vector<double> samples;
      // Warmup above. Batch ten optimizations per sample to reduce timer noise.
      for (int s = 0; s < 21; ++s) {
        auto start = std::chrono::steady_clock::now();
        for (int n = 0; n < 10; ++n) {
          auto result = miniopt::optimize(p);
          if (result.size() != q.size())
            throw std::runtime_error("nondeterministic optimizer");
        }
        samples.push_back(std::chrono::duration<double, std::micro>(
                              std::chrono::steady_clock::now() - start)
                              .count() /
                          10.0);
      }
      std::sort(samples.begin(), samples.end());
      auto blocks = miniopt::cfg(p);
      auto unreachable =
          std::count_if(blocks.begin(), blocks.end(),
                        [](const auto &b) { return !b.reachable; });
      std::cout << "{\"original_instructions\":"
                << miniopt::instruction_count(p)
                << ",\"optimized_instructions\":"
                << miniopt::instruction_count(q)
                << ",\"original_blocks\":" << blocks.size()
                << ",\"optimized_blocks\":" << miniopt::cfg(q).size()
                << ",\"initial_unreachable_blocks\":" << unreachable
                << ",\"unreachable_blocks_removed\":"
                << metrics.unreachable_blocks_removed
                << ",\"samples\":21,\"batch_size\":10,\"optimization_us_min\":"
                << samples.front()
                << ",\"optimization_us_median\":" << samples[10]
                << ",\"optimization_us_max\":" << samples.back()
                << ",\"optimization_us_samples\":[";
      for (std::size_t n = 0; n < samples.size(); ++n) {
        if (n)
          std::cout << ',';
        std::cout << samples[n];
      }
      std::cout << "]}\n";
    } else
      throw std::runtime_error("unknown command");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "miniopt: " << e.what() << '\n';
    return 1;
  }
}
