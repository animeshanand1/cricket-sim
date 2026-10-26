#pragma once

#include <string>
#include <unordered_map>
#include <functional>

namespace t20::ui {

enum class EaseType {
  Linear,
  EaseOutQuad,
  EaseOutCubic,
  EaseInOutQuad,
  BounceOut
};

struct Tween {
  double startTime = 0.0;
  double duration = 0.35;
  EaseType ease = EaseType::EaseOutQuad;
  float from = 0.0f;
  float to = 1.0f;
};

class AnimationManager {
public:
  static AnimationManager& instance();

  void update(double currentTime);
  void start(const std::string& id, float from, float to, double duration, EaseType ease = EaseType::EaseOutQuad);
  float get(const std::string& id, float defaultValue = 1.0f);
  bool isRunning(const std::string& id) const;
  void cancel(const std::string& id);

  void setReduceMotion(bool reduce) { m_reduceMotion = reduce; }
  bool reduceMotion() const { return m_reduceMotion; }

  // Returns true if any animation is currently active (forces UI re-render)
  bool hasActiveAnimations() const;

  // Helper for pulsing dot (sine oscillation)
  float getPulse(float periodSeconds = 1.2f) const;

private:
  AnimationManager() = default;
  double m_currentTime = 0.0;
  bool m_reduceMotion = false;
  std::unordered_map<std::string, Tween> m_tweens;
};

// Global convenience wrappers
inline void startTween(const std::string& id, float from, float to, double duration, EaseType ease = EaseType::EaseOutQuad) {
  AnimationManager::instance().start(id, from, to, duration, ease);
}

inline float sampleTween(const std::string& id, float defaultValue = 1.0f) {
  return AnimationManager::instance().get(id, defaultValue);
}

inline bool hasActiveTweens() {
  return AnimationManager::instance().hasActiveAnimations();
}

} // namespace t20::ui
