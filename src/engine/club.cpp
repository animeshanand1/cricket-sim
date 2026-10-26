#include "engine/club.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

#include "core/str.hpp"
#include "engine/ageing.hpp"
#include "engine/development.hpp"
#include "engine/finance.hpp"
#include "engine/league.hpp"
#include "engine/pitch.hpp"
#include "engine/rng.hpp"
#include "engine/skills.hpp"

namespace t20 {

namespace {
constexpr int32_t kSalt = static_cast<int32_t>(0xc1b5);

struct TeamNum {
  const char* id;
  double v;
};
constexpr TeamNum POPULARITY[] = {{"CHK", 92}, {"MUI", 90}, {"RGC", 88}, {"KNR", 80}, {"SNH", 70},
                                  {"DLC", 68}, {"RJR", 66}, {"GUT", 65}, {"PBK", 62}, {"LKG", 60}};
std::optional<double> lookup(const TeamNum* table, size_t n, const std::string& id) {
  for (size_t i = 0; i < n; ++i)
    if (id == table[i].id) return table[i].v;
  return std::nullopt;
}
double stagePull(Stage s) {
  switch (s) {
    case Stage::league: return 1;
    case Stage::q1:
    case Stage::elim: return 1.4;
    case Stage::q2: return 1.5;
    case Stage::final_: return 1.7;
  }
  return 1;
}
const std::vector<std::string> BRANDS = {
    "Zephyr Mobile", "Rangoli Paints", "NovaPay",        "Kesari Cement",    "Orbit Fintech", "Sapphire Tyres",
    "Monsoon Cola",  "Indus Energy",   "Peacock Airways", "Anvil Steel",      "Banyan Insurance", "Garuda Motors",
    "Saffron Leaf Tea", "Pixel Bank",  "Neon Arcade",    "Glacier Springs",  "Comet Logistics", "Tusker Biscuits",
    "Vayu Fans",     "Lattice Cloud",
};

std::string lowerAll(std::string_view s) {
  std::string out(s);
  for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

Books& book(Club& club, int year) { return club.books[year]; }
void credit(Club& club, int year, const std::string& key, double amount) {
  if (!amount || std::isnan(amount)) return;
  club.bank += amount;
  Books& b = book(club, year);
  b.income[key] = (b.income.find(key) ? *b.income.find(key) : 0) + amount;
}
void debit(Club& club, int year, const std::string& key, double amount) {
  if (!amount || std::isnan(amount)) return;
  club.bank -= amount;
  Books& b = book(club, year);
  b.spend[key] = (b.spend.find(key) ? *b.spend.find(key) : 0) + amount;
}
void note(Club& club, int year, std::string text) {
  club.log.insert(club.log.begin(), ClubNote{year, std::move(text)});
  if (club.log.size() > 30) club.log.resize(30);
}
void trimBooks(Club& club) {
  while (club.books.size() > HISTORY_KEEP) club.books.erase(club.books.begin());
}
void endCoach(Club& club, Coach* c, int year) {
  if (c->from >= year) {
    for (auto it = club.coaches.begin(); it != club.coaches.end(); ++it)
      if (&*it == c) {
        club.coaches.erase(it);
        break;
      }
  } else {
    c->ends = year;
  }
}
double ageFactor(int age) { return age <= 21 ? 1.2 : age <= 24 ? 1 : age <= 27 ? 0.7 : age <= 30 ? 0.4 : 0.25; }

GateTake gateForClub(const Club& club, const TeamId& teamId, int year, const GateFixture& fx, double form) {
  const TicketTier* tier = ticketTier(club.active.tier);
  if (!tier) tier = ticketTier("standard");
  const double cap = capacity(club, teamId);
  const double pull = stagePull(fx.stage);
  const double opp = fx.away ? basePopularity(*fx.away) : 65;
  Rng rng = hashRng(kSalt, "gate", teamId, year, fx.no);
  const double demand = (12000 + club.popularity * 450) * tier->demand * (0.85 + 0.3 * form) *
                        (0.9 + 0.2 * (opp / 100)) * pull * (0.92 + rng() * 0.16);
  const double att = js_round(std::min(cap, demand) / 10) * 10;
  const double spend = FAN_SPEND + 40 * club.levels["fanzone"];
  const double tickets = js_round((att * tier->price * std::min(pull, 1.5)) / 1e5);
  const double extras = js_round((att * spend) / 1e5);
  const double hospitality = js_round(club.levels["hospitality"] * HOSPITALITY_PER_LEVEL * pull);
  return {att, cap, tickets, extras, hospitality, tickets + extras + hospitality};
}

// ---- Coaching
template <class K>
double addSkill(Player& p, SkillSet<K>& skills, SkillSet<K>& frac, K k, double d) {
  if (!skills.contains(k) || !d || std::isnan(d)) return 0;
  const double top = std::max(0.2, (99 - skills[k]) / 40);
  const double raw = clamp(skills[k] + frac.get_or(k, 0) + d * top, 1, 99);
  const double before = skills[k];
  skills.set(k, js_round(raw));
  frac.set(k, js_round((raw - skills[k]) * 100) / 100);
  (void)p;
  return skills[k] - before;
}
double work(Player& p, const std::string& areaKey, double amount) {
  const CoachArea* a = coachArea(areaKey);
  if (!a) return 0;
  if (!p.skillFrac) p.skillFrac = SkillFrac{};
  double gained = 0;
  for (BatK k : a->bat) gained += addSkill(p, p.skills.bat, p.skillFrac->bat, k, amount);
  for (BowlK k : a->bowl) gained += addSkill(p, p.skills.bowl, p.skillFrac->bowl, k, amount);
  for (FieldK k : a->field) gained += addSkill(p, p.skills.field, p.skillFrac->field, k, amount);
  return gained;
}
std::vector<std::string> ownAreas(const Player& p) {
  const std::string bowl = isPaceType(p.bowlType) ? "pace" : "spin";
  if (p.role == Role::BOWL) return {bowl};
  if (p.role == Role::AR) return {"batting", bowl};
  if (p.role == Role::WK) return {"batting", "keeping"};
  return {"batting"};
}

}  // namespace

double basePopularity(const TeamId& team) { return lookup(POPULARITY, std::size(POPULARITY), team).value_or(65); }

const std::vector<Facility>& FACILITIES() {
  static const std::vector<Facility> f = {
      {"stands", "Stands", "ground", {1500, 2200, 3000, 4000, 5200},
       cat("+", fmt_indian(SEATS_PER_STAND), " seats per level. Only pays when the ground sells out.")},
      {"hospitality", "Corporate hospitality", "ground", {800, 1200, 1700, 2300, 3000},
       "Boxes and lounges: ₹40 lakh a home game per level, more in the playoffs."},
      {"fanzone", "Fan zone and concessions", "ground", {500, 800, 1200, 1600, 2100},
       "Food, drink and merchandise: +₹40 spent per fan per level, and +1 popularity a season per level."},
      {"curator", "Curator and ground staff", "ground", {400, 700, 1000},
       "Choose the pitch for your home league games: 80%, 90%, then every game on your chosen surface."},
      {"academy", "Youth academy", "youth", {700, 1100, 1600, 2200, 3000},
       "Better academy graduates each off-season, and one extra scout report in the draft per level."},
      {"hpc", "High-performance centre", "youth", {1000, 1500, 2100, 2800, 3600},
       "One more personal-coach slot per level, +10% to all coaching, and under-24s grow a little faster."},
  };
  return f;
}
const Facility* facility(std::string_view key) {
  for (const auto& f : FACILITIES())
    if (f.key == key) return &f;
  return nullptr;
}
int maxLevel(std::string_view key) {
  const Facility* f = facility(key);
  return f ? static_cast<int>(f->costs.size()) : 0;
}
std::optional<double> upgradeCost(std::string_view key, int level) {
  const Facility* f = facility(key);
  if (!f || level < 0 || level >= static_cast<int>(f->costs.size())) return std::nullopt;
  return f->costs[static_cast<size_t>(level)];
}

const std::vector<TicketTier>& TICKET_TIERS() {
  static const std::vector<TicketTier> t = {
      {"budget", "Budget", 800, 1.3, 1, "Full houses and new fans; less from each seat."},
      {"standard", "Standard", 1500, 1, 0, "The usual prices."},
      {"premium", "Premium", 2500, 0.62, 0, "More from each seat; fewer people come."},
      {"elite", "Elite", 4000, 0.38, -1, "Top prices. Only the most popular clubs fill a ground, and fans drift away."},
  };
  return t;
}
const TicketTier* ticketTier(std::string_view key) {
  for (const auto& t : TICKET_TIERS())
    if (t.key == key) return &t;
  return nullptr;
}

double prizeFor(int position) {
  switch (position) {
    case 1: return 2000;
    case 2: return 1300;
    case 3: return 700;
    case 4: return 650;
    default: return 0;
  }
}
double goodwillFrom(double shared) { return clamp(js_round(shared / 10) / 10, 0, MAX_GOODWILL); }

const std::vector<SponsorSlot>& SPONSOR_SLOTS() {
  static const std::vector<SponsorSlot> s = {
      {"shirt", "Principal shirt sponsor", 1000}, {"kit", "Kit partner", 500},  {"sleeve", "Sleeve sponsor", 300},
      {"ground", "Ground partner", 250},          {"helmet", "Helmet and cap", 200}, {"digital", "Digital partner", 200},
  };
  return s;
}
const std::vector<DealType>& DEAL_TYPES() {
  static const std::vector<DealType> d = {
      {"fixed", "Fixed fee", "The same money whatever happens."},
      {"performance", "Performance", "Smaller fee, bonuses for the playoffs and the title."},
      {"long", "Long term", "Three seasons locked in at a little less a year."},
  };
  return d;
}

const std::vector<CoachArea>& COACH_AREAS() {
  using B = BatK;
  using W = BowlK;
  using F = FieldK;
  static const std::vector<CoachArea> a = {
      {"batting", "Batting", {B::defence, B::vsPace, B::vsSpin, B::grounded, B::lofted, B::rotation}, {}, {}},
      {"pace", "Pace bowling", {}, {W::accuracy, W::swing, W::seam, W::variations, W::death}, {}},
      {"spin", "Spin bowling", {}, {W::turn, W::flight, W::accuracy, W::variations}, {}},
      {"fielding", "Fielding", {}, {}, {F::catching, F::ground, F::throwing}},
      {"keeping", "Wicketkeeping", {}, {}, {F::keeping, F::catching}},
      {"mental", "Mental skills", {B::awareness, B::temperament}, {}, {}},
  };
  return a;
}
const CoachArea* coachArea(std::string_view key) {
  for (const auto& a : COACH_AREAS())
    if (a.key == key) return &a;
  return nullptr;
}
const CoachTier* coachTier(int tier) {
  static const CoachTier t[] = {{"Specialist", 40, 1, 0}, {"Elite", 100, 1.5, 0}, {"World-class", 200, 2.1, 1}};
  return tier >= 1 && tier <= 3 ? &t[tier - 1] : nullptr;
}
const std::vector<StaffRole>& STAFF_ROLES() {
  static const std::vector<StaffRole> r = {
      {"bat", "Batting coach", "batting"}, {"bowl", "Bowling coach", ""}, {"field", "Fielding coach", "fielding"}};
  return r;
}
const std::vector<StaffTier>& STAFF_TIERS() {
  static const std::vector<StaffTier> t = {{"None", 0, 0}, {"Good", 100, 1}, {"Elite", 200, 1.5}, {"World-class", 350, 2}};
  return t;
}
int staffOf(const Staff& s, std::string_view key) { return key == "bat" ? s.bat : key == "bowl" ? s.bowl : s.field; }
void setStaffOf(Staff& s, std::string_view key, int tier) {
  (key == "bat" ? s.bat : key == "bowl" ? s.bowl : s.field) = tier;
}

std::vector<std::string> areasFor(const Player& p) {
  std::vector<std::string> out = {"batting", "fielding", "mental"};
  if (p.bowlType && p.bowl >= 35) out.insert(out.begin() + 1, isPaceType(p.bowlType) ? "pace" : "spin");
  if (p.role == Role::WK) out.push_back("keeping");
  return out;
}

Club newClub(const TeamId& teamId) {
  Club c;
  c.bank = START_BANK;
  c.popularity = basePopularity(teamId);
  for (const auto& f : FACILITIES()) c.levels.v.set(std::string(f.key), 0);
  return c;
}
Club& ensureClub(World& world, const TeamId& teamId) {
  if (Club* c = world.clubs.find(teamId)) return *c;
  return world.clubs.set(teamId, newClub(teamId));
}
Club clubOf(const World& world, const TeamId& teamId) {
  const Club* c = world.clubs.find(teamId);
  return c ? *c : newClub(teamId);
}

double capacity(const Club& club, const TeamId& teamId) { return capacity(club, teamId, club.levels); }
double capacity(const Club&, const TeamId& teamId, const Levels& levels) {
  return groundCapacity(teamId) + levels["stands"] * SEATS_PER_STAND;
}
int planYear(const World& world, const Club& club) {
  return club.inSeason || club.closedYear == world.year ? world.year + 1 : world.year;
}

void transferMoney(World& world, const TeamId& teamId, double amount, const std::string& text) {
  Club& club = ensureClub(world, teamId);
  if (amount > 0)
    debit(club, world.year, "transfers", amount);
  else
    credit(club, world.year, "transfers", -amount);
  note(club, world.year, text);
}

std::vector<Coach*> coachesIn(Club& club, int year) {
  std::vector<Coach*> out;
  for (auto& c : club.coaches)
    if (c.from <= year && (!c.ends || year < *c.ends)) out.push_back(&c);
  return out;
}
std::vector<const Coach*> coachesIn(const Club& club, int year) {
  std::vector<const Coach*> out;
  for (const auto& c : club.coaches)
    if (c.from <= year && (!c.ends || year < *c.ends)) out.push_back(&c);
  return out;
}
int coachSlots(const Club& club) { return coachSlots(club.levels); }
int coachSlots(const Levels& levels) { return 1 + levels["hpc"]; }
double staffCost(const Staff& staff) {
  double s = 0;
  for (const auto& r : STAFF_ROLES()) {
    const int t = staffOf(staff, r.key);
    s += STAFF_TIERS()[static_cast<size_t>(std::clamp(t, 0, 3))].cost;
  }
  return s;
}
double upkeep(const Levels& levels) {
  double s = 0;
  for (const auto& f : FACILITIES()) s += levels[std::string(f.key)];
  return s * UPKEEP_PER_LEVEL;
}
Levels levelsAfterBuild(const Club& club) {
  Levels out = club.levels;
  for (const auto& [k, b] : club.building) out.v.set(k, std::max(out[k], b.level));
  return out;
}
double committed(const World& world, const Club& club) {
  const int year = planYear(world, club);
  if (club.activatedYear == year) return 0;
  double s = 0;
  for (const Coach* c : coachesIn(club, year)) s += coachTier(c->tier)->cost;
  return s + staffCost(club.staffPlan);
}
double available(const World& world, const Club& club) { return club.bank - committed(world, club); }

bool orderUpgrade(World& world, const TeamId& teamId, const std::string& key) {
  Club& club = ensureClub(world, teamId);
  const Facility* f = facility(key);
  if (!f || club.building.contains(key)) return false;
  const int level = club.levels[key];
  const auto cost = upgradeCost(key, level);
  if (!cost || *cost > available(world, club)) return false;
  const int ready = planYear(world, club);
  debit(club, world.year, "building", *cost);
  club.building.set(key, Building{level + 1, ready, *cost});
  note(club, world.year, cat(f->label, " level ", level + 1, " ordered for ", money(*cost), ". Opens next season."));
  return true;
}

bool setTicketTier(World& world, const TeamId& teamId, const std::string& tier) {
  if (!ticketTier(tier)) return false;
  ensureClub(world, teamId).plan.tier = tier;
  return true;
}

bool setHomePitch(World& world, const TeamId& teamId, std::optional<Pitch> pitch) {
  Club& club = ensureClub(world, teamId);
  if (pitch && levelsAfterBuild(club)["curator"] < 1) return false;
  club.plan.pitch = pitch;
  return true;
}

bool setPrizeShare(World& world, const TeamId& teamId, int share) {
  if (std::find(std::begin(PRIZE_SHARES), std::end(PRIZE_SHARES), share) == std::end(PRIZE_SHARES)) return false;
  ensureClub(world, teamId).plan.prizeShare = share;
  return true;
}

bool setStaff(World& world, const TeamId& teamId, const std::string& role, int tier) {
  Club& club = ensureClub(world, teamId);
  const bool roleOk = std::any_of(STAFF_ROLES().begin(), STAFF_ROLES().end(), [&](const StaffRole& r) { return r.key == role; });
  if (!roleOk || tier < 0 || tier > 3) return false;
  const double extra = STAFF_TIERS()[static_cast<size_t>(tier)].cost -
                       STAFF_TIERS()[static_cast<size_t>(staffOf(club.staffPlan, role))].cost;
  if (extra > 0 && extra > available(world, club)) return false;
  setStaffOf(club.staffPlan, role, tier);
  return true;
}

bool assignCoach(World& world, const TeamId& teamId, const PlayerId& playerId, const std::string& area, int tier) {
  Club& club = ensureClub(world, teamId);
  const Player* p = world.players.find(playerId);
  if (!p || p->team != teamId || !coachTier(tier)) return false;
  const auto areas = areasFor(*p);
  if (std::find(areas.begin(), areas.end(), area) == areas.end()) return false;
  const int year = planYear(world, club);
  int others = 0;
  Coach* current = nullptr;
  for (Coach* c : coachesIn(club, year)) {
    if (c->playerId != playerId)
      ++others;
    else if (!current)
      current = c;
  }
  if (others >= coachSlots(levelsAfterBuild(club))) return false;
  const double extra = coachTier(tier)->cost - (current ? coachTier(current->tier)->cost : 0);
  if (extra > 0 && extra > available(world, club)) return false;
  if (current) endCoach(club, current, year);
  club.coaches.push_back(Coach{playerId, area, tier, year, std::nullopt, std::nullopt});
  return true;
}

bool removeCoach(World& world, const TeamId& teamId, const PlayerId& playerId) {
  Club& club = ensureClub(world, teamId);
  const int year = planYear(world, club);
  for (Coach* c : coachesIn(club, year))
    if (c->playerId == playerId) {
      endCoach(club, c, year);
      return true;
    }
  return false;
}

bool dealCovers(const Sponsor& deal, int year) { return deal.from <= year && year < deal.from + deal.years; }
const Sponsor* slotDeal(const Club& club, std::string_view slot, int year) {
  for (const auto& d : club.sponsors)
    if (d.slot == slot && dealCovers(d, year)) return &d;
  return nullptr;
}

OrderedMap<std::string, std::vector<Sponsor>> sponsorOffers(const World& world, const TeamId& teamId) {
  const Club club = clubOf(world, teamId);
  const int year = planYear(world, club);
  const double pull = 0.55 + 0.9 * (club.popularity / 100);
  OrderedMap<std::string, std::vector<Sponsor>> out;
  for (const auto& slot : SPONSOR_SLOTS()) {
    if (slotDeal(club, slot.key, year)) continue;
    Rng rngState = hashRng(kSalt, "offer", teamId, std::string(slot.key), year);
    RandFn rng(rngState);
    double base = slot.base * pull;
    if (slot.key == "ground") base *= capacity(club, teamId, levelsAfterBuild(club)) / 40000;
    std::set<std::string> brands;
    std::vector<Sponsor> offers;
    auto r5 = [](double x) { return std::max(5.0, js_round(x / 5) * 5); };
    for (const auto& type : DEAL_TYPES()) {
      std::string brand = pick(rng, BRANDS);
      while (brands.count(brand)) brand = pick(rng, BRANDS);
      brands.insert(brand);
      const double v = base * (0.9 + rng() * 0.2);
      Sponsor s;
      s.slot = std::string(slot.key);
      s.brand = brand;
      s.type = std::string(type.key);
      if (type.key == "performance") {
        s.fee = r5(v * 0.7);
        s.bonus = SponsorBonus{r5(v * 0.35), r5(v * 0.6)};
        s.years = 2;
      } else if (type.key == "long") {
        s.fee = r5(v * 0.9);
        s.years = 3;
      } else {
        s.fee = r5(v);
        s.years = rng() < 0.5 ? 1 : 2;
      }
      offers.push_back(std::move(s));
    }
    out.set(std::string(slot.key), std::move(offers));
  }
  return out;
}

bool signSponsor(World& world, const TeamId& teamId, const std::string& slot, size_t index) {
  const auto offers = sponsorOffers(world, teamId);
  const auto* list = offers.find(slot);
  if (!list || index >= list->size()) return false;
  Club& club = ensureClub(world, teamId);
  Sponsor offer = (*list)[index];
  offer.from = planYear(world, club);
  club.sponsors.push_back(offer);
  std::string_view label;
  for (const auto& s : SPONSOR_SLOTS())
    if (s.key == slot) label = s.label;
  note(club, world.year,
       cat(offer.brand, " signed as ", lowerAll(label), ": ", money(offer.fee), " a season for ", offer.years, " season",
           offer.years > 1 ? "s" : "", ", from next season."));
  return true;
}

Club& activateClub(World& world, const TeamId& teamId) {
  Club& club = ensureClub(world, teamId);
  const int year = world.year;
  if (club.activatedYear == year) {
    club.inSeason = true;
    return club;
  }
  club.activatedYear = year;
  club.inSeason = true;
  std::vector<std::string> opened;
  for (const auto& [k, b] : club.building) {
    if (b.ready > year) continue;
    club.levels.v.set(k, std::max(club.levels[k], b.level));
    opened.push_back(k);
  }
  for (const auto& k : opened) {
    const int level = club.building.at(k).level;
    club.building.erase(k);
    note(club, year, cat(facility(k)->label, " level ", level, " is open."));
  }
  if (club.plan.pitch && club.levels["curator"] < 1) club.plan.pitch.reset();
  club.active = club.plan;
  credit(club, year, "central", CENTRAL_SHARE);
  for (const auto& d : club.sponsors)
    if (dealCovers(d, year)) credit(club, year, "sponsors", d.fee);
  debit(club, year, "running", RUNNING_COSTS);
  debit(club, year, "upkeep", upkeep(club.levels));
  Staff staff;
  for (const auto& r : STAFF_ROLES()) {
    const int tier = staffOf(club.staffPlan, r.key);
    const double cost = STAFF_TIERS()[static_cast<size_t>(tier)].cost;
    if (!cost) continue;
    if (cost > club.bank) {
      setStaffOf(club.staffPlan, r.key, 0);
      note(club, year, cat("Couldn't afford the ", lowerAll(r.label), "."));
      continue;
    }
    debit(club, year, "staff", cost);
    setStaffOf(staff, r.key, tier);
  }
  club.staff = staff;
  club.staffPaid = year;
  std::vector<Coach> keep;
  for (const auto& c : club.coaches) {
    const Player* p = world.players.find(c.playerId);
    if ((!c.ends || *c.ends > year) && p && p->team == teamId) keep.push_back(c);
  }
  club.coaches = std::move(keep);
  int i = 0;
  for (Coach* c : coachesIn(club, year)) {
    const int idx = i++;
    if (idx >= coachSlots(club)) {
      c->ends = year;
      continue;
    }
    const double cost = coachTier(c->tier)->cost;
    if (cost > club.bank) {
      c->ends = year;
      note(club, year, cat("Couldn't afford ", world.players.at(c->playerId).name, "'s coach."));
      continue;
    }
    debit(club, year, "coaches", cost);
    c->paid = year;
  }
  std::vector<Coach> still;
  for (const auto& c : club.coaches)
    if (!c.ends || *c.ends > year) still.push_back(c);
  club.coaches = std::move(still);
  trimBooks(club);
  return club;
}

void applyHomePitch(Season& season, const TeamId& teamId, const Club* club) {
  if (!club || !club->active.pitch) return;
  const int lvl = club->levels["curator"];
  const double share = lvl >= 0 && lvl < 4 ? CURATOR_SHARE[lvl] : 0;
  if (!share) return;
  Rng rng(season.seed ^ 0xc0de);
  for (auto& fx : season.fixtures) {
    if (fx.stage != Stage::league || fx.home != teamId) continue;
    if (rng() < share) fx.pitch = *club->active.pitch;
  }
  if (Venue* v = season.venues.find(teamId)) v->pitch = *club->active.pitch;
}

GateTake gateFor(const World& world, const TeamId& teamId, const GateFixture& fixture, double form) {
  return gateForClub(clubOf(world, teamId), teamId, world.year, fixture, form);
}

std::optional<GateTake> recordGate(World& world, const TeamId& teamId, const GateFixture& fixture, double form) {
  Club& club = ensureClub(world, teamId);
  for (const auto& g : club.gates)
    if (g.year == world.year && g.no == fixture.no) return std::nullopt;
  const GateTake g = gateForClub(club, teamId, world.year, fixture, form);
  credit(club, world.year, "tickets", g.tickets);
  credit(club, world.year, "matchday", g.extras);
  credit(club, world.year, "hospitality", g.hospitality);
  club.gates.push_back(Gate{world.year, fixture.no, fixture.away, fixture.stage, g.att, g.cap, g.revenue});
  if (club.gates.size() > 40) club.gates.erase(club.gates.begin(), club.gates.end() - 40);
  return g;
}

std::optional<ClubSeasonSummary> closeClubSeason(World& world, const TeamId& teamId, int position) {
  Club& club = ensureClub(world, teamId);
  const int year = world.year;
  if (club.closedYear == year) return std::nullopt;
  club.closedYear = year;
  club.inSeason = false;
  const double prize = prizeFor(position);
  const double share = club.active.prizeShare;
  const double shared = js_round((prize * share) / 100);
  credit(club, year, "prize", prize);
  debit(club, year, "playerBonus", shared);
  if (shared > 0)
    club.goodwill = Goodwill{year + 1, goodwillFrom(shared)};
  else
    club.goodwill.reset();
  for (const auto& d : club.sponsors) {
    if (!dealCovers(d, year) || !d.bonus) continue;
    if (position <= 4) credit(club, year, "bonuses", d.bonus->playoffs);
    if (position == 1) credit(club, year, "bonuses", d.bonus->title);
  }
  std::vector<Sponsor> keep;
  for (const auto& d : club.sponsors)
    if (d.from + d.years > year + 1) keep.push_back(d);
  club.sponsors = std::move(keep);
  std::vector<const Gate*> homes;
  for (const auto& g : club.gates)
    if (g.year == year && g.stage == Stage::league) homes.push_back(&g);
  double fill = 0;
  if (!homes.empty()) {
    double s = 0;
    for (const Gate* g : homes) s += g->att / g->cap;
    fill = s / static_cast<double>(homes.size());
  }
  const TicketTier* tier = ticketTier(club.active.tier);
  if (!tier) tier = ticketTier("standard");
  const double base = basePopularity(teamId);
  double pop = position == 1 ? 8 : position == 2 ? 5 : position <= 4 ? 3 : position <= 6 ? 0 : -3;
  pop += tier->fans + (fill >= 0.9 ? 1 : 0) + club.levels["fanzone"];
  pop += js_round((base - club.popularity) * 0.1);
  const double before = club.popularity;
  club.popularity = clamp(club.popularity + pop, 20, 100);
  Books& b = book(club, year);
  auto sum = [](const OrderedMap<std::string, double>& o) {
    double s = 0;
    for (const auto& [k, v] : o) s += v;
    return s;
  };
  ClubSeasonSummary summary;
  summary.year = year;
  summary.position = position;
  summary.prize = prize;
  summary.shared = shared;
  summary.popBefore = before;
  summary.popAfter = club.popularity;
  summary.income = sum(b.income);
  summary.spend = sum(b.spend);
  summary.net = sum(b.income) - sum(b.spend);
  summary.bank = club.bank;
  summary.homeGames = static_cast<int>(homes.size());
  summary.fill = js_round(fill * 100);
  summary.inRed = club.bank < 0;
  club.history.push_back(summary);
  if (club.history.size() > HISTORY_KEEP) club.history.erase(club.history.begin());
  if (summary.inRed)
    note(club, year, cat("The club finished the season ", money(-club.bank), " in the red. The board isn't impressed."));
  return summary;
}

std::vector<CoachingRow> applyCoaching(World& world, const TeamId& teamId, int year) {
  Club* club = world.clubs.find(teamId);
  if (!club || club->activatedYear != year) return {};
  const int hpc = club->levels["hpc"];
  const double boost = 1 + 0.1 * hpc;
  OrderedMap<PlayerId, CoachingRow> rows;
  auto row = [&](const Player& p) -> CoachingRow& {
    if (CoachingRow* r = rows.find(p.id)) return *r;
    CoachingRow r;
    r.id = p.id;
    r.before = mainRating(p);
    r.potBefore = p.potential;
    return rows.set(p.id, r);
  };
  const std::vector<PlayerId> squad = world.teams.at(teamId).squad;
  for (const auto& pid : squad) {
    Player* pp = world.players.find(pid);
    if (!pp) continue;
    Player& p = *pp;
    const double room = growthRoom(p);
    const double age = ageFactor(p.age);
    const Coach* c = nullptr;
    for (const auto& x : club->coaches)
      if (x.playerId == p.id && x.paid == year) {
        c = &x;
        break;
      }
    if (c) {
      const CoachTier& t = *coachTier(c->tier);
      CoachingRow& r = row(p);
      if (t.lift && p.age <= 24 && p.coachLift.value_or(0) < MAX_LIFT) {
        p.coachLift = p.coachLift.value_or(0) + t.lift;
        p.potential = std::min(97.0, p.potential.value_or(mainRating(p)) + t.lift);
      }
      r.gain += work(p, c->area, PERSONAL_BASE * t.mult * age * boost * std::max(0.25, growthRoom(p)));
      rows.find(p.id)->notes.push_back(cat(t.label, " ", lowerAll(coachArea(c->area)->label), " coach"));
    }
    if (p.age <= STAFF_MAX_AGE) {
      for (const auto& role : STAFF_ROLES()) {
        const int ti = staffOf(club->staff, role.key);
        const StaffTier& tier = STAFF_TIERS()[static_cast<size_t>(std::clamp(ti, 0, 3))];
        if (!tier.mult) continue;
        std::string area(role.area);
        if (role.key == "bowl") {
          if (!p.bowlType || p.bowl < 35) continue;
          area = isPaceType(p.bowlType) ? "pace" : "spin";
        }
        if (role.key == "bat" && p.role == Role::BOWL) continue;
        CoachingRow& r = row(p);
        r.gain += work(p, area, STAFF_BASE * tier.mult * age * boost * room);
        rows.find(p.id)->notes.emplace_back(role.label);
      }
    }
    if (hpc && p.age <= 23) {
      row(p);
      double g = 0;
      for (const auto& a : ownAreas(p)) g += work(p, a, HPC_BASE * hpc * age * room);
      CoachingRow& r = *rows.find(p.id);
      r.gain += g;
      r.notes.emplace_back("High-performance centre");
    }
    if (rows.contains(p.id)) {
      recomputeRatings(p);
      p.potential = std::max(p.potential.value_or(0), mainRating(p));
      if (!p.history.empty() && p.history.back().year == world.year) {
        p.history.back().bat = p.bat;
        p.history.back().bowl = p.bowl;
        p.history.back().field = p.field;
      }
    }
  }
  std::vector<CoachingRow> out;
  for (auto& [id, r0] : rows) {
    CoachingRow r = r0;
    const Player& p = world.players.at(id);
    r.after = mainRating(p);
    r.potAfter = p.potential;
    std::vector<std::string> uniq;
    for (const auto& n : r.notes)
      if (std::find(uniq.begin(), uniq.end(), n) == uniq.end()) uniq.push_back(n);
    r.notes = uniq;
    if (r.gain > 0 || r.potAfter != r.potBefore) out.push_back(std::move(r));
  }
  return out;
}

void clubOffseason(World& world, OffseasonReport& report) {
  const int year = world.year - 1;
  report.coaching = std::vector<CoachingRow>{};
  for (const auto& t : TEAMS()) {
    Club* club = world.clubs.find(t.id);
    if (!club) continue;
    club->inSeason = false;
    auto rows = applyCoaching(world, t.id, year);
    club = world.clubs.find(t.id);
    if (club->activatedYear == year) club->lastCoaching = LastCoaching{world.year, rows};
    for (auto r : rows) {
      r.team = t.id;
      report.coaching->push_back(std::move(r));
    }
  }
}

double academyQuality(const World& world, const TeamId& teamId) {
  const Club* c = world.clubs.find(teamId);
  return 2.5 * (c ? c->levels["academy"] : 0);
}
int academyScouts(const World& world, const std::optional<TeamId>& teamId) {
  if (!teamId) return 0;
  const Club* c = world.clubs.find(*teamId);
  return c ? c->levels["academy"] : 0;
}

double renewalPrice(const World& world, const TeamId& teamId, const Player& p, int years) {
  const double demand = renewalDemand(p, years);
  const Club* c = world.clubs.find(teamId);
  if (!c || !c->goodwill || c->goodwill->year != world.year) return demand;
  return std::max(30.0, js_round((demand * (1 - c->goodwill->pct / 100)) / 5) * 5);
}

double seasonForm(const Season& season, const TeamId& teamId) {
  int games = 0, won = 0;
  for (const auto& f : season.fixtures) {
    if (!f.result || (f.home != teamId && f.away != teamId)) continue;
    ++games;
    if (f.result->winner == teamId) ++won;
  }
  return games ? static_cast<double>(won) / games : 0.5;
}

GateTake projectGate(const World& world, const TeamId& teamId, const std::string& tier) {
  Club next = clubOf(world, teamId);
  next.active.tier = tier;
  next.levels = levelsAfterBuild(next);
  return gateForClub(next, teamId, world.year, GateFixture{0, std::nullopt, Stage::league}, 0.5);
}

}  // namespace t20
