/*
  StreamIMDb TV App — SDL2 + libVLC Desktop Client
  =================================================
  Build (Linux):
    g++ main.cpp -o streamimdb \
        $(sdl2-config --cflags --libs) \
        -lSDL2_ttf \
        $(pkg-config --cflags --libs libvlc) \
        -lSDL2 -lSDL2_ttf

  Build (macOS, Homebrew):
    g++ main.cpp -o streamimdb \
        $(sdl2-config --cflags --libs) \
        -lSDL2_ttf \
        $(pkg-config --cflags --libs libvlc) \
        -lSDL2 -lSDL2_ttf

  Keys:
    ← →     Browse movies
    Enter   Select / play
    Esc     Back to menu / quit
    Space   Pause / resume (while playing)

  Notes:
  ─ VLC is embedded into the SDL window via libvlc_media_player_set_xwindow
    (Linux X11) / set_hwnd (Windows) / set_nsobject (macOS).
  ─ Loading delay uses SDL_GetTicks() so it is real-clock accurate.
  ─ Two fonts are opened: large (48px) for headers, small (28px) for labels.
  ─ VLC end-of-media event fires goBackToMenu automatically.
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>   // Needed for SDL_GetWindowWMInfo
#include <vlc/vlc.h>

#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <algorithm>

/* ─── Display ────────────────────────────────────────────────── */
static const int SCREEN_W = 1920;
static const int SCREEN_H = 1080;

/* ─── App State ──────────────────────────────────────────────── */
enum class AppState { MENU, LOADING, PLAYING };
static AppState gState = AppState::MENU;
static std::atomic<bool> gEndOfMedia{false};   // Set from VLC callback thread

/* ─── Data ───────────────────────────────────────────────────── */
struct Movie {
    std::string title;
    std::string imdbId;
    SDL_Color   posterColor;
    SDL_Color   accentColor;
    int         year;
    float       rating;
};

static const std::vector<Movie> MOVIES = {
    { "The Dark Knight", "tt0468569", {30,40,96,255},   {229,147,32,255},  2008, 9.0f },
    { "Inception",       "tt1375666", {26,48,26,255},   {76,175,80,255},   2010, 8.8f },
    { "Interstellar",    "tt0816692", {42,16,16,255},   {255,107,53,255},  2014, 8.7f },
    { "Pulp Fiction",    "tt0110912", {42,32,16,255},   {255,215,0,255},   1994, 8.9f },
    { "Fight Club",      "tt0137523", {28,16,16,255},   {204,51,0,255},    1999, 8.8f },
    { "The Matrix",      "tt0133093", {10,26,10,255},   {0,255,65,255},    1999, 8.7f },
};

/* ─── VLC ────────────────────────────────────────────────────── */
static libvlc_instance_t*     gVLC    = nullptr;
static libvlc_media_player_t* gPlayer = nullptr;

static const char* DEMO_STREAM =
    "https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.m3u8";

/* ─── VLC event callback (runs on VLC thread) ─────────────────── */
static void onVlcEvent(const libvlc_event_t* event, void* /*userdata*/) {
    if (event->type == libvlc_MediaPlayerEndReached ||
        event->type == libvlc_MediaPlayerEncounteredError) {
        gEndOfMedia.store(true);
    }
}

/* ─── Fonts ──────────────────────────────────────────────────── */
static TTF_Font* gFontLarge  = nullptr;   // 48px — headers
static TTF_Font* gFontMedium = nullptr;   // 28px — labels / meta
static TTF_Font* gFontSmall  = nullptr;   // 20px — hint text

static TTF_Font* openFont(const char* path, int pt) {
    TTF_Font* f = TTF_OpenFont(path, pt);
    if (!f) {
        // Fallback paths
        const char* alts[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            nullptr
        };
        for (int i = 0; alts[i]; ++i) {
            f = TTF_OpenFont(alts[i], pt);
            if (f) break;
        }
    }
    return f;
}

/* ─── Drawing helpers ────────────────────────────────────────── */
static void drawFilledRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void drawOutlineRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c, int thickness = 3) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect rect{x - i, y - i, w + i*2, h + i*2};
        SDL_RenderDrawRect(r, &rect);
    }
}

// Returns rendered text width; renders at (x,y) using given font.
static int drawText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                    int x, int y, SDL_Color color, float scale = 1.0f) {
    if (!font || text.empty()) return 0;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surf) return 0;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    int tw = static_cast<int>(surf->w * scale);
    int th = static_cast<int>(surf->h * scale);
    SDL_Rect dst{x, y, tw, th};
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
    return tw;
}

/* ─── State vars ─────────────────────────────────────────────── */
static int   gFocusIndex    = 0;
static Uint32 gLoadStart    = 0;   // Real timestamp for loading delay
static bool  gPaused        = false;

static const int CARD_W     = 260;
static const int CARD_H     = 390;
static const int CARD_GAP   = 28;
static const int CARD_TOP   = 340;
static const int CARDS_LEFT = 80;

/* ─── Start / stop ───────────────────────────────────────────── */
static void startPlayback(SDL_Window* window, const std::string& /*imdbId*/) {
    if (!gVLC) return;

    libvlc_media_t* media = libvlc_media_new_location(gVLC, DEMO_STREAM);
    if (!media) { std::cerr << "[VLC] Failed to create media\n"; return; }

    gPlayer = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);
    if (!gPlayer) { std::cerr << "[VLC] Failed to create media player\n"; return; }

    /* ── Embed VLC into the SDL window ── */
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (SDL_GetWindowWMInfo(window, &wm)) {
#if defined(_WIN32)
        libvlc_media_player_set_hwnd(gPlayer, wm.info.win.window);
#elif defined(__APPLE__)
        libvlc_media_player_set_nsobject(gPlayer, (void*)wm.info.cocoa.window);
#else  // Linux / X11
        libvlc_media_player_set_xwindow(gPlayer, (uint32_t)wm.info.x11.window);
#endif
    } else {
        std::cerr << "[SDL] SDL_GetWindowWMInfo failed: " << SDL_GetError() << "\n";
    }

    /* ── Subscribe to end-of-media and error events ── */
    libvlc_event_manager_t* em = libvlc_media_player_event_manager(gPlayer);
    libvlc_event_attach(em, libvlc_MediaPlayerEndReached,       onVlcEvent, nullptr);
    libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, onVlcEvent, nullptr);

    libvlc_media_player_play(gPlayer);
    gPaused = false;

    std::cout << "[PLAYBACK] Stream started\n";
}

static void stopPlayback() {
    if (!gPlayer) return;
    libvlc_media_player_stop(gPlayer);
    libvlc_media_player_release(gPlayer);
    gPlayer = nullptr;
    gEndOfMedia.store(false);
    gPaused = false;
    std::cout << "[PLAYBACK] Stopped\n";
}

/* ─── Draw: Menu ─────────────────────────────────────────────── */
static void drawMenu(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 8, 8, 8, 255);
    SDL_RenderClear(r);

    // Header bar
    drawFilledRect(r, 0, 0, SCREEN_W, 100, {18, 18, 18, 255});

    SDL_Color white  = {240, 236, 228, 255};
    SDL_Color muted  = {160, 155, 145, 255};
    SDL_Color red    = {229,   9,  26, 255};

    // Logo
    drawText(r, gFontLarge, "Stream", 60, 26, muted);
    drawText(r, gFontLarge, "IMDb",   60 + drawText(r, gFontLarge, "Stream", 60, 26, muted), 26, red);
    // Hint
    drawText(r, gFontSmall, "ARROW KEYS to browse   ENTER to play   ESC to quit",
             SCREEN_W - 740, 40, muted);

    // Selected movie title (large, above cards)
    const Movie& sel = MOVIES[gFocusIndex];
    SDL_Color accent = sel.accentColor;
    drawText(r, gFontLarge, sel.title, CARDS_LEFT, 140, white);
    // Year + rating
    char meta[64];
    snprintf(meta, sizeof(meta), "%d  ★ %.1f", sel.year, sel.rating);
    drawText(r, gFontMedium, meta, CARDS_LEFT, 210, accent);

    // Cards
    int totalW = (int)MOVIES.size() * (CARD_W + CARD_GAP) - CARD_GAP;
    int startX = std::max(CARDS_LEFT, (SCREEN_W - totalW) / 2);

    for (int i = 0; i < (int)MOVIES.size(); ++i) {
        int cx = startX + i * (CARD_W + CARD_GAP);
        bool focused = (i == gFocusIndex);

        // Focus ring (red halo behind card)
        if (focused) {
            drawFilledRect(r, cx - 10, CARD_TOP - 10, CARD_W + 20, CARD_H + 20, {229, 9, 26, 180});
        }

        // Poster block
        drawFilledRect(r, cx, CARD_TOP, CARD_W, CARD_H, MOVIES[i].posterColor);

        // Accent stripe at top of card
        drawFilledRect(r, cx, CARD_TOP, CARD_W, 6, MOVIES[i].accentColor);

        // Initials overlay
        std::string initials;
        for (const char ch : MOVIES[i].title) {
            if (ch == ' ' && !initials.empty()) break;
            if (initials.empty() || MOVIES[i].title[&ch - MOVIES[i].title.c_str() - 1] == ' ')
                initials += ch;
        }
        // Just take first 2 chars as initial placeholder
        std::string big = MOVIES[i].title.substr(0, 1);
        SDL_Color dim = {255, 255, 255, 40};
        drawText(r, gFontLarge, big, cx + CARD_W/2 - 24, CARD_TOP + CARD_H/2 - 40, dim, 2.0f);

        // Bottom gradient-style dark band
        for (int row = 0; row < 100; ++row) {
            Uint8 alpha = (Uint8)((row / 100.0f) * 210);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);
            SDL_RenderDrawLine(r, cx, CARD_TOP + CARD_H - 100 + row, cx + CARD_W - 1, CARD_TOP + CARD_H - 100 + row);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

        // Card title
        drawText(r, gFontSmall, MOVIES[i].title, cx + 12, CARD_TOP + CARD_H - 60, white);

        // Year
        std::string yr = std::to_string(MOVIES[i].year);
        drawText(r, gFontSmall, yr, cx + 12, CARD_TOP + CARD_H - 30, muted);

        // Rating dot (accent colour dot) for focused card
        if (focused) {
            char rating[16];
            snprintf(rating, sizeof(rating), "★ %.1f", MOVIES[i].rating);
            drawText(r, gFontSmall, rating, cx + CARD_W - 80, CARD_TOP + CARD_H - 30, MOVIES[i].accentColor);
        }
    }

    // Key hints at bottom
    drawText(r, gFontSmall, "← →  Browse        Enter  Play        Esc  Quit",
             CARDS_LEFT, SCREEN_H - 48, muted);

    SDL_RenderPresent(r);
}

/* ─── Draw: Loading ──────────────────────────────────────────── */
static void drawLoading(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 5, 5, 5, 255);
    SDL_RenderClear(r);

    SDL_Color red  = {229,  9, 26, 255};
    SDL_Color gray = {100, 95, 88, 255};

    const Movie& m = MOVIES[gFocusIndex];
    drawText(r, gFontLarge, "Loading Stream", 100, SCREEN_H/2 - 100, red);
    drawText(r, gFontMedium, m.title, 100, SCREEN_H/2, {240, 236, 228, 255});
    drawText(r, gFontSmall,  "StreamIMDb · Buffering HLS stream...", 100, SCREEN_H/2 + 60, gray);

    // Simple progress bar (animates over 1.5 s)
    Uint32 elapsed = SDL_GetTicks() - gLoadStart;
    float  progress = std::min(1.0f, elapsed / 1500.0f);
    int barW = 600;
    int barX = 100, barY = SCREEN_H/2 + 120;
    drawFilledRect(r, barX, barY, barW, 4, {30, 30, 30, 255});
    drawFilledRect(r, barX, barY, (int)(barW * progress), 4, red);

    SDL_RenderPresent(r);
}

/* ─── Draw: Playing ──────────────────────────────────────────── */
static void drawPlaying(SDL_Renderer* r) {
    // Black canvas — VLC renders video directly onto the window surface.
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // Minimal HUD: movie name + pause indicator
    SDL_Color white = {240, 236, 228, 255};
    SDL_Color red   = {229,   9,  26, 255};
    const Movie& m  = MOVIES[gFocusIndex];

    drawText(r, gFontSmall, m.title, 40, 40, white);
    if (gPaused) {
        drawText(r, gFontMedium, "II  PAUSED", SCREEN_W/2 - 100, SCREEN_H/2 - 30, red);
    }
    drawText(r, gFontSmall, "Space = Pause/Resume   Esc/Backspace = Back to Menu",
             40, SCREEN_H - 44, {100, 95, 88, 255});

    SDL_RenderPresent(r);
}

/* ═══════════════════════════════════════════════════════════════
   MAIN
═══════════════════════════════════════════════════════════════ */
int main(int /*argc*/, char* /*argv*/[]) {
    /* ── SDL init ── */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    /* ── VLC init ── */
    const char* vlcArgs[] = { "--no-osd", "--no-video-title-show" };
    gVLC = libvlc_new(2, vlcArgs);
    if (!gVLC) {
        std::cerr << "libvlc_new failed\n";
        TTF_Quit(); SDL_Quit();
        return 1;
    }

    /* ── Window ── */
    SDL_Window* window = SDL_CreateWindow(
        "StreamIMDb",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        libvlc_release(gVLC); TTF_Quit(); SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        libvlc_release(gVLC); TTF_Quit(); SDL_Quit();
        return 1;
    }

    /* ── Fonts ── */
    const char* FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    gFontLarge  = openFont(FONT_PATH, 48);
    gFontMedium = openFont(FONT_PATH, 28);
    gFontSmall  = openFont(FONT_PATH, 20);
    if (!gFontLarge || !gFontMedium || !gFontSmall) {
        std::cerr << "Warning: could not open fonts — text will be invisible\n";
    }

    /* ── Main loop ── */
    bool running = true;
    SDL_Event ev;

    while (running) {
        /* ── Events ── */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            }
            else if (ev.type == SDL_KEYDOWN) {
                switch (gState) {
                  case AppState::MENU:
                    switch (ev.key.keysym.sym) {
                      case SDLK_RIGHT:
                        if (gFocusIndex < (int)MOVIES.size() - 1) gFocusIndex++;
                        break;
                      case SDLK_LEFT:
                        if (gFocusIndex > 0) gFocusIndex--;
                        break;
                      case SDLK_RETURN:
                      case SDLK_KP_ENTER:
                        gState     = AppState::LOADING;
                        gLoadStart = SDL_GetTicks();
                        break;
                      case SDLK_ESCAPE:
                        running = false;
                        break;
                      default: break;
                    }
                    break;

                  case AppState::LOADING:
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        gState = AppState::MENU;
                    }
                    break;

                  case AppState::PLAYING:
                    switch (ev.key.keysym.sym) {
                      case SDLK_ESCAPE:
                      case SDLK_BACKSPACE:
                        stopPlayback();
                        gState = AppState::MENU;
                        break;
                      case SDLK_SPACE:
                        if (gPlayer) {
                            if (gPaused) {
                                libvlc_media_player_play(gPlayer);
                                gPaused = false;
                            } else {
                                libvlc_media_player_pause(gPlayer);
                                gPaused = true;
                            }
                        }
                        break;
                      default: break;
                    }
                    break;
                }
            }
        }

        /* ── VLC end-of-media check (signal from callback thread) ── */
        if (gState == AppState::PLAYING && gEndOfMedia.load()) {
            stopPlayback();
            gState = AppState::MENU;
        }

        /* ── State machine ── */
        switch (gState) {
          case AppState::MENU:
            drawMenu(renderer);
            break;

          case AppState::LOADING:
            drawLoading(renderer);
            if (SDL_GetTicks() - gLoadStart >= 1500) {
                gState = AppState::PLAYING;
                startPlayback(window, MOVIES[gFocusIndex].imdbId);
            }
            break;

          case AppState::PLAYING:
            drawPlaying(renderer);
            break;
        }

        SDL_Delay(16);  // ~60fps cap; VSync also limits when supported
    }

    /* ── Cleanup ── */
    stopPlayback();
    if (gFontLarge)  TTF_CloseFont(gFontLarge);
    if (gFontMedium) TTF_CloseFont(gFontMedium);
    if (gFontSmall)  TTF_CloseFont(gFontSmall);
    if (gVLC)        libvlc_release(gVLC);

    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
