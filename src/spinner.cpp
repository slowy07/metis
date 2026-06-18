#include "sniffercommit/spinner.hpp"

#include <iostream>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace sniffercommit {

Spinner::Spinner(std::string_view message, Mode mode, std::vector<std::string_view> frames,
                 std::chrono::milliseconds interval_ms)
    : message_(message), frames_(std::move(frames)), interval_ms_(interval_ms) {
  if (mode == Mode::Auto) {
    start();
  }
}

Spinner::~Spinner() { stop(); }

void Spinner::start() {
  if (silent_.load(std::memory_order_relaxed)) {
    return;
  }

  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return;
  }

  stop_requested_.store(false, std::memory_order_relaxed);

  if (thread_.joinable()) {
    thread_.join();
  }

  thread_ = std::thread(&Spinner::run_loop, this);
}

void Spinner::stop(std::string_view final_message) {
  if (!running_.load(std::memory_order_relaxed)) {
    return;
  }

  stop_requested_.store(true, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lock(mutex_);
  }

  cv_.notify_all();

  if (thread_.joinable()) {
  thread_.join();
  }

  running_.store(false, std::memory_order_relaxed);

  if (!silent_.load(std::memory_order_relaxed)) {
    clear_line();

    if (!final_message.empty()) {
      std::cout << final_message << "\n";
    }
  }
}

bool Spinner::is_running() const noexcept {
  return running_.load(std::memory_order_relaxed);
}

void Spinner::set_silent(bool silent) noexcept {
  silent_.store(silent, std::memory_order_relaxed);
}

bool Spinner::is_silent() noexcept {
  return silent_.load(std::memory_order_relaxed);
}

void Spinner::run_loop() {
  size_t frame_idx = 0;

  while (!stop_requested_.load(std::memory_order_relaxed)) {
    std::cout << "\r" << message_ << " " << frames_[frame_idx] << " " << std::flush;

    frame_idx = (frame_idx + 1) % frames_.size();

    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, interval_ms_, [this] {
      return stop_requested_.load(std::memory_order_relaxed);
    });
  }
}

void Spinner::clear_line() const {
  std::cout << "\r" << std::string(message_.size() + frames_[0].size() + 2, ' ') << "\r"
            << std::flush;
}

}  // namespace sniffercommit
