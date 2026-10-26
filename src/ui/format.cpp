#include "ui/format.hpp"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace t20::ui {

std::string money(std::optional<double> lakh) {
  if (!lakh.has_value()) return "–";
  double l = lakh.value();
  if (l >= 100.0) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", l / 100.0);
    std::string s(buf);
    // trim trailing zeros and dot
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return "₹" + s + " cr";
  }
  return "₹" + std::to_string(static_cast<int>(std::round(l))) + " L";
}

std::string oversText(int balls) {
  int overs = balls / 6;
  int rem = balls % 6;
  if (rem == 0) {
    return std::to_string(overs);
  }
  return std::to_string(overs) + "." + std::to_string(rem);
}

std::string ordinal(int n) {
  if (n == 1) return "1st";
  if (n == 2) return "2nd";
  if (n == 3) return "3rd";
  return std::to_string(n) + "th";
}

std::string nrrText(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%+.3f", v);
  return std::string(buf);
}

std::string fmt(std::optional<double> v, int decimals) {
  if (!v.has_value() || std::isnan(v.value())) return "–";
  char buf[32];
  char fmtStr[16];
  std::snprintf(fmtStr, sizeof(fmtStr), "%%.%df", decimals);
  std::snprintf(buf, sizeof(buf), fmtStr, v.value());
  return std::string(buf);
}

std::string fmt_indian(int64_t n) {
  bool neg = (n < 0);
  std::string s = std::to_string(neg ? -n : n);
  if (s.length() <= 3) {
    return neg ? "-" + s : s;
  }
  std::string last3 = s.substr(s.length() - 3);
  std::string rest = s.substr(0, s.length() - 3);
  std::string res;
  while (rest.length() > 2) {
    res = "," + rest.substr(rest.length() - 2) + res;
    rest = rest.substr(0, rest.length() - 2);
  }
  res = rest + res + "," + last3;
  return neg ? "-" + res : res;
}

std::string fmt_thousands(int64_t n) {
  bool neg = (n < 0);
  std::string s = std::to_string(neg ? -n : n);
  int insertPos = static_cast<int>(s.length()) - 3;
  while (insertPos > 0) {
    s.insert(static_cast<size_t>(insertPos), ",");
    insertPos -= 3;
  }
  return neg ? "-" + s : s;
}

std::string surname(const std::string& name) {
  size_t space = name.find(' ');
  if (space == std::string::npos || space + 1 >= name.length()) {
    return name;
  }
  return name.substr(space + 1);
}

std::string tempoName(double t) {
  if (t < 1.6) return "Defend";
  if (t < 2.5) return "Careful";
  if (t < 3.35) return "Balanced";
  if (t < 4.3) return "Attack";
  return "All-out";
}

std::string confidenceLabel(double v) {
  if (v >= 80.0) return "In the zone";
  if (v >= 65.0) return "Confident";
  if (v >= 45.0) return "Settled";
  if (v >= 30.0) return "Tentative";
  return "Nervy";
}

std::string rhythmLabel(double v) {
  if (v >= 70.0) return "On fire";
  if (v >= 55.0) return "In rhythm";
  if (v >= 40.0) return "Steady";
  return "Rattled";
}

std::string fitnessLabel(double v) {
  if (v >= 90.0) return "Fresh";
  if (v >= 75.0) return "Fit";
  if (v >= 60.0) return "Tired";
  return "Exhausted";
}

std::string boardConfidenceLabel(double v) {
  if (v >= 80.0) return "Delighted";
  if (v >= 60.0) return "Happy";
  if (v >= 40.0) return "Watching closely";
  if (v >= 25.0) return "Concerned";
  return "Your job is at risk";
}

std::string fansMoodLabel(double v) {
  if (v >= 78.0) return "Roaring";
  if (v >= 60.0) return "In full voice";
  if (v >= 42.0) return "Watching closely";
  if (v >= 25.0) return "Gone quiet";
  return "Heading home";
}

std::string potentialGrade(double v) {
  if (v >= 88.0) return "A+";
  if (v >= 82.0) return "A";
  if (v >= 76.0) return "B+";
  if (v >= 70.0) return "B";
  if (v >= 64.0) return "C+";
  if (v >= 58.0) return "C";
  return "D";
}

std::string agePhase(int age) {
  if (age <= 23) return "Rising";
  if (age <= 29) return "Prime";
  if (age <= 32) return "Experienced";
  if (age <= 35) return "Veteran";
  return "Twilight";
}

} // namespace t20::ui
