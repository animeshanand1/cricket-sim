#include "ui/anim.hpp"
#include <cmath>
#include <algorithm>

namespace t20::ui {

AnimationManager& AnimationManager::instance() {
  static AnimationManager s_instance;
  return s_instance;
}

static float applyEase(EaseType ease, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  switch (ease) {
    case EaseType::Linear:
      return t;
    case EaseType::EaseOutQuad:
      return t * (2.0f - t);
    case EaseType::EaseOutCubic: {
      float f = t - 1.0f;
      return f * f * f + 1.0f;
    }
    case EaseType::EaseInOutQuad:
      return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    case EaseType::BounceOut: {
      if (t < (1.0f / 2.75f)) {
        return 7.5625f * t * t;
      } else if (t < (2.0f / 2.75f)) {
        float f = t - (1.5f / 2.75f);
        return 7.5625f * f * f + 0.75f;
      } else if (t < (2.5f / 2.75f)) {
        float f = t - (2.25f / 2.75f);
        return 7.5625f * f * f + 0.9375f;
      } else {
        float f = t - (2.625f / 2.75f);
        return 7.5625f * f * f + 0.984375f;
      }
    }
  }
  return t;
}

void AnimationManager::update(double currentTime) {
  m_currentTime = currentTime;
  // Prune completed tweens
  for (auto it = m_tweens.begin(); it != m_tweens.end();) {
    double elapsed = m_currentTime - it->second.startTime;
    if (elapsed >= it->second.duration) {
      it = m_tweens.erase(it);
    } else {
      ++it;
    }
  }
}

void AnimationManager::start(const std::string& id, float from, float to, double duration, EaseType ease) {
  if (m_reduceMotion) {
    m_tweens.erase(id);
    return;
  }
  Tween tw;
  tw.startTime = m_currentTime;
  tw.duration = std::max(0.001, duration);
  tw.ease = ease;
  tw.from = from;
  tw.to = to;
  m_tweens[id] = tw;
}

float AnimationManager::get(const std::string& id, float defaultValue) {
  if (m_reduceMotion) return defaultValue;
  auto it = m_tweens.find(id);
  if (it == m_tweens.end()) return defaultValue;

  const auto& tw = it->second;
  double elapsed = m_currentTime - tw.startTime;
  if (elapsed >= tw.duration) {
    return tw.to;
  }
  float t = static_cast<float>(elapsed / tw.duration);
  float eased = applyEase(tw.ease, t);
  return tw.from + (tw.to - tw.from) * eased;
}

bool AnimationManager::isRunning(const std::string& id) const {
  return m_tweens.find(id) != m_tweens.end();
}

void AnimationManager::cancel(const std::string& id) {
  m_tweens.erase(id);
}

bool AnimationManager::hasActiveAnimations() const {
  return !m_tweens.empty();
}

float AnimationManager::getPulse(float periodSeconds) const {
  if (m_reduceMotion) return 1.0f;
  if (periodSeconds <= 0.0f) periodSeconds = 1.0f;
  float rad = static_cast<float>(std::fmod(m_currentTime, static_cast<double>(periodSeconds)) / periodSeconds) * 6.2831853f;
  return 0.5f + 0.5f * std::sin(rad);
}

} // namespace t20::ui
