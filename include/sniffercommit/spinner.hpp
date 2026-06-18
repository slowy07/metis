#ifndef SNIFFERCOMMIT_SPINNER_HPP
#define SNIFFERCOMMIT_SPINNER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <string_view>
#include <vector>

namespace sniffercommit {
class Spinner {
 public:
  enum class Mode { Auto, Manual };

  explicit Spinner(std::string_view message, Mode mode = Mode::Auto,
                   std::vector<std::string_view> frames = k_default_frames,
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

 private:
  void run_loop();
  void clear_line() const;

  static constexpr std::chrono::milliseconds k_default_interval{80};
  static inline const std::vector<std::string_view> k_default_frames = {"⠋", "⠙", "⠹", "⠸", "⠼",
                                                                        "⠴", "⠦", "⠧", "⠇", "⠏"};

  std::string message_;
  std::vector<std::string_view> frames_;
  std::chrono::milliseconds interval_ms_;

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread thread_;

  std::mutex mutex_;
  std::condition_variable cv_;

  static inline std::atomic<bool> silent_{false};
};
}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_SPINNER_HPP
