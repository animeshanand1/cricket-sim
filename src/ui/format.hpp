#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace t20::ui {

// Money formatting in Lakhs and Crores: ₹1.6 cr, ₹18 cr, ₹75 L, or –
std::string money(std::optional<double> lakh);

// Overs text: 12, 12.3
std::string oversText(int balls);

// Ordinal representation: 1st, 2nd, 3rd, 4th, ..., 10th
std::string ordinal(int n);

// Net Run Rate formatting with explicit +/- sign: +0.432, -1.205
std::string nrrText(double v);

// Fixed decimals formatting with "–" fallback for null/invalid values
std::string fmt(std::optional<double> v, int decimals = 2);

// Indian numbering system formatting: 1,00,000
std::string fmt_indian(int64_t n);

// Standard comma thousands formatting: 10,000
std::string fmt_thousands(int64_t n);

// Player surname extractor: everything after first word, or full name
std::string surname(const std::string& name);

// Batter tempo tier label
std::string tempoName(double t);

// Batter in-match confidence tier label
std::string confidenceLabel(double v);

// Bowler in-match rhythm tier label
std::string rhythmLabel(double v);

// Player physical fitness tier label
std::string fitnessLabel(double v);

// Board of directors confidence label
std::string boardConfidenceLabel(double v);

// Fans mood label in match / stadium
std::string fansMoodLabel(double v);

// Potential rating grade: A+, A, B+, B, C+, C, D
std::string potentialGrade(double v);

// Age phase classification: Rising, Prime, Experienced, Veteran, Twilight
std::string agePhase(int age);

} // namespace t20::ui
