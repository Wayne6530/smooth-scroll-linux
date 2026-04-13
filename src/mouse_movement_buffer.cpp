// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "mouse_movement_buffer.h"

namespace smooth_scroll
{

MouseMovementBuffer::MouseMovementBuffer(std::chrono::milliseconds window)
  : window_(window)
  , buffer_(1 << (32 - __builtin_clz(static_cast<uint32_t>(window.count()))),
            MouseMovement{ std::chrono::milliseconds::min(), 0, 0 })
  , mask_(buffer_.size() - 1)
  , pos_(buffer_.size() - 1)
{
  assert(window.count() > 1);
}

MouseMovementBuffer::Result MouseMovementBuffer::add(std::chrono::milliseconds time, int rel_x, int rel_y) noexcept
{
  if (time <= buffer_[pos_].time)
  {
    buffer_[pos_].rel_x += rel_x;
    buffer_[pos_].rel_y += rel_y;
  }
  else
  {
    pos_ = (pos_ + 1) & mask_;

    buffer_[pos_].time = time;
    buffer_[pos_].rel_x = rel_x;
    buffer_[pos_].rel_y = rel_y;
  }

  std::chrono::milliseconds end = buffer_[pos_].time - window_;

  Result result{};
  for (int i = pos_; buffer_[i].time > end; i = (i - 1) & mask_)
  {
    result.x += buffer_[i].rel_x;
    result.y += buffer_[i].rel_y;
  }

  return result;
}

}  // namespace smooth_scroll
