#include "sniffercommit/spinner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif  // _WIN32

namespace sniffercommit {

#ifdef _WIN32
void enable_windows_ansi() noexcept {
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hOut == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD mode = 0;
  if (!GetConsoleMode(hOut, &mode)) {
    return;
  }

  SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static void set_utf8_console() noexcept { SetConsoleOutputCP(CP_UTF8); }
#endif

std::vector<std::string> Spinner::default_frames() {
#ifdef _WIN32
  return {"|", "/", "-", "\\"};
#endif  // _WIN32
  return {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
}

bool Spinner::is_stdout_tty() noexcept {
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif  // _WIN32
}

Spinner::Spinner(std::string_view message, Mode mode, std::vector<std::string> frames,
                 std::chrono::milliseconds interval_ms)
    : message_(message),
      frames_(frames.empty() ? default_frames() : std::move(frames)),
      interval_ms_(interval_ms) {
#ifdef _WIN32
  enable_windows_ansi();
  set_utf8_console();
#endif  // _WIN32

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

bool Spinner::is_running() const noexcept { return running_.load(std::memory_order_relaxed); }

void Spinner::set_silent(bool silent) noexcept { silent_.store(silent, std::memory_order_relaxed); }

bool Spinner::is_silent() noexcept { return silent_.load(std::memory_order_relaxed); }

void Spinner::run_loop() {
  size_t frame_idx = 0;

  while (!stop_requested_.load(std::memory_order_relaxed)) {
    std::cout << "\r" << message_ << " " << frames_[frame_idx] << " " << std::flush;

    frame_idx = (frame_idx + 1) % frames_.size();

    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, interval_ms_,
                 [this] { return stop_requested_.load(std::memory_order_relaxed); });
  }
}

void Spinner::clear_line() const {
  size_t frame_len = frames_.empty() ? 1 : frames_[0].size();
  std::cout << "\r" << std::string(message_.size() + frame_len + 2, ' ') << "\r" << std::flush;
}

}  // namespace sniffercommit
