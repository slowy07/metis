#ifndef SNIFFERCOMMIT_SPINNER_HPP
#define SNIFFERCOMMIT_SPINNER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace sniffercommit {
class Spinner {
 public:
  enum class Mode { Auto };

  explicit Spinner(std::string_view message, Mode mode = Mode::Auto,
                   std::vector<std::string> frames = {},
                   std::chrono::milliseconds interval_ms = k_default_interval);

  ~Spinner();

  Spinner(const Spinner&) = delete;
  Spinner& operator=(const Spinner&) = delete;
  Spinner(Spinner&&) = delete;
  Spinner& operator=(Spinner&&) = delete;

  void start();

  void stop(std::string_view final_message = {});

  [[nodiscard]] bool is_running() const noexcept;

  static void set_silent(bool silent) noexcept;
  [[nodiscard]] static bool is_silent() noexcept;

  [[nodiscard]] static bool is_stdout_tty() noexcept;

 private:
  void run_loop();
  void clear_line() const;
  static std::vector<std::string> default_frames();

  static constexpr std::chrono::milliseconds k_default_interval{80};

  std::string message_;
  std::vector<std::string> frames_;
  std::chrono::milliseconds interval_ms_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread thread_;

  std::mutex mutex_;
  std::condition_variable cv_;

  static inline std::atomic<bool> silent_{false};
};
#ifdef _WIN32
void enable_windows_ansi();
#endif

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_SPINNER_HPP
