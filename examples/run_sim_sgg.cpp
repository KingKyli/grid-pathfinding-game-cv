#include <sgg/graphics.h>

#include <GL/glew.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../grid/app/GlobalState.h"
#include "../grid/core/Agent.h"
#include "../grid/core/Map.h"
#include "../grid/pathfinding/AStar.h"

namespace {

// Ο καμβάς επεκτείνεται για να χωρέσει μια λωρίδα πληροφοριών πάνω από το πλέγμα.
constexpr float kHudHeight = 7.4f;

// Η βιβλιοθήκη γραφικών έχει μία «ενεργή» γραμματοσειρά συνολικά. Κρατάμε μια ευανάγνωστη γραμματοσειρά διεπαφής.
static std::string g_font_ui;
static std::string g_font_display;
static std::string g_obstacle_texture;
static std::string g_pacman_wall_texture;
static std::string g_pacman_player_texture;
static std::string g_pacman_ghost_red_texture;
static std::string g_pacman_ghost_pink_texture;
static std::string g_pacman_ghost_cyan_texture;
static std::string g_pacman_pellet_texture;

// Για να βρίσκουμε πόρους ακόμη κι όταν ο τρέχων φάκελος εργασίας δεν είναι η ρίζα του έργου.
static std::filesystem::path g_exe_dir;

static std::string g_sfx_countdown;
static std::string g_sfx_end;
static std::string g_sfx_winner;
static std::string g_cfg_path = "configs/large_agents.txt";
static std::vector<std::string> g_demo_maps = {
    "maps/large.json",
    "maps/huge.json",
    "maps/arena_rings.json",
    "maps/symmetric_lanes.json",
    "maps/zigzag_channels.json",
    "maps/pacman_classic.json",
    "maps/pacman_crossroads.json",
    "maps/pacman_ms1.json",
    "maps/pacman_ms2.json"
};
static int g_demo_map_index = 0;
static bool g_recording = false;
static int g_record_frame = 0;
static std::string g_record_dir = "captures";
static std::string g_record_dir_absolute;
static std::string g_record_session_tag;
static std::string g_record_last_video_path;
static std::string g_record_notice;
static float g_record_notice_ms = 0.0f;
static FILE* g_record_pipe = nullptr;
static int g_record_viewport_x = 0;
static int g_record_viewport_y = 0;
static int g_record_width = 0;
static int g_record_height = 0;
static std::vector<unsigned char> g_record_rgba_buffer;
static bool g_ffmpeg_checked = false;
static bool g_ffmpeg_available = false;
static std::string g_ffmpeg_executable;
constexpr int kRecordFps = 30;
constexpr std::chrono::milliseconds kRecordFrameInterval(1000 / kRecordFps);
static std::chrono::steady_clock::time_point g_record_next_capture_tp;

std::string currentDemoMapName() {
    namespace fs = std::filesystem;
    if (g_demo_maps.empty() || g_demo_map_index < 0 || g_demo_map_index >= static_cast<int>(g_demo_maps.size())) {
        return "n/a";
    }
    return fs::path(g_demo_maps[static_cast<size_t>(g_demo_map_index)]).filename().string();
}

std::string currentDemoMapLabel() {
    std::string name = currentDemoMapName();
    const std::string ext = ".json";
    if (name.size() > ext.size() && name.rfind(ext) == name.size() - ext.size()) {
        name.erase(name.size() - ext.size());
    }
    if (name == "maze_library_like") return "maze";
    if (name == "pacman_classic") return "pac_classic";
    if (name == "pacman_crossroads") return "pac_cross";
    if (name == "pacman_ms1") return "pac_ms1";
    if (name == "pacman_ms2") return "pac_ms2";
    if (name == "large") return "large";
    if (name == "huge") return "huge";
    if (name.size() > 14) {
        name = name.substr(0, 14);
    }
    return name;
}

bool isPacmanThemeMapName(const std::string& map_name) {
    return map_name.rfind("pacman_", 0) == 0;
}

bool isPacmanThemeActive() {
    return isPacmanThemeMapName(currentDemoMapLabel()) || isPacmanThemeMapName(currentDemoMapName());
}

const std::string* pacmanAgentTextureForId(int agent_id) {
    switch (agent_id % 4) {
    case 0:
        return g_pacman_player_texture.empty() ? nullptr : &g_pacman_player_texture;
    case 1:
        return g_pacman_ghost_red_texture.empty() ? nullptr : &g_pacman_ghost_red_texture;
    case 2:
        return g_pacman_ghost_pink_texture.empty() ? nullptr : &g_pacman_ghost_pink_texture;
    default:
        return g_pacman_ghost_cyan_texture.empty() ? nullptr : &g_pacman_ghost_cyan_texture;
    }
}

void pacmanAgentColor(int agent_id, float out_rgb[3]) {
    switch (agent_id % 4) {
    case 0: // pacman yellow
        out_rgb[0] = 0.98f;
        out_rgb[1] = 0.88f;
        out_rgb[2] = 0.10f;
        break;
    case 1: // ghost red
        out_rgb[0] = 0.98f;
        out_rgb[1] = 0.20f;
        out_rgb[2] = 0.20f;
        break;
    case 2: // ghost pink
        out_rgb[0] = 0.98f;
        out_rgb[1] = 0.45f;
        out_rgb[2] = 0.78f;
        break;
    default: // ghost cyan
        out_rgb[0] = 0.20f;
        out_rgb[1] = 0.92f;
        out_rgb[2] = 0.98f;
        break;
    }
}

std::string resolveMapPath(const std::string& map_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    const fs::path input(map_path);
    if (fs::exists(input, ec) && !ec) {
        return input.string();
    }

    std::vector<fs::path> bases;
    bases.push_back(fs::current_path(ec));
    if (!g_exe_dir.empty()) {
        bases.push_back(g_exe_dir);
        bases.push_back(g_exe_dir.parent_path());
        bases.push_back(g_exe_dir.parent_path().parent_path());
    }

    std::sort(bases.begin(), bases.end());
    bases.erase(std::unique(bases.begin(), bases.end()), bases.end());

    for (const auto& b : bases) {
        if (b.empty()) continue;
        const fs::path candidate = b / input;
        if (fs::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
    }

    return map_path;
}

std::string resolveSggHitSoundPath() {
    static std::string cached;
    if (!cached.empty()) return cached;

    namespace fs = std::filesystem;
    std::error_code ec;

    std::vector<fs::path> bases;
    bases.push_back(fs::current_path(ec));
    if (!g_exe_dir.empty()) {
        bases.push_back(g_exe_dir);
        bases.push_back(g_exe_dir.parent_path());
        bases.push_back(g_exe_dir.parent_path().parent_path());
        bases.push_back(g_exe_dir.parent_path().parent_path().parent_path());
    }
    if (!bases.empty() && !bases.front().empty()) {
        const fs::path cwd = bases.front();
        bases.push_back(cwd.parent_path());
        bases.push_back(cwd.parent_path().parent_path());
        bases.push_back(cwd.parent_path().parent_path().parent_path());
    }

    // Αφαίρεση διπλοεγγραφών.
    std::sort(bases.begin(), bases.end());
    bases.erase(std::unique(bases.begin(), bases.end()), bases.end());

    const fs::path rel1 = fs::path("sgg-main") / "assets" / "hit1.wav";
    const fs::path rel2 = fs::path("sgg-main") / "3rdparty" / "assets" / "hit1.wav";

    for (const auto& base : bases) {
        if (base.empty()) continue;
        for (const auto& rel : {rel1, rel2}) {
            const fs::path p = base / rel;
            if (fs::exists(p, ec) && !ec) {
                cached = fs::absolute(p, ec).string();
                if (ec) cached = p.string();
                return cached;
            }
        }
    }

    return cached;
}

void ensureCaptureDir() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(g_record_dir), ec);
    const fs::path abs = fs::absolute(fs::path(g_record_dir), ec);
    g_record_dir_absolute = ec ? fs::path(g_record_dir).string() : abs.string();
}

void setRecordingNotice(const std::string& msg) {
    g_record_notice = msg;
    g_record_notice_ms = 3500.0f;
}

std::string makeRecordingSessionTag() {
    using namespace std::chrono;
    const auto now_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return currentDemoMapLabel() + "_" + std::to_string(now_ms);
}

bool checkFfmpegAvailable() {
    if (g_ffmpeg_checked) return g_ffmpeg_available;

    g_ffmpeg_checked = true;
    g_ffmpeg_available = false;

#ifdef _WIN32
    // 1) Try PATH first.
    if (std::system("ffmpeg -version >nul 2>&1") == 0) {
        g_ffmpeg_executable = "ffmpeg";
        g_ffmpeg_available = true;
        return true;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<fs::path> candidates;

    const char* local_app_data = std::getenv("LOCALAPPDATA");
    if (local_app_data && *local_app_data) {
        const fs::path lad(local_app_data);
        candidates.push_back(lad / "Microsoft" / "WinGet" / "Links" / "ffmpeg.exe");
        candidates.push_back(lad / "Microsoft" / "WindowsApps" / "ffmpeg.exe");

        const fs::path packages_dir = lad / "Microsoft" / "WinGet" / "Packages";
        if (fs::exists(packages_dir, ec) && !ec) {
            for (const auto& e : fs::directory_iterator(packages_dir, ec)) {
                if (ec) break;
                if (!e.is_directory(ec) || ec) continue;
                const std::string dir_name = e.path().filename().string();
                if (dir_name.rfind("Gyan.FFmpeg", 0) != 0) continue;

                for (const auto& sub : fs::recursive_directory_iterator(e.path(), ec)) {
                    if (ec) break;
                    if (!sub.is_regular_file(ec) || ec) continue;
                    if (sub.path().filename() == "ffmpeg.exe") {
                        candidates.push_back(sub.path());
                    }
                }
            }
        }
    }

    for (const auto& p : candidates) {
        if (p.empty()) continue;
        if (fs::exists(p, ec) && !ec) {
            g_ffmpeg_executable = p.string();
            g_ffmpeg_available = true;
            return true;
        }
    }

    return false;
#else
    g_ffmpeg_available = (std::system("ffmpeg -version >/dev/null 2>&1") == 0);
    if (g_ffmpeg_available) g_ffmpeg_executable = "ffmpeg";
    return g_ffmpeg_available;
#endif
}

void stopLiveRecording(bool keep_notice = true) {
    if (!g_record_pipe) {
        g_recording = false;
        return;
    }

#ifdef _WIN32
    const int rc = _pclose(g_record_pipe);
#else
    const int rc = pclose(g_record_pipe);
#endif
    g_record_pipe = nullptr;
    g_record_rgba_buffer.clear();
    g_recording = false;

    if (!keep_notice) return;

    if (rc == 0 && !g_record_last_video_path.empty()) {
        setRecordingNotice("MP4 saved -> " + g_record_last_video_path);
        std::cout << "[recording] MP4 saved: " << g_record_last_video_path << "\n";
    } else if (rc == 0) {
        setRecordingNotice("REC OFF");
    } else {
        setRecordingNotice("MP4 export failed (ffmpeg)");
        std::cout << "[recording] ffmpeg exited with code " << rc << "\n";
    }
}

bool startLiveRecording() {
    namespace fs = std::filesystem;

    ensureCaptureDir();
    if (!checkFfmpegAvailable()) {
        setRecordingNotice("REC error: ffmpeg not found");
        std::cout << "[recording] ffmpeg not found (PATH/WinGet)\n";
        return false;
    }

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    g_record_viewport_x = viewport[0];
    g_record_viewport_y = viewport[1];
    g_record_width = viewport[2];
    g_record_height = viewport[3];

    if (g_record_width <= 0 || g_record_height <= 0) {
        setRecordingNotice("REC error: invalid viewport");
        return false;
    }

    g_record_session_tag = makeRecordingSessionTag();
    std::error_code ec;
    const fs::path out_rel = fs::path(g_record_dir) / (g_record_session_tag + ".mp4");
    const fs::path out_abs = fs::absolute(out_rel, ec);
    g_record_last_video_path = ec ? out_rel.string() : out_abs.string();

    std::ostringstream cmd;
    cmd << "\"" << g_ffmpeg_executable << "\""
        << " -y -hide_banner -loglevel error"
        << " -f rawvideo -pixel_format rgba"
        << " -video_size " << g_record_width << "x" << g_record_height
        << " -framerate " << kRecordFps
        << " -i -"
        << " -vf vflip"
        << " -an -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p"
        << " \"" << g_record_last_video_path << "\"";

#ifdef _WIN32
    g_record_pipe = _popen(cmd.str().c_str(), "wb");
#else
    g_record_pipe = popen(cmd.str().c_str(), "w");
#endif
    if (!g_record_pipe) {
        setRecordingNotice("REC error: cannot start ffmpeg");
        std::cout << "[recording] failed to start ffmpeg process\n";
        return false;
    }

    g_record_rgba_buffer.assign(static_cast<size_t>(g_record_width) * static_cast<size_t>(g_record_height) * 4U, 0U);
    g_record_frame = 0;
    g_record_next_capture_tp = std::chrono::steady_clock::now();
    g_recording = true;
    setRecordingNotice("REC ON -> " + g_record_last_video_path);
    std::cout << "[recording] writing live MP4: " << g_record_last_video_path
              << " | ffmpeg=" << g_ffmpeg_executable << "\n";
    return true;
}

bool captureLiveVideoFrame() {
    if (!g_record_pipe || g_record_width <= 0 || g_record_height <= 0) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < g_record_next_capture_tp) {
        return true;
    }
    while (g_record_next_capture_tp <= now) {
        g_record_next_capture_tp += kRecordFrameInterval;
    }

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[0] != g_record_viewport_x || viewport[1] != g_record_viewport_y ||
        viewport[2] != g_record_width || viewport[3] != g_record_height) {
        setRecordingNotice("REC stopped: viewport changed");
        std::cout << "[recording] viewport changed during recording; stopping.\n";
        return false;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(g_record_viewport_x, g_record_viewport_y,
                 static_cast<GLsizei>(g_record_width), static_cast<GLsizei>(g_record_height),
                 GL_RGBA, GL_UNSIGNED_BYTE, g_record_rgba_buffer.data());

    const size_t bytes = g_record_rgba_buffer.size();
    const size_t written = std::fwrite(g_record_rgba_buffer.data(), 1, bytes, g_record_pipe);
    if (written != bytes) {
        setRecordingNotice("REC write failed");
        std::cout << "[recording] failed to write frame to ffmpeg pipe\n";
        return false;
    }

    ++g_record_frame;
    return true;
}

void logAudioEvent(const std::string& msg) {
    // Μικρή καταγραφή αποσφαλμάτωσης για τον ήχο (βοηθάει όταν "δεν ακούγεται τίποτα").
    // Γράφει δίπλα στον τρέχοντα φάκελο εργασίας (συνήθως τη ρίζα του έργου).
    std::ofstream out("audio_debug.txt", std::ios::app);
    if (!out) return;
    out << msg << "\n";
}

int clampCpuDifficulty(int v) {
    return std::clamp(v, 0, 2);
}

const char* cpuDifficultyName(const grid::GlobalState& state) {
    switch (clampCpuDifficulty(state.cpu_difficulty)) {
    case 0: return "EASY";
    case 2: return "HARD";
    default: return "NORMAL";
    }
}

float cpuStepThrottleMs(const grid::GlobalState& state) {
    switch (clampCpuDifficulty(state.cpu_difficulty)) {
    case 0: return 360.0f; // εύκολο: αργά
    case 2: return 180.0f; // δύσκολο: πιο γρήγορα
    default: return 260.0f; // κανονικό: γρήγορα
    }
}

int cpuProbeLimit(const grid::GlobalState& state) {
    switch (clampCpuDifficulty(state.cpu_difficulty)) {
    case 0: return 10;
    case 2: return 45;
    default: return 25;
    }
}

void cycleCpuDifficulty(grid::GlobalState& state) {
    state.cpu_difficulty = clampCpuDifficulty(state.cpu_difficulty);
    state.cpu_difficulty = (state.cpu_difficulty + 1) % 3;
}

static std::string formatTimeMMSS(int total_seconds) {
    total_seconds = std::max(0, total_seconds);
    const int mm = total_seconds / 60;
    const int ss = total_seconds % 60;
    std::string out;
    out += std::to_string(mm);
    out += ":";
    if (ss < 10) out += "0";
    out += std::to_string(ss);
    return out;
}

std::string resolveClickSoundPath() {
    static std::string cached;
    if (!cached.empty()) return cached;

    namespace fs = std::filesystem;
    const fs::path cwd = fs::current_path();
    const char* candidates[] = {
       
        "assets/click.wav",
        "assets/hit1.wav",
        "../assets/click.wav",
        "../assets/hit1.wav",

        "sgg-main/assets/hit1.wav",
        "../sgg-main/assets/hit1.wav",
        "../../sgg-main/assets/hit1.wav",
        "../../../sgg-main/assets/hit1.wav",
        "sgg-main/3rdparty/assets/hit1.wav",
        "../sgg-main/3rdparty/assets/hit1.wav",
        "../../sgg-main/3rdparty/assets/hit1.wav",
        "../../../sgg-main/3rdparty/assets/hit1.wav",
    };

    for (const char* rel : candidates) {
        std::error_code ec;
        if (fs::exists(rel, ec) && !ec) {
            cached = fs::absolute(fs::path(rel), ec).string();
            if (ec) cached = rel;
            return cached;
        }
    }
    return cached;
}

std::string resolveCollectSoundPath() {
    static std::string cached;
    if (!cached.empty()) return cached;

    namespace fs = std::filesystem;
    const fs::path cwd = fs::current_path();

 
    const char* candidates[] = {
        "assets/hit1.wav",
        "assets/click.wav",
        "../assets/hit1.wav",
        "../assets/click.wav",
        "sgg-main/assets/hit1.wav",
        "../sgg-main/assets/hit1.wav",
        "sgg-main/3rdparty/assets/hit1.wav",
        "../sgg-main/3rdparty/assets/hit1.wav",
    };

    for (const char* rel : candidates) {
        fs::path p = cwd / rel;
        std::error_code ec;
        if (fs::exists(p, ec)) {
            cached = p.lexically_normal().string();
            break;
        }
    }

    return cached;
}

std::string resolveCountdownSoundPath() {
    return resolveSggHitSoundPath();
}

std::string resolveEndSoundPath() {
    return resolveSggHitSoundPath();
}

std::string resolveWinnerSoundPath() {
    return resolveSggHitSoundPath();
}

std::string resolveFontPath() {
    namespace fs = std::filesystem;

    const fs::path cwd = fs::current_path();

    const std::vector<fs::path> candidates = {
        fs::path("C:/Windows/Fonts/bahnschrift.ttf"),
        fs::path("C:/Windows/Fonts/segoeui.ttf"),
        fs::path("C:/Windows/Fonts/arial.ttf"),

      
        cwd / "sgg-main" / "assets" / "orange juice 2.0.ttf",
        cwd / "sgg-main" / "3rdparty" / "assets" / "orange juice 2.0.ttf",
        cwd / "assets" / "orange juice 2.0.ttf",
    };

    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && !ec) {
            return p.string();
        }
    }
    return {};
}

std::string resolveDisplayFontPath() {
    namespace fs = std::filesystem;

    const fs::path cwd = fs::current_path();
    const std::vector<fs::path> candidates = {
        cwd / "sgg-main" / "assets" / "orange juice 2.0.ttf",
        cwd / "sgg-main" / "3rdparty" / "assets" / "orange juice 2.0.ttf",
        cwd / "assets" / "orange juice 2.0.ttf",
    };

    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && !ec) {
            return p.string();
        }
    }
    return {};
}

std::string resolveObstacleTexturePath() {
    static std::string cached;
    if (!cached.empty()) return cached;

    namespace fs = std::filesystem;
    const fs::path cwd = fs::current_path();
    const std::vector<fs::path> candidates = {
        cwd / "assets" / "iron.png",
        cwd / "sgg-main" / "assets" / "iron.png",
        cwd / "sgg-main" / "3rdparty" / "assets" / "iron.png",
        cwd / "../assets" / "iron.png",
        cwd / "../sgg-main" / "assets" / "iron.png",
    };

    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && !ec) {
            cached = p.string();
            return cached;
        }
    }
    return cached;
}

std::string resolvePacmanTexturePath(const std::string& filename) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);

    std::vector<fs::path> bases;
    bases.push_back(cwd);
    if (!g_exe_dir.empty()) {
        bases.push_back(g_exe_dir);
        bases.push_back(g_exe_dir.parent_path());
        bases.push_back(g_exe_dir.parent_path().parent_path());
    }

    std::sort(bases.begin(), bases.end());
    bases.erase(std::unique(bases.begin(), bases.end()), bases.end());

    for (const auto& b : bases) {
        if (b.empty()) continue;
        const fs::path p = b / "pacman_ui_kit" / filename;
        if (fs::exists(p, ec) && !ec) {
            return p.string();
        }
    }
    return {};
}

static int manhattan(const grid::Point& a, const grid::Point& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

void agentColor(int agent_id, float out_rgb[3]) {
    
    switch (agent_id % 6) {
    case 0: // κόκκινο
        out_rgb[0] = 0.90f;
        out_rgb[1] = 0.20f;
        out_rgb[2] = 0.20f;
        break;
    case 1: // κυανό
        out_rgb[0] = 0.20f;
        out_rgb[1] = 0.80f;
        out_rgb[2] = 0.95f;
        break;
    case 2: // πράσινο
        out_rgb[0] = 0.25f;
        out_rgb[1] = 0.90f;
        out_rgb[2] = 0.35f;
        break;
    case 3: // πορτοκαλί
        out_rgb[0] = 0.95f;
        out_rgb[1] = 0.55f;
        out_rgb[2] = 0.15f;
        break;
    case 4: // μωβ
        out_rgb[0] = 0.72f;
        out_rgb[1] = 0.35f;
        out_rgb[2] = 0.95f;
        break;
    default: // κίτρινο
        out_rgb[0] = 0.95f;
        out_rgb[1] = 0.85f;
        out_rgb[2] = 0.20f;
        break;
    }
}

class TargetEntity final : public grid::Entity {
public:
    explicit TargetEntity(grid::Point cell) : cell_(cell) {}

    const grid::Point& cell() const { return cell_; }

    bool update(grid::GlobalState&, float) override { return true; }

    void draw(const grid::GlobalState&) const override {
        graphics::Brush t;
        t.fill_color[0] = 0.95f;
        t.fill_color[1] = 0.85f;
        t.fill_color[2] = 0.20f;
        t.fill_opacity = 0.85f;
        t.outline_opacity = 0.0f;

        if (isPacmanThemeActive() && !g_pacman_pellet_texture.empty()) {
            t.texture = g_pacman_pellet_texture;
            t.fill_opacity = 1.0f;
            graphics::drawRect(cell_.x + 0.5f, cell_.y + 0.5f + kHudHeight, 0.44f, 0.44f, t);
            return;
        }

        graphics::drawDisk(cell_.x + 0.5f, cell_.y + 0.5f + kHudHeight, 0.22f, t);
    }

private:
    grid::Point cell_{0, 0};
};

std::optional<grid::Path> bfsPathToNearestTarget(const grid::Map& map, const grid::Point& start, const std::vector<const TargetEntity*>& targets) {
    if (targets.empty()) return std::nullopt;
    if (!map.isFree(start)) return std::nullopt;

    const int w = map.width();
    const int h = map.height();
    if (w <= 0 || h <= 0) return std::nullopt;

    auto key = [&](const grid::Point& p) -> int { return p.y * w + p.x; };

    std::vector<uint8_t> is_target(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (const auto* t : targets) {
        const grid::Point c = t->cell();
        if (c.x < 0 || c.x >= w || c.y < 0 || c.y >= h) continue;
        is_target[static_cast<size_t>(key(c))] = 1;
    }

    std::vector<int> parent(static_cast<size_t>(w) * static_cast<size_t>(h), -1);
    std::vector<int> dist(static_cast<size_t>(w) * static_cast<size_t>(h), -1);
    std::vector<int> q;
    q.reserve(static_cast<size_t>(w) * static_cast<size_t>(h));

    const int start_k = key(start);
    dist[static_cast<size_t>(start_k)] = 0;
    q.push_back(start_k);

    size_t qi = 0;
    int found_k = -1;
    while (qi < q.size()) {
        const int cur_k = q[qi++];
        const grid::Point cur{cur_k % w, cur_k / w};

        if (is_target[static_cast<size_t>(cur_k)] != 0) {
            found_k = cur_k;
            break; // αναζήτηση κατά πλάτος
        }

        for (const auto& n : map.neighbors(cur)) {
            const int nk = key(n);
            if (dist[static_cast<size_t>(nk)] >= 0) continue;
            dist[static_cast<size_t>(nk)] = dist[static_cast<size_t>(cur_k)] + 1;
            parent[static_cast<size_t>(nk)] = cur_k;
            q.push_back(nk);
        }
    }

    if (found_k < 0) return std::nullopt;

    grid::Path path;
    int cur = found_k;
    while (cur >= 0) {
        path.push_back(grid::Point{cur % w, cur / w});
        if (cur == start_k) break;
        cur = parent[static_cast<size_t>(cur)];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

class AgentEntity final : public grid::Entity {
public:
    explicit AgentEntity(grid::Agent agent) : agent_(std::move(agent)) {}

    int id() const { return agent_.id(); }
    const grid::Point& position() const { return agent_.position(); }
    const grid::Point& goalPoint() const { return agent_.goalPoint(); }
    void setGoalPoint(const grid::Point& g) {
        agent_.setGoalPoint(g);
        repath_cooldown_ms_ = 0.0f;
        stuck_ticks_ = 0;
        last_pos_ = agent_.position();
    }
    bool atGoal() const { return agent_.atGoal(); }
    size_t remainingPathNodes() const {
        const auto& p = agent_.currentPath();
        const size_t idx = agent_.currentPathIndex();
        return (idx < p.size()) ? (p.size() - idx) : 0u;
    }

    grid::AgentState agentState() const { return agent_.state(); }

    int score() const { return score_; }
    void addScore(int delta) { score_ += delta; }
    void resetScore() { score_ = 0; }

    void resetForRestart() {
        // Σταμάτα την κίνηση και καθάρισε τυχόν pending path/intent.
        agent_.clearPath();
        const grid::Point p = agent_.position();
        if (!(agent_.goalPoint() == p)) {
            agent_.setGoalPoint(p);
        }
        repath_cooldown_ms_ = 0.0f;
        stuck_ticks_ = 0;
        wants_step_ = false;
        intended_cell_ = p;
        cpu_throttle_ms_ = 0.0f;
        last_pos_ = p;
    }

    // Αποφυγή σύγκρουσης 
    bool wantsStep() const { return wants_step_; }
    const grid::Point& intendedCell() const { return intended_cell_; }

    void commitStepAllowed() {
        const grid::Point before = agent_.position();
        agent_.step();
        const grid::Point after = agent_.position();

        if (!agent_.atGoal() && before == after) {
            ++stuck_ticks_;
        } else {
            stuck_ticks_ = 0;
        }
        last_pos_ = after;
        wants_step_ = false;
    }

    void commitStepBlocked() {
        // Αν μπλοκαριστεί, το μετράμε σαν "κόλλημα" ώστε να ξανακάνει path
        if (!agent_.atGoal()) {
            ++stuck_ticks_;
        }
        last_pos_ = agent_.position();
        wants_step_ = false;
    }

    bool update(grid::GlobalState& state, float) override {
        const float dt_ms = static_cast<float>(state.tick_delay_ms);
        repath_cooldown_ms_ = std::max(0.0f, repath_cooldown_ms_ - dt_ms);

        // Autopilot  όταν είναι ενεργό για αυτόν τον agent και
        // είναι idle/τερμάτισε, διάλεξε το κοντινότερο target.
        if (state.autopilot_agent_id == agent_.id() &&
            repath_cooldown_ms_ <= 0.0f &&
            (agent_.state() == grid::AgentState::Idle || agent_.state() == grid::AgentState::ReachedGoal || agent_.state() == grid::AgentState::NoPath)) {

            // Διάλεξε έναν reachable στόχο.
            // Αν επιλέγουμε συνέχεια unreachable target, ο agent μπορεί να μείνει σε NoPath για πάντα.
            std::vector<const TargetEntity*> targets;
            targets.reserve(state.entities.size());
            for (const auto& e : state.entities) {
                const auto* te = dynamic_cast<const TargetEntity*>(e.get());
                if (!te) continue;
                targets.push_back(te);
            }

            std::sort(targets.begin(), targets.end(), [&](const TargetEntity* a, const TargetEntity* b) {
                return manhattan(agent_.position(), a->cell()) < manhattan(agent_.position(), b->cell());
            });

            // HARD: ένα BFS για να βρούμε το πραγματικά κοντινότερο reachable target και το shortest path.
            // Είναι πιο γρήγορο (1 traversal) και συνήθως κάνει καλύτερες επιλογές από πολλά A* probes.
            if (state.cpu_agent_id == agent_.id() && clampCpuDifficulty(state.cpu_difficulty) == 2) {
                const auto path = bfsPathToNearestTarget(state.map, agent_.position(), targets);
                if (path && !path->empty()) {
                    const grid::Point goal = path->back();
                    setGoalPoint(goal);
                    agent_.setPath(*path);
                    repath_cooldown_ms_ = 120.0f;
                } else {
                    repath_cooldown_ms_ = 220.0f;
                }
            } else {
                const int kMaxProbe = cpuProbeLimit(state);
                int probed = 0;
                for (const auto* te : targets) {
                    if (probed++ >= kMaxProbe) break;
                    int expanded = 0;
                    const auto t0 = std::chrono::high_resolution_clock::now();
                    const auto p = grid::findPath(state.map, agent_.position(), te->cell(),
                        [&](const grid::Point&, const std::string& phase) {
                            if (phase == "closed") ++expanded;
                        });
                    const auto t1 = std::chrono::high_resolution_clock::now();
                    state.hud_last_expanded = expanded;
                    state.hud_last_path_len = p ? static_cast<int>(p->size()) : 0;
                    state.hud_last_search_ms =
                        std::chrono::duration<float, std::milli>(t1 - t0).count();
                    state.hud_search_calls += 1;
                    if (!p) continue;

                    setGoalPoint(te->cell());
                    agent_.setPath(*p);
                    repath_cooldown_ms_ = 220.0f;
                    break;
                }
            }
        }

        if (agent_.atGoal()) {
            agent_.clearPath();
            return true;
        }

        // Κάνε repath μόνο όταν χρειάζεται:
        // - όταν αλλάζει ο στόχος
        // - όταν το path είναι άδειο/έχει καταναλωθεί
        // - όταν φαίνεται ότι "κολλήσαμε" για λίγα ticks
        bool need_repath = false;
        if (agent_.consumeGoalDirty()) {
            need_repath = true;
        }

        const bool no_remaining_path = (remainingPathNodes() == 0u);
        if (!need_repath && agent_.state() != grid::AgentState::NoPath) {
            if (no_remaining_path && repath_cooldown_ms_ <= 0.0f) {
                need_repath = true;
            }
            if (!need_repath && stuck_ticks_ >= 6 && repath_cooldown_ms_ <= 0.0f) {
                need_repath = true;
            }
        }

        if (need_repath) {
            int expanded = 0;
            const auto t0 = std::chrono::high_resolution_clock::now();
            const auto p = grid::findPath(state.map, agent_.position(), agent_.goalPoint(),
                [&](const grid::Point&, const std::string& phase) {
                    if (phase == "closed") ++expanded;
                });
            const auto t1 = std::chrono::high_resolution_clock::now();
            state.hud_last_expanded = expanded;
            state.hud_last_path_len = p ? static_cast<int>(p->size()) : 0;
            state.hud_last_search_ms =
                std::chrono::duration<float, std::milli>(t1 - t0).count();
            state.hud_search_calls += 1;
            if (p) {
                agent_.setPath(*p);
                repath_cooldown_ms_ = 200.0f;
            } else {
                agent_.setNoPath();
                repath_cooldown_ms_ = 800.0f;
            }
            stuck_ticks_ = 0;
            last_pos_ = agent_.position();
        }

        // Φάση 1: υπολόγισε το "επόμενο κελί"
        wants_step_ = false;
        intended_cell_ = agent_.position();
        if (!agent_.atGoal() && agent_.state() != grid::AgentState::NoPath) {
            const auto& p = agent_.currentPath();
            const size_t idx = agent_.currentPathIndex();
            if (!p.empty() && idx < p.size()) {
                intended_cell_ = p[idx];
                wants_step_ = true;
            }
        }

        // Κόψε ταχύτητα στον CPU agent
        if (state.cpu_agent_id == agent_.id()) {
            cpu_throttle_ms_ = std::max(0.0f, cpu_throttle_ms_ - dt_ms);

            // Αν η δυσκολία γίνει πιο γρήγορη, μην περιμένεις με παλιό throttle.
            const float desired = cpuStepThrottleMs(state);
            cpu_throttle_ms_ = std::min(cpu_throttle_ms_, desired);

            if (cpu_throttle_ms_ > 0.0f) {
                wants_step_ = false;
                intended_cell_ = agent_.position();
            } else if (wants_step_) {
                // Αφού επιτρέψουμε ένα βήμα, περίμενε λίγο πριν το επόμενο.
                cpu_throttle_ms_ = desired;
            }
        }

        return true;
    }

    void draw(const grid::GlobalState& state) const override {
        const bool selected = (state.selected_agent_id == agent_.id());

        // Ζωγράφισε το υπόλοιπο path (μόνο για τον selected agent)
        if (selected) {
            graphics::Brush path;
            path.fill_color[0] = 0.2f;
            path.fill_color[1] = 0.8f;
            path.fill_color[2] = 0.9f;
            path.fill_opacity = 0.35f;
            path.outline_opacity = 0.0f;

            const auto& p = agent_.currentPath();
            for (size_t i = agent_.currentPathIndex(); i < p.size(); ++i) {
                graphics::drawDisk(p[i].x + 0.5f, p[i].y + 0.5f + kHudHeight, 0.12f, path);
            }

            // Ζωγράφισε δείκτη στόχου
            graphics::Brush goal;
            goal.fill_color[0] = 0.2f;
            goal.fill_color[1] = 0.95f;
            goal.fill_color[2] = 0.3f;
            goal.fill_opacity = 0.0f;
            goal.outline_color[0] = goal.fill_color[0];
            goal.outline_color[1] = goal.fill_color[1];
            goal.outline_color[2] = goal.fill_color[2];
            goal.outline_opacity = 0.9f;
            goal.outline_width = 0.12f;

            const auto& g = agent_.goalPoint();
            graphics::drawDisk(g.x + 0.5f, g.y + 0.5f + kHudHeight, 0.38f, goal);
        }

        graphics::Brush agent;
        float rgb[3];
        if (isPacmanThemeActive()) {
            pacmanAgentColor(agent_.id(), rgb);
        } else {
            agentColor(agent_.id(), rgb);
        }
        agent.fill_color[0] = rgb[0];
        agent.fill_color[1] = rgb[1];
        agent.fill_color[2] = rgb[2];
        agent.fill_opacity = 1.0f;
        if (selected) {
            agent.outline_color[0] = 1.0f;
            agent.outline_color[1] = 1.0f;
            agent.outline_color[2] = 1.0f;
            agent.outline_opacity = 0.9f;
            agent.outline_width = 0.14f;
        } else {
            agent.outline_opacity = 0.0f;
        }

        const auto& pos = agent_.position();

        if (isPacmanThemeActive()) {
            const std::string* tex = pacmanAgentTextureForId(agent_.id());
            if (tex) {
                agent.texture = *tex;
                agent.fill_color[0] = 1.0f;
                agent.fill_color[1] = 1.0f;
                agent.fill_color[2] = 1.0f;
                agent.fill_opacity = 1.0f;
                graphics::drawRect(pos.x + 0.5f, pos.y + 0.5f + kHudHeight, 0.78f, 0.78f, agent);
                return;
            }
        }

        graphics::drawDisk(pos.x + 0.5f, pos.y + 0.5f + kHudHeight, 0.35f, agent);
    }

private:
    grid::Agent agent_;
    int score_ = 0;
    float repath_cooldown_ms_ = 0.0f;
    int stuck_ticks_ = 0;
    grid::Point last_pos_{0, 0};

    // Intent για αποφυγή σύγκρουσης.
    bool wants_step_ = false;
    grid::Point intended_cell_{0, 0};

    // Throttle κίνησης για CPU agent.
    float cpu_throttle_ms_ = 0.0f;
};

const AgentEntity* findSelectedAgent(const grid::GlobalState& state) {
    if (state.selected_agent_id < 0) return nullptr;
    for (const auto& e : state.entities) {
        const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
        if (!ae) continue;
        if (ae->id() == state.selected_agent_id) return ae;
    }
    return nullptr;
}

AgentEntity* findAgentById(grid::GlobalState& state, int agent_id) {
    if (agent_id < 0) return nullptr;
    for (auto& e : state.entities) {
        auto* ae = dynamic_cast<AgentEntity*>(e.get());
        if (!ae) continue;
        if (ae->id() == agent_id) return ae;
    }
    return nullptr;
}

bool readDirWASD(int& out_dx, int& out_dy) {
    out_dx = 0;
    out_dy = 0;
    if (graphics::getKeyState(graphics::SCANCODE_W)) { out_dy = -1; return true; }
    if (graphics::getKeyState(graphics::SCANCODE_S)) { out_dy = 1; return true; }
    if (graphics::getKeyState(graphics::SCANCODE_A)) { out_dx = -1; return true; }
    if (graphics::getKeyState(graphics::SCANCODE_D)) { out_dx = 1; return true; }
    return false;
}

bool readDirArrows(int& out_dx, int& out_dy) {
    out_dx = 0;
    out_dy = 0;
    if (graphics::getKeyState(graphics::SCANCODE_UP)) { out_dy = -1; return true; }
    if (graphics::getKeyState(graphics::SCANCODE_DOWN)) { out_dy = 1; return true; }
    if (graphics::getKeyState(graphics::SCANCODE_LEFT)) { out_dx = -1; return true; }
    if (graphics::getKeyState(graphics::SCANCODE_RIGHT)) { out_dx = 1; return true; }
    return false;
}

class RippleEntity final : public grid::Entity {
public:
    RippleEntity(float x, float y, float lifetime_ms) : x_(x), y_(y), remaining_ms_(lifetime_ms) {}

    bool update(grid::GlobalState&, float dt_ms) override {
        remaining_ms_ -= dt_ms;
        radius_ += 0.0035f * dt_ms;
        return remaining_ms_ > 0.0f;
    }

    void draw(const grid::GlobalState&) const override {
        graphics::Brush br;
        br.fill_color[0] = 0.2f;
        br.fill_color[1] = 0.9f;
        br.fill_color[2] = 0.6f;
        br.fill_opacity = 0.0f;
        br.outline_color[0] = br.fill_color[0];
        br.outline_color[1] = br.fill_color[1];
        br.outline_color[2] = br.fill_color[2];
        br.outline_width = 0.08f;
        br.outline_opacity = std::max(0.0f, std::min(1.0f, remaining_ms_ / 900.0f));

        graphics::drawDisk(x_, y_, radius_, br);
    }

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    float radius_ = 0.15f;
    float remaining_ms_ = 0.0f;
};

void drawWorld(const grid::GlobalState& state) {
    const int w = state.map.width();
    const int h = state.map.height();

    graphics::Brush obstacle;
    // Υψηλότερη αντίθεση + περίγραμμα, ώστε τα εμπόδια να ξεχωρίζουν καθαρά.
    obstacle.fill_color[0] = 0.85f;
    obstacle.fill_color[1] = 0.85f;
    obstacle.fill_color[2] = 0.90f;
    obstacle.fill_opacity = 0.92f;
    obstacle.outline_color[0] = 0.05f;
    obstacle.outline_color[1] = 0.05f;
    obstacle.outline_color[2] = 0.06f;
    obstacle.outline_opacity = 0.85f;
    obstacle.outline_width = 2.0f;
    const std::string& obstacle_texture = (isPacmanThemeActive() && !g_pacman_wall_texture.empty())
        ? g_pacman_wall_texture
        : g_obstacle_texture;

    if (!obstacle_texture.empty()) {
        // Το SGG υποστηρίζει PNG textures με ανάμιξη (blend) με το fill color.
        obstacle.texture = obstacle_texture;
        obstacle.fill_opacity = 0.85f;
        obstacle.outline_opacity = 0.70f;
    } else {
        // Εφεδρικά: gradient για να μην δείχνουν τα blocks «επίπεδα».
        obstacle.gradient = true;
        obstacle.fill_secondary_color[0] = 0.55f;
        obstacle.fill_secondary_color[1] = 0.60f;
        obstacle.fill_secondary_color[2] = 0.70f;
        obstacle.fill_secondary_opacity = 1.0f;
        obstacle.gradient_dir_u = 1.0f;
        obstacle.gradient_dir_v = 0.0f;
    }

    graphics::Brush free_cell;
    if (isPacmanThemeActive()) {
        free_cell.fill_color[0] = 0.05f;
        free_cell.fill_color[1] = 0.05f;
        free_cell.fill_color[2] = 0.12f;
        free_cell.fill_opacity = 0.32f;
    } else {
        free_cell.fill_color[0] = 0.92f;
        free_cell.fill_color[1] = 0.92f;
        free_cell.fill_color[2] = 0.95f;
        free_cell.fill_opacity = 0.09f;
    }
    free_cell.outline_opacity = 0.0f;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const grid::Point p{x, y};
            const float cx = x + 0.5f;
            const float cy = y + 0.5f + kHudHeight;

            if (state.map.isFree(p)) {
                graphics::drawRect(cx, cy, 1.0f, 1.0f, free_cell);
            } else {
                graphics::drawRect(cx, cy, 1.0f, 1.0f, obstacle);
            }
        }
    }
}

void drawHud(const grid::GlobalState& state) {
    // Συμπαγής λωρίδα HUD πάνω από το grid.
    graphics::Brush hud_bg;
    hud_bg.fill_color[0] = 0.0f;
    hud_bg.fill_color[1] = 0.0f;
    hud_bg.fill_color[2] = 0.0f;
    hud_bg.fill_opacity = 0.65f;
    hud_bg.outline_opacity = 0.0f;
    graphics::drawRect(state.map.width() * 0.5f, kHudHeight * 0.5f, static_cast<float>(state.map.width()), kHudHeight, hud_bg);

    graphics::Brush text;
    text.fill_color[0] = 1.0f;
    text.fill_color[1] = 1.0f;
    text.fill_color[2] = 1.0f;
    text.fill_opacity = 1.0f;
    text.outline_opacity = 0.0f;

    // Κρατάμε το HUD διακριτικό και ευανάγνωστο.
    const float kFontMain = 0.80f;
    const float kFontSub = 0.65f;
    const float kFontScoreHeader = 0.95f;

    if (!g_font_ui.empty()) {
        graphics::setFont(g_font_ui);
    }

    auto approxTextHalfWidth = [](const std::string& s, float size) -> float {
        // Το SGG δεν δίνει μέτρηση πλάτους κειμένου· χρησιμοποιούμε ένα «ζυγισμένο» heuristic που
        // συμπεριφέρεται καλύτερα για φαρδιά glyphs (W/M) και στενά (I), ώστε οι τίτλοι να φαίνονται κεντραρισμένοι.
        float units = 0.0f;
        for (const char c : s) {
            if (c == ' ') {
                units += 0.35f;
            } else if (c == 'I' || c == '1') {
                units += 0.35f;
            } else if (c == 'W' || c == 'M') {
                units += 0.75f;
            } else {
                units += 0.55f;
            }
        }
        return units * size * 0.50f;
    };

    auto drawTextShadowed = [](float x, float y, float size, const std::string& s, const graphics::Brush& fg, const graphics::Brush& sh, float dx = 0.04f, float dy = 0.04f) {
        graphics::drawText(x + dx, y + dy, size, s, sh);
        graphics::drawText(x, y, size, s, fg);
    };

    auto drawKeyAccentLine = [&](float x, float y, float size, const std::string& s, const graphics::Brush& normal, const graphics::Brush& accent, const graphics::Brush& sh) {
        // Χρωματίζουμε tokens σε αγκύλες, π.χ. [ENTER], [R], [C], [1], κρατώντας το υπόλοιπο κείμενο «κανονικό».
        // Το SGG δεν έχει rich text, άρα ζωγραφίζουμε τμήματα ένα-ένα και προχωράμε το x με το ίδιο heuristic.
        const auto approxTextWidth = [&](const std::string& seg) {
            return approxTextHalfWidth(seg, size) * 2.0f;
        };

        float pen_x = x;
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '[') {
                const size_t close = s.find(']', i + 1);
                if (close != std::string::npos) {
                    const std::string tok = s.substr(i, close - i + 1);
                    drawTextShadowed(pen_x, y, size, tok, accent, sh);
                    pen_x += approxTextWidth(tok);
                    i = close + 1;
                    continue;
                }
            }

            const size_t next = s.find('[', i);
            const size_t end = (next == std::string::npos) ? s.size() : next;
            const std::string seg = s.substr(i, end - i);
            if (!seg.empty()) {
                drawTextShadowed(pen_x, y, size, seg, normal, sh);
                pen_x += approxTextWidth(seg);
            }
            i = end;
        }
    };

    // Setup mode (επιλογή διάρκειας match).
    if (!state.match_started) {
        const float cx = state.map.width() * 0.5f;
        const bool compact = (state.map.width() < 45);

        // Setup lines με ομοιόμορφο line rhythm και μικρότερα chunks για καλύτερη ανάγνωση.
        const std::string line_duration = compact
            ? "[1] 60s   [2] 90s   [3] 120s"
            : "Duration:   [1] 60s     [2] 90s     [3] 120s";
        const std::string line_actions  = compact
            ? "[ENTER] Start   [R] Restart"
            : "[ENTER] Start    [R] Restart    [SPACE] Pause";
        const std::string line_players1 = compact
            ? "P1: WASD | P2: Arrows"
            : "P1: WASD   |   P2: Arrow Keys";
        const std::string line_players2 = compact
            ? "[N] Step | [-/+] Speed"
            : "[N] Step   |   [-/+] Speed";
        const std::string line_editor   = "[LMB] Draw wall   [RMB] Erase wall";
        std::string line_settings;
        if (state.cpu_agent_id >= 0) {
            line_settings = compact
                ? (std::string("[C] Difficulty (") + cpuDifficultyName(state) + ")   [M] Next Map")
                : (std::string("CPU: Auto  [C] Difficulty (") + cpuDifficultyName(state) + ")   [M] Next Map");
        } else {
            line_settings = "[M] Next Map";
        }
        const std::string line_status = compact
            ? ("> " + std::to_string(state.match_duration_sec) + "s | " + currentDemoMapLabel())
            : (">  " + std::to_string(state.match_duration_sec) + "s   |   Map: " + currentDemoMapLabel());

        const float kSetupYOffset = 0.12f;
        const float kPanelTextX = compact ? 0.78f : 0.92f;
        const float kHeaderY    = 0.85f + kSetupYOffset;   // τίτλος «SETUP» πιο ψηλά
        const float kLineY0     = (compact ? 0.98f : 1.28f) + kSetupYOffset;   // πρώτη γραμμή περιεχομένου
        const float kLineDY     = 0.62f;   // σταθερό line-spacing που χωράει καθαρά 7 γραμμές
        const float kTextSize     = compact ? 0.58f : 0.72f;
        const float kTextSizeEmph = compact ? 0.66f : 0.80f;

        // Panel: υπολογίζουμε πλάτος από το μακρύτερο κείμενο.
        {
            float max_text_w = 0.0f;
            max_text_w = std::max(max_text_w, approxTextHalfWidth("SETUP", 0.92f) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_duration, kTextSize) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_actions,  kTextSize) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_players1, kTextSize) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_players2, kTextSize) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_editor,   kTextSize) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_settings, kTextSize) * 2.0f);
            max_text_w = std::max(max_text_w, approxTextHalfWidth(line_status,   kTextSizeEmph) * 2.0f);

            const float padding  = 1.30f;
            float panel_w = max_text_w + padding;
            panel_w = std::min(panel_w, std::max(compact ? 20.0f : 16.0f, state.map.width() * (compact ? 0.72f : 0.58f)));
            panel_w = std::min(panel_w, state.map.width() - 1.0f);
            // Height κοντά στο πραγματικό περιεχόμενο ώστε το περίγραμμα να μη μένει μισοάδειο.
            const float panel_h  = compact ? 4.95f : 5.15f;
            const float panel_left = compact ? 0.35f : 0.55f;
            const float panel_cx = panel_left + panel_w * 0.5f;
            const float panel_top = 0.08f;
            const float panel_cy = panel_top + panel_h * 0.5f;

            graphics::Brush panel;
            panel.fill_color[0] = 0.0f;
            panel.fill_color[1] = 0.0f;
            panel.fill_color[2] = 0.0f;
            panel.fill_opacity = 0.45f;
            panel.outline_color[0] = 0.95f;
            panel.outline_color[1] = 0.80f;
            panel.outline_color[2] = 0.30f;
            panel.outline_opacity = 0.85f;
            panel.outline_width = 5.0f;
            graphics::drawRect(panel_cx, panel_cy, panel_w, panel_h, panel);

            // Accent bar στα αριστερά.
            graphics::Brush bar;
            bar.fill_color[0] = 1.0f;
            bar.fill_color[1] = 0.85f;
            bar.fill_color[2] = 0.20f;
            bar.fill_opacity = 0.90f;
            bar.outline_opacity = 0.0f;
            graphics::drawRect(panel_left + 0.10f, panel_cy, 0.20f, panel_h - 0.40f, bar);

            graphics::Brush sep;
            sep.fill_color[0] = 1.0f;
            sep.fill_color[1] = 0.85f;
            sep.fill_color[2] = 0.20f;
            sep.fill_opacity = 0.45f;
            sep.outline_opacity = 0.0f;
            graphics::drawRect(panel_left + panel_w * 0.5f, panel_top + 0.95f, panel_w - 0.45f, 0.04f, sep);
        }

        graphics::Brush accent;
        accent.fill_color[0] = 1.0f;
        accent.fill_color[1] = 0.85f;
        accent.fill_color[2] = 0.20f;
        accent.fill_opacity = 1.0f;
        accent.outline_opacity = 0.0f;

        graphics::Brush shadow;
        shadow.fill_color[0] = 0.0f;
        shadow.fill_color[1] = 0.0f;
        shadow.fill_color[2] = 0.0f;
        shadow.fill_opacity = 0.75f;
        shadow.outline_opacity = 0.0f;

        graphics::Brush ui_text;
        ui_text.fill_color[0] = 0.95f;
        ui_text.fill_color[1] = 0.95f;
        ui_text.fill_color[2] = 0.97f;
        ui_text.fill_opacity = 1.0f;
        ui_text.outline_opacity = 0.0f;

        // Τίτλος "GRID WORLD" κεντραρισμένος στη HUD ζώνη.
        if (!g_font_display.empty()) {
            graphics::setFont(g_font_display);
        }
        const std::string title = "GRID WORLD";
        const float title_size = compact ? 0.82f : 0.98f;
        graphics::drawText(std::max(0.6f, cx - approxTextHalfWidth(title, title_size)), 2.45f, title_size, title, text);
        if (!g_font_ui.empty()) {
            graphics::setFont(g_font_ui);
        }

        // Header «SETUP» μέσα στο panel.
        {
            if (!g_font_display.empty()) {
                graphics::setFont(g_font_display);
            }
            drawTextShadowed(kPanelTextX, kHeaderY, compact ? 0.80f : 0.92f, "SETUP", accent, shadow, 0.06f, 0.06f);
            if (!g_font_ui.empty()) {
                graphics::setFont(g_font_ui);
            }
        }

        // 7 γραμμές setup με σαφή θέματα.
        drawKeyAccentLine(kPanelTextX, kLineY0 + kLineDY * 0.0f, kTextSize,     line_duration, ui_text, accent, shadow);
        drawKeyAccentLine(kPanelTextX, kLineY0 + kLineDY * 1.0f, kTextSize,     line_actions,  ui_text, accent, shadow);
        drawKeyAccentLine(kPanelTextX, kLineY0 + kLineDY * 2.0f, kTextSize,     line_players1, ui_text, accent, shadow);
        drawKeyAccentLine(kPanelTextX, kLineY0 + kLineDY * 3.0f, kTextSize,     line_players2, ui_text, accent, shadow);
        drawKeyAccentLine(kPanelTextX, kLineY0 + kLineDY * 4.0f, kTextSize,     line_editor,   ui_text, accent, shadow);
        drawKeyAccentLine(kPanelTextX, kLineY0 + kLineDY * 5.0f, kTextSize,     line_settings, ui_text, accent, shadow);
        drawTextShadowed (kPanelTextX, kLineY0 + kLineDY * 6.0f, kTextSizeEmph, line_status,   ui_text, shadow);
        return;
    }

    // Banner «λήξης match».
    if (state.match_over) {
        // Εύρεση νικητή(ών) βάσει σκορ.
        int best_score = -1;
        std::vector<int> best_ids;
        for (const auto& e : state.entities) {
            const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
            if (!ae) continue;
            const int s = ae->score();
            if (s > best_score) {
                best_score = s;
                best_ids.clear();
                best_ids.push_back(ae->id());
            } else if (s == best_score) {
                best_ids.push_back(ae->id());
            }
        }

        if (!g_font_display.empty()) {
            graphics::setFont(g_font_display);
        }

        std::string winner;
        if (best_ids.empty()) {
            winner = "TIME UP";
        } else if (best_ids.size() == 1) {
            winner = "WINNER A" + std::to_string(best_ids.front()) + " (" + std::to_string(best_score) + ")";
        } else {
            winner = "TIE";
        }

        // Banner (κεντράρισμα με heuristic).
        const float cx = state.map.width() * 0.5f;
        graphics::drawText(std::max(0.6f, cx - approxTextHalfWidth(winner, 1.15f)), 1.85f, 1.15f, winner, text);

        if (!g_font_ui.empty()) {
            graphics::setFont(g_font_ui);
        }

        if (best_ids.size() > 1) {
            std::string ids = "A" + std::to_string(best_ids[0]);
            for (size_t i = 1; i < best_ids.size(); ++i) {
                ids += " & A" + std::to_string(best_ids[i]);
            }
            ids += " (" + std::to_string(best_score) + ")";
            graphics::drawText(std::max(0.6f, cx - approxTextHalfWidth(ids, kFontMain)), 0.75f, kFontMain, ids, text);
        }

        {
            const std::string restart = "Press R to restart";
            graphics::drawText(std::max(0.6f, cx - approxTextHalfWidth(restart, kFontSub)), 3.15f, kFontSub, restart, text);
        }

        return;
    }

    // HUD layout constants για ομοιόμορφη απόσταση/στοίχιση.
    constexpr float kHudLine1Y = 0.95f;
    constexpr float kHudLine2Y = 1.70f;
    constexpr float kHudLine25Y = 2.38f;
    constexpr float kHudLine3Y = 3.08f;
    constexpr float kHudLine4Y = 3.72f;

    constexpr float kHudPanelTop = 0.35f;
    constexpr float kHudPanelHeight = 3.8f;
    constexpr float kHudPanelSideMargin = 0.45f;
    constexpr float kHudPanelGap = 0.45f;
    constexpr float kScorePanelWidth = 6.0f;
    const float hud_width = static_cast<float>(state.map.width());
    constexpr float kScorePanelOffsetX = 0.12f;
    constexpr float kScorePanelOffsetY = 0.10f;
    const float score_panel_left_ref = std::max(kHudPanelSideMargin, hud_width - kScorePanelWidth - kHudPanelSideMargin + kScorePanelOffsetX);
    const float score_panel_center_x = score_panel_left_ref + kScorePanelWidth * 0.5f;
    const float shared_panel_center_y = kHudPanelTop + kHudPanelHeight * 0.5f;
    const float score_panel_center_y = shared_panel_center_y + kScorePanelOffsetY;

    auto makeHudPanelBrush = []() {
        graphics::Brush panel;
        panel.fill_color[0] = 0.0f;
        panel.fill_color[1] = 0.0f;
        panel.fill_color[2] = 0.0f;
        panel.fill_opacity = 0.42f;
        panel.outline_opacity = 0.85f;
        panel.outline_width = 5.0f;
        panel.outline_color[0] = 0.95f;
        panel.outline_color[1] = 0.80f;
        panel.outline_color[2] = 0.30f;
        return panel;
    };

    // Info panel (αριστερά), οπτικά ευθυγραμμισμένο με score panel.
    {
        const float panel_left = kHudPanelSideMargin;
        const float panel_right = std::max(panel_left + 10.0f, score_panel_left_ref - kHudPanelGap);
        const float panel_w = panel_right - panel_left;
        graphics::Brush info_panel = makeHudPanelBrush();
        graphics::drawRect(panel_left + panel_w * 0.5f, shared_panel_center_y, panel_w, kHudPanelHeight, info_panel);
    }

    const float hud_text_x = 0.62f;
    graphics::Brush hud_shadow;
    hud_shadow.fill_color[0] = 0.0f;
    hud_shadow.fill_color[1] = 0.0f;
    hud_shadow.fill_color[2] = 0.0f;
    hud_shadow.fill_opacity = 0.78f;
    hud_shadow.outline_opacity = 0.0f;
    const float hud_text_max_w = std::max(9.5f, score_panel_left_ref - hud_text_x - kHudPanelGap);

    auto fitToHudWidth = [&](const std::string& s, float size) -> std::string {
        if (approxTextHalfWidth(s, size) * 2.0f <= hud_text_max_w) return s;
        std::string out = s;
        while (out.size() > 8) {
            out.pop_back();
            const std::string cand = out + "...";
            if (approxTextHalfWidth(cand, size) * 2.0f <= hud_text_max_w) {
                return cand;
            }
        }
        return "...";
    };

    // Γραμμή 1: κατάσταση + tick + targets + χρόνος.
    {
        std::string line1 = (state.paused ? "PAUSE" : "RUN");
        line1 += " | " + std::to_string(state.tick_delay_ms) + "ms";
        line1 += " | FPS " + std::to_string(static_cast<int>(std::round(state.hud_fps)));
        line1 += " | Time " + std::to_string(static_cast<int>(state.sim_elapsed_ms / 1000.0f)) + "s";
        if (state.targets_total > 0) {
            line1 += " | Targets " + std::to_string(state.targets_collected);
        }
        if (state.cpu_agent_id >= 0) {
            line1 += std::string(" | CPU ") + cpuDifficultyName(state);
        }
        line1 += std::string(" | REC ") + (g_recording ? "ON" : "OFF");
        if (g_recording || g_record_frame > 0) {
            line1 += " #" + std::to_string(g_record_frame);
        }
        const int secs_left = static_cast<int>(std::ceil(state.match_time_left_ms / 1000.0f));
        line1 += " | " + formatTimeMMSS(secs_left);
        drawTextShadowed(hud_text_x, kHudLine1Y, kFontMain, fitToHudWidth(line1, kFontMain), text, hud_shadow);
    }

    // Γραμμή 2: επιλογή + autopilot + A* analytics.
    {
        std::string line2;
        if (state.selected_agent_id >= 0) {
            const bool ap = (state.autopilot_agent_id == state.selected_agent_id);
            line2 = "Sel A" + std::to_string(state.selected_agent_id);
            line2 += std::string(" | AP ") + (ap ? "ON" : "OFF");
        } else {
            line2 = "Sel none";
        }

        line2 += " | A*: N=" + std::to_string(state.hud_last_expanded);
        line2 += " P=" + std::to_string(state.hud_last_path_len);

        if (state.hud_last_search_ms > 0.1f) {
            const float nodes_per_ms = static_cast<float>(state.hud_last_expanded) / state.hud_last_search_ms;
            line2 += " Perf=" + std::to_string(static_cast<int>(nodes_per_ms)) + "n/ms";
        }

        line2 += " t=" + std::to_string(static_cast<int>(std::round(state.hud_last_search_ms))) + "ms";
        if (state.hud_search_calls > 0) {
            line2 += " Calls=" + std::to_string(state.hud_search_calls);
        }

        line2 += " | Map " + currentDemoMapLabel();
        drawTextShadowed(hud_text_x, kHudLine2Y, kFontSub + 0.03f, fitToHudWidth(line2, kFontSub + 0.03f), text, hud_shadow);
    }

    // Γραμμή 2.5: CPU AI difficulty + performance tracking
    if (state.cpu_agent_id >= 0) {
        std::string cpu_line = "CPU AI: ";
        cpu_line += std::string(cpuDifficultyName(state));
        cpu_line += " | Response: ";
        
        const float throttle_ms = cpuStepThrottleMs(state);
        if (throttle_ms < 200.0f) {
            cpu_line += "FAST";
        } else if (throttle_ms > 300.0f) {
            cpu_line += "SLOW";
        } else {
            cpu_line += "BALANCED";
        }
        
        cpu_line += " | Probe=" + std::to_string(cpuProbeLimit(state));

        drawTextShadowed(hud_text_x, kHudLine25Y, kFontSub + 0.03f, fitToHudWidth(cpu_line, kFontSub + 0.03f), text, hud_shadow);
    }

    // Γραμμή 3+4: οδηγός κουμπιών gameplay στη μπάρα HUD (σπάει σε 2 γραμμές για αποφυγή overlap).
    {
        std::string line3a = "[SPACE] Pause/Run | [N] Step 1 tick | [-]/[+] Speed | [F5/V] Rec";
        std::string line3b = "[P] AP | [R] Restart | [M] Next map";
        line3b += (state.cpu_agent_id >= 0) ? " | [C] CPU" : "";
        line3b += " | [Q] Quit";

        graphics::Brush accent = text;
        accent.fill_color[0] = 1.0f;
        accent.fill_color[1] = 0.85f;
        accent.fill_color[2] = 0.20f;

        graphics::Brush shadow;
        shadow.fill_color[0] = 0.0f;
        shadow.fill_color[1] = 0.0f;
        shadow.fill_color[2] = 0.0f;
        shadow.fill_opacity = 0.75f;
        shadow.outline_opacity = 0.0f;

        drawKeyAccentLine(hud_text_x, kHudLine3Y, 0.62f, fitToHudWidth(line3a, 0.62f), text, accent, shadow);
        drawKeyAccentLine(hud_text_x, kHudLine4Y, 0.62f, fitToHudWidth(line3b, 0.62f), text, accent, shadow);
    }

    if (g_record_notice_ms > 0.0f && !g_record_notice.empty()) {
        graphics::Brush rec_text = text;
        rec_text.fill_color[0] = 1.0f;
        rec_text.fill_color[1] = 0.93f;
        rec_text.fill_color[2] = 0.35f;
        drawTextShadowed(hud_text_x, 4.42f, 0.52f, fitToHudWidth(g_record_notice, 0.52f), rec_text, hud_shadow);
    }

    // Το scoreboard «ζει» στη λωρίδα HUD (εκτός του grid).
    {
        std::vector<const AgentEntity*> agents;
        agents.reserve(state.entities.size());
        for (const auto& e : state.entities) {
            const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
            if (!ae) continue;
            agents.push_back(ae);
        }
        std::sort(agents.begin(), agents.end(), [](const AgentEntity* a, const AgentEntity* b) { return a->id() < b->id(); });

        const float panel_w = kScorePanelWidth;
        const float panel_left = score_panel_left_ref;
        const float text_left = panel_left + 0.28f;
        const float score_panel_top = score_panel_center_y - kHudPanelHeight * 0.5f;
        const float title_y = score_panel_top + 0.50f;
        const float rows_top = score_panel_top + 1.42f;
        const float row_h = 0.62f;

        // Panel αντίθεσης πίσω από το scoreboard.
        graphics::Brush panel = makeHudPanelBrush();
        graphics::drawRect(score_panel_center_x, score_panel_center_y, panel_w, kHudPanelHeight, panel);

        graphics::Brush header = text;
        header.fill_opacity = 0.95f;
        if (!g_font_display.empty()) {
            graphics::setFont(g_font_display);
        }
        constexpr float kScoreTitleOffsetX = 0.80f;
        constexpr float kScoreTitleOffsetY = 0.32f;
        graphics::drawText(text_left + kScoreTitleOffsetX, title_y + kScoreTitleOffsetY, kFontScoreHeader, "SCORE", header);
        if (!g_font_ui.empty()) {
            graphics::setFont(g_font_ui);
        }

        // Διαχωριστική γραμμή κάτω από τον τίτλο.
        graphics::Brush sep;
        sep.fill_color[0] = 1.0f;
        sep.fill_color[1] = 0.85f;
        sep.fill_color[2] = 0.20f;
        sep.fill_opacity = 0.55f;
        sep.outline_opacity = 0.0f;
        graphics::drawRect(panel_left + panel_w * 0.5f, score_panel_top + 1.06f, panel_w - 0.45f, 0.045f, sep);

        for (size_t i = 0; i < agents.size(); ++i) {
            const auto* ae = agents[i];
            float rgb[3];
            if (isPacmanThemeActive()) {
                pacmanAgentColor(ae->id(), rgb);
            } else {
                agentColor(ae->id(), rgb);
            }

            // Απαλό row background για ομοιόμορφη σάρωση γραμμών.
            graphics::Brush row_bg;
            row_bg.fill_color[0] = 1.0f;
            row_bg.fill_color[1] = 1.0f;
            row_bg.fill_color[2] = 1.0f;
            row_bg.fill_opacity = (i % 2 == 0) ? 0.05f : 0.02f;
            row_bg.outline_opacity = 0.0f;
            const float row_cy = rows_top + static_cast<float>(i) * row_h;
            graphics::drawRect(panel_left + panel_w * 0.5f, row_cy, panel_w - 0.40f, 0.50f, row_bg);

            graphics::Brush line = text;
            line.fill_color[0] = rgb[0];
            line.fill_color[1] = rgb[1];
            line.fill_color[2] = rgb[2];
            const bool is_sel = (state.selected_agent_id == ae->id());
            const std::string s = std::string(is_sel ? "> " : "  ") +
                                  "A" + std::to_string(ae->id()) + ": " + std::to_string(ae->score());
            graphics::drawText(text_left, row_cy + 0.14f, kFontSub, s, line);
        }
    }
}

grid::Point nearestFreeCell(const grid::Map& map, grid::Point desired) {
    const int w = map.width();
    const int h = map.height();
    if (w <= 0 || h <= 0) return {0, 0};

    // Οι configs μπορεί να έχουν συντεταγμένες εκτός ορίων για μικρότερους χάρτες.
    desired.x = std::clamp(desired.x, 0, w - 1);
    desired.y = std::clamp(desired.y, 0, h - 1);

    if (map.isFree(desired)) return desired;

    const int max_r = std::max(w, h);
    for (int r = 1; r <= max_r; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            const int y = desired.y + dy;
            const int rem = r - std::abs(dy);
            const int xs[2] = { desired.x - rem, desired.x + rem };
            for (int i = 0; i < 2; ++i) {
                const int x = xs[i];
                const grid::Point p{x, y};
                if (map.isFree(p)) return p;
            }
        }
    }

    // Fallback: αν η ακτινική αναζήτηση δεν βρει, σκάναρε όλο το grid.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const grid::Point p{x, y};
            if (map.isFree(p)) return p;
        }
    }

    return {0, 0};
}

grid::Point nearestFreeCellAvoiding(const grid::Map& map, grid::Point desired, const std::unordered_set<int>& banned_keys) {
    const int w = map.width();
    const int h = map.height();
    if (w <= 0 || h <= 0) return {0, 0};

    auto key = [&](const grid::Point& p) { return p.y * w + p.x; };

    desired.x = std::clamp(desired.x, 0, w - 1);
    desired.y = std::clamp(desired.y, 0, h - 1);
    if (map.isFree(desired) && !banned_keys.count(key(desired))) return desired;

    const int max_r = std::max(w, h);
    for (int r = 1; r <= max_r; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            const int y = desired.y + dy;
            const int rem = r - std::abs(dy);
            const int xs[2] = { desired.x - rem, desired.x + rem };
            for (int i = 0; i < 2; ++i) {
                const grid::Point p{xs[i], y};
                if (p.x < 0 || p.y < 0 || p.x >= w || p.y >= h) continue;
                if (!map.isFree(p)) continue;
                if (banned_keys.count(key(p))) continue;
                return p;
            }
        }
    }

    return nearestFreeCell(map, desired);
}

bool hasPathBetween(const grid::Map& map, const grid::Point& a, const grid::Point& b) {
    if (!map.isFree(a) || !map.isFree(b)) return false;
    const auto p = grid::findPath(map, a, b);
    return p.has_value() && !p->empty();
}

grid::Point nearestReachableGoal(const grid::Map& map, const grid::Point& start, grid::Point desired) {
    desired = nearestFreeCell(map, desired);
    if (hasPathBetween(map, start, desired)) return desired;

    const int w = map.width();
    const int h = map.height();
    const int max_r = std::max(w, h);
    for (int r = 1; r <= max_r; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            const int y = desired.y + dy;
            const int rem = r - std::abs(dy);
            const int xs[2] = { desired.x - rem, desired.x + rem };
            for (int i = 0; i < 2; ++i) {
                const grid::Point p{xs[i], y};
                if (p.x < 0 || p.y < 0 || p.x >= w || p.y >= h) continue;
                if (!map.isFree(p)) continue;
                if (hasPathBetween(map, start, p)) return p;
            }
        }
    }

    // Εγγυημένα reachable fallback: το start cell.
    return start;
}

bool loadAgentsConfig(const std::string& cfgPath, const grid::Map& map, std::vector<std::unique_ptr<grid::Entity>>& out_entities) {
    std::ifstream in(cfgPath);
    if (!in) return false;

    int id, sx, sy, gx, gy;
    out_entities.clear();
    std::unordered_set<int> used_starts;
    auto key = [&](const grid::Point& p) { return p.y * map.width() + p.x; };

    while (in >> id >> sx >> sy >> gx >> gy) {
        grid::Point start{sx, sy};
        grid::Point goal{gx, gy};
        start = nearestFreeCellAvoiding(map, start, used_starts);
        used_starts.insert(key(start));
        goal = nearestReachableGoal(map, start, goal);

        grid::Agent agent{id, start, goal};
        out_entities.push_back(std::make_unique<AgentEntity>(std::move(agent)));
    }
    return true;
}

std::unordered_set<int> computeReachableCellsFromAgents(const grid::GlobalState& state);
bool cellOccupiedByAgent(const grid::GlobalState& state, const grid::Point& cell);

void collectTargets(grid::GlobalState& state) {
    if (state.targets_total <= 0) return;

    auto key = [&](const grid::Point& p) -> int { return p.y * state.map.width() + p.x; };

    std::unordered_map<int, AgentEntity*> agent_at;
    agent_at.reserve(state.entities.size());
    for (auto& e : state.entities) {
        auto* ae = dynamic_cast<AgentEntity*>(e.get());
        if (!ae) continue;
        agent_at[key(ae->position())] = ae;
    }

    const std::string sound = resolveCollectSoundPath();

    int removed = 0;

    for (auto it = state.entities.begin(); it != state.entities.end();) {
        const auto* te = dynamic_cast<const TargetEntity*>((*it).get());
        if (!te) {
            ++it;
            continue;
        }

        const int tk = key(te->cell());
        auto it_agent = agent_at.find(tk);
        if (it_agent != agent_at.end()) {
            state.targets_collected += 1;
            it_agent->second->addScore(1);
            if (!sound.empty()) {
                graphics::playSound(sound, 0.35f);
            }
            it = state.entities.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    // Κάνουμε respawn στα targets ώστε το match να μένει «ζωντανό» (το autopilot δεν «μένει από στόχους»).
    if (removed > 0) {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dx(0, std::max(0, state.map.width() - 1));
        std::uniform_int_distribution<int> dy(0, std::max(0, state.map.height() - 1));
        const std::unordered_set<int> reachable = computeReachableCellsFromAgents(state);

        std::unordered_set<int> used;
        used.reserve(state.entities.size() + static_cast<size_t>(removed));
        for (const auto& e : state.entities) {
            if (const auto* ae = dynamic_cast<const AgentEntity*>(e.get())) {
                used.insert(key(ae->position()));
            } else if (const auto* te2 = dynamic_cast<const TargetEntity*>(e.get())) {
                used.insert(key(te2->cell()));
            }
        }

        std::vector<std::unique_ptr<grid::Entity>> targets;
        targets.reserve(static_cast<size_t>(removed));

        int attempts = 0;
        const int max_attempts = std::max(2000, removed * 250);
        while (static_cast<int>(targets.size()) < removed && attempts < max_attempts) {
            ++attempts;
            grid::Point p{dx(rng), dy(rng)};
            if (!state.map.isFree(p)) continue;
            const int k = key(p);
            if (!reachable.empty() && !reachable.count(k)) continue;
            if (cellOccupiedByAgent(state, p)) continue;
            if (used.count(k)) continue;
            used.insert(k);
            targets.push_back(std::make_unique<TargetEntity>(p));
        }

        state.entities.insert(state.entities.begin(),
                              std::make_move_iterator(targets.begin()),
                              std::make_move_iterator(targets.end()));
    }
}

bool cellOccupiedByAgent(const grid::GlobalState& state, const grid::Point& cell) {
    for (const auto& e : state.entities) {
        const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
        if (!ae) continue;
        if (ae->position() == cell) return true;
    }
    return false;
}

std::unordered_set<int> computeReachableCellsFromAgents(const grid::GlobalState& state) {
    std::unordered_set<int> reachable;
    const int w = state.map.width();
    const int h = state.map.height();
    if (w <= 0 || h <= 0) return reachable;

    auto key = [&](const grid::Point& p) -> int { return p.y * w + p.x; };
    std::vector<grid::Point> queue;
    queue.reserve(static_cast<size_t>(w * h));

    for (const auto& e : state.entities) {
        const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
        if (!ae) continue;
        const grid::Point s = ae->position();
        if (s.x < 0 || s.y < 0 || s.x >= w || s.y >= h) continue;
        if (!state.map.isFree(s)) continue;
        if (reachable.insert(key(s)).second) {
            queue.push_back(s);
        }
    }

    for (size_t i = 0; i < queue.size(); ++i) {
        const auto p = queue[i];
        for (const auto& n : state.map.neighbors(p)) {
            const int k = key(n);
            if (reachable.insert(k).second) {
                queue.push_back(n);
            }
        }
    }

    return reachable;
}

void spawnInitialTargets(grid::GlobalState& state, int count) {
    // Αφαιρούμε τυχόν υπάρχοντα targets πριν κάνουμε spawn καινούργια (χρήσιμο όταν αλλάζει η διάρκεια του match).
    for (auto it = state.entities.begin(); it != state.entities.end();) {
        if (dynamic_cast<TargetEntity*>((*it).get())) {
            it = state.entities.erase(it);
        } else {
            ++it;
        }
    }

    state.targets_total = 0;
    state.targets_collected = 0;
    if (count <= 0) return;

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dx(0, std::max(0, state.map.width() - 1));
    std::uniform_int_distribution<int> dy(0, std::max(0, state.map.height() - 1));
    const std::unordered_set<int> reachable = computeReachableCellsFromAgents(state);

    auto key = [&](const grid::Point& p) -> int { return p.y * state.map.width() + p.x; };
    std::unordered_set<int> used;

    std::vector<std::unique_ptr<grid::Entity>> targets;
    targets.reserve(static_cast<size_t>(count));

    int attempts = 0;
    const int max_attempts = std::max(5000, count * 250);
    while (static_cast<int>(targets.size()) < count && attempts < max_attempts) {
        ++attempts;
        grid::Point p{dx(rng), dy(rng)};
        if (!state.map.isFree(p)) continue;
        const int k = key(p);
        if (!reachable.empty() && !reachable.count(k)) continue;
        if (cellOccupiedByAgent(state, p)) continue;
        if (used.count(k)) continue;
        used.insert(k);
        targets.push_back(std::make_unique<TargetEntity>(p));
    }

    state.targets_total = static_cast<int>(targets.size());

    // Ζωγραφίζουμε targets «κάτω» από τους agents βάζοντάς τα στην αρχή του vector.
    state.entities.insert(state.entities.begin(),
                          std::make_move_iterator(targets.begin()),
                          std::make_move_iterator(targets.end()));
}

int computeTargetPoolSize(const grid::GlobalState& state) {
    const int area = state.map.width() * state.map.height();
    const int base = std::max(1, area / 45);
    const int duration_factor = std::clamp(state.match_duration_sec / 60, 1, 5);
    return std::clamp(base * duration_factor, 20, 180);
}

void restartToSetup(grid::GlobalState& state) {
    state.match_started = false;
    state.match_over = false;
    state.paused = true;
    state.step_once = false;
    state.step_skip_cpu = false;
    state.accumulator_ms = 0.0f;
    state.sim_elapsed_ms = 0.0f;
    state.hud_last_expanded = 0;
    state.hud_last_path_len = 0;
    state.hud_last_search_ms = 0.0f;
    state.hud_search_calls = 0;
    state.match_time_left_ms = static_cast<float>(state.match_duration_sec) * 1000.0f;
    state.targets_collected = 0;

    // Reset στα σκορ των agents + σταμάτημα της κίνησής τους.
    for (auto& e : state.entities) {
        if (auto* ae = dynamic_cast<AgentEntity*>(e.get())) {
            ae->resetScore();
            ae->resetForRestart();
        }
    }

    // Κρατάμε τον CPU agent σε autopilot.
    state.autopilot_agent_id = state.cpu_agent_id;

    // Respawn στο pool των targets ώστε να ταιριάζει με την επιλεγμένη διάρκεια.
    spawnInitialTargets(state, computeTargetPoolSize(state));
}

void draw_callback() {
    const auto* state = static_cast<const grid::GlobalState*>(graphics::getUserData());
    if (!state) return;

    drawWorld(*state);
    state->draw();
    drawHud(*state);

    if (g_recording && !captureLiveVideoFrame()) {
        stopLiveRecording();
    }
}

void update_callback(float ms) {
    auto* state = static_cast<grid::GlobalState*>(graphics::getUserData());
    if (!state) return;

    // Smoothed FPS estimate για HUD.
    {
        static float fps_smoothed = 0.0f;
        if (ms > 0.001f) {
            const float inst = 1000.0f / ms;
            if (fps_smoothed <= 0.0f) fps_smoothed = inst;
            else fps_smoothed = fps_smoothed * 0.90f + inst * 0.10f;
            state->hud_fps = fps_smoothed;
        }
    }

    if (g_record_notice_ms > 0.0f) {
        g_record_notice_ms = std::max(0.0f, g_record_notice_ms - ms);
        if (g_record_notice_ms <= 0.0f) {
            g_record_notice.clear();
        }
    }

    // Ανίχνευση «ακμής» πλήκτρων ( για toggles.
    static bool prev_space = false;
    static bool prev_n = false;
    static bool prev_p = false;
    static bool prev_enter = false;
    static bool prev_1 = false;
    static bool prev_2 = false;
    static bool prev_3 = false;
    static bool prev_kp1 = false;
    static bool prev_kp2 = false;
    static bool prev_kp3 = false;
    static bool prev_r = false;
    static bool prev_c = false;
    static bool prev_m = false;
    static bool prev_minus = false;
    static bool prev_plus = false;
    static bool prev_f5 = false;
    static bool prev_v = false;

    // Κατάσταση ήχων για countdown / τέλος / νικητή.
    static bool played_countdown[4] = {false, false, false, false}; // indices 1..3
    static bool played_end_sound = false;
    static bool played_winner_sound = false;
    static bool prev_match_started = false;

    // Scheduler για ακολουθίες beep (ώστε να φτιάχνουμε διαφορετικά patterns με ένα μόνο sample).
    static int beep_remaining = 0;
    static float beep_timer_ms = 0.0f;
    static float beep_gap_ms = 0.0f;
    static float beep_volume = 1.0f;

    static bool winner_pending = false;
    static float winner_delay_ms = 0.0f;

    // Καθαρό quit, για να μην μείνει η διεργασία να τρέχει 
    if (graphics::getKeyState(graphics::SCANCODE_ESCAPE) || graphics::getKeyState(graphics::SCANCODE_Q)) {
        if (g_recording) {
            stopLiveRecording(false);
        }
        graphics::stopMessageLoop();
        return;
    }

    // Γρήγορο restart 
    const bool cur_r = graphics::getKeyState(graphics::SCANCODE_R);
    if (cur_r && !prev_r) {
        restartToSetup(*state);

        // Reset one-shot sound state.
        played_countdown[1] = played_countdown[2] = played_countdown[3] = false;
        played_end_sound = false;
        played_winner_sound = false;
        prev_match_started = false;
        beep_remaining = 0;
        beep_timer_ms = 0.0f;
        winner_pending = false;
        winner_delay_ms = 0.0f;
        logAudioEvent("[restart] reset sound flags");

        prev_r = cur_r;
        return;
    }

    // Μικρή αλληλεπίδραση (απαίτηση: να υπάρχει έστω κάτι από input)
    const bool cur_space = graphics::getKeyState(graphics::SCANCODE_SPACE);
    if (cur_space && !prev_space) {
        state->paused = !state->paused;
    }
    const bool cur_n = graphics::getKeyState(graphics::SCANCODE_N);
    if (cur_n && !prev_n) {
        state->step_once = true;
        state->step_skip_cpu = true;  // παίκτες κινούνται, CPU agent παραλείπεται
    }

    // Cycle δυσκολίας CPU (έχει νόημα μόνο αν υπάρχει CPU agent).
    const bool cur_c = graphics::getKeyState(graphics::SCANCODE_C);
    if (cur_c && !prev_c && state->cpu_agent_id >= 0) {
        cycleCpuDifficulty(*state);
    }

    // Autopilot toggle με 'P' (για να μην μπλέκει με το WASD 'A').
    // Αν υπάρχει CPU agent, το autopilot είναι δεσμευμένο γι' αυτόν.
    const bool cur_p = graphics::getKeyState(graphics::SCANCODE_P);
    if (cur_p && !prev_p && state->selected_agent_id >= 0 && state->cpu_agent_id < 0) {
        if (state->autopilot_agent_id == state->selected_agent_id) {
            state->autopilot_agent_id = -1;
        } else {
            state->autopilot_agent_id = state->selected_agent_id;
        }
    }
    const bool cur_minus = graphics::getKeyState(graphics::SCANCODE_MINUS) || graphics::getKeyState(graphics::SCANCODE_KP_MINUS);
    const bool cur_plus = graphics::getKeyState(graphics::SCANCODE_EQUALS) || graphics::getKeyState(graphics::SCANCODE_KP_PLUS);
    const bool cur_f5 = graphics::getKeyState(graphics::SCANCODE_F5);
    // Fallback key for laptops/OS setups where function keys are intercepted.
    const bool cur_v = graphics::getKeyState(graphics::SCANCODE_V);
    if (cur_minus && !prev_minus) {
        state->tick_delay_ms = std::min(2000, state->tick_delay_ms + 50);
    }
    if (cur_plus && !prev_plus) {
        state->tick_delay_ms = std::max(10, state->tick_delay_ms - 50);
    }
    if ((cur_f5 || cur_v) && !(prev_f5 || prev_v)) {
        if (!g_recording) {
            (void)startLiveRecording();
        } else {
            stopLiveRecording();
        }
    }

    prev_space = cur_space;
    prev_n = cur_n;
    prev_p = cur_p;
    prev_r = cur_r;
    prev_c = cur_c;
    prev_minus = cur_minus;
    prev_plus = cur_plus;
    prev_f5 = cur_f5;
    prev_v = cur_v;

    // Setup / start του match.
    const bool cur_enter = graphics::getKeyState(graphics::SCANCODE_RETURN) || graphics::getKeyState(graphics::SCANCODE_RETURN2);
    const bool cur_1 = graphics::getKeyState(graphics::SCANCODE_1);
    const bool cur_2 = graphics::getKeyState(graphics::SCANCODE_2);
    const bool cur_3 = graphics::getKeyState(graphics::SCANCODE_3);
    const bool cur_kp1 = graphics::getKeyState(graphics::SCANCODE_KP_1);
    const bool cur_kp2 = graphics::getKeyState(graphics::SCANCODE_KP_2);
    const bool cur_kp3 = graphics::getKeyState(graphics::SCANCODE_KP_3);
    const bool cur_m = graphics::getKeyState(graphics::SCANCODE_M);

    if (!state->match_started) {
        bool changed = false;
        const bool press1 = (cur_1 && !prev_1) || (cur_kp1 && !prev_kp1);
        const bool press2 = (cur_2 && !prev_2) || (cur_kp2 && !prev_kp2);
        const bool press3 = (cur_3 && !prev_3) || (cur_kp3 && !prev_kp3);

        if (press1) {
            state->match_duration_sec = 60;
            changed = true;
        }
        if (press2) {
            state->match_duration_sec = 120;
            changed = true;
        }
        if (press3) {
            state->match_duration_sec = 180;
            changed = true;
        }

        if (cur_m && !prev_m && !g_demo_maps.empty()) {
            const int next_idx = (g_demo_map_index + 1) % static_cast<int>(g_demo_maps.size());
            const std::string next_map = g_demo_maps[static_cast<size_t>(next_idx)];

            grid::Map tmp_map;
            std::vector<std::unique_ptr<grid::Entity>> tmp_entities;
            if (tmp_map.loadFromFile(resolveMapPath(next_map)) && loadAgentsConfig(g_cfg_path, tmp_map, tmp_entities)) {
                state->map = std::move(tmp_map);
                state->entities = std::move(tmp_entities);
                g_demo_map_index = next_idx;

                std::vector<int> ids;
                ids.reserve(state->entities.size());
                for (const auto& e : state->entities) {
                    const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
                    if (!ae) continue;
                    ids.push_back(ae->id());
                }
                std::sort(ids.begin(), ids.end());
                state->player1_agent_id = (ids.size() >= 1) ? ids[0] : -1;
                state->player2_agent_id = (ids.size() >= 2) ? ids[1] : -1;
                state->cpu_agent_id = (ids.size() >= 3) ? ids[2] : -1;

                graphics::setCanvasSize(static_cast<float>(state->map.width()), static_cast<float>(state->map.height()) + kHudHeight);
                restartToSetup(*state);
                changed = false;
            }
        }

        state->match_time_left_ms = static_cast<float>(state->match_duration_sec) * 1000.0f;

        // Κλιμακώνουμε και τον αριθμό των targets ώστε να ταιριάζει με την επιλεγμένη διάρκεια.
        if (changed) {
            const int area = state->map.width() * state->map.height();
            const int base = std::max(1, area / 45);
            const int duration_factor = std::clamp(state->match_duration_sec / 60, 1, 5);
            const int target_count = std::clamp(base * duration_factor, 20, 180);
            spawnInitialTargets(*state, target_count);
        }

        // Δεν αφήνουμε το simulation να τρέχει 
        state->paused = true;

        // Map editor: στο setup mode κρατάμε click για paint/erase walls.
        {
            graphics::MouseState mouse_edit;
            graphics::getMouseState(mouse_edit);
            const bool left_down  = mouse_edit.button_left_pressed  || mouse_edit.button_left_down;
            const bool right_down = mouse_edit.button_right_pressed || mouse_edit.button_right_down;
            if ((left_down || right_down)) {
                const float mcx = graphics::windowToCanvasX(static_cast<float>(mouse_edit.cur_pos_x));
                const float mcy = graphics::windowToCanvasY(static_cast<float>(mouse_edit.cur_pos_y));
                if (mcy >= kHudHeight) {
                    const float world_y = mcy - kHudHeight;
                    const int cell_x = std::clamp(static_cast<int>(std::floor(mcx)), 0, state->map.width()  - 1);
                    const int cell_y = std::clamp(static_cast<int>(std::floor(world_y)), 0, state->map.height() - 1);
                    const grid::Point cell{cell_x, cell_y};
                    // Βεβαιωνόμαστε ότι δεν «χτίζουμε» πάνω σε agent.
                    bool occupied_by_agent = false;
                    for (const auto& e : state->entities) {
                        const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
                        if (ae && ae->position() == cell) { occupied_by_agent = true; break; }
                    }
                    if (!occupied_by_agent) {
                        state->map.setCell(cell, left_down ? 1 : 0);
                    }
                }
            }
        }

        if (cur_enter && !prev_enter) {
            state->match_started = true;
            state->paused = false;
        }

        prev_enter = cur_enter;
        prev_1 = cur_1;
        prev_2 = cur_2;
        prev_3 = cur_3;
        prev_kp1 = cur_kp1;
        prev_kp2 = cur_kp2;
        prev_kp3 = cur_kp3;
        prev_m = cur_m;
        return;
    }

   
    if (state->match_started && !prev_match_started) {
        played_countdown[1] = played_countdown[2] = played_countdown[3] = false;
        played_end_sound = false;
        played_winner_sound = false;
        beep_remaining = 0;
        beep_timer_ms = 0.0f;
        winner_pending = false;
        winner_delay_ms = 0.0f;
        logAudioEvent("[match] started");
    }
    prev_match_started = state->match_started;

    // Τρέχουμε τυχόν pending ακολουθίες beep / καθυστερημένο winner, ακόμα κι αν είμαστε paused.
    {
        if (winner_pending) {
            winner_delay_ms = std::max(0.0f, winner_delay_ms - ms);
            if (winner_delay_ms <= 0.0f) {
                // Μονο-μπιπ (single beep) για αποκάλυψη νικητή.
                beep_remaining = 1;
                beep_timer_ms = 0.0f;
                beep_gap_ms = 0.0f;
                beep_volume = 1.0f;
                winner_pending = false;
            }
        }

        if (beep_remaining > 0) {
            beep_timer_ms = std::max(0.0f, beep_timer_ms - ms);
            if (beep_timer_ms <= 0.0f) {
                if (!g_sfx_countdown.empty()) {
                    graphics::playSound(g_sfx_countdown, beep_volume);
                }
                --beep_remaining;
                beep_timer_ms = beep_gap_ms;
            }
        }
    }

    prev_enter = cur_enter;
    prev_1 = cur_1;
    prev_2 = cur_2;
    prev_3 = cur_3;
    prev_kp1 = cur_kp1;
    prev_kp2 = cur_kp2;
    prev_kp3 = cur_kp3;
    prev_m = cur_m;

    // Χρονόμετρο match — σταματά όταν το game είναι paused
    if (!state->match_over && !state->paused) {
        state->match_time_left_ms = std::max(0.0f, state->match_time_left_ms - ms);

        // Beeps για countdown 3-2-1 (μία φορά ανά δευτερόλεπτο)
        const int secs_left = static_cast<int>(std::ceil(state->match_time_left_ms / 1000.0f));
        if (secs_left >= 1 && secs_left <= 3 && !played_countdown[secs_left]) {
            played_countdown[secs_left] = true;

            
            // 3 -> τριπλό beep, 2 -> διπλό beep, 1 -> μονό beep
            beep_remaining = secs_left;
            beep_timer_ms = 0.0f;
            beep_gap_ms = 120.0f;
            beep_volume = 0.95f;
            logAudioEvent("[countdown] pattern x" + std::to_string(secs_left) + " sfx='" + g_sfx_countdown + "'");
        }

        if (state->match_time_left_ms <= 0.0f) {
            state->match_over = true;
            state->paused = true;
            state->step_once = false;

            if (!played_end_sound) {
                if (!g_sfx_end.empty()) {
                    // Διπλό beep ως σήμα τερματισμού.
                    beep_remaining = 2;
                    beep_timer_ms = 0.0f;
                    beep_gap_ms = 180.0f;
                    beep_volume = 1.0f;
                    logAudioEvent("[end] pattern x2 sfx='" + g_sfx_end + "'");
                } else {
                    logAudioEvent("[end] (missing sfx)");
                }
                played_end_sound = true;
            }
        }
    }

    // Αποκάλυψη νικητή στο τέλος του match.
    if (state->match_over) {
        if (!played_winner_sound) {
            if (!g_sfx_winner.empty()) {
                
                winner_pending = true;
                winner_delay_ms = 350.0f;
                logAudioEvent("[winner] scheduled after " + std::to_string(static_cast<int>(winner_delay_ms)) + "ms sfx='" + g_sfx_winner + "'");
            } else {
                logAudioEvent("[winner] (missing sfx)");
            }
            played_winner_sound = true;
        }

        // Σιγουρευόμαστε ότι το simulation παραμένει σταματημένο
        state->paused = true;
        state->step_once = false;
    }

   
    {
        int dx = 0, dy = 0;
        if (readDirWASD(dx, dy) && state->player1_agent_id >= 0) {
            if (auto* p1 = findAgentById(*state, state->player1_agent_id)) {
                const grid::Point next{p1->position().x + dx, p1->position().y + dy};
                if (state->map.isFree(next)) {
                    if (state->autopilot_agent_id == p1->id()) state->autopilot_agent_id = -1;
                    p1->setGoalPoint(next);
                }
            }
        }

        dx = 0; dy = 0;
        if (readDirArrows(dx, dy) && state->player2_agent_id >= 0) {
            if (auto* p2 = findAgentById(*state, state->player2_agent_id)) {
                const grid::Point next{p2->position().x + dx, p2->position().y + dy};
                if (state->map.isFree(next)) {
                    if (state->autopilot_agent_id == p2->id()) state->autopilot_agent_id = -1;
                    p2->setGoalPoint(next);
                }
            }
        }
    }

    // Κρατάμε τον CPU agent μόνιμα σε autopilot (αν έχει οριστεί).
    if (state->cpu_agent_id >= 0) {
        state->autopilot_agent_id = state->cpu_agent_id;
    }

    // Αλληλεπίδραση με mouse: κάνουμε spawn ένα εφέ μικρής διάρκειας στο click.
    graphics::MouseState mouse;
    graphics::getMouseState(mouse);
    if (mouse.button_left_pressed || mouse.button_right_pressed) {
        const float cx = graphics::windowToCanvasX(static_cast<float>(mouse.cur_pos_x));
        const float cy = graphics::windowToCanvasY(static_cast<float>(mouse.cur_pos_y));

        // Αγνοούμε clicks μέσα στη λωρίδα HUD.
        if (cy < kHudHeight) {
            return;
        }

        const float world_y = cy - kHudHeight;

       
        // Συντεταγμένες ακριβώς ίσες με width/height· κάνουμε clamp τους δείκτες σε έγκυρα κελιά.
        const int cell_x = std::clamp(static_cast<int>(std::floor(cx)), 0, std::max(0, state->map.width() - 1));
        const int cell_y = std::clamp(static_cast<int>(std::floor(world_y)), 0, std::max(0, state->map.height() - 1));
        const grid::Point cell{cell_x, cell_y};

        const std::string sound = resolveClickSoundPath();
        if (!sound.empty()) {
            graphics::playSound(sound, 0.5f);
        }

        // Επιλογή / έλεγχος goal.
        if (mouse.button_left_pressed) {
            // «Ανεκτική» επιλογή: διαλέγουμε τον πιο κοντινό agent αν το click είναι κοντά στο ζωγραφισμένο disk.
            int found_id = -1;
            float best_d2 = 0.0f;
            constexpr float kPickRadius = 0.55f;
            constexpr float kPickRadius2 = kPickRadius * kPickRadius;
            for (const auto& e : state->entities) {
                const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
                if (!ae) continue;
                const auto& p = ae->position();
                const float ax = p.x + 0.5f;
                const float ay = p.y + 0.5f + kHudHeight;
                const float dx = cx - ax;
                const float dy = cy - ay;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= kPickRadius2 && (found_id < 0 || d2 < best_d2)) {
                    found_id = ae->id();
                    best_d2 = d2;
                }
            }

            if (found_id >= 0) {
                // Click κοντά σε agent -> τον επιλέγουμε.
                state->selected_agent_id = found_id;
            } else if (state->selected_agent_id >= 0 && state->map.isFree(cell)) {
                // Click σε άδειο κελί -> θέτουμε goal για τον επιλεγμένο agent.
                for (auto& e : state->entities) {
                    auto* ae = dynamic_cast<AgentEntity*>(e.get());
                    if (!ae) continue;
                    if (ae->id() == state->selected_agent_id) {
                        ae->setGoalPoint(cell);
                        break;
                    }
                }
            }
        }

        if (mouse.button_right_pressed && state->selected_agent_id >= 0 && state->map.isFree(cell)) {
            for (auto& e : state->entities) {
                auto* ae = dynamic_cast<AgentEntity*>(e.get());
                if (!ae) continue;
                if (ae->id() == state->selected_agent_id) {
                    ae->setGoalPoint(cell);
                    break;
                }
            }
        }

        // Ρητή δυναμική δέσμευση ("new") + αυτόματη αποδέσμευση μέσω unique_ptr.
        state->entities.push_back(std::unique_ptr<grid::Entity>(new RippleEntity(cx, cy, 900.0f)));
    }

    state->accumulator_ms += ms;
    while (state->accumulator_ms >= static_cast<float>(state->tick_delay_ms)) {
        if (state->paused && !state->step_once) break;

        state->sim_elapsed_ms += static_cast<float>(state->tick_delay_ms);

        state->update(static_cast<float>(state->tick_delay_ms));

        // Πέρασμα αποφυγής συγκρούσεων: επιλύουμε την κίνηση των agents «ταυτόχρονα».
        {
            auto key = [&](const grid::Point& p) -> int { return p.y * state->map.width() + p.x; };

            std::vector<AgentEntity*> agents;
            agents.reserve(state->entities.size());
            std::unordered_set<int> occupied;
            occupied.reserve(state->entities.size());

            for (auto& e : state->entities) {
                auto* ae = dynamic_cast<AgentEntity*>(e.get());
                if (!ae) continue;
                agents.push_back(ae);
                occupied.insert(key(ae->position()));
            }

            std::sort(agents.begin(), agents.end(), [](const AgentEntity* a, const AgentEntity* b) { return a->id() < b->id(); });

            std::unordered_set<int> reserved;
            reserved.reserve(agents.size());

            for (auto* ae : agents) {
                if (!ae->wantsStep()) continue;

                // [N] step mode: παίκτες κινούνται, CPU agent παραλείπεται
                if (state->step_skip_cpu && ae->id() == state->cpu_agent_id) {
                    ae->commitStepBlocked();
                    continue;
                }

                const grid::Point next = ae->intendedCell();
                const int nk = key(next);

                // Μπλοκάρουμε αν το κελί-στόχος δεν είναι free ή αν είναι ήδη occupied/reserved από agent.
                if (!state->map.isFree(next) || occupied.count(nk) || reserved.count(nk)) {
                    ae->commitStepBlocked();
                    continue;
                }

                reserved.insert(nk);
                ae->commitStepAllowed();
            }
        }

        // Αφού ενημερωθούν όλα τα entities για το tick, επιλύουμε τη συλλογή targets.
        collectTargets(*state);

        state->accumulator_ms -= static_cast<float>(state->tick_delay_ms);

        if (state->step_once) {
            state->step_once = false;
            state->step_skip_cpu = false;
            break;
        }
    }
}

} 

int main(int argc, char** argv) {
    std::string mapPath = "maps/huge.json";
    std::string cfgPath = "configs/large_agents.txt";
    if (argc >= 2) mapPath = argv[1];
    if (argc >= 3) cfgPath = argv[2];
    g_cfg_path = cfgPath;
    for (size_t i = 0; i < g_demo_maps.size(); ++i) {
        if (g_demo_maps[i] == mapPath) {
            g_demo_map_index = static_cast<int>(i);
            break;
        }
    }

    // Κάνουμε cache τον φάκελο του executable για asset lookup, ακόμη κι αν αλλάξει το cwd.
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (argv && argc >= 1 && argv[0]) {
            const fs::path exe = fs::absolute(fs::path(argv[0]), ec);
            if (!ec) g_exe_dir = exe.parent_path();
        }
    }

    grid::GlobalState state;

    if (!state.map.loadFromFile(resolveMapPath(mapPath))) {
        std::cerr << "Failed to load map: " << mapPath << "\n";
        return 1;
    }
    if (!loadAgentsConfig(cfgPath, state.map, state.entities)) {
        std::cerr << "Failed to load agents config: " << cfgPath << "\n";
        return 1;
    }

    // Αντιστοιχίζουμε Player 1/2/CPU στους τρεις πρώτους agents (ταξινόμηση κατά id).
    {
        std::vector<int> ids;
        ids.reserve(state.entities.size());
        for (const auto& e : state.entities) {
            const auto* ae = dynamic_cast<const AgentEntity*>(e.get());
            if (!ae) continue;
            ids.push_back(ae->id());
        }
        std::sort(ids.begin(), ids.end());
        if (ids.size() >= 1) state.player1_agent_id = ids[0];
        if (ids.size() >= 2) state.player2_agent_id = ids[1];
        if (ids.size() >= 3) state.cpu_agent_id = ids[2];
    }


    // Ο CPU agent (αν υπάρχει) παίζει με autopilot για να μαζεύει targets.
    // Το match ξεκινάει σε "setup" mode, όπου ο χρήστης διαλέγει διάρκεια.
    state.match_started = false;
    state.match_over = false;
    state.match_duration_sec = 120;
    state.match_time_left_ms = static_cast<float>(state.match_duration_sec) * 1000.0f;

    // Κρατάμε το autopilot για τον CPU agent.
    state.autopilot_agent_id = state.cpu_agent_id;

    const int area = state.map.width() * state.map.height();
    // Περισσότερα targets σε μεγαλύτερο map / μεγαλύτερη διάρκεια.
    const int base = std::max(1, area / 45);
    const int duration_factor = std::clamp(state.match_duration_sec / 60, 1, 5);
    const int target_count = std::clamp(base * duration_factor, 20, 180);
    spawnInitialTargets(state, target_count);

    graphics::createWindow(900, 700, "GridWorld - SGG sim");

    // Για να εμφανιστεί κείμενο, πρέπει να έχει οριστεί font.
    {
        g_font_ui = resolveFontPath();
        g_font_display = resolveDisplayFontPath();
        g_obstacle_texture = resolveObstacleTexturePath();
        g_pacman_wall_texture = resolvePacmanTexturePath("wall.png");
        g_pacman_player_texture = resolvePacmanTexturePath("player_pacman.png");
        g_pacman_ghost_red_texture = resolvePacmanTexturePath("ghost_red.png");
        g_pacman_ghost_pink_texture = resolvePacmanTexturePath("ghost_pink.png");
        g_pacman_ghost_cyan_texture = resolvePacmanTexturePath("ghost_cyan.png");
        g_pacman_pellet_texture = resolvePacmanTexturePath("pellet.png");

        g_sfx_countdown = resolveCountdownSoundPath();
        g_sfx_end = resolveEndSoundPath();
        g_sfx_winner = resolveWinnerSoundPath();
        logAudioEvent("[startup] countdown='" + g_sfx_countdown + "' end='" + g_sfx_end + "' winner='" + g_sfx_winner + "'");
        if (g_sfx_countdown.empty() || g_sfx_end.empty() || g_sfx_winner.empty()) {
            std::cerr << "Warning: could not locate SGG hit1.wav for sounds."
                      << " (countdown='" << g_sfx_countdown << "' end='" << g_sfx_end << "' winner='" << g_sfx_winner << "')\n";
        }

        const std::string initial_font = !g_font_ui.empty() ? g_font_ui : g_font_display;
        if (!initial_font.empty()) {
            if (!graphics::setFont(initial_font)) {
                std::cerr << "Warning: graphics::setFont failed for: " << initial_font << "\n";
            }
        } else {
            std::cerr << "Warning: could not locate a .ttf font; HUD text will not render."
                      << " Expected under sgg-main/assets.\n";
        }
    }

    graphics::Brush bg;
    bg.fill_color[0] = 0.06f;
    bg.fill_color[1] = 0.06f;
    bg.fill_color[2] = 0.07f;
    graphics::setWindowBackground(bg);

    graphics::setCanvasSize(static_cast<float>(state.map.width()), static_cast<float>(state.map.height()) + kHudHeight);
    graphics::setCanvasScaleMode(graphics::CANVAS_SCALE_FIT);

    graphics::setUserData(&state);
    graphics::setDrawFunction(draw_callback);
    graphics::setUpdateFunction(update_callback);

    graphics::startMessageLoop();

    graphics::setUserData(nullptr);
    graphics::destroyWindow();
    return 0;
}
