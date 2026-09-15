#include <cassert>
#include <cstdint>
#include <random>
#include "library/axis-aligned-rectangle.hpp"
#include "library/largest-empty-rectangle.hpp"

int main() {
  using Rect = AxisAlignedRectangle<int>;
  std::mt19937 random(1);
  for (int trial = 0; trial < 1200; ++trial) {
    const int width = 2 + static_cast<int>(random() % 7);
    const int height = 2 + static_cast<int>(random() % 7);
    const int x = static_cast<int>(random() % static_cast<unsigned>(width));
    const int y = static_cast<int>(random() % static_cast<unsigned>(height));
    std::vector<Rect> obstacles;
    for (int k = 0; k < 8; ++k) {
      const int a = static_cast<int>(random() % static_cast<unsigned>(width));
      const int b = static_cast<int>(random() % static_cast<unsigned>(height));
      const int c = a + 1 + static_cast<int>(random() % static_cast<unsigned>(width - a));
      const int d = b + 1 + static_cast<int>(random() % static_cast<unsigned>(height - b));
      Rect r{a, b, c, d};
      if (!r.contains(x, y)) obstacles.push_back(r);
    }
    const auto answer = largest_empty_rectangle(Rect{0, 0, width, height}, x, y, obstacles);
    assert(answer.contains(x, y));
    for (const auto& o : obstacles) assert(!answer.overlaps(o));
    long long exact = 0;
    for (int a = 0; a <= x; ++a) for (int b = 0; b <= y; ++b) {
      for (int c = x + 1; c <= width; ++c) for (int d = y + 1; d <= height; ++d) {
        Rect r{a, b, c, d};
        bool valid = true;
        for (const auto& o : obstacles) if (r.overlaps(o)) valid = false;
        if (valid) exact = std::max(exact, r.area());
      }
    }
    assert(answer.area() == exact);
  }
  using Wide = AxisAlignedRectangle<long long>;
  const Wide bounds{-10, -20, 20, 30};
  assert(largest_empty_rectangle(bounds, 0LL, 0LL, std::vector<Wide>{}).area() == 1500);
  bool threw = false;
  try { largest_empty_rectangle(bounds, 0LL, 0LL, std::vector<Wide>{{0,0,1,1}}); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
  const std::vector<Wide> outside{{-100,-100,100,-30}, {25,-100,30,100}};
  assert(largest_empty_rectangle(bounds, 0LL, 0LL, outside).area() == 1500);
  // 指定セルの上下にある障害物がboundsの外へはみ出していても扱える。
  const std::vector<Wide> crossing{{-100,5,100,100}, {-100,-100,100,-5}};
  assert(largest_empty_rectangle(bounds, 0LL, 0LL, crossing).area() == 300);
  for (const auto& invalid : std::vector<Wide>{{0,0,0,1}, {1,0,0,1}}) {
    threw = false;
    try { largest_empty_rectangle(bounds, 0LL, 0LL, std::vector<Wide>{invalid}); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
  }
  threw = false;
  try { largest_empty_rectangle(bounds, bounds.right, 0LL, std::vector<Wide>{}); }
  catch (const std::invalid_argument&) { threw = true; }
  assert(threw);
}
