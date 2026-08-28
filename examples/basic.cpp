#include <iostream>
#include <vector>

#include "library/ahc/random.hpp"
#include "library/ahc/timer.hpp"

int main() {
  ahc::Timer timer;
  ahc::Random random(123456789);

  std::vector<int> order{0, 1, 2, 3, 4};
  random.shuffle(order.begin(), order.end());

  std::cout << "first=" << order.front() << '\n';
  std::cerr << "elapsed_ms=" << timer.elapsed_ms() << '\n';
}

