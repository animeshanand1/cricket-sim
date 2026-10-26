#include "engine/state_json.hpp"

#include <cmath>

namespace t20 {

namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

template <class E, size_t N>
std::optional<E> parseName(const std::string_view (&names)[N], const Json& j) {
  if (!j.isString()) return std::nullopt;
  for (size_t i = 0; i < N; ++i)
    if (names[i] == j.str()) return static_cast<E>(i);
  return std::nullopt;
}

constexpr std::string_view kStageNames[] = {"league", "q1", "elim", "q2", "final"};

Json optStr(const std::optional<std::string>& s) { return s ? Json(*s) : Json(); }
Json optNum(const std::optional<double>& v) { return v ? Json(*v) : Json(); }
Json optInt(const std::optional<int>& v) { return v ? Json(*v) : Json(); }
std::optional<std::string> readOptStr(const Json& j) {
  if (j.isString()) return j.str();
  return std::nullopt;
}
std::optional<double> readOptNum(const Json& j) {
  if (j.isNumber()) return j.num();
  return std::nullopt;
}
std::optional<int> readOptInt(const Json& j) {
  if (j.isNumber()) return static_cast<int>(j.num());
  return std::nullopt;
}
Json strings(const std::vector<std::string>& v) {
  Json a = Json::array();
  for (const auto& s : v) a.push(s);
  return a;
}
std::vector<std::string> readStrings(const Json& j) {
  std::vector<std::string> out;
  for (const auto& e : j.elements())
    if (e.isString()) out.push_back(e.str());
  return out;
}

template <class K, size_t N>
Json skillSet(const SkillSet<K>& s, const std::string_view (&names)[N]) {
  Json o = Json::object();
  for (size_t i = 0; i < SkillSet<K>::N; ++i) {
    const K k = static_cast<K>(i);
    if (s.contains(k)) o.add(std::string(names[i]), s[k]);
  }
  return o;
}
template <class K, size_t N>
SkillSet<K> readSkillSet(const Json& j, const std::string_view (&names)[N]) {
  SkillSet<K> s;
  for (const auto& [k, v] : j.items()) {
    if (!v.isNumber()) continue;
    for (size_t i = 0; i < SkillSet<K>::N; ++i)
      if (names[i] == k) s.set(static_cast<K>(i), v.num());
  }
  return s;
}
Json skills(const Skills& s) {
  Json o = Json::object();
  o.add("bat", skillSet(s.bat, kBatKNames));
  o.add("bowl", skillSet(s.bowl, kBowlKNames));
  o.add("field", skillSet(s.field, kFieldKNames));
  return o;
}
Skills readSkills(const Json& j) {
  Skills s;
  s.bat = readSkillSet<BatK>(j["bat"], kBatKNames);
  s.bowl = readSkillSet<BowlK>(j["bowl"], kBowlKNames);
  s.field = readSkillSet<FieldK>(j["field"], kFieldKNames);
  return s;
}

Json teamIds(const std::vector<TeamId>& v) { return strings(v); }

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

Json toJson(const Contract& c) {
  Json o = Json::object();
  o.add("cost", c.cost);
  o.add("salary", c.salary);
  o.add("years", c.years);
  o.add("via", name(c.via));
  o.add("signed", c.signed_);
  return o;
}
Contract contractFrom(const Json& j) {
  Contract c;
  c.cost = j["cost"].num();
  c.salary = j["salary"].num(c.cost);
  c.years = j["years"].integer();
  c.via = parseName<ContractVia>(kContractViaNames, j["via"]).value_or(ContractVia::free);
  c.signed_ = j["signed"].integer();
  return c;
}

Json toJson(const Injury& i) {
  Json o = Json::object();
  o.add("type", i.type);
  o.add("label", i.label);
  o.add("matches", i.matches);
  o.add("year", i.year);
  if (i.replacedBy) o.add("replacedBy", *i.replacedBy);
  return o;
}
std::optional<Injury> injuryFrom(const Json& j) {
  if (!j.isObject() || !j["matches"].truthy()) return std::nullopt;
  Injury i;
  i.type = j["type"].str();
  i.label = j["label"].str();
  i.matches = j["matches"].integer();
  i.year = j["year"].integer();
  i.replacedBy = readOptStr(j["replacedBy"]);
  return i;
}

Json toJson(const HistoryPt& h) {
  Json o = Json::object();
  o.add("year", h.year);
  o.add("age", h.age);
  o.add("bat", h.bat);
  o.add("bowl", h.bowl);
  o.add("field", h.field);
  return o;
}

}  // namespace

Json toJson(const Player& p) {
  Json j = Json::object();
  j.add("id", p.id);
  j.add("name", p.name);
  j.add("role", name(p.role));
  j.add("hand", name(p.hand));
  j.add("age", p.age);
  j.add("bat", p.bat);
  j.add("power", p.power);
  j.add("bowl", p.bowl);
  j.add("bowlType", p.bowlType ? Json(name(*p.bowlType)) : Json());
  j.add("field", p.field);
  j.add("awareness", p.awareness);
  j.add("skills", skills(p.skills));
  j.add("batStyle", p.batStyle);
  j.add("bowlStyle", optStr(p.bowlStyle));
  j.add("overseas", p.overseas);
  if (p.special) j.add("special", name(*p.special));
  if (p.potential) j.add("potential", *p.potential);
  j.add("team", optStr(p.team));
  j.add("origin", name(p.origin));
  if (p.retired) {
    Json r = Json::object();
    r.add("year", p.retired->year);
    r.add("age", p.retired->age);
    r.add("team", optStr(p.retired->team));
    if (p.retired->quiet) r.add("quiet", true);
    j.add("retired", r);
  } else {
    j.add("retired", Json());
  }
  Json h = Json::array();
  for (const auto& x : p.history) h.push(toJson(x));
  j.add("history", h);
  if (p.joined) j.add("joined", *p.joined);
  if (p.contract) j.add("contract", toJson(*p.contract));
  if (p.capped) j.add("capped", *p.capped);
  if (p.fitness) j.add("fitness", *p.fitness);
  if (p.injury) j.add("injury", toJson(*p.injury));
  if (p.skillFrac) {
    Json f = Json::object();
    f.add("bat", skillSet(p.skillFrac->bat, kBatKNames));
    f.add("bowl", skillSet(p.skillFrac->bowl, kBowlKNames));
    f.add("field", skillSet(p.skillFrac->field, kFieldKNames));
    j.add("skillFrac", f);
  }
  if (p.edited) j.add("edited", true);
  if (p.preEdit) {
    const PreEdit& e = *p.preEdit;
    Json o = Json::object();
    o.add("name", e.name);
    o.add("age", e.age);
    o.add("skills", skills(e.skills));
    o.add("year", e.year);
    if (e.hasKept) {
      o.add("role", name(e.role));
      o.add("hand", name(e.hand));
      o.add("bowlType", e.bowlType ? Json(name(*e.bowlType)) : Json());
      o.add("bowlStyle", optStr(e.bowlStyle));
      o.add("special", e.special ? Json(name(*e.special)) : Json());
    }
    j.add("preEdit", o);
  }
  if (p.drafted) {
    Json d = Json::object();
    d.add("year", p.drafted->year);
    d.add("no", p.drafted->no);
    d.add("team", p.drafted->team);
    j.add("drafted", d);
  }
  if (p.formerTeam) j.add("formerTeam", *p.formerTeam);
  if (p.unsoldYears) j.add("unsoldYears", *p.unsoldYears);
  if (p.coachLift) j.add("coachLift", *p.coachLift);
  return j;
}

Player playerFromJson(const Json& j) {
  Player p;
  p.id = j["id"].str();
  p.name = j["name"].str();
  p.role = parseName<Role>(kRoleNames, j["role"]).value_or(Role::BAT);
  p.hand = parseName<Hand>(kHandNames, j["hand"]).value_or(Hand::R);
  p.age = j["age"].integer();
  p.bat = j["bat"].num();
  p.power = j["power"].num();
  p.bowl = j["bowl"].num();
  p.bowlType = parseName<BowlType>(kBowlTypeNames, j["bowlType"]);
  p.field = j["field"].num();
  p.awareness = j["awareness"].num();
  p.skills = readSkills(j["skills"]);
  p.batStyle = j["batStyle"].str();
  p.bowlStyle = readOptStr(j["bowlStyle"]);
  p.overseas = j["overseas"].truthy();
  p.special = parseName<SpecialKey>(kSpecialKeyNames, j["special"]);
  p.potential = readOptNum(j["potential"]);
  p.team = readOptStr(j["team"]);
  p.origin = parseName<Origin>(kOriginNames, j["origin"]).value_or(Origin::roster);
  if (j["retired"].isObject()) {
    const Json& r = j["retired"];
    p.retired = Retired{r["year"].integer(), r["age"].integer(), readOptStr(r["team"]), r["quiet"].truthy()};
  }
  for (const auto& h : j["history"].elements())
    p.history.push_back({h["year"].integer(), h["age"].integer(), h["bat"].num(), h["bowl"].num(), h["field"].num()});
  p.joined = readOptInt(j["joined"]);
  if (j["contract"].isObject()) p.contract = contractFrom(j["contract"]);
  if (j["capped"].isBool()) p.capped = j["capped"].boolean();
  p.fitness = readOptNum(j["fitness"]);
  p.injury = injuryFrom(j["injury"]);
  if (j["skillFrac"].isObject()) {
    SkillFrac f;
    f.bat = readSkillSet<BatK>(j["skillFrac"]["bat"], kBatKNames);
    f.bowl = readSkillSet<BowlK>(j["skillFrac"]["bowl"], kBowlKNames);
    f.field = readSkillSet<FieldK>(j["skillFrac"]["field"], kFieldKNames);
    p.skillFrac = f;
  }
  p.edited = j["edited"].truthy();
  if (j["preEdit"].isObject()) {
    const Json& o = j["preEdit"];
    PreEdit e;
    e.name = o["name"].str();
    e.age = o["age"].integer();
    e.skills = readSkills(o["skills"]);
    e.year = o["year"].integer();
    e.hasKept = o.find("role") != nullptr;
    e.role = parseName<Role>(kRoleNames, o["role"]).value_or(p.role);
    e.hand = parseName<Hand>(kHandNames, o["hand"]).value_or(p.hand);
    e.bowlType = parseName<BowlType>(kBowlTypeNames, o["bowlType"]);
    e.bowlStyle = readOptStr(o["bowlStyle"]);
    e.special = parseName<SpecialKey>(kSpecialKeyNames, o["special"]);
    p.preEdit = e;
  }
  if (j["drafted"].isObject())
    p.drafted = Drafted{j["drafted"]["year"].integer(), j["drafted"]["no"].integer(), j["drafted"]["team"].str()};
  p.formerTeam = readOptStr(j["formerTeam"]);
  p.unsoldYears = readOptInt(j["unsoldYears"]);
  p.coachLift = readOptInt(j["coachLift"]);
  return p;
}

namespace {

// ---------------------------------------------------------------------------
// World parts
// ---------------------------------------------------------------------------

Json toJson(const Rating3& r) {
  Json o = Json::object();
  o.add("bat", r.bat);
  o.add("bowl", r.bowl);
  o.add("field", r.field);
  return o;
}
Rating3 ratingFrom(const Json& j) { return {j["bat"].num(), j["bowl"].num(), j["field"].num()}; }

Json toJson(const Sale& s) {
  Json o = Json::object();
  o.add("id", s.id);
  o.add("team", s.team);
  o.add("price", s.price);
  o.add("rtm", s.rtm);
  o.add("no", s.no);
  o.add("set", s.set);
  o.add("former", optStr(s.former));
  return o;
}
Sale saleFrom(const Json& j) {
  return {j["id"].str(), j["team"].str(), j["price"].num(), j["rtm"].truthy(), j["no"].integer(), j["set"].str(),
          readOptStr(j["former"])};
}
Json toJson(const Signing& s) {
  Json o = Json::object();
  o.add("id", s.id);
  o.add("team", s.team);
  if (s.price) o.add("price", *s.price);
  return o;
}
Signing signingFrom(const Json& j) { return {j["id"].str(), j["team"].str(), readOptNum(j["price"])}; }

Json toJson(const AuctionResult& r) {
  Json o = Json::object();
  o.add("type", r.type);
  Json sold = Json::array();
  for (const auto& s : r.sold) sold.push(toJson(s));
  o.add("sold", sold);
  o.add("unsold", strings(r.unsold));
  Json sg = Json::array();
  for (const auto& s : r.signed_) sg.push(toJson(s));
  o.add("signed", sg);
  return o;
}
AuctionResult auctionResultFrom(const Json& j) {
  AuctionResult r;
  r.type = j["type"].str();
  for (const auto& s : j["sold"].elements()) r.sold.push_back(saleFrom(s));
  r.unsold = readStrings(j["unsold"]);
  for (const auto& s : j["signed"].elements()) r.signed_.push_back(signingFrom(s));
  return r;
}

Json toJson(const CoachingRow& r) {
  Json o = Json::object();
  o.add("id", r.id);
  o.add("before", r.before);
  o.add("potBefore", optNum(r.potBefore));
  o.add("gain", r.gain);
  o.add("notes", strings(r.notes));
  o.add("after", r.after);
  o.add("potAfter", optNum(r.potAfter));
  if (r.team) o.add("team", *r.team);
  return o;
}
CoachingRow coachingRowFrom(const Json& j) {
  CoachingRow r;
  r.id = j["id"].str();
  r.before = j["before"].num();
  r.potBefore = readOptNum(j["potBefore"]);
  r.gain = j["gain"].num();
  r.notes = readStrings(j["notes"]);
  r.after = j["after"].num();
  r.potAfter = readOptNum(j["potAfter"]);
  r.team = readOptStr(j["team"]);
  return r;
}

Json toJson(const OffseasonReport& r) {
  Json o = Json::object();
  o.add("year", r.year);
  o.add("mega", r.mega);
  auto rows = [&](const char* key, const auto& list, auto fn) {
    Json a = Json::array();
    for (const auto& x : list) a.push(fn(x));
    o.add(key, a);
  };
  rows("aged", r.aged, [](const AgedRow& x) {
    Json e = Json::object();
    e.add("id", x.id);
    e.add("team", x.team);
    e.add("age", x.age);
    e.add("before", toJson(x.before));
    e.add("after", toJson(x.after));
    return e;
  });
  rows("retired", r.retired, [](const RetiredRow& x) {
    Json e = Json::object();
    e.add("id", x.id);
    e.add("team", x.team);
    e.add("age", x.age);
    e.add("main", x.main);
    return e;
  });
  auto move = [](const MoveRow& x) {
    Json e = Json::object();
    e.add("id", x.id);
    e.add("team", x.team);
    return e;
  };
  rows("released", r.released, move);
  rows("renewed", r.renewed, [](const RenewedRow& x) {
    Json e = Json::object();
    e.add("id", x.id);
    e.add("team", x.team);
    e.add("cost", x.cost);
    e.add("years", x.years);
    return e;
  });
  rows("retained", r.retained, [](const RetainedRow& x) {
    Json e = Json::object();
    e.add("id", x.id);
    e.add("team", x.team);
    e.add("cost", x.cost);
    return e;
  });
  rows("academy", r.academy, move);
  rows("drafted", r.drafted, [](const DraftedRow& x) {
    Json e = Json::object();
    e.add("id", x.id);
    e.add("team", x.team);
    e.add("no", x.no);
    return e;
  });
  rows("signed", r.signed_, [](const Signing& x) { return toJson(x); });
  o.add("auction", r.auction ? toJson(*r.auction) : Json());
  if (r.coaching) rows("coaching", *r.coaching, [](const CoachingRow& x) { return toJson(x); });
  return o;
}
OffseasonReport reportFrom(const Json& j) {
  OffseasonReport r;
  r.year = j["year"].integer();
  r.mega = j["mega"].truthy();
  for (const auto& e : j["aged"].elements())
    r.aged.push_back({e["id"].str(), e["team"].str(), e["age"].integer(), ratingFrom(e["before"]), ratingFrom(e["after"])});
  for (const auto& e : j["retired"].elements())
    r.retired.push_back({e["id"].str(), e["team"].str(), e["age"].integer(), e["main"].num()});
  for (const auto& e : j["released"].elements()) r.released.push_back({e["id"].str(), e["team"].str()});
  for (const auto& e : j["renewed"].elements())
    r.renewed.push_back({e["id"].str(), e["team"].str(), e["cost"].num(), e["years"].integer()});
  for (const auto& e : j["retained"].elements()) r.retained.push_back({e["id"].str(), e["team"].str(), e["cost"].num()});
  for (const auto& e : j["academy"].elements()) r.academy.push_back({e["id"].str(), e["team"].str()});
  for (const auto& e : j["drafted"].elements()) r.drafted.push_back({e["id"].str(), e["team"].str(), e["no"].integer()});
  for (const auto& e : j["signed"].elements()) r.signed_.push_back(signingFrom(e));
  if (j["auction"].isObject()) r.auction = auctionResultFrom(j["auction"]);
  if (j["coaching"].isArray()) {
    r.coaching = std::vector<CoachingRow>{};
    for (const auto& e : j["coaching"].elements()) r.coaching->push_back(coachingRowFrom(e));
  }
  return r;
}

Json toJson(const Club& c) {
  Json o = Json::object();
  o.add("bank", c.bank);
  o.add("popularity", c.popularity);
  Json lv = Json::object();
  for (const auto& [k, v] : c.levels.v) lv.add(k, v);
  o.add("levels", lv);
  Json b = Json::object();
  for (const auto& [k, x] : c.building) {
    Json e = Json::object();
    e.add("level", x.level);
    e.add("ready", x.ready);
    e.add("cost", x.cost);
    b.add(k, e);
  }
  o.add("building", b);
  auto plan = [](const ClubPlan& p) {
    Json e = Json::object();
    e.add("tier", p.tier);
    e.add("pitch", p.pitch ? Json(name(*p.pitch)) : Json());
    e.add("prizeShare", p.prizeShare);
    return e;
  };
  o.add("plan", plan(c.plan));
  o.add("active", plan(c.active));
  auto staff = [](const Staff& s) {
    Json e = Json::object();
    e.add("bat", s.bat);
    e.add("bowl", s.bowl);
    e.add("field", s.field);
    return e;
  };
  o.add("staffPlan", staff(c.staffPlan));
  o.add("staff", staff(c.staff));
  o.add("staffPaid", optInt(c.staffPaid));
  Json sp = Json::array();
  for (const auto& s : c.sponsors) {
    Json e = Json::object();
    e.add("slot", s.slot);
    e.add("brand", s.brand);
    e.add("type", s.type);
    e.add("fee", s.fee);
    if (s.bonus) {
      Json bo = Json::object();
      bo.add("playoffs", s.bonus->playoffs);
      bo.add("title", s.bonus->title);
      e.add("bonus", bo);
    } else {
      e.add("bonus", Json());
    }
    e.add("years", s.years);
    e.add("from", s.from);
    sp.push(e);
  }
  o.add("sponsors", sp);
  Json co = Json::array();
  for (const auto& x : c.coaches) {
    Json e = Json::object();
    e.add("playerId", x.playerId);
    e.add("area", x.area);
    e.add("tier", x.tier);
    e.add("from", x.from);
    e.add("ends", optInt(x.ends));
    e.add("paid", optInt(x.paid));
    co.push(e);
  }
  o.add("coaches", co);
  if (c.goodwill) {
    Json g = Json::object();
    g.add("year", c.goodwill->year);
    g.add("pct", c.goodwill->pct);
    o.add("goodwill", g);
  } else {
    o.add("goodwill", Json());
  }
  o.add("inSeason", c.inSeason);
  o.add("activatedYear", optInt(c.activatedYear));
  o.add("closedYear", optInt(c.closedYear));
  Json bk = Json::object();
  for (const auto& [year, books] : c.books) {
    Json e = Json::object();
    Json in = Json::object(), out = Json::object();
    for (const auto& [k, v] : books.income) in.add(k, v);
    for (const auto& [k, v] : books.spend) out.add(k, v);
    e.add("income", in);
    e.add("spend", out);
    bk.add(std::to_string(year), e);
  }
  o.add("books", bk);
  Json ga = Json::array();
  for (const auto& g : c.gates) {
    Json e = Json::object();
    e.add("year", g.year);
    e.add("no", g.no);
    e.add("opp", optStr(g.opp));
    e.add("stage", name(g.stage));
    e.add("att", g.att);
    e.add("cap", g.cap);
    e.add("revenue", g.revenue);
    ga.push(e);
  }
  o.add("gates", ga);
  Json hi = Json::array();
  for (const auto& s : c.history) {
    Json e = Json::object();
    e.add("year", s.year);
    e.add("position", s.position);
    e.add("prize", s.prize);
    e.add("shared", s.shared);
    Json pop = Json::object();
    pop.add("before", s.popBefore);
    pop.add("after", s.popAfter);
    e.add("popularity", pop);
    e.add("income", s.income);
    e.add("spend", s.spend);
    e.add("net", s.net);
    e.add("bank", s.bank);
    e.add("homeGames", s.homeGames);
    e.add("fill", s.fill);
    e.add("inRed", s.inRed);
    hi.push(e);
  }
  o.add("history", hi);
  if (c.lastCoaching) {
    Json e = Json::object();
    e.add("year", c.lastCoaching->year);
    Json rows = Json::array();
    for (const auto& r : c.lastCoaching->rows) rows.push(toJson(r));
    e.add("rows", rows);
    o.add("lastCoaching", e);
  } else {
    o.add("lastCoaching", Json());
  }
  Json lg = Json::array();
  for (const auto& n : c.log) {
    Json e = Json::object();
    e.add("year", n.year);
    e.add("text", n.text);
    lg.push(e);
  }
  o.add("log", lg);
  return o;
}

Club clubFrom(const Json& j) {
  Club c;
  c.bank = j["bank"].num(3000);
  c.popularity = j["popularity"].num(65);
  for (const auto& [k, v] : j["levels"].items()) c.levels.v.set(k, v.integer());
  for (const auto& [k, v] : j["building"].items())
    c.building.set(k, Building{v["level"].integer(), v["ready"].integer(), v["cost"].num()});
  auto plan = [](const Json& e) {
    ClubPlan p;
    p.tier = e["tier"].isString() ? e["tier"].str() : "standard";
    p.pitch = parseName<Pitch>(kPitchNames, e["pitch"]);
    p.prizeShare = e["prizeShare"].integer();
    return p;
  };
  c.plan = plan(j["plan"]);
  c.active = plan(j["active"]);
  auto staff = [](const Json& e) { return Staff{e["bat"].integer(), e["bowl"].integer(), e["field"].integer()}; };
  c.staffPlan = staff(j["staffPlan"]);
  c.staff = staff(j["staff"]);
  c.staffPaid = readOptInt(j["staffPaid"]);
  for (const auto& e : j["sponsors"].elements()) {
    Sponsor s;
    s.slot = e["slot"].str();
    s.brand = e["brand"].str();
    s.type = e["type"].str();
    s.fee = e["fee"].num();
    if (e["bonus"].isObject()) s.bonus = SponsorBonus{e["bonus"]["playoffs"].num(), e["bonus"]["title"].num()};
    s.years = e["years"].integer();
    s.from = e["from"].integer();
    c.sponsors.push_back(s);
  }
  for (const auto& e : j["coaches"].elements())
    c.coaches.push_back({e["playerId"].str(), e["area"].str(), e["tier"].integer(1), e["from"].integer(),
                         readOptInt(e["ends"]), readOptInt(e["paid"])});
  if (j["goodwill"].isObject()) c.goodwill = Goodwill{j["goodwill"]["year"].integer(), j["goodwill"]["pct"].num()};
  c.inSeason = j["inSeason"].truthy();
  c.activatedYear = readOptInt(j["activatedYear"]);
  c.closedYear = readOptInt(j["closedYear"]);
  for (const auto& [k, v] : j["books"].items()) {
    Books b;
    for (const auto& [ik, iv] : v["income"].items()) b.income.set(ik, iv.num());
    for (const auto& [sk, sv] : v["spend"].items()) b.spend.set(sk, sv.num());
    c.books[std::atoi(k.c_str())] = std::move(b);
  }
  for (const auto& e : j["gates"].elements()) {
    Gate g;
    g.year = e["year"].integer();
    g.no = e["no"].integer();
    g.opp = readOptStr(e["opp"]);
    g.stage = parseName<Stage>(kStageNames, e["stage"]).value_or(Stage::league);
    g.att = e["att"].num();
    g.cap = e["cap"].num();
    g.revenue = e["revenue"].num();
    c.gates.push_back(g);
  }
  for (const auto& e : j["history"].elements()) {
    ClubSeasonSummary s;
    s.year = e["year"].integer();
    s.position = e["position"].integer();
    s.prize = e["prize"].num();
    s.shared = e["shared"].num();
    s.popBefore = e["popularity"]["before"].num();
    s.popAfter = e["popularity"]["after"].num();
    s.income = e["income"].num();
    s.spend = e["spend"].num();
    s.net = e["net"].num();
    s.bank = e["bank"].num();
    s.homeGames = e["homeGames"].integer();
    s.fill = e["fill"].num();
    s.inRed = e["inRed"].truthy();
    c.history.push_back(s);
  }
  if (j["lastCoaching"].isObject()) {
    LastCoaching lc;
    lc.year = j["lastCoaching"]["year"].integer();
    for (const auto& e : j["lastCoaching"]["rows"].elements()) lc.rows.push_back(coachingRowFrom(e));
    c.lastCoaching = lc;
  }
  for (const auto& e : j["log"].elements()) c.log.push_back({e["year"].integer(), e["text"].str()});
  return c;
}

Json toJson(const Lot& l) {
  Json o = Json::object();
  o.add("id", l.id);
  o.add("base", l.base);
  o.add("price", optNum(l.price));
  o.add("leader", optStr(l.leader));
  Json bids = Json::array();
  for (const auto& b : l.bids) {
    Json e = Json::object();
    e.add("team", b.team);
    e.add("price", b.price);
    bids.push(e);
  }
  o.add("bids", bids);
  Json out = Json::object();
  for (const auto& [t, v] : l.out) out.add(t, v);
  o.add("out", out);
  o.add("stage", l.stage);
  Json vals = Json::object();
  for (const auto& [t, v] : l.vals) vals.add(t, v);
  o.add("vals", vals);
  o.add("rtmTeam", optStr(l.rtmTeam));
  o.add("userMax", optNum(l.userMax));
  return o;
}
Lot lotFrom(const Json& j) {
  Lot l;
  l.id = j["id"].str();
  l.base = j["base"].num();
  l.price = readOptNum(j["price"]);
  l.leader = readOptStr(j["leader"]);
  for (const auto& e : j["bids"].elements()) l.bids.push_back({e["team"].str(), e["price"].num()});
  for (const auto& [t, v] : j["out"].items())
    if (v.truthy()) l.out.set(t, true);
  l.stage = j["stage"].isString() ? j["stage"].str() : "bidding";
  for (const auto& [t, v] : j["vals"].items()) l.vals.set(t, v.num());
  l.rtmTeam = readOptStr(j["rtmTeam"]);
  l.userMax = readOptNum(j["userMax"]);
  return l;
}

Json toJson(const BoardEntry& b) {
  Json o = Json::object();
  if (b.season) o.add("season", *b.season);
  if (b.event) o.add("event", *b.event);
  if (b.team) o.add("team", *b.team);
  if (b.target) o.add("target", *b.target);
  if (b.position) o.add("position", *b.position);
  if (b.verdict) o.add("verdict", *b.verdict);
  if (b.delta) o.add("delta", *b.delta);
  if (b.confidence) o.add("confidence", *b.confidence);
  if (b.inRed) o.add("inRed", *b.inRed);
  if (b.from) o.add("from", *b.from);
  if (b.to) o.add("to", *b.to);
  return o;
}
BoardEntry boardEntryFrom(const Json& j) {
  BoardEntry b;
  b.season = readOptInt(j["season"]);
  b.event = readOptStr(j["event"]);
  b.team = readOptStr(j["team"]);
  b.target = readOptStr(j["target"]);
  b.position = readOptInt(j["position"]);
  b.verdict = readOptStr(j["verdict"]);
  b.delta = readOptNum(j["delta"]);
  b.confidence = readOptNum(j["confidence"]);
  if (j["inRed"].isBool()) b.inRed = j["inRed"].boolean();
  b.from = readOptStr(j["from"]);
  b.to = readOptStr(j["to"]);
  return b;
}

}  // namespace

Json toJson(const World& w) {
  Json o = Json::object();
  o.add("version", w.version);
  o.add("year", w.year);
  o.add("nextId", w.nextId);
  Json players = Json::object();
  for (const auto& [id, p] : w.players) players.add(id, toJson(p));
  o.add("players", players);
  Json teams = Json::object();
  for (const auto& [id, t] : w.teams) {
    Json e = Json::object();
    e.add("squad", strings(t.squad));
    if (t.rtm) e.add("rtm", *t.rtm);
    if (t.captainId) e.add("captainId", *t.captainId);
    teams.add(id, e);
  }
  o.add("teams", teams);
  if (w.draft) {
    const Draft& d = *w.draft;
    Json e = Json::object();
    e.add("year", d.year);
    e.add("pool", strings(d.pool));
    e.add("order", teamIds(d.order));
    Json made = Json::array();
    for (const auto& m : d.made) {
      Json x = Json::object();
      x.add("team", m.team);
      x.add("id", optStr(m.id));
      x.add("no", m.no);
      if (m.skipped) x.add("skipped", true);
      made.push(x);
    }
    e.add("made", made);
    Json sc = Json::object();
    for (const auto& [id, v] : d.scouted) sc.add(id, v);
    e.add("scouted", sc);
    e.add("scoutsLeft", d.scoutsLeft);
    e.add("userTeam", optStr(d.userTeam));
    e.add("then", optStr(d.then));
    o.add("draft", e);
  } else {
    o.add("draft", Json());
  }
  if (w.auction) {
    const Auction& a = *w.auction;
    Json e = Json::object();
    e.add("year", a.year);
    e.add("type", a.type);
    e.add("lots", strings(a.lots));
    Json so = Json::object();
    for (const auto& [id, s] : a.setOf) so.add(id, s);
    e.add("setOf", so);
    e.add("idx", a.idx);
    e.add("lot", a.lot ? toJson(*a.lot) : Json());
    Json res = Json::array();
    for (const auto& s : a.results) res.push(toJson(s));
    e.add("results", res);
    e.add("unsold", strings(a.unsold));
    e.add("userTeam", optStr(a.userTeam));
    e.add("userAuto", a.userAuto);
    e.add("userMax", Json());
    e.add("log", strings(a.log));
    o.add("auction", e);
  } else {
    o.add("auction", Json());
  }
  if (w.offseason) {
    const Offseason& s = *w.offseason;
    Json e = Json::object();
    e.add("year", s.year);
    e.add("stage", s.stage);
    e.add("mega", s.mega);
    e.add("order", teamIds(s.order));
    e.add("userTeam", optStr(s.userTeam));
    e.add("then", optStr(s.then));
    e.add("retain", strings(s.retain));
    o.add("offseason", e);
  } else {
    o.add("offseason", Json());
  }
  o.add("lastOffseason", w.lastOffseason ? toJson(*w.lastOffseason) : Json());
  {
    const Manager& m = w.manager;
    Json e = Json::object();
    e.add("team", optStr(m.team));
    e.add("confidence", m.confidence);
    if (m.target) {
      Json t = Json::object();
      t.add("key", m.target->key);
      t.add("label", m.target->label);
      t.add("need", m.target->need);
      t.add("rank", m.target->rank);
      t.add("season", m.target->season);
      e.add("target", t);
    } else {
      e.add("target", Json());
    }
    Json h = Json::array();
    for (const auto& b : m.history) h.push(toJson(b));
    e.add("history", h);
    if (m.sacked) {
      Json s = Json::object();
      s.add("season", m.sacked->season);
      s.add("team", m.sacked->team);
      s.add("offers", teamIds(m.sacked->offers));
      s.add("order", m.sacked->order ? teamIds(*m.sacked->order) : Json());
      e.add("sacked", s);
    } else {
      e.add("sacked", Json());
    }
    o.add("manager", e);
  }
  Json clubs = Json::object();
  for (const auto& [id, c] : w.clubs) clubs.add(id, toJson(c));
  o.add("clubs", clubs);
  Json tr = Json::array();
  for (const auto& t : w.transfers) {
    Json e = Json::object();
    e.add("year", t.year);
    e.add("kind", t.kind);
    e.add("team", t.team);
    if (t.with) e.add("with", *t.with);
    e.add("got", strings(t.got));
    if (t.gave) e.add("gave", strings(*t.gave));
    if (t.cash) e.add("cash", *t.cash);
    if (t.for_) e.add("for", *t.for_);
    e.add("text", t.text);
    tr.push(e);
  }
  o.add("transfers", tr);
  Json inj = Json::array();
  for (const auto& n : w.lastInjuries) {
    Json e = Json::object();
    e.add("id", n.id);
    e.add("team", n.team);
    e.add("label", n.label);
    e.add("matches", n.matches);
    inj.push(e);
  }
  o.add("lastInjuries", inj);
  return o;
}

World worldFromJson(const Json& j) {
  World w;
  w.version = j["version"].integer(1);
  w.year = j["year"].integer();
  w.nextId = j["nextId"].integer(1);
  const Json& players = j["players"];
  w.players.reserve(players.size());
  for (const auto& [id, p] : players.items()) {
    Player x = playerFromJson(p);
    if (x.id.empty()) x.id = id;
    w.players.set(id, std::move(x));
  }
  for (const auto& [id, t] : j["teams"].items()) {
    WorldTeam wt;
    wt.squad = readStrings(t["squad"]);
    wt.rtm = readOptInt(t["rtm"]);
    wt.captainId = readOptStr(t["captainId"]);
    w.teams.set(id, wt);
  }
  if (j["draft"].isObject()) {
    const Json& e = j["draft"];
    Draft d;
    d.year = e["year"].integer();
    d.pool = readStrings(e["pool"]);
    d.order = readStrings(e["order"]);
    for (const auto& m : e["made"].elements())
      d.made.push_back({m["team"].str(), readOptStr(m["id"]), m["no"].integer(), m["skipped"].truthy()});
    for (const auto& [id, v] : e["scouted"].items())
      if (v.truthy()) d.scouted.set(id, true);
    d.scoutsLeft = e["scoutsLeft"].integer();
    d.userTeam = readOptStr(e["userTeam"]);
    d.then = readOptStr(e["then"]);
    w.draft = std::move(d);
  }
  if (j["auction"].isObject()) {
    const Json& e = j["auction"];
    Auction a;
    a.year = e["year"].integer();
    a.type = e["type"].str();
    a.lots = readStrings(e["lots"]);
    for (const auto& [id, s] : e["setOf"].items()) a.setOf.set(id, s.str());
    a.idx = e["idx"].integer();
    if (e["lot"].isObject()) a.lot = lotFrom(e["lot"]);
    for (const auto& s : e["results"].elements()) a.results.push_back(saleFrom(s));
    a.unsold = readStrings(e["unsold"]);
    a.userTeam = readOptStr(e["userTeam"]);
    a.userAuto = e["userAuto"].truthy();
    a.log = readStrings(e["log"]);
    w.auction = std::move(a);
  }
  if (j["offseason"].isObject()) {
    const Json& e = j["offseason"];
    Offseason s;
    s.year = e["year"].integer();
    s.stage = e["stage"].str();
    s.mega = e["mega"].truthy();
    s.order = readStrings(e["order"]);
    s.userTeam = readOptStr(e["userTeam"]);
    s.then = readOptStr(e["then"]);
    s.retain = readStrings(e["retain"]);
    w.offseason = std::move(s);
  }
  if (j["lastOffseason"].isObject()) w.lastOffseason = reportFrom(j["lastOffseason"]);
  {
    const Json& e = j["manager"];
    Manager& m = w.manager;
    m.team = readOptStr(e["team"]);
    m.confidence = e["confidence"].num(60);
    if (e["target"].isObject()) {
      const Json& t = e["target"];
      m.target = Target{t["key"].str(), t["label"].str(), t["need"].integer(), t["rank"].integer(), t["season"].integer()};
    }
    for (const auto& b : e["history"].elements()) m.history.push_back(boardEntryFrom(b));
    if (e["sacked"].isObject()) {
      const Json& s = e["sacked"];
      Sacked sk;
      sk.season = s["season"].integer();
      sk.team = s["team"].str();
      sk.offers = readStrings(s["offers"]);
      if (s["order"].isArray()) sk.order = readStrings(s["order"]);
      m.sacked = sk;
    }
  }
  for (const auto& [id, c] : j["clubs"].items()) w.clubs.set(id, clubFrom(c));
  for (const auto& e : j["transfers"].elements()) {
    Transfer t;
    t.year = e["year"].integer();
    t.kind = e["kind"].str();
    t.team = e["team"].str();
    t.with = readOptStr(e["with"]);
    t.got = readStrings(e["got"]);
    if (e["gave"].isArray()) t.gave = readStrings(e["gave"]);
    t.cash = readOptNum(e["cash"]);
    t.for_ = readOptStr(e["for"]);
    t.text = e["text"].str();
    w.transfers.push_back(std::move(t));
  }
  for (const auto& e : j["lastInjuries"].elements())
    w.lastInjuries.push_back({e["id"].str(), e["team"].str(), e["label"].str(), e["matches"].integer()});
  return w;
}

// ---------------------------------------------------------------------------
// Career
// ---------------------------------------------------------------------------

namespace {

Json toJson(const CareerStats& s) {
  Json o = Json::object();
  o.add("matches", s.matches);
  o.add("wins", s.wins);
  o.add("potm", s.potm);
  Json b = Json::object();
  b.add("inns", s.bat.inns);
  b.add("notOuts", s.bat.notOuts);
  b.add("runs", s.bat.runs);
  b.add("balls", s.bat.balls);
  b.add("fours", s.bat.fours);
  b.add("sixes", s.bat.sixes);
  b.add("hs", s.bat.hs);
  b.add("hsNotOut", s.bat.hsNotOut);
  b.add("fifties", s.bat.fifties);
  b.add("hundreds", s.bat.hundreds);
  b.add("ducks", s.bat.ducks);
  o.add("bat", b);
  Json w = Json::object();
  w.add("inns", s.bowl.inns);
  w.add("balls", s.bowl.balls);
  w.add("runs", s.bowl.runs);
  w.add("wickets", s.bowl.wickets);
  w.add("maidens", s.bowl.maidens);
  w.add("dots", s.bowl.dots);
  w.add("bestW", s.bowl.bestW);
  w.add("bestR", s.bowl.bestR);
  w.add("fourW", s.bowl.fourW);
  o.add("bowl", w);
  Json f = Json::object();
  f.add("catches", s.field.catches);
  f.add("stumpings", s.field.stumpings);
  f.add("runOuts", s.field.runOuts);
  f.add("drops", s.field.drops);
  o.add("field", f);
  Json r = Json::array();
  for (const auto& x : s.recent) {
    Json e = Json::object();
    e.add("vs", x.vs);
    e.add("bat", optStr(x.bat));
    e.add("bowl", optStr(x.bowl));
    e.add("result", x.result);
    r.push(e);
  }
  o.add("recent", r);
  return o;
}
CareerStats statsFrom(const Json& j) {
  CareerStats s;
  s.matches = j["matches"].integer();
  s.wins = j["wins"].integer();
  s.potm = j["potm"].integer();
  const Json& b = j["bat"];
  s.bat = {b["inns"].integer(),  b["notOuts"].integer(), b["runs"].integer(),     b["balls"].integer(),
           b["fours"].integer(), b["sixes"].integer(),   b["hs"].integer(),       b["hsNotOut"].truthy(),
           b["fifties"].integer(), b["hundreds"].integer(), b["ducks"].integer()};
  const Json& w = j["bowl"];
  s.bowl = {w["inns"].integer(), w["balls"].integer(), w["runs"].integer(), w["wickets"].integer(), w["maidens"].integer(),
            w["dots"].integer(), w["bestW"].integer(), w["bestR"].integer(), w["fourW"].integer()};
  const Json& f = j["field"];
  s.field = {f["catches"].integer(), f["stumpings"].integer(), f["runOuts"].integer(), f["drops"].integer()};
  for (const auto& e : j["recent"].elements())
    s.recent.push_back({e["vs"].str(), readOptStr(e["bat"]), readOptStr(e["bowl"]), e["result"].str()});
  return s;
}

Json toJson(const Overalls& o) {
  Json e = Json::object();
  e.add("bat", o.bat);
  e.add("bowl", o.bowl);
  e.add("field", o.field);
  return e;
}
Overalls overallsFrom(const Json& j) { return {j["bat"].num(), j["bowl"].num(), j["field"].num()}; }

Json toJson(const DevReport& r) {
  Json o = Json::object();
  o.add("id", r.id);
  auto perf = [](const std::optional<DevPerf>& p) {
    if (!p) return Json();
    Json e = Json::object();
    e.add("perf", p->perf);
    e.add("label", p->label);
    e.add("line", p->line);
    e.add("delta", p->delta);
    return e;
  };
  o.add("bat", perf(r.bat));
  o.add("bowl", perf(r.bowl));
  if (r.field) {
    Json e = Json::object();
    e.add("taken", r.field->taken);
    e.add("dropped", r.field->dropped);
    e.add("delta", r.field->delta);
    o.add("field", e);
  } else {
    o.add("field", Json());
  }
  o.add("captain", r.captain);
  o.add("before", toJson(r.before));
  o.add("after", toJson(r.after));
  return o;
}
DevReport devReportFrom(const Json& j) {
  DevReport r;
  r.id = j["id"].str();
  auto perf = [](const Json& e) -> std::optional<DevPerf> {
    if (!e.isObject()) return std::nullopt;
    return DevPerf{e["perf"].num(), e["label"].str(), e["line"].str(), e["delta"].num()};
  };
  r.bat = perf(j["bat"]);
  r.bowl = perf(j["bowl"]);
  if (j["field"].isObject())
    r.field = DevField{j["field"]["taken"].integer(), j["field"]["dropped"].integer(), j["field"]["delta"].num()};
  r.captain = j["captain"].truthy();
  r.before = overallsFrom(j["before"]);
  r.after = overallsFrom(j["after"]);
  return r;
}

void base(Json& o, const RecBase& b) {
  o.add("matchId", b.matchId);
  o.add("when", b.when);
}
void baseFrom(const Json& j, RecBase& b) {
  b.matchId = j["matchId"].str();
  b.when = j["when"].str();
}

Json toJson(const Records& r) {
  Json o = Json::object();
  auto list = [&](const char* key, const auto& v, auto fn) {
    Json a = Json::array();
    for (const auto& x : v) {
      Json e = Json::object();
      base(e, x);
      fn(e, x);
      a.push(e);
    }
    o.add(key, a);
  };
  list("highScores", r.highScores, [](Json& e, const ScoreRec& x) {
    e.add("id", x.id), e.add("team", x.team), e.add("vs", x.vs), e.add("runs", x.runs), e.add("balls", x.balls);
    e.add("notOut", x.notOut), e.add("fours", x.fours), e.add("sixes", x.sixes);
  });
  list("bestBowling", r.bestBowling, [](Json& e, const BowlRec& x) {
    e.add("id", x.id), e.add("team", x.team), e.add("vs", x.vs), e.add("wickets", x.wickets), e.add("runs", x.runs);
    e.add("balls", x.balls);
  });
  auto fast = [](Json& e, const FastRec& x) {
    e.add("id", x.id), e.add("team", x.team), e.add("vs", x.vs), e.add("balls", x.balls), e.add("runs", x.runs);
  };
  list("fastestFifties", r.fastestFifties, fast);
  list("fastestHundreds", r.fastestHundreds, fast);
  list("mostSixes", r.mostSixes, [](Json& e, const SixesRec& x) {
    e.add("id", x.id), e.add("team", x.team), e.add("vs", x.vs), e.add("sixes", x.sixes), e.add("runs", x.runs);
    e.add("balls", x.balls);
  });
  auto total = [](Json& e, const TotalRec& x) {
    e.add("team", x.team), e.add("vs", x.vs), e.add("runs", x.runs), e.add("wickets", x.wickets), e.add("balls", x.balls);
    e.add("overs", x.overs);
  };
  list("highTotals", r.highTotals, total);
  list("lowTotals", r.lowTotals, total);
  auto margin = [](Json& e, const MarginRec& x) {
    e.add("team", x.team), e.add("vs", x.vs), e.add("margin", x.margin);
    if (x.ballsLeft) e.add("ballsLeft", *x.ballsLeft);
  };
  list("bigWinsRuns", r.bigWinsRuns, margin);
  list("bigWinsWkts", r.bigWinsWkts, margin);
  return o;
}

Records recordsFrom(const Json& j) {
  Records r;
  for (const auto& e : j["highScores"].elements()) {
    ScoreRec x;
    baseFrom(e, x);
    x.id = e["id"].str(), x.team = e["team"].str(), x.vs = e["vs"].str(), x.runs = e["runs"].integer();
    x.balls = e["balls"].integer(), x.notOut = e["notOut"].truthy(), x.fours = e["fours"].integer(),
    x.sixes = e["sixes"].integer();
    r.highScores.push_back(x);
  }
  for (const auto& e : j["bestBowling"].elements()) {
    BowlRec x;
    baseFrom(e, x);
    x.id = e["id"].str(), x.team = e["team"].str(), x.vs = e["vs"].str(), x.wickets = e["wickets"].integer();
    x.runs = e["runs"].integer(), x.balls = e["balls"].integer();
    r.bestBowling.push_back(x);
  }
  auto fast = [](const Json& list, std::vector<FastRec>& out) {
    for (const auto& e : list.elements()) {
      FastRec x;
      baseFrom(e, x);
      x.id = e["id"].str(), x.team = e["team"].str(), x.vs = e["vs"].str(), x.balls = e["balls"].integer();
      x.runs = e["runs"].integer();
      out.push_back(x);
    }
  };
  fast(j["fastestFifties"], r.fastestFifties);
  fast(j["fastestHundreds"], r.fastestHundreds);
  for (const auto& e : j["mostSixes"].elements()) {
    SixesRec x;
    baseFrom(e, x);
    x.id = e["id"].str(), x.team = e["team"].str(), x.vs = e["vs"].str(), x.sixes = e["sixes"].integer();
    x.runs = e["runs"].integer(), x.balls = e["balls"].integer();
    r.mostSixes.push_back(x);
  }
  auto totals = [](const Json& list, std::vector<TotalRec>& out) {
    for (const auto& e : list.elements()) {
      TotalRec x;
      baseFrom(e, x);
      x.team = e["team"].str(), x.vs = e["vs"].str(), x.runs = e["runs"].integer(), x.wickets = e["wickets"].integer();
      x.balls = e["balls"].integer(), x.overs = e["overs"].num(20);
      out.push_back(x);
    }
  };
  totals(j["highTotals"], r.highTotals);
  totals(j["lowTotals"], r.lowTotals);
  auto margins = [](const Json& list, std::vector<MarginRec>& out) {
    for (const auto& e : list.elements()) {
      MarginRec x;
      baseFrom(e, x);
      x.team = e["team"].str(), x.vs = e["vs"].str(), x.margin = e["margin"].integer();
      x.ballsLeft = readOptInt(e["ballsLeft"]);
      out.push_back(x);
    }
  };
  margins(j["bigWinsRuns"], r.bigWinsRuns);
  margins(j["bigWinsWkts"], r.bigWinsWkts);
  return r;
}

Json toJson(const MilestoneHit& m) {
  Json o = Json::object();
  o.add("id", m.id);
  o.add("team", m.team);
  o.add("type", m.type);
  o.add("value", m.value);
  if (m.when) o.add("when", *m.when);
  return o;
}
MilestoneHit milestoneFrom(const Json& j) {
  return {j["id"].str(), j["team"].str(), j["type"].str(), j["value"].integer(), readOptStr(j["when"])};
}

}  // namespace

Json toJson(const Career& c) {
  Json o = Json::object();
  Json players = Json::object();
  for (const auto& [id, s] : c.players) players.add(id, toJson(s));
  o.add("players", players);
  o.add("recorded", strings(c.recorded));
  Json prog = Json::object();
  for (const auto& [id, p] : c.progress) {
    Json e = Json::object();
    e.add("bat", skillSet(p.bat, kBatKNames));
    e.add("bowl", skillSet(p.bowl, kBowlKNames));
    e.add("field", skillSet(p.field, kFieldKNames));
    prog.add(id, e);
  }
  o.add("progress", prog);
  if (c.lastReport) {
    Json e = Json::object();
    e.add("matchId", c.lastReport->matchId);
    Json rs = Json::array();
    for (const auto& r : c.lastReport->reports) rs.push(toJson(r));
    e.add("reports", rs);
    o.add("lastReport", e);
  } else {
    o.add("lastReport", Json());
  }
  o.add("records", toJson(c.records));
  Json pop = Json::object();
  for (const auto& [id, p] : c.popularity) {
    Json e = Json::object();
    e.add("v", p.v);
    e.add("d", p.d);
    pop.add(id, e);
  }
  o.add("popularity", pop);
  Json mu = Json::object();
  for (const auto& [k, row] : c.matchups) {
    Json a = Json::array();
    for (int v : row) a.push(v);
    mu.add(k, a);
  }
  o.add("matchups", mu);
  Json ms = Json::array();
  for (const auto& m : c.milestones) ms.push(toJson(m));
  o.add("milestones", ms);
  if (c.lastMilestones) {
    Json e = Json::object();
    e.add("matchId", c.lastMilestones->matchId);
    Json l = Json::array();
    for (const auto& m : c.lastMilestones->list) l.push(toJson(m));
    e.add("list", l);
    o.add("lastMilestones", e);
  } else {
    o.add("lastMilestones", Json());
  }
  {
    Json e = Json::object();
    e.add("played", c.manager.played);
    e.add("wins", c.manager.wins);
    e.add("streak", c.manager.streak);
    e.add("bestStreak", c.manager.bestStreak);
    Json u = Json::object();
    for (const auto& [k, x] : c.manager.unlocked) {
      Json v = Json::object();
      v.add("when", x.when);
      v.add("vs", x.vs);
      u.add(k, v);
    }
    e.add("unlocked", u);
    o.add("manager", e);
  }
  if (c.lastUnlocked) {
    Json e = Json::object();
    e.add("matchId", c.lastUnlocked->matchId);
    e.add("keys", strings(c.lastUnlocked->keys));
    o.add("lastUnlocked", e);
  } else {
    o.add("lastUnlocked", Json());
  }
  return o;
}

Career careerFromJson(const Json& j) {
  Career c;
  c.players.reserve(j["players"].size());
  for (const auto& [id, s] : j["players"].items()) c.players.set(id, statsFrom(s));
  c.recorded = readStrings(j["recorded"]);
  for (const auto& [id, p] : j["progress"].items()) {
    Progress pr;
    pr.bat = readSkillSet<BatK>(p["bat"], kBatKNames);
    pr.bowl = readSkillSet<BowlK>(p["bowl"], kBowlKNames);
    pr.field = readSkillSet<FieldK>(p["field"], kFieldKNames);
    c.progress.set(id, pr);
  }
  if (j["lastReport"].isObject()) {
    LastReport lr;
    lr.matchId = j["lastReport"]["matchId"].str();
    for (const auto& r : j["lastReport"]["reports"].elements()) lr.reports.push_back(devReportFrom(r));
    c.lastReport = lr;
  }
  c.records = recordsFrom(j["records"]);
  for (const auto& [id, p] : j["popularity"].items()) c.popularity.set(id, Popularity{p["v"].num(), p["d"].num()});
  c.matchups.reserve(j["matchups"].size());
  for (const auto& [k, a] : j["matchups"].items()) {
    MatchupRow row{};
    for (size_t i = 0; i < row.size() && i < a.size(); ++i) row[i] = a.at(i).integer();
    c.matchups.set(k, row);
  }
  for (const auto& m : j["milestones"].elements()) c.milestones.push_back(milestoneFrom(m));
  if (j["lastMilestones"].isObject()) {
    LastMilestones lm;
    lm.matchId = j["lastMilestones"]["matchId"].str();
    for (const auto& m : j["lastMilestones"]["list"].elements()) lm.list.push_back(milestoneFrom(m));
    c.lastMilestones = lm;
  }
  const Json& m = j["manager"];
  c.manager.played = m["played"].integer();
  c.manager.wins = m["wins"].integer();
  c.manager.streak = m["streak"].integer();
  c.manager.bestStreak = m["bestStreak"].integer();
  for (const auto& [k, u] : m["unlocked"].items()) c.manager.unlocked.set(k, Unlock{u["when"].str(), u["vs"].str()});
  if (j["lastUnlocked"].isObject())
    c.lastUnlocked = LastUnlocked{j["lastUnlocked"]["matchId"].str(), readStrings(j["lastUnlocked"]["keys"])};
  return c;
}

// ---------------------------------------------------------------------------
// Season
// ---------------------------------------------------------------------------

Json toJson(const MatchSummary& s) {
  Json o = Json::object();
  o.add("matchId", s.matchId);
  o.add("winner", optStr(s.winner));
  o.add("text", s.text);
  o.add("noResult", s.noResult);
  o.add("dls", s.dls);
  if (s.superOver) {
    Json a = Json::array();
    for (const auto& x : *s.superOver) {
      Json e = Json::object();
      e.add("team", x.team);
      e.add("runs", x.runs);
      e.add("wickets", x.wickets);
      a.push(e);
    }
    o.add("superOver", a);
  } else {
    o.add("superOver", Json());
  }
  o.add("potm", optStr(s.potm));
  if (s.toss) {
    Json e = Json::object();
    e.add("winner", s.toss->winner);
    e.add("decision", s.toss->decision ? Json(name(*s.toss->decision)) : Json());
    o.add("toss", e);
  } else {
    o.add("toss", Json());
  }
  o.add("dew", s.dew);
  Json inns = Json::array();
  for (const auto& i : s.innings) {
    Json e = Json::object();
    e.add("team", i.team);
    e.add("runs", i.runs);
    e.add("wickets", i.wickets);
    e.add("balls", i.balls);
    e.add("allOut", i.allOut);
    e.add("maxBalls", i.maxBalls);
    e.add("extras", i.extras);
    Json ov = Json::array();
    for (const auto& x : i.overs) {
      Json pr = Json::array();
      pr.push(x[0]);
      pr.push(x[1]);
      ov.push(pr);
    }
    e.add("overs", ov);
    Json bat = Json::array();
    for (const auto& r : i.batting) {
      Json row = Json::array();
      row.push(r.id), row.push(r.runs), row.push(r.balls), row.push(r.fours), row.push(r.sixes), row.push(r.out);
      row.push(r.dots);
      bat.push(row);
    }
    e.add("batting", bat);
    Json bowl = Json::array();
    for (const auto& r : i.bowling) {
      Json row = Json::array();
      row.push(r.id), row.push(r.balls), row.push(r.runs), row.push(r.wkts), row.push(r.maidens), row.push(r.dots);
      row.push(r.fours), row.push(r.sixes), row.push(r.wides), row.push(r.noballs);
      bowl.push(row);
    }
    e.add("bowling", bowl);
    inns.push(e);
  }
  o.add("innings", inns);
  Json f = Json::object();
  for (const auto& [id, x] : s.fielding) {
    Json e = Json::object();
    e.add("ct", x.ct);
    e.add("st", x.st);
    e.add("ro", x.ro);
    e.add("drops", x.drops);
    f.add(id, e);
  }
  o.add("fielding", f);
  Json im = Json::array();
  for (const auto& x : s.impact) {
    Json e = Json::object();
    e.add("team", x.team);
    e.add("out", x.out);
    e.add("in", x.in);
    im.push(e);
  }
  o.add("impact", im);
  return o;
}

MatchSummary summaryFromJson(const Json& j) {
  MatchSummary s;
  s.matchId = j["matchId"].str();
  s.winner = readOptStr(j["winner"]);
  s.text = j["text"].str();
  s.noResult = j["noResult"].truthy();
  s.dls = j["dls"].truthy();
  if (j["superOver"].isArray()) {
    s.superOver = std::vector<SuperOverLine>{};
    for (const auto& e : j["superOver"].elements())
      s.superOver->push_back({e["team"].str(), e["runs"].integer(), e["wickets"].integer()});
  }
  s.potm = readOptStr(j["potm"]);
  if (j["toss"].isObject())
    s.toss = TossLine{j["toss"]["winner"].str(), parseName<TossDecision>(kTossDecisionNames, j["toss"]["decision"])};
  s.dew = j["dew"].truthy();
  for (const auto& e : j["innings"].elements()) {
    InningsSummary i;
    i.team = e["team"].str();
    i.runs = e["runs"].integer();
    i.wickets = e["wickets"].integer();
    i.balls = e["balls"].integer();
    i.allOut = e["allOut"].truthy();
    i.maxBalls = e["maxBalls"].integer(120);
    i.extras = e["extras"].integer();
    for (const auto& ov : e["overs"].elements()) i.overs.push_back({ov.at(0).integer(), ov.size() > 1 ? ov.at(1).integer() : 0});
    for (const auto& r : e["batting"].elements()) {
      if (r.size() < 7) continue;
      i.batting.push_back({r.at(0).str(), r.at(1).integer(), r.at(2).integer(), r.at(3).integer(), r.at(4).integer(),
                           r.at(5).integer(), r.at(6).integer()});
    }
    for (const auto& r : e["bowling"].elements()) {
      if (r.size() < 10) continue;
      i.bowling.push_back({r.at(0).str(), r.at(1).integer(), r.at(2).integer(), r.at(3).integer(), r.at(4).integer(),
                           r.at(5).integer(), r.at(6).integer(), r.at(7).integer(), r.at(8).integer(), r.at(9).integer()});
    }
    s.innings.push_back(std::move(i));
  }
  for (const auto& [id, x] : j["fielding"].items())
    s.fielding.set(id, FieldingLine{x["ct"].integer(), x["st"].integer(), x["ro"].integer(), x["drops"].integer()});
  for (const auto& e : j["impact"].elements()) s.impact.push_back({e["team"].str(), e["out"].str(), e["in"].str()});
  return s;
}

Json toJson(const Season& s) {
  Json o = Json::object();
  o.add("id", s.id);
  o.add("number", s.number);
  o.add("seed", s.seed);
  o.add("userTeam", s.userTeam);
  Json g = Json::object();
  g.add("A", teamIds(s.groupA));
  g.add("B", teamIds(s.groupB));
  o.add("groups", g);
  Json v = Json::object();
  for (const auto& [id, x] : s.venues) {
    Json e = Json::object();
    e.add("ground", x.ground);
    e.add("pitch", name(x.pitch));
    v.add(id, e);
  }
  o.add("venues", v);
  Json fx = Json::array();
  for (const auto& f : s.fixtures) {
    Json e = Json::object();
    e.add("no", f.no);
    e.add("stage", name(f.stage));
    e.add("home", f.home);
    e.add("away", f.away);
    e.add("venue", f.venue);
    e.add("pitch", name(f.pitch));
    e.add("seed", f.seed);
    e.add("result", f.result ? toJson(*f.result) : Json());
    fx.push(e);
  }
  o.add("fixtures", fx);
  o.add("champion", optStr(s.champion));
  o.add("runnerUp", optStr(s.runnerUp));
  o.add("top4", s.top4 ? teamIds(*s.top4) : Json());
  return o;
}

Season seasonFromJson(const Json& j) {
  Season s;
  s.id = j["id"].str();
  s.number = j["number"].integer(1);
  s.seed = static_cast<int32_t>(j["seed"].num());
  s.userTeam = j["userTeam"].str();
  s.groupA = readStrings(j["groups"]["A"]);
  s.groupB = readStrings(j["groups"]["B"]);
  for (const auto& [id, x] : j["venues"].items())
    s.venues.set(id, Venue{x["ground"].str(), parseName<Pitch>(kPitchNames, x["pitch"]).value_or(Pitch::balanced)});
  for (const auto& e : j["fixtures"].elements()) {
    Fixture f;
    f.no = e["no"].integer();
    f.stage = parseName<Stage>(kStageNames, e["stage"]).value_or(Stage::league);
    f.home = e["home"].str();
    f.away = e["away"].str();
    f.venue = e["venue"].str();
    f.pitch = parseName<Pitch>(kPitchNames, e["pitch"]).value_or(Pitch::balanced);
    f.seed = static_cast<int32_t>(e["seed"].num());
    if (e["result"].isObject()) f.result = summaryFromJson(e["result"]);
    s.fixtures.push_back(std::move(f));
  }
  s.champion = readOptStr(j["champion"]);
  s.runnerUp = readOptStr(j["runnerUp"]);
  if (j["top4"].isArray()) s.top4 = readStrings(j["top4"]);
  return s;
}

Json toJson(const SeasonState& s) {
  Json o = Json::object();
  o.add("season", s.season ? toJson(*s.season) : Json());
  Json h = Json::array();
  for (const auto& a : s.history) {
    Json e = Json::object();
    e.add("number", a.number);
    e.add("champion", a.champion);
    e.add("runnerUp", a.runnerUp);
    e.add("userTeam", a.userTeam);
    e.add("userPos", optInt(a.userPos));
    auto cap = [](const std::optional<CapLine>& c, const char* key) {
      if (!c) return Json();
      Json x = Json::object();
      x.add("id", c->id);
      x.add(key, c->value);
      return x;
    };
    e.add("orangeCap", cap(a.orangeCap, "runs"));
    e.add("purpleCap", cap(a.purpleCap, "wkts"));
    e.add("mvp", cap(a.mvp, "pts"));
    h.push(e);
  }
  o.add("history", h);
  return o;
}

SeasonState seasonStateFromJson(const Json& j) {
  SeasonState s;
  if (j["season"].isObject()) s.season = seasonFromJson(j["season"]);
  for (const auto& e : j["history"].elements()) {
    SeasonArchive a;
    a.number = e["number"].integer();
    a.champion = e["champion"].str();
    a.runnerUp = e["runnerUp"].str();
    a.userTeam = e["userTeam"].str();
    a.userPos = readOptInt(e["userPos"]);
    auto cap = [](const Json& c, const char* key) -> std::optional<CapLine> {
      if (!c.isObject()) return std::nullopt;
      return CapLine{c["id"].str(), c[key].num()};
    };
    a.orangeCap = cap(e["orangeCap"], "runs");
    a.purpleCap = cap(e["purpleCap"], "wkts");
    a.mvp = cap(e["mvp"], "pts");
    s.history.push_back(std::move(a));
  }
  return s;
}

}  // namespace t20
