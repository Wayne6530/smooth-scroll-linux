#pragma once

#include <vector>
#include <chrono>
#include <cassert>

namespace smooth_scroll
{

class MouseMovementBuffer
{
public:
  struct Result
  {
    int x;
    int y;
  };

  explicit MouseMovementBuffer(std::chrono::milliseconds window);

  Result add(std::chrono::milliseconds time, int rel_x, int rel_y);

private:
  struct MouseMovement
  {
    std::chrono::milliseconds time;
    int rel_x;
    int rel_y;
  };

  std::chrono::milliseconds window_;

  std::vector<MouseMovement> buffer_;
  int mask_;
  int pos_;
};

}  // namespace smooth_scroll
