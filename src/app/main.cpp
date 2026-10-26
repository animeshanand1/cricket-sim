// T20 Coach desktop entry point (step A1): an SDL3 window with Dear ImGui on SDL_Renderer,
// the app shell, and the Match screen driving the C++ engine.
//
//   t20coach.exe                       play
//   t20coach.exe --screenshot out.bmp  render, save one frame, exit (docs and visual checks)
//       [--balls N]    play N balls first        [--moment]  then play on until a 4, 6 or wicket
//       [--size WxH]   window size               [--seed S]  match seed
//       [--frames F]   frames to render before the capture (default 30)
//       [--until X]    then play on until the captain is asked for X: bowler, batter, review,
//                      batting or bowling (the controls with nothing pending)
//   Pending calls along the way are answered by the AI.
//       [--click X,Y]  left-click at (X, Y) before the capture; repeat for several clicks
//                      (one every 8 frames from frame 20, so each click sees the last one's result)
//   Screenshot runs only read and write saves when given --save-dir.
//
// Saves (step A5) live in %APPDATA%\T20Coach\T20Coach\saves\slot1 (SDL_GetPrefPath), with a log in
// ...\logs	20coach.log. Every finished match goes into the career and is saved straight away.
//       [--save-dir DIR]  use DIR instead of the usual save folder
//       [--no-save]       play without reading or writing saves

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/game.hpp"
#include "app/match_session.hpp"
#include "app/save_manager.hpp"
#include "core/str.hpp"
#include "ui/app_shell.hpp"
#include "ui/screens/match_screen.hpp"
#include "ui/gfx.hpp"
#include "ui/theme.hpp"

namespace {

struct Options {
  std::string screenshot;
  int balls = 0;
  bool moment = false;
  int width = 1440, height = 900;
  int32_t seed = 2026;
  int frames = 30;
  std::string until;
  std::vector<std::pair<float, float>> clicks;
  std::string saveDir;
  bool noSave = false;
};

Options parseArgs(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--screenshot") o.screenshot = next();
    else if (a == "--balls") o.balls = std::atoi(next().c_str());
    else if (a == "--moment") o.moment = true;
    else if (a == "--seed") o.seed = std::atoi(next().c_str());
    else if (a == "--frames") o.frames = std::atoi(next().c_str());
    else if (a == "--until") o.until = next();
    else if (a == "--click") {
      float x = 0, y = 0;
      if (std::sscanf(next().c_str(), "%f,%f", &x, &y) == 2) o.clicks.emplace_back(x, y);
    }
    else if (a == "--size") std::sscanf(next().c_str(), "%dx%d", &o.width, &o.height);
    else if (a == "--save-dir") o.saveDir = next();
    else if (a == "--no-save") o.noSave = true;
  }
  return o;
}

bool saveFrame(SDL_Renderer* renderer, const std::string& path) {
  SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
  if (!shot) return false;
  const bool ok = SDL_SaveBMP(shot, path.c_str());
  SDL_DestroySurface(shot);
  return ok;
}

// One line in logs/t20coach.log, with the time
void logLine(const std::filesystem::path& root, const std::string& text) {
  if (root.empty()) return;
  std::error_code ec;
  std::filesystem::create_directories(root / "logs", ec);
  std::ofstream out(root / "logs" / "t20coach.log", std::ios::app);
  const std::time_t t = std::time(nullptr);
  char stamp[32];
  std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
  out << stamp << "  " << text << "\n";
}

// A path from UTF-8 text (SDL hands paths over as UTF-8)
std::filesystem::path pathFromUtf8(const std::string& s) {
  return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

std::string nowMs() {
  return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count());
}

}  // namespace

int main(int argc, char** argv) {
  const Options opt = parseArgs(argc, argv);
  const bool capture = !opt.screenshot.empty();

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (capture) flags |= SDL_WINDOW_HIDDEN;
  SDL_Window* window = SDL_CreateWindow("T20 Coach", opt.width, opt.height, flags);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetWindowMinimumSize(window, 1280, 720);
  SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;  // no imgui.ini next to the exe
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Fonts are rasterised at the real pixel density and drawn at 1/density, so text stays sharp
  const float density = SDL_GetWindowPixelDensity(window) > 0 ? SDL_GetWindowPixelDensity(window) : 1.0f;
  t20::ui::setTheme(true);
  t20::ui::loadFonts(density);
  io.FontGlobalScale = 1.0f / density;
  t20::ui::applyImGuiStyle();

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  // --- game state and saves --------------------------------------------------------------
  std::filesystem::path saveRoot;
  if ((!capture || !opt.saveDir.empty()) && !opt.noSave) {
    if (!opt.saveDir.empty()) {
      saveRoot = pathFromUtf8(opt.saveDir);
    } else if (char* pref = SDL_GetPrefPath("T20Coach", "T20Coach")) {
      saveRoot = pathFromUtf8(pref);
      SDL_free(pref);
    }
  }
  std::unique_ptr<t20::app::SaveManager> saves;
  std::vector<std::string> loadNotes;
  if (!saveRoot.empty()) saves = std::make_unique<t20::app::SaveManager>(saveRoot);
  t20::app::Game game(saves ? saves->load(&loadNotes) : t20::app::Game::freshState());
  game.boot();
  for (const auto& n : loadNotes) logLine(saveRoot, n);

  t20::app::MatchSession session;
  bool leagueStale = false;
  if (saves) {
    // The squads as the saved career has left them; each finished match goes into the career
    session.setLeague(game.league());
    session.setSavedMatchups(&game.state().career.matchups);
    session.makeId = [](int32_t seed) { return t20::cat(seed, '-', nowMs()); };
    session.onComplete = [&](const t20::Match& m) {
      game.recordFinishedMatch(m);
      const bool ok = saves->flush(game);
      logLine(saveRoot, t20::cat("match ", m.id, " recorded: ", m.result ? m.result->text : std::string(), ok ? "; saved" : "; save failed: " + saves->lastError()));
      leagueStale = true;
    };
  }
  session.newMatch(5, 7, opt.seed, saves ? std::optional<std::string>(t20::cat(opt.seed, '-', nowMs())) : std::nullopt);
  // One scripted step: the AI answers any pending call, then a ball is bowled
  auto step = [&session] {
    if (session.match().awaiting != t20::Awaiting::none) session.letAiDecide();
    session.nextBall();
  };
  for (int i = 0; i < opt.balls; ++i) step();
  if (opt.moment) {
    for (int guard = 0; guard < 240 && session.match().status != t20::MatchStatus::complete; ++guard) {
      step();
      const auto& inn = session.match().innings[static_cast<size_t>(session.match().current)];
      const int oi = inn.legalBalls / 6;
      const t20::Over* ov = oi < static_cast<int>(inn.overs.size()) && !inn.overs[static_cast<size_t>(oi)].balls.empty()
                                ? &inn.overs[static_cast<size_t>(oi)]
                                : nullptr;
      const auto k = ov ? ov->balls.back().kind : t20::BallKind::run;
      if (k == t20::BallKind::b6 || k == t20::BallKind::wicket) break;
    }
  }
  if (!opt.until.empty()) {
    auto reached = [&] {
      const t20::Match& m = session.match();
      const auto aw = m.awaiting;
      if (opt.until == "bowler") return aw == t20::Awaiting::bowler;
      if (opt.until == "batter") return aw == t20::Awaiting::batter;
      if (opt.until == "review") return aw == t20::Awaiting::review;
      if (opt.until == "batting") return aw == t20::Awaiting::none && t20::userIsBatting(m);
      if (opt.until == "bowling") return aw == t20::Awaiting::none && t20::userIsBowling(m);
      return true;
    };
    for (int guard = 0; guard < 600 && !reached() && session.match().status != t20::MatchStatus::complete; ++guard) {
      if (session.match().awaiting != t20::Awaiting::none) session.letAiDecide();
      else session.nextBall();
    }
  }

  t20::ui::AppShell shell;
  shell.registerView(t20::View::match, std::make_unique<t20::ui::MatchScreen>(session));
  shell.setMatchState(session.hasMatch(), session.isLive());
  shell.goTo(t20::Screen::match);

  // --- main loop ----------------------------------------------------------------------------
  bool running = true;
  int frame = 0;
  Uint64 last = SDL_GetPerformanceCounter();
  const ImVec4 bg = t20::ui::vec(t20::ui::currentTheme().bg);
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL3_ProcessEvent(&e);
      if (e.type == SDL_EVENT_QUIT) running = false;
      if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(window)) running = false;
    }
    const Uint64 now = SDL_GetPerformanceCounter();
    // a fixed 60 fps clock when capturing, so screenshots are reproducible
    const double dt = capture ? 1.0 / 60.0 : static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency());
    last = now;

    // Scripted clicks (capture mode): move, press, release on consecutive frames
    if (capture && !opt.clicks.empty()) {
      // from frame 20, once the first win-chance estimate has settled the layout
      const int rel = frame - 20;
      const size_t k = rel >= 0 ? static_cast<size_t>(rel / 8) : opt.clicks.size();
      if (k < opt.clicks.size()) {
        const auto [cx, cy] = opt.clicks[k];
        const int phase = rel % 8;
        if (phase == 0) io.AddMousePosEvent(cx, cy);
        if (phase == 1) io.AddMouseButtonEvent(0, true);
        if (phase == 2) io.AddMouseButtonEvent(0, false);
      }
    }

    session.update(dt);
    shell.setMatchState(session.hasMatch(), session.isLive());
    if (saves) {
      // Development and fitness from the last match show in the next one
      if (leagueStale && session.match().status == t20::MatchStatus::complete) {
        session.setLeague(game.league());
        leagueStale = false;
      }
      saves->tick(game, dt);
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    shell.renderFrame(io.DisplaySize.x, io.DisplaySize.y, dt);
    ImGui::Render();

    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColorFloat(renderer, bg.x, bg.y, bg.z, 1.0f);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

    ++frame;
    if (capture && frame >= std::max(opt.frames, 20 + static_cast<int>(opt.clicks.size()) * 8 + 10)) {
      const bool ok = saveFrame(renderer, opt.screenshot);
      std::printf("%s %s\n", ok ? "saved" : "failed to save", opt.screenshot.c_str());
      running = false;
    }
    SDL_RenderPresent(renderer);
  }

  if (saves) logLine(saveRoot, saves->flush(game) ? "quit: saved" : "quit: save failed: " + saves->lastError());

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
