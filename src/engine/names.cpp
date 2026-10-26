#include "engine/names.hpp"

#include <array>
#include <cmath>
#include <string_view>

#include "core/str.hpp"

namespace t20 {

namespace {
// clang-format off
constexpr std::string_view FIRST[] = {
  "Aarav", "Kabir", "Rohan", "Ishaan", "Devansh", "Arjun", "Nikhil", "Siddharth", "Vihaan", "Tanmay",
  "Farhan", "Zayd", "Hamza", "Danish", "Tariq", "Callum", "Rhys", "Declan", "Ollie", "Finn",
  "Hamish", "Lachlan", "Bailey", "Jarrod", "Keegan", "Dewald", "Thando", "Sipho", "Andile", "Marlon",
  "Akeem", "Jaden", "Tevin", "Sahan", "Tharindu", "Asanka", "Najib", "Farid", "Wahid", "Ehsan",
  "Tomas", "Logan", "Blake", "Corey", "Nathan", "Yash", "Pranav", "Aditya", "Omkar", "Karthik",
  "Mayur", "Rehan", "Samar", "Kunal", "Emeka", "Liam", "Joel", "Reece", "Dion", "Kofi",
  "Aryan", "Vedant", "Harsh", "Parth", "Anmol", "Ritvik", "Sameer", "Uday", "Gaurav", "Chirag",
};

constexpr std::string_view LAST[] = {
  "Mehra", "Bhandari", "Rastogi", "Deshmukh", "Sahni", "Banerjee", "Talwar", "Oberoi", "Saxena", "Venkatesh",
  "Naidu", "Menon", "Pillai", "Qureshi", "Siddiqui", "Durrani", "Baig", "Hollis", "Pemberton", "Carrick",
  "Ashdown", "Fairweather", "Whitlock", "Kinsella", "Dunmore", "Rawlings", "Calloway", "Brandt", "Vermaak", "Mokoena",
  "Nkosi", "Greaves", "Alleyne", "Bascombe", "Moseley", "Wickramaratne", "Jayaweera", "Senanayake", "Wijesinghe", "Ahmadzai",
  "Sherzad", "Popal", "McKenzie", "Galbraith", "Tolland", "Ferris", "Ashby", "Kerrigan", "Donnelly", "Whitford",
  "Harland", "Corrie", "Laidlaw", "Strachan", "Haddon", "Pryce", "Okafor", "Kamath", "Gokhale", "Rathore",
  "Chandran", "Iyengar", "Mistry", "Dalal", "Varma", "Lobo", "Castellino", "Vandermeer", "Botha", "Khurana",
  "Sethi", "Grover", "Bajwa", "Chawla", "Dhillon", "Kapoor", "Malhotra", "Parekh", "Joshi", "Kulkarni",
};
// clang-format on
}  // namespace

std::string freshName(RandFn rng, std::unordered_set<std::string>& used) {
  for (int i = 0; i < 400; ++i) {
    const auto& first = FIRST[static_cast<size_t>(std::floor(rng() * std::size(FIRST)))];
    const auto& last = LAST[static_cast<size_t>(std::floor(rng() * std::size(LAST)))];
    std::string name = cat(first, ' ', last);
    if (!used.count(name)) {
      used.insert(name);
      return name;
    }
  }
  std::string name = cat("Player ", used.size() + 1);
  used.insert(name);
  return name;
}

}  // namespace t20
