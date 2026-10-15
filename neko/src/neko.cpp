#define IMGUI_DEFINE_MATH_OPERATORS
#include "neko.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace neko {

namespace col {
    constexpr ImU32 WindowBg    = IM_COL32( 42,  45,  58, 255);
    constexpr ImU32 Panel       = IM_COL32( 37,  41,  53, 255);
    constexpr ImU32 Card        = IM_COL32( 33,  36,  47, 255);
    constexpr ImU32 TopBorder   = IM_COL32(222, 150, 190, 255);
    constexpr ImU32 Accent      = IM_COL32(228, 146, 192, 255);
    constexpr ImU32 Text        = IM_COL32(226, 229, 238, 255);
    constexpr ImU32 TextDim     = IM_COL32(146, 150, 162, 255);
    constexpr ImU32 NavText     = IM_COL32(192, 196, 214, 255);
    constexpr ImU32 Pill        = IM_COL32( 66,  70,  90, 255);
    constexpr ImU32 PillHover   = IM_COL32( 58,  62,  82, 255);
    constexpr ImU32 Control     = IM_COL32( 64,  66,  80, 255);
    constexpr ImU32 ControlHov  = IM_COL32( 72,  74,  90, 255);
    constexpr ImU32 ControlText = IM_COL32(214, 216, 228, 255);
    constexpr ImU32 Separator   = IM_COL32( 50,  53,  64, 255);
    constexpr ImU32 RowSep      = IM_COL32( 29,  32,  42, 255);
    constexpr ImU32 LogoSep     = IM_COL32( 64,  67,  82, 255);
    constexpr ImU32 Knob        = IM_COL32(240, 243, 245, 255);
    constexpr ImU32 PopupBg     = IM_COL32( 45,  48,  62, 255);
}

// Lucide codepoints (UTF-8), see assets/fonts/lucide.ttf.
#define ICON_LOCATE_FIXED "\xEE\x87\x9B"   // U+E1DB
#define ICON_EYE_OFF      "\xEE\x82\xBB"   // U+E0BB
#define ICON_MENU         "\xEE\x84\x95"   // U+E115
#define ICON_SEARCH       "\xEE\x85\x91"   // U+E151
#define ICON_CHEVRON_DOWN "\xEE\x81\xAD"   // U+E06D
#define ICON_CHEVRON_UP   "\xEE\x81\xB0"   // U+E070

static ImFont* g_fontMedium = nullptr;
static ImFont* g_fontSemi   = nullptr;
static ImFont* g_fontBold   = nullptr;
static ImFont* g_fontIcons  = nullptr;
static ImFont* g_fontJp     = nullptr;

// ------------------------------------------------------------------ state --

struct Config {
    // Aiming / Main
    bool  aimEnabled = false;
    int   targeting  = 0;
    int   aimKey     = ImGuiKey_None;
    int   keyMode    = 0;
    bool  sticky     = false;
    int   aimMode    = 0;
    bool  noSleep    = false;
    float smoothness = 35.0f;
    float fov        = 120.0f;
    bool  showFov    = true;
    int   hitPart    = 0;
    bool  prediction = false;
    float predAmount = 0.135f;
    bool  teamCheck  = true;
    bool  wallCheck  = false;
    bool  aliveCheck = true;
    // Aiming / Silent
    bool  silentEnabled = false;
    int   silentKey     = ImGuiKey_None;
    float hitChance     = 100.0f;
    int   silentPart    = 0;
    float silentFov     = 90.0f;
    bool  silentShowFov = false;
    // Aiming / Triggerbot
    bool  trigEnabled = false;
    int   trigKey     = ImGuiKey_None;
    int   trigMode    = 0;
    float trigDelay   = 40.0f;
    bool  trigTeam    = true;
    // Visuals / Players
    bool  espEnabled = false;
    bool  boxes      = true;
    int   boxStyle   = 0;
    bool  names      = true;
    bool  health     = true;
    bool  tracers    = false;
    bool  chams      = false;
    float espDist    = 1500.0f;
    // Visuals / World
    bool  fullbright = false;
    bool  noFog      = false;
    bool  customTime = false;
    float timeOfDay  = 14.0f;
    // Visuals / Crosshair
    bool  xhair      = false;
    int   xhairStyle = 0;
    float xhairSize  = 10.0f;
    bool  xhairSpin  = false;
    // Misc / Movement
    bool  walkspeed  = false;
    float walkAmount = 24.0f;
    bool  infJump    = false;
    bool  noclip     = false;
    int   noclipKey  = ImGuiKey_None;
    // Misc / Character
    bool  antiAfk    = true;
    bool  autoRespawn= false;
    float fovCamera  = 70.0f;
    // Misc / Menu
    int   menuKey    = ImGuiKey_Insert;
    bool  watermark  = true;
    bool  keybindList= false;
    int   uiScale    = 1;
    // Cloud
    int   config     = 0;
    bool  autoload   = false;
    bool  autosave   = true;
    // Games
    bool  gameOverrides = true;
    int   gameProfile   = 0;
};

static Config g_cfg;

static int   g_tab      = 0;       // sidebar
static int   g_sub[5]   = {};      // top tab per sidebar entry
static char  g_search[64] = "";
static float g_pendingScroll = -1.0f;
static ImGuiID g_bindingId   = 0;  // keybind currently waiting for input

// Animation clocks.
static float g_dt       = 0.0f;    // clamped frame delta
static float g_time     = 0.0f;
static float g_pageTime = 0.0f;    // seconds since the current page appeared
static float g_tabsTime = 0.0f;    // seconds since the sidebar tab changed
static int   g_rowIdx   = 0;       // per-frame counters for staggering
static int   g_secIdx   = 0;
static float g_alpha    = 1.0f;    // fade applied to content colours
static ImGuiWindow* g_childWin = nullptr;

void SetDebugState(int tab, int subTab, float scroll)
{
    g_tab = ImClamp(tab, 0, 4);
    g_sub[g_tab] = ImMax(subTab, 0);
    g_pendingScroll = scroll;
}

// ---------------------------------------------------------------- helpers --

static ImVec2 g_origin;            // window top-left
static inline ImVec2 P(float x, float y) { return ImVec2(g_origin.x + x, g_origin.y + y); }

static ImU32 Fade(ImU32 c, float a)
{
    ImU32 al = (ImU32)(((c >> IM_COL32_A_SHIFT) & 0xFF) * ImSaturate(a) + 0.5f);
    return (c & ~IM_COL32_A_MASK) | (al << IM_COL32_A_SHIFT);
}

static ImU32 Mix(ImU32 a, ImU32 b, float t)
{
    ImVec4 va = ImGui::ColorConvertU32ToFloat4(a), vb = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImLerp(va, vb, ImSaturate(t)));
}

// Content colours follow the current row/card fade.
static inline ImU32 C(ImU32 c) { return Fade(c, g_alpha); }

// Smoothly approaches `target`; one float per id in the window's storage.
static float Anim(ImGuiID id, float target, float speed = 14.0f)
{
    float* v = ImGui::GetStateStorage()->GetFloatRef(id, target);
    *v = ImLerp(*v, target, ImSaturate(ImGui::GetIO().DeltaTime * speed));
    if (std::fabs(*v - target) < 0.001f) *v = target;
    return *v;
}

// ---------------------------------------------------------------- motion --
//
// Everything moves on damped springs. They are deliberately underdamped, so
// values overshoot a touch and settle, which is where the squishy feel
// comes from.

static void SpringStep(float& x, float& v, float target, float freq, float damping)
{
    const int steps = 4;
    const float h = g_dt / steps;
    for (int i = 0; i < steps; ++i) {
        float a = freq * freq * (target - x) - 2.0f * damping * freq * v;
        v += a * h;
        x += v * h;
    }
    if (std::fabs(target - x) < 1e-4f && std::fabs(v) < 1e-3f) { x = target; v = 0.0f; }
}

static ImGuiID SubId(ImGuiID id, const char* s) { return ImHashStr(s, 0, id); }

// Spring kept in the current window's storage; starts at rest on `target`.
// `vel` receives the velocity, used for stretch effects.
static float Spring(ImGuiID id, float target, float freq = 16.0f, float damping = 0.5f, float* vel = nullptr)
{
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID vid = SubId(id, "#v");
    float x = st->GetFloat(id, target);
    float v = st->GetFloat(vid, 0.0f);
    SpringStep(x, v, target, freq, damping);
    st->SetFloat(id, x);
    st->SetFloat(vid, v);
    if (vel) *vel = v;
    return x;
}

static void SpringSet(ImGuiID id, float x, float v = 0.0f)
{
    ImGuiStorage* st = ImGui::GetStateStorage();
    st->SetFloat(id, x);
    st->SetFloat(SubId(id, "#v"), v);
}

static float EaseOutBack(float t, float s = 1.7f)
{
    t = ImSaturate(t) - 1.0f;
    return 1.0f + (s + 1.0f) * t * t * t + s * t * t;
}

// Scale for pressable things: holding squashes (wider + shorter), letting go
// wobbles back. x and y ring at different frequencies so they drift out of
// phase, which reads as jelly rather than a plain bounce.
static ImVec2 Jelly(ImGuiID id, bool held, bool hovered, float amount = 1.0f)
{
    float tx = held ? 1.0f + 0.05f * amount : (hovered ? 1.0f + 0.018f * amount : 1.0f);
    float ty = held ? 1.0f - 0.10f * amount : (hovered ? 1.0f + 0.018f * amount : 1.0f);
    return ImVec2(Spring(SubId(id, "jx"), tx, 17.0f, 0.3f), Spring(SubId(id, "jy"), ty, 21.0f, 0.28f));
}

// Scales every vertex emitted since `vtxStart` around `c`. Channels share
// one vertex buffer, so this works across ChannelsSplit too.
static void ScaleVerts(ImDrawList* dl, int vtxStart, ImVec2 c, ImVec2 s)
{
    if (std::fabs(s.x - 1.0f) < 1e-4f && std::fabs(s.y - 1.0f) < 1e-4f) return;
    for (int i = vtxStart; i < dl->VtxBuffer.Size; ++i) {
        ImVec2& p = dl->VtxBuffer[i].pos;
        p = c + (p - c) * s;
    }
}

// Scales (and fades) a whole finished draw list, clip rects included.
static void TransformDrawList(ImDrawList* dl, ImVec2 c, ImVec2 s, float alpha)
{
    for (ImDrawVert& v : dl->VtxBuffer) {
        v.pos = c + (v.pos - c) * s;
        if (alpha < 1.0f) v.col = Fade(v.col, alpha);
    }
    for (ImDrawCmd& cmd : dl->CmdBuffer) {
        ImVec2 p0 = c + (ImVec2(cmd.ClipRect.x, cmd.ClipRect.y) - c) * s;
        ImVec2 p1 = c + (ImVec2(cmd.ClipRect.z, cmd.ClipRect.w) - c) * s;
        cmd.ClipRect = ImVec4(p0.x, p0.y, p1.x, p1.y);
    }
}

// An "up" chevron rotated by `angle` (pi = pointing down).
static void Chevron(ImDrawList* dl, ImVec2 c, float size, float angle, ImU32 color, float thick = 2.0f)
{
    const ImVec2 local[3] = { { -size, size * 0.5f }, { 0.0f, -size * 0.5f }, { size, size * 0.5f } };
    const float ca = std::cos(angle), sa = std::sin(angle);
    ImVec2 pts[3];
    for (int i = 0; i < 3; ++i)
        pts[i] = c + ImVec2(local[i].x * ca - local[i].y * sa, local[i].x * sa + local[i].y * ca);
    dl->AddPolyline(pts, 3, color, ImDrawFlags_None, thick);
    dl->AddCircleFilled(pts[1], thick * 0.5f, color, 8);        // rounded tip
}

static ImVec2 TextSize(ImFont* f, float size, const char* t)
{
    return f->CalcTextSizeA(size, FLT_MAX, 0.0f, t);
}

// Comfortaa's line box sits a little low relative to the glyphs, so centring
// on the line box needs a small upward nudge.
static void TextAt(ImDrawList* dl, ImFont* f, float size, float x, float cy, ImU32 c, const char* t)
{
    dl->AddText(f, size, ImVec2(IM_ROUND(x), IM_ROUND(cy - size * 0.47f)), c, t);
}

static void TextCentered(ImDrawList* dl, ImFont* f, float size, float cx, float cy, ImU32 c, const char* t)
{
    TextAt(dl, f, size, cx - TextSize(f, size, t).x * 0.5f, cy, c, t);
}

// Lucide strokes are thinner than the reference icons; `bold` smears the
// glyph over a few sub-pixel offsets to thicken it.
static void Icon(ImDrawList* dl, float size, float cx, float cy, ImU32 c, const char* glyph, float bold = 0.7f)
{
    ImVec2 s = TextSize(g_fontIcons, size, glyph);
    ImVec2 p(IM_ROUND(cx - s.x * 0.5f), IM_ROUND(cy - s.y * 0.5f));
    dl->AddText(g_fontIcons, size, p, c, glyph);
    if (bold > 0.0f) {
        dl->AddText(g_fontIcons, size, p + ImVec2(bold, 0), c, glyph);
        dl->AddText(g_fontIcons, size, p + ImVec2(0, bold), c, glyph);
        dl->AddText(g_fontIcons, size, p + ImVec2(bold, bold), c, glyph);
    }
}

// Panel with a soft lighter rim, like the blurred edges in the reference.
static void PanelRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float r)
{
    for (int i = 3; i >= 1; --i)
        dl->AddRectFilled(a - ImVec2((float)i, (float)i), b + ImVec2((float)i, (float)i),
                          IM_COL32(40, 43, 56, 60), r + i);
    dl->AddRectFilled(a, b, col::Panel, r);
}

static void Heart(ImDrawList* dl, ImVec2 c, float s, ImU32 color)
{
    dl->AddCircleFilled(c + ImVec2(-s * 0.5f, -s * 0.2f), s * 0.55f, color, 12);
    dl->AddCircleFilled(c + ImVec2( s * 0.5f, -s * 0.2f), s * 0.55f, color, 12);
    dl->AddTriangleFilled(c + ImVec2(-s * 1.02f, 0.0f), c + ImVec2(s * 1.02f, 0.0f),
                          c + ImVec2(0.0f, s * 1.15f), color);
}

static void Star(ImDrawList* dl, ImVec2 c, float ro, float ri, int points, ImU32 color, float rot = 0.0f)
{
    ImVec2 pts[64];
    int n = ImMin(points * 2, 64);
    for (int i = 0; i < n; ++i) {
        float a = rot + (float)i / n * IM_PI * 2.0f - IM_PI * 0.5f;
        float r = (i & 1) ? ri : ro;
        pts[i] = c + ImVec2(std::cos(a) * r, std::sin(a) * r);
    }
    dl->AddConcavePolyFilled(pts, n, color);
}

static bool PassFilter(const char* title)
{
    if (!g_search[0]) return true;
    std::string h(title), n(g_search);
    for (auto& ch : h) ch = (char)std::tolower((unsigned char)ch);
    for (auto& ch : n) ch = (char)std::tolower((unsigned char)ch);
    return h.find(n) != std::string::npos;
}

// ------------------------------------------------------------------ fonts --

void LoadFonts(const char* dir)
{
    ImGuiIO& io = ImGui::GetIO();
    char path[512];

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;

    snprintf(path, sizeof(path), "%s/Comfortaa-Medium.ttf", dir);
    g_fontMedium = io.Fonts->AddFontFromFileTTF(path, 16.0f, &cfg);
    snprintf(path, sizeof(path), "%s/Comfortaa-SemiBold.ttf", dir);
    g_fontSemi = io.Fonts->AddFontFromFileTTF(path, 16.0f, &cfg);
    snprintf(path, sizeof(path), "%s/Comfortaa-Bold.ttf", dir);
    g_fontBold = io.Fonts->AddFontFromFileTTF(path, 16.0f, &cfg);

    snprintf(path, sizeof(path), "%s/lucide.ttf", dir);
    g_fontIcons = io.Fonts->AddFontFromFileTTF(path, 20.0f, &cfg);

    // Katakana for the logo; optional.
    g_fontJp = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/YuGothB.ttc", 12.0f, &cfg);

    if (!g_fontMedium) g_fontMedium = io.Fonts->AddFontDefault();
    if (!g_fontSemi)   g_fontSemi   = g_fontMedium;
    if (!g_fontBold)   g_fontBold   = g_fontSemi;
    if (!g_fontIcons)  g_fontIcons  = g_fontMedium;
    io.FontDefault = g_fontMedium;
}

// ----------------------------------------------------------------- sidebar --

static void DrawLogo(ImDrawList* dl, ImVec2 c)
{
    // Die-cut sticker: white body, pale pink rim and a pink "thickness" edge
    // along the bottom.
    struct Blob { float x, y, r; };
    const Blob blobs[] = {
        {-50, -4, 25}, {-22, -16,  9}, {  4, -18, 10}, { 32, -17, 10}, { 59,   2, 12},
        { 52, 18, 10}, { 20,  22,  9}, {-16,  22,  9}, {-42,  15, 12},
    };
    const ImVec2 bodyA = c + ImVec2(-46, -16), bodyB = c + ImVec2(66, 30);
    auto body = [&](ImVec2 off, float grow, ImU32 color) {
        for (const Blob& bl : blobs)
            dl->AddCircleFilled(c + off + ImVec2(bl.x, bl.y), bl.r + grow, color, 32);
        dl->AddRectFilled(bodyA + off - ImVec2(grow, grow), bodyB + off + ImVec2(grow, grow), color, 18.0f + grow);
    };
    body(ImVec2(0, 3.5f), 1.5f, IM_COL32(232, 160, 196, 255));
    body(ImVec2(0, 0),    1.5f, IM_COL32(244, 204, 224, 255));
    body(ImVec2(0, 0),    0.0f, IM_COL32(254, 248, 251, 255));

    // Pink starburst with a paw print.
    const ImVec2 paw = c + ImVec2(-50, -5);
    Star(dl, paw, 24.0f, 18.5f, 12, IM_COL32(243, 140, 182, 255), 0.1f + g_time * 0.35f);   // slow spin
    const ImU32 pad = IM_COL32(253, 208, 226, 255);
    dl->AddCircleFilled(paw + ImVec2(1.0f, 6.0f), 7.5f, pad, 20);
    dl->AddCircleFilled(paw + ImVec2(-9.0f, -2.0f), 3.6f, pad, 12);
    dl->AddCircleFilled(paw + ImVec2(-4.0f, -8.5f), 3.6f, pad, 12);
    dl->AddCircleFilled(paw + ImVec2( 3.5f, -9.0f), 3.6f, pad, 12);
    dl->AddCircleFilled(paw + ImVec2( 9.5f, -3.0f), 3.6f, pad, 12);

    // Confetti.
    Heart(dl, c + ImVec2(-9, -23), 4.2f * (1.0f + 0.1f * std::sin(g_time * 4.0f)), IM_COL32(168, 128, 232, 255));
    Star(dl, c + ImVec2(30, -21), 5.5f * (0.8f + 0.2f * std::sin(g_time * 3.0f + 1.0f)), 2.0f, 4, IM_COL32(214, 188, 240, 255));   // twinkle
    dl->AddCircleFilled(c + ImVec2(20, -22), 1.6f, IM_COL32(150, 206, 236, 255));
    dl->AddCircleFilled(c + ImVec2(47, -21), 1.8f, IM_COL32(240, 150, 190, 255));
    dl->AddBezierCubic(c + ImVec2(50, -11), c + ImVec2(54, -15), c + ImVec2(57, -7), c + ImVec2(61, -11),
                       IM_COL32(236, 150, 186, 255), 1.4f);
    dl->AddLine(c + ImVec2(58, -4), c + ImVec2(64, -4), IM_COL32(236, 150, 186, 255), 1.4f);
    dl->AddLine(c + ImVec2(64, -4), c + ImVec2(61, 4), IM_COL32(236, 150, 186, 255), 1.4f);
    dl->AddRectFilled(c + ImVec2(-40, 20), c + ImVec2(-33, 24), IM_COL32(146, 192, 240, 255), 2.0f);
    dl->AddCircleFilled(c + ImVec2(-12, 22), 1.4f, IM_COL32(200, 170, 230, 255));
    dl->AddCircleFilled(c + ImVec2(58, 14), 1.4f, IM_COL32(150, 206, 236, 255));
    Heart(dl, c + ImVec2(52, 24), 3.6f * (1.0f + 0.12f * std::sin(g_time * 4.0f + 2.0f)), IM_COL32(236, 76, 118, 255));
    dl->PathArcTo(c + ImVec2(25, 19), 6.5f, IM_PI * 0.12f, IM_PI * 0.88f, 12);
    dl->PathStroke(IM_COL32(244, 176, 96, 255), 0, 2.4f);

    // "neko", chunky pink-to-peach letters.
    const float fs = 48.0f;
    const char* word = "neko";
    const ImU32 letters[] = {
        IM_COL32(240, 108, 158, 255), IM_COL32(242, 122, 156, 255),
        IM_COL32(244, 136, 150, 255), IM_COL32(245, 150, 138, 255),
    };
    ImVec2 pos = c + ImVec2(-44.0f, 23.0f - fs * 0.79f);    // baseline 23px below centre
    float x = pos.x;
    for (int i = 0; i < 4; ++i) {
        char ch[2] = { word[i], 0 };
        for (int k = 0; k < 12; ++k) {
            float ang = k / 12.0f * IM_PI * 2.0f;
            dl->AddText(g_fontBold, fs, ImVec2(x + 1.2f + std::cos(ang) * 3.0f, pos.y + 0.4f + std::sin(ang) * 3.0f),
                        IM_COL32(255, 252, 253, 255), ch);
        }
        x += TextSize(g_fontBold, fs, ch).x - 5.0f;
    }
    x = pos.x;
    for (int i = 0; i < 4; ++i) {
        char ch[2] = { word[i], 0 };
        for (float o = 0.0f; o <= 2.4f; o += 0.8f) {
            dl->AddText(g_fontBold, fs, ImVec2(x + o, pos.y), letters[i], ch);
            dl->AddText(g_fontBold, fs, ImVec2(x + o, pos.y + 0.8f), letters[i], ch);
        }
        x += TextSize(g_fontBold, fs, ch).x - 5.0f;
    }

    if (g_fontJp)
        dl->AddText(g_fontJp, 10.5f, c + ImVec2(-21, -18), IM_COL32(236, 112, 160, 255), u8"ネコ");
    dl->AddLine(c + ImVec2(-28, -15), c + ImVec2(-25, -12), IM_COL32(236, 112, 160, 255), 1.3f);
}

static void DrawAvatar(ImDrawList* dl, ImVec2 c, float r)
{
    dl->AddCircleFilled(c, r, IM_COL32(206, 206, 200, 255), 48);
    dl->AddCircleFilled(c + ImVec2(-4, -3), r * 0.78f, IM_COL32(222, 222, 218, 255), 48);
    // Sketchy pencil strokes.
    const ImU32 ink = IM_COL32(120, 120, 124, 200);
    dl->AddBezierCubic(c + ImVec2(-16, -10), c + ImVec2(-8, -18), c + ImVec2(4, -16), c + ImVec2(10, -6), ink, 1.0f);
    dl->AddBezierCubic(c + ImVec2(-14, -4), c + ImVec2(-10, 6), c + ImVec2(-2, 10), c + ImVec2(6, 4), ink, 1.0f);
    dl->AddBezierCubic(c + ImVec2(-6, -12), c + ImVec2(-4, -4), c + ImVec2(2, -2), c + ImVec2(4, -10), ink, 1.0f);
    dl->AddLine(c + ImVec2(-18, 8), c + ImVec2(-8, 16), ink, 1.0f);
    dl->AddLine(c + ImVec2(-12, 2), c + ImVec2(-4, 10), IM_COL32(120, 120, 124, 120), 1.0f);
    // Warm shadow on the right edge.
    dl->PathArcTo(c, r - 3.0f, -IM_PI * 0.35f, IM_PI * 0.45f, 16);
    dl->PathStroke(IM_COL32(176, 160, 146, 200), 0, 5.0f);
}

static void DrawCloud(ImDrawList* dl, ImVec2 c, ImU32 color)
{
    dl->AddCircleFilled(c + ImVec2(-6.0f, 2.0f), 5.8f, color, 20);
    dl->AddCircleFilled(c + ImVec2( 1.0f, -2.0f), 7.2f, color, 20);
    dl->AddCircleFilled(c + ImVec2( 7.5f, 2.5f), 5.2f, color, 20);
    dl->AddRectFilled(c + ImVec2(-11.0f, 1.0f), c + ImVec2(12.5f, 7.8f), color, 3.8f);
}

static void DrawMeeple(ImDrawList* dl, ImVec2 c, ImU32 color)
{
    // Board-game meeple, slightly tilted like the reference icon.
    const ImVec2 local[] = {
        {-10.5f, -2.0f}, {-4.0f, -4.5f}, {4.0f, -4.5f}, {10.5f, -2.0f}, {10.0f, 2.0f},
        {4.5f, 3.0f}, {7.5f, 11.0f}, {1.5f, 11.0f}, {0.0f, 6.5f}, {-1.5f, 11.0f},
        {-7.5f, 11.0f}, {-4.5f, 3.0f}, {-10.0f, 2.0f},
    };
    const float a = -0.28f, ca = std::cos(a), sa = std::sin(a);
    ImVec2 pts[IM_ARRAYSIZE(local)];
    for (int i = 0; i < IM_ARRAYSIZE(local); ++i)
        pts[i] = c + ImVec2(local[i].x * ca - local[i].y * sa, local[i].x * sa + local[i].y * ca);
    dl->AddConcavePolyFilled(pts, IM_ARRAYSIZE(pts), color);
    dl->AddCircleFilled(c + ImVec2(-0.0f * ca + 9.0f * sa, -9.0f * ca), 4.6f, color, 16);
}

static const char* kNavNames[] = { "Aiming", "Visuals", "Miscellaneous", "Cloud", "Games" };

static void SwitchPage()
{
    g_pageTime = 0.0f;
    g_pendingScroll = 0.0f;
}

static void DrawSidebar(ImDrawList* dl)
{
    // --- Logo card: bobs gently, squishes when poked.
    PanelRect(dl, P(12, 12), P(302, 180), 16.0f);
    {
        ImGuiID id = ImGui::GetID("##logo");
        ImGui::SetCursorScreenPos(P(82, 26));
        ImGui::InvisibleButton("##logo", ImVec2(150, 70));
        if (ImGui::IsItemActivated()) SpringSet(SubId(id, "jy"), 0.82f, 0.0f);
        ImVec2 js = Jelly(id, ImGui::IsItemActive(), ImGui::IsItemHovered(), 1.6f);
        float bob = std::sin(g_time * 1.7f) * 1.6f;
        int vs = dl->VtxBuffer.Size;
        DrawLogo(dl, P(157, 60 + bob));
        ScaleVerts(dl, vs, P(157, 70 + bob), js);
    }
    dl->AddLine(P(34, 108.5f), P(280, 108.5f), col::LogoSep, 1.0f);
    TextCentered(dl, g_fontSemi, 18.8f, g_origin.x + 157, g_origin.y + 123.5f, col::Accent, "Neko for Roblox!");
    TextCentered(dl, g_fontMedium, 18.4f, g_origin.x + 157, g_origin.y + 144.5f, IM_COL32(170, 174, 186, 255), "09/06/2026");

    // --- Profile card: avatar pops on hover.
    PanelRect(dl, P(12, 194), P(302, 296), 16.0f);
    {
        ImGuiID id = ImGui::GetID("##avatar");
        ImGui::SetCursorScreenPos(P(26, 218));
        ImGui::InvisibleButton("##avatar", ImVec2(52, 52));
        float s = Spring(id, ImGui::IsItemActive() ? 0.92f : (ImGui::IsItemHovered() ? 1.1f : 1.0f), 15.0f, 0.3f);
        int vs = dl->VtxBuffer.Size;
        DrawAvatar(dl, P(52, 244), 25.0f);
        ScaleVerts(dl, vs, P(52, 244), ImVec2(s, s));
    }
    TextAt(dl, g_fontSemi, 19.3f, g_origin.x + 88, g_origin.y + 233, col::Accent, "formula");
    float fw = TextSize(g_fontSemi, 19.3f, "formula").x;
    TextAt(dl, g_fontMedium, 15.6f, g_origin.x + 88 + fw + 8, g_origin.y + 231, IM_COL32(118, 122, 136, 255), "#2458");
    TextAt(dl, g_fontMedium, 19.0f, g_origin.x + 88, g_origin.y + 255, IM_COL32(160, 164, 176, 255), "7 days remaining");

    // --- Navigation card
    PanelRect(dl, P(12, 308), P(302, 677), 16.0f);

    // Pills go on channel 0 underneath the icons/labels on channel 1.
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    const float itemH = 56.0f, firstCy = 351.0f;
    bool heldActive = false;
    for (int i = 0; i < 5; ++i) {
        float cy = firstCy + i * itemH;
        ImGui::SetCursorScreenPos(P(24, cy - 24.5f));
        ImGui::PushID(i);
        ImGuiID iconId = ImGui::GetID("##ic");
        if (ImGui::InvisibleButton("##nav", ImVec2(265, 49)) && g_tab != i) {
            g_tab = i;
            g_tabsTime = 0.0f;
            SwitchPage();
            SpringSet(iconId, 0.55f, 0.0f);                 // icon pops from small
        }
        bool hov = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
        if (held && g_tab == i) heldActive = true;
        float active = Anim(ImGui::GetID("##a"), g_tab == i ? 1.0f : 0.0f);
        float hover  = Spring(ImGui::GetID("##h"), hov ? 1.0f : 0.0f, 15.0f, 0.55f);
        float icS    = Spring(iconId, held ? 0.85f : (hov ? 1.14f : 1.0f), 16.0f, 0.3f);
        float ghost  = Spring(ImGui::GetID("##g"), (hov && g_tab != i) ? 1.0f : 0.0f, 16.0f, 0.6f);
        ImVec2 gj    = Jelly(ImGui::GetID("##gj"), held && g_tab != i, false, 0.7f);
        ImGui::PopID();

        // Faint hover pill for inactive items.
        if (ghost > 0.01f) {
            dl->ChannelsSetCurrent(0);
            int vs = dl->VtxBuffer.Size;
            dl->AddRectFilled(P(24, cy - 24.5f), P(289, cy + 24.5f), Fade(col::PillHover, 0.55f * ImSaturate(ghost)), 24.5f);
            float gs = 0.9f + 0.1f * ghost;
            ScaleVerts(dl, vs, P(156.5f, cy), ImVec2(gs * gj.x, gs * gj.y));
            dl->ChannelsSetCurrent(1);
        }

        ImU32 c = Mix(Mix(col::NavText, IM_COL32(236, 238, 246, 255), ImSaturate(hover)), col::Accent, active);
        const ImVec2 ic = P(56, cy);
        int vs = dl->VtxBuffer.Size;
        switch (i) {
        case 0: Icon(dl, 22.0f, ic.x, ic.y, c, ICON_LOCATE_FIXED); break;
        case 1: Icon(dl, 22.0f, ic.x, ic.y, c, ICON_EYE_OFF); break;
        case 2: Icon(dl, 22.0f, ic.x, ic.y, c, ICON_MENU); break;
        case 3: DrawCloud(dl, ic + ImVec2(0, -1), c); break;
        case 4: DrawMeeple(dl, ic + ImVec2(0, -1), c); break;
        }
        ScaleVerts(dl, vs, ic, ImVec2(icS, icS));
        TextAt(dl, g_fontSemi, 19.3f, g_origin.x + 78 + hover * 4.0f, g_origin.y + cy, c, kNavNames[i]);
    }

    // Selection pill: springs to the new item and stretches along the way.
    dl->ChannelsSetCurrent(0);
    {
        ImGuiID id = ImGui::GetID("##navsel");
        float vel = 0.0f;
        float selY = Spring(id, firstCy + g_tab * itemH, 13.0f, 0.5f, &vel);
        float stretch = ImMin(std::fabs(vel) * 0.03f, 24.0f);
        float hh = 24.5f + stretch * 0.5f;
        float hw = (265.0f - ImMin(std::fabs(vel) * 0.02f, 16.0f)) * 0.5f;
        const float cx = 156.5f;
        ImVec2 js = Jelly(id, heldActive, false, 0.8f);
        int vs = dl->VtxBuffer.Size;
        dl->AddRectFilled(P(cx - hw, selY - hh), P(cx + hw, selY + hh), col::Pill, ImMin(hh, hw));
        ScaleVerts(dl, vs, P(cx, selY), js);
    }
    dl->ChannelsMerge();
}

// ----------------------------------------------------------------- top bar --

static const char* kSubTabs[5][3] = {
    { "Main", "Silent", "Triggerbot" },
    { "Players", "World", "Crosshair" },
    { "Movement", "Character", "Menu" },
    { "Configs", "Scripts", nullptr },
    { "Supported", "Settings", nullptr },
};

static void DrawTopBar(ImDrawList* dl)
{
    PanelRect(dl, P(318, 12), P(1047, 94), 16.0f);

    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    // Sub tabs
    float x = 337.0f, selL = 337.0f, selR = 337.0f;
    bool heldActive = false;
    for (int i = 0; i < 3 && kSubTabs[g_tab][i]; ++i) {
        const char* name = kSubTabs[g_tab][i];
        const bool isSel = g_sub[g_tab] == i;
        float w = TextSize(g_fontSemi, 18.6f, name).x + 36.0f;
        if (isSel) { selL = x; selR = x + w; }

        ImGui::SetCursorScreenPos(P(x, 30));
        ImGui::PushID(g_tab * 8 + i);
        if (ImGui::InvisibleButton("##sub", ImVec2(w, 46)) && !isSel) {
            g_sub[g_tab] = i;
            SwitchPage();
        }
        bool hov = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
        if (held && isSel) heldActive = true;
        float active = Anim(ImGui::GetID("##a"), isSel ? 1.0f : 0.0f);
        float ghost  = Spring(ImGui::GetID("##g"), (hov && !isSel) ? 1.0f : 0.0f, 16.0f, 0.55f);
        ImVec2 tj    = Jelly(ImGui::GetID("##j"), held, hov, 1.0f);
        ImGui::PopID();

        const ImVec2 ctr = P(x + w * 0.5f, 53);
        if (ghost > 0.01f) {
            dl->ChannelsSetCurrent(0);
            int vs = dl->VtxBuffer.Size;
            dl->AddRectFilled(P(x, 30), P(x + w, 76), Fade(col::PillHover, ImSaturate(ghost)), 23.0f);
            float gs = 0.85f + 0.15f * ghost;
            ScaleVerts(dl, vs, ctr, ImVec2(gs * tj.x, gs * tj.y));
            dl->ChannelsSetCurrent(1);
        }

        // Labels drop in one after another when the tab set changes.
        float lt = g_tabsTime - i * 0.06f;
        float la = ImSaturate(lt / 0.22f);
        float ly = (1.0f - EaseOutBack(lt / 0.45f)) * 10.0f;
        ImU32 tc = Mix(Mix(IM_COL32(186, 190, 206, 255), IM_COL32(210, 214, 230, 255), ghost), col::Accent, active);
        int vs = dl->VtxBuffer.Size;
        TextCentered(dl, g_fontSemi, 18.6f, ctr.x, ctr.y + ly, Fade(tc, la), name);
        ScaleVerts(dl, vs, ctr, tj);
        x += w + 7.5f;
    }

    // Active pill: the leading edge moves faster than the trailing one, so the
    // pill stretches toward the new tab and then catches up with itself.
    dl->ChannelsSetCurrent(0);
    {
        ImGuiID id = ImGui::GetID("##subpill");
        ImGuiStorage* st = ImGui::GetStateStorage();
        const bool right = selL > st->GetFloat(SubId(id, "L"), selL) + 0.5f;
        float L = Spring(SubId(id, "L"), selL, right ? 11.0f : 19.0f, 0.6f);
        float Rr = Spring(SubId(id, "R"), selR, right ? 19.0f : 11.0f, 0.6f);
        if (Rr - L < 30.0f) Rr = L + 30.0f;
        ImVec2 js = Jelly(id, heldActive, false, 0.9f);
        int vs = dl->VtxBuffer.Size;
        dl->AddRectFilled(P(L, 30), P(Rr, 76), col::Pill, 23.0f);
        ScaleVerts(dl, vs, P((L + Rr) * 0.5f, 53), js);
    }
    dl->ChannelsMerge();

    // Search box: stretches open and picks up an accent rim while focused.
    ImGuiID sid = ImGui::GetID("##searchbox");
    ImGuiStorage* st = ImGui::GetStateStorage();
    const bool focused = st->GetBool(SubId(sid, "focus"), false);
    float fx = Spring(SubId(sid, "f"), focused ? 1.0f : 0.0f, 14.0f, 0.5f);
    const float left = 828.0f - 24.0f * fx;
    const ImVec2 sa = P(left, 31), sb = P(1030, 74);
    ImGui::SetCursorScreenPos(sa);
    ImGui::InvisibleButton("##searchbg", sb - sa);
    bool focusSearch = ImGui::IsItemClicked();
    bool sHov = ImGui::IsItemHovered(), sHeld = ImGui::IsItemActive();
    float sh = Anim(SubId(sid, "h"), sHov ? 1.0f : 0.0f);
    ImVec2 js = Jelly(sid, sHeld, false, 0.5f);
    int vs = dl->VtxBuffer.Size;
    dl->AddRectFilled(sa, sb, Mix(IM_COL32(64, 69, 89, 255), IM_COL32(72, 77, 98, 255), ImMax(sh, ImSaturate(fx))), 21.5f);
    if (fx > 0.01f)
        dl->AddRect(sa, sb, Fade(col::Accent, 0.6f * ImSaturate(fx)), 21.5f, 0, 1.5f);
    float iS = Spring(SubId(sid, "i"), focused ? 1.12f : (sHov ? 1.06f : 1.0f), 16.0f, 0.3f);
    int ivs = dl->VtxBuffer.Size;
    const ImVec2 ic(g_origin.x + left + 30, g_origin.y + 52.0f);
    Icon(dl, 24.0f, ic.x, ic.y, Mix(IM_COL32(218, 222, 242, 255), col::Accent, ImSaturate(fx)), ICON_SEARCH, 1.1f);
    ScaleVerts(dl, ivs, ic, ImVec2(iS, iS));
    ScaleVerts(dl, vs, (sa + sb) * 0.5f, js);

    ImGui::PushFont(g_fontMedium, 19.2f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(222, 226, 240, 255));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(170, 175, 196, 255));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Fade(col::Accent, 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::SetCursorScreenPos(P(left + 54, 52.5f - 19.2f * 0.47f));
    ImGui::SetNextItemWidth(1030.0f - 22.0f - (left + 54));
    if (focusSearch) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputTextWithHint("##search", "Search", g_search, sizeof(g_search)))
        SwitchPage();                                        // results cascade in
    st->SetBool(SubId(sid, "focus"), ImGui::IsItemActive());
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
}

// ------------------------------------------------------------- content ui --

struct Section {
    ImGuiID id;
    const char* title;
    float top;          // screen y of the card
    float rise;         // intro offset baked into `top`
    float x0, x1;       // card extents
    bool  open;
    bool  drawRows;     // open, or still folding shut
    bool  hdrHovered, hdrHeld;
    float openT;        // 0 = collapsed, 1 = open (springy)
    float alpha;
    int   rows;
    float lastRowTop;
    int   vtxStart;
};
static Section g_sec;

static constexpr float kRowH = 73.0f;

static void BeginSection(const char* title)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cur = ImGui::GetCursorScreenPos();

    g_sec = {};
    g_sec.id = ImGui::GetID(title);
    g_sec.title = title;
    g_sec.vtxStart = dl->VtxBuffer.Size;

    // Cards float up into place one after another when a page appears.
    float st = g_pageTime - g_secIdx * 0.09f;
    g_sec.alpha = ImSaturate(st / 0.22f);
    g_sec.rise = (1.0f - EaseOutBack(st / 0.5f)) * 20.0f;
    g_secIdx++;

    g_sec.top = cur.y + g_sec.rise;
    g_sec.x0 = cur.x;
    g_sec.x1 = cur.x + 694.0f;
    g_sec.open = !ImGui::GetStateStorage()->GetBool(g_sec.id, false);   // stored = collapsed

    ImGui::PushID(title);
    ImGui::SetCursorScreenPos(ImVec2(g_sec.x0, g_sec.top));
    if (ImGui::InvisibleButton("##hdr", ImVec2(694, 52))) {
        g_sec.open = !g_sec.open;
        ImGui::GetStateStorage()->SetBool(g_sec.id, !g_sec.open);
    }
    g_sec.hdrHovered = ImGui::IsItemHovered();
    g_sec.hdrHeld = ImGui::IsItemActive();
    g_sec.openT = Spring(ImGui::GetID("##open"), g_sec.open ? 1.0f : 0.0f, 13.0f, 0.55f);
    ImGui::PopID();

    g_sec.drawRows = g_sec.open || g_sec.openT > 0.01f;
    g_alpha = g_sec.alpha;

    // Header goes on channel 0 together with the card, rows on channel 1,
    // clipped to the unfolding body.
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    float fullH = ImGui::GetStateStorage()->GetFloat(SubId(g_sec.id, "fullh"), 0.0f);
    float clipB = g_sec.top + 54.0f + ImMax(g_sec.openT, 0.0f) * fullH;
    ImGui::PushClipRect(ImVec2(g_sec.x0, g_sec.top + 53.0f), ImVec2(g_sec.x1, clipB), true);
    // Rows slide down out of the header as it opens.
    ImGui::SetCursorScreenPos(ImVec2(g_sec.x0, g_sec.top + 61.0f - (1.0f - ImSaturate(g_sec.openT)) * 24.0f));
}

static void EndSection()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PopClipRect();
    dl->ChannelsSetCurrent(0);

    const bool filtering = g_search[0] != 0;
    if (filtering && g_sec.rows == 0) {
        // Nothing matched: drop the card entirely.
        dl->ChannelsMerge();
        ImGui::SetCursorScreenPos(ImVec2(g_sec.x0, g_sec.top - g_sec.rise));
        g_alpha = 1.0f;
        return;
    }

    ImGuiStorage* st = ImGui::GetStateStorage();
    float fullH = st->GetFloat(SubId(g_sec.id, "fullh"), 0.0f);
    if (g_sec.drawRows) {
        fullH = g_sec.rows * kRowH;
        st->SetFloat(SubId(g_sec.id, "fullh"), fullH);
    }
    const float openT = ImMax(g_sec.openT, -0.05f);
    const float bottom = fullH > 0.0f ? g_sec.top + 58.0f + openT * (fullH - 4.0f) : g_sec.top + 58.0f;

    dl->AddRectFilled(ImVec2(g_sec.x0, g_sec.top), ImVec2(g_sec.x1, bottom), C(col::Card), 12.0f);

    ImGui::PushID(g_sec.title);
    float hv = Spring(ImGui::GetID("##hh"), g_sec.hdrHovered ? 1.0f : 0.0f, 15.0f, 0.55f);
    ImVec2 js = Jelly(ImGui::GetID("##cj"), g_sec.hdrHeld, false, 0.22f);
    ImGui::PopID();

    ImU32 hc = Mix(IM_COL32(214, 138, 180, 255), IM_COL32(240, 168, 208, 255), hv);
    TextAt(dl, g_fontSemi, 19.0f, g_sec.x0 + 24 + hv * 4.0f, g_sec.top + 32, C(hc), g_sec.title);
    Chevron(dl, ImVec2(g_sec.x1 - 43, g_sec.top + 31), 5.5f, (1.0f - g_sec.openT) * IM_PI,
            C(Mix(IM_COL32(184, 187, 198, 255), IM_COL32(230, 232, 240, 255), hv)), 2.2f);
    if (fullH > 0.0f && g_sec.openT > 0.01f)
        dl->AddLine(ImVec2(g_sec.x0 + 20, g_sec.top + 52.5f), ImVec2(g_sec.x1 - 18, g_sec.top + 52.5f),
                    C(Fade(col::Separator, ImSaturate(g_sec.openT))), 1.0f);

    // Pressing the header squishes the whole card (rows included).
    ScaleVerts(dl, g_sec.vtxStart, ImVec2((g_sec.x0 + g_sec.x1) * 0.5f, (g_sec.top + bottom) * 0.5f), js);

    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos(ImVec2(g_sec.x0, bottom - g_sec.rise + 18.0f));
    g_alpha = 1.0f;
}

// Lays out the title/description of a row; returns false when hidden.
// On success the control is placed using `rightX` / `cy` (screen space).
static bool RowBegin(const char* title, const char* desc, float* rightX, float* cy)
{
    if (!g_sec.drawRows || !PassFilter(title)) return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float top = ImGui::GetCursorScreenPos().y;
    ImGuiID rid = ImGui::GetID(title);

    // Rows cascade in from the right with a little overshoot.
    float rt = g_pageTime - 0.06f - g_rowIdx * 0.035f;
    g_rowIdx++;
    float dx = (1.0f - EaseOutBack(rt / 0.45f)) * 28.0f;
    g_alpha = g_sec.alpha * ImSaturate(rt / 0.25f) * ImSaturate(g_sec.openT * 1.3f);

    // Soft hover wash.
    bool rh = ImGui::IsWindowHovered() &&
              ImGui::IsMouseHoveringRect(ImVec2(g_sec.x0 + 12, top - 10), ImVec2(g_sec.x1 - 12, top + 58));
    float hv = Spring(SubId(rid, "rh"), rh ? 1.0f : 0.0f, 14.0f, 0.65f);
    if (hv > 0.01f)
        dl->AddRectFilled(ImVec2(g_sec.x0 + 14, top - 9), ImVec2(g_sec.x1 - 14, top + 57),
                          C(IM_COL32(255, 255, 255, (int)(7 * ImSaturate(hv)))), 12.0f);

    if (g_sec.rows > 0)
        dl->AddRectFilled(ImVec2(g_sec.x0 + 20, top - 15.5f), ImVec2(g_sec.x1 - 18, top - 12.5f), C(col::RowSep), 1.5f);

    const float tx = g_sec.x0 + 33 + dx + hv * 3.0f;
    TextAt(dl, g_fontBold, 19.3f, tx, top + 19, C(col::Text), title);
    TextAt(dl, g_fontMedium, 19.0f, tx, top + 40, C(col::TextDim), desc);

    g_sec.rows++;
    g_sec.lastRowTop = top;
    *rightX = g_sec.x1 - 31.0f + dx;
    *cy = top + 30.0f;
    ImGui::PushID(title);
    return true;
}

static void RowEnd()
{
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(g_sec.x0, g_sec.lastRowTop + kRowH));
    g_alpha = g_sec.alpha;
}

static bool Toggle(const char* title, const char* desc, bool* v)
{
    float rx, cy;
    if (!RowBegin(title, desc, &rx, &cy)) return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float w = 70.0f, h = 31.0f;
    ImVec2 a(rx - w, cy - h * 0.5f), b(rx, cy + h * 0.5f);
    ImGui::SetCursorScreenPos(a);
    ImGuiID id = ImGui::GetID("##tgl");
    bool changed = ImGui::InvisibleButton("##tgl", b - a);
    if (changed) *v = !*v;
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();

    float vel = 0.0f;
    float t     = Spring(SubId(id, "t"), *v ? 1.0f : 0.0f, 15.0f, 0.45f, &vel);
    float tint  = Anim(SubId(id, "c"), *v ? 1.0f : 0.0f, 12.0f);
    float hov   = Anim(SubId(id, "h"), hovered ? 1.0f : 0.0f);
    float press = Spring(SubId(id, "p"), held ? 1.0f : 0.0f, 22.0f, 0.5f);

    int vs = dl->VtxBuffer.Size;
    ImU32 off = Mix(col::Control, col::ControlHov, hov);
    dl->AddRectFilled(a, b, C(Mix(off, col::Accent, tint)), h * 0.5f);

    // Knob: stretches along its travel, squashes flat against the ends when it
    // overshoots, and grows sideways while held.
    const float tc = ImSaturate(t), over = std::fabs(t - tc);
    const float stretch = 1.0f + ImMin(std::fabs(vel) * 0.045f, 0.5f);
    const float grow = press * 4.0f;
    float krx = ImMax(12.5f * (stretch - over * 1.6f), 7.0f) + grow;
    float kry = 12.5f / std::sqrt(stretch) * (1.0f + over * 1.2f) * (1.0f - press * 0.06f);
    float kx = ImLerp(a.x + 15.5f, b.x - 15.5f, tc) + (*v ? -grow : grow);
    ImVec2 kc(kx, cy);
    dl->AddRectFilled(kc - ImVec2(krx, kry) + ImVec2(0, 1.5f), kc + ImVec2(krx, kry) + ImVec2(0, 1.5f),
                      C(IM_COL32(0, 0, 0, 50)), kry);
    dl->AddRectFilled(kc - ImVec2(krx, kry), kc + ImVec2(krx, kry), C(col::Knob), ImMin(krx, kry));
    ScaleVerts(dl, vs, (a + b) * 0.5f, Jelly(id, held, hovered, 0.6f));

    RowEnd();
    return changed;
}

static bool Combo(const char* title, const char* desc, int* v, const char* const items[], int count, float w)
{
    float rx, cy;
    if (!RowBegin(title, desc, &rx, &cy)) return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiStorage* st = ImGui::GetStateStorage();

    const float h = 37.0f;
    ImVec2 a(rx - w, cy - h * 0.5f), b(rx, cy + h * 0.5f);
    ImGui::SetCursorScreenPos(a);
    ImGuiID id = ImGui::GetID("##box");
    const ImGuiID openId = SubId(id, "o");
    if (ImGui::InvisibleButton("##box", b - a)) {
        ImGui::OpenPopup("##pop");
        SpringSet(openId, 0.0f, 0.0f);
    }
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    bool open = ImGui::IsPopupOpen("##pop");
    float hov  = Anim(SubId(id, "h"), (hovered || open) ? 1.0f : 0.0f);
    float chev = Spring(SubId(id, "ch"), open ? 1.0f : 0.0f, 15.0f, 0.4f);

    // The value drops in from above when it changes.
    if (st->GetInt(SubId(id, "last"), *v) != *v) {
        st->SetInt(SubId(id, "last"), *v);
        SpringSet(SubId(id, "txt"), 0.0f, 0.0f);
    }
    float tt = Spring(SubId(id, "txt"), 1.0f, 15.0f, 0.45f);

    int vs = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, C(Mix(col::Control, col::ControlHov, hov)), h * 0.5f);
    dl->PushClipRect(a, b, true);
    TextAt(dl, g_fontSemi, 19.5f, a.x + 16, cy - (1.0f - tt) * 12.0f, C(Fade(col::ControlText, ImSaturate(tt * 1.5f))), items[*v]);
    dl->PopClipRect();
    Chevron(dl, ImVec2(b.x - 28, cy + 0.5f), 4.6f, IM_PI * (1.0f - chev), C(IM_COL32(172, 174, 188, 255)), 2.0f);
    ScaleVerts(dl, vs, (a + b) * 0.5f, Jelly(id, held, hovered, 0.7f));

    // Popup unrolls downward with a wobble; items stagger in behind it.
    bool changed = false;
    const float itemH = 32.0f, gap = 2.0f, pad = 5.0f;
    const float fullH = count * itemH + (count - 1) * gap + pad * 2.0f;
    float p  = open ? Spring(openId, 1.0f, 15.0f, 0.45f) : 0.0f;
    float sy = ImMax(p, 0.03f);
    float pw = w * (0.9f + 0.1f * p);
    float pa = ImSaturate(p * 2.5f);
    ImGui::SetNextWindowPos(ImVec2(a.x + (w - pw) * 0.5f, b.y + 6.0f));
    ImGui::SetNextWindowSize(ImVec2(pw, fullH * sy));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pa);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, col::PopupBg);
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(58, 62, 78, 255));
    if (ImGui::BeginPopup("##pop", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        const float step = itemH + gap;      // items keep their spacing; the window reveals them

        int hovIdx = -1;
        if (ImGui::IsWindowHovered()) {
            float my = ImGui::GetIO().MousePos.y - (wp.y + pad);
            int k = (int)std::floor(my / step);
            if (my >= 0.0f && k >= 0 && k < count) hovIdx = k;
        }
        // One highlight that glides to whatever is hovered (or the selection).
        float hl = Spring(ImGui::GetID("##hl"), (float)(hovIdx >= 0 ? hovIdx : *v), 20.0f, 0.55f);
        float hy = wp.y + pad + hl * step;
        pdl->AddRectFilled(ImVec2(wp.x + pad, hy), ImVec2(wp.x + pw - pad, hy + itemH),
                           Fade(IM_COL32(60, 64, 84, 255), pa), 12.0f);

        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            float iy = wp.y + pad + i * step;
            ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, iy));
            if (ImGui::InvisibleButton("##it", ImVec2(pw - pad * 2.0f, itemH))) {
                *v = i;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            float ih = Spring(ImGui::GetID("##ih"), hovIdx == i ? 1.0f : 0.0f, 16.0f, 0.55f);
            float ia = ImSaturate(p * 3.0f - i * 0.35f);
            ImU32 tc = i == *v ? col::Accent : Mix(IM_COL32(190, 194, 208, 255), col::Text, ih);
            TextAt(pdl, g_fontSemi, 18.5f, wp.x + pad + 11 + ih * 4.0f + (1.0f - ia) * 12.0f,
                   iy + itemH * 0.5f + (1.0f - ia) * 6.0f, Fade(tc, ia * pa), items[i]);
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(5);

    RowEnd();
    return changed;
}

static const char* KeyName(int key)
{
    if (key == ImGuiKey_None) return "Unbound";
    switch (key) {
    case ImGuiKey_MouseLeft:   return "Mouse 1";
    case ImGuiKey_MouseRight:  return "Mouse 2";
    case ImGuiKey_MouseMiddle: return "Mouse 3";
    case ImGuiKey_MouseX1:     return "Mouse 4";
    case ImGuiKey_MouseX2:     return "Mouse 5";
    }
    return ImGui::GetKeyName((ImGuiKey)key);
}

static bool Keybind(const char* title, const char* desc, int* key)
{
    float rx, cy;
    if (!RowBegin(title, desc, &rx, &cy)) return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiStorage* st = ImGui::GetStateStorage();

    ImGuiID id = ImGui::GetID("##bind");
    bool waiting = g_bindingId == id;
    bool changed = false;

    const char* label = KeyName(*key);
    float targetW = waiting ? 62.0f : ImMax(TextSize(g_fontSemi, 18.6f, label).x + 22.0f, 50.0f);
    float aw = Spring(SubId(id, "w"), targetW, 15.0f, 0.45f);
    const float h = 31.0f;
    ImVec2 a(rx - aw, cy - h * 0.5f), b(rx, cy + h * 0.5f);
    ImGui::SetCursorScreenPos(a);
    bool clicked = ImGui::InvisibleButton("##bind", b - a);
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    float hov = Anim(SubId(id, "h"), (hovered || waiting) ? 1.0f : 0.0f);

    if (clicked && !waiting) {
        g_bindingId = id;
    } else if (waiting) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            *key = ImGuiKey_None;
            g_bindingId = 0;
            changed = true;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()) {
            g_bindingId = 0;                                        // click elsewhere cancels
        } else {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                if (k == ImGuiKey_MouseLeft || k == ImGuiKey_MouseWheelX || k == ImGuiKey_MouseWheelY) continue;
                if (k >= ImGuiKey_ReservedForModCtrl && k <= ImGuiKey_ReservedForModSuper) continue;
                if (ImGui::IsKeyPressed((ImGuiKey)k, false)) {
                    *key = k;
                    g_bindingId = 0;
                    changed = true;
                    break;
                }
            }
        }
    }
    waiting = g_bindingId == id;
    if (changed || (clicked && waiting)) SpringSet(SubId(id, "pop"), 0.6f, 0.0f);
    float pop = Spring(SubId(id, "pop"), 1.0f, 16.0f, 0.32f);

    const ImVec2 ctr = (a + b) * 0.5f;
    int vs = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, C(Mix(col::Control, col::ControlHov, hov)), h * 0.5f);
    if (waiting) {
        // Breathing rim and three hopping dots while listening for a key.
        dl->AddRect(a, b, C(Fade(col::Accent, 0.45f + 0.3f * std::sin(g_time * 5.0f))), h * 0.5f, 0, 1.5f);
        for (int i = 0; i < 3; ++i) {
            float hop = ImMax(0.0f, std::sin(g_time * 7.0f - i * 0.8f)) * 4.5f;
            dl->AddCircleFilled(ImVec2(ctr.x + (i - 1) * 9.0f, cy + 1.5f - hop), 2.6f, C(col::Accent), 12);
        }
    } else {
        int tvs = dl->VtxBuffer.Size;
        TextCentered(dl, g_fontSemi, 18.6f, ctr.x, cy, C(col::ControlText), label);
        ScaleVerts(dl, tvs, ctr, ImVec2(pop, pop));
    }
    ScaleVerts(dl, vs, ctr, Jelly(id, held, hovered, 0.8f));

    RowEnd();
    return changed;
}

static bool Slider(const char* title, const char* desc, float* v, float mn, float mx, const char* fmt)
{
    float rx, cy;
    if (!RowBegin(title, desc, &rx, &cy)) return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float w = 180.0f;
    ImVec2 a(rx - w, cy - 13.0f), b(rx, cy + 13.0f);
    ImGui::SetCursorScreenPos(a);
    ImGuiID id = ImGui::GetID("##sl");
    ImGui::InvisibleButton("##sl", b - a);
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    bool changed = false;
    if (held) {
        float t = ImSaturate((ImGui::GetIO().MousePos.x - (a.x + 8)) / (w - 16));
        float nv = mn + (mx - mn) * t;
        if (nv != *v) { *v = nv; changed = true; }
    }

    // Knob trails the value on a spring and stretches with its speed.
    float vel = 0.0f;
    float t    = Spring(SubId(id, "t"), (*v - mn) / (mx - mn), 18.0f, 0.6f, &vel);
    float kr   = Spring(SubId(id, "r"), held ? 11.0f : (hovered ? 9.5f : 8.0f), 18.0f, 0.32f);
    float halo = Anim(SubId(id, "halo"), held ? 1.0f : 0.0f, 12.0f);
    float tc = ImSaturate(t);

    const float tx0 = a.x + 8, tx1 = b.x - 8;
    dl->AddRectFilled(ImVec2(tx0 - 3, cy - 3), ImVec2(tx1 + 3, cy + 3), C(col::Control), 3.0f);
    float kx = ImLerp(tx0, tx1, tc);
    dl->AddRectFilled(ImVec2(tx0 - 3, cy - 3), ImVec2(kx + 3, cy + 3), C(col::Accent), 3.0f);
    if (halo > 0.01f)
        dl->AddCircleFilled(ImVec2(kx, cy), kr + 6.0f * halo, C(Fade(col::Accent, 0.22f * halo)), 32);
    const float sx = 1.0f + ImMin(std::fabs(vel) * 0.06f, 0.45f);
    const float krx = kr * sx, kry = kr / std::sqrt(sx);
    dl->AddRectFilled(ImVec2(kx - krx, cy - kry + 1.5f), ImVec2(kx + krx, cy + kry + 1.5f), C(IM_COL32(0, 0, 0, 50)), kry);
    dl->AddRectFilled(ImVec2(kx - krx, cy - kry), ImVec2(kx + krx, cy + kry), C(col::Knob), ImMin(krx, kry));

    // Value pill: width follows the text, puffs up while dragging.
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, *v);
    float vw = Spring(SubId(id, "vw"), TextSize(g_fontMedium, 17.5f, buf).x, 18.0f, 0.6f);
    float ps = Spring(SubId(id, "ps"), held ? 1.1f : 1.0f, 16.0f, 0.3f);
    ImVec2 pa(a.x - vw - 36, cy - 15.5f), pb(a.x - 10, cy + 15.5f);
    int vs = dl->VtxBuffer.Size;
    dl->AddRectFilled(pa, pb, C(Mix(col::Control, col::ControlHov, halo)), 15.5f);
    TextCentered(dl, g_fontMedium, 17.5f, (pa.x + pb.x) * 0.5f, cy, C(Mix(col::ControlText, col::Accent, halo)), buf);
    ScaleVerts(dl, vs, (pa + pb) * 0.5f, ImVec2(ps, ps));

    RowEnd();
    return changed;
}

static bool Button(const char* title, const char* desc, const char* label)
{
    float rx, cy;
    if (!RowBegin(title, desc, &rx, &cy)) return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiStorage* st = ImGui::GetStateStorage();

    float w = TextSize(g_fontSemi, 18.6f, label).x + 34.0f;
    const float h = 34.0f;
    ImVec2 a(rx - w, cy - h * 0.5f), b(rx, cy + h * 0.5f);
    ImGui::SetCursorScreenPos(a);
    ImGuiID id = ImGui::GetID("##btn");
    bool pressed = ImGui::InvisibleButton("##btn", b - a);
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    float hov = Anim(SubId(id, "h"), hovered ? 1.0f : 0.0f);

    // Ripple: a pill of light that spreads out from where you clicked.
    const ImGuiID rtId = SubId(id, "rt"), rxId = SubId(id, "rx");
    if (ImGui::IsItemActivated()) {
        st->SetFloat(rtId, 0.0f);
        st->SetFloat(rxId, ImGui::GetIO().MousePos.x - a.x);
    }
    float rt = st->GetFloat(rtId, 10.0f) + g_dt;
    st->SetFloat(rtId, rt);

    int vs = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, C(Mix(col::Control, col::ControlHov, hov)), h * 0.5f);
    const float life = 0.6f;
    if (rt < life) {
        float k = rt / life;
        float e = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);
        float cxr = a.x + st->GetFloat(rxId, w * 0.5f), half = ImMax(e * w, h * 0.5f);
        dl->AddRectFilled(ImVec2(ImMax(a.x, cxr - half), a.y), ImVec2(ImMin(b.x, cxr + half), b.y),
                          C(Fade(col::Accent, 0.45f * (1.0f - k))), h * 0.5f);
    }
    TextCentered(dl, g_fontSemi, 18.6f, (a.x + b.x) * 0.5f, cy, C(Mix(col::ControlText, col::Accent, hov)), label);
    ScaleVerts(dl, vs, (a + b) * 0.5f, Jelly(id, held, hovered, 1.0f));

    RowEnd();
    return pressed;
}

// ------------------------------------------------------------------ pages --

static const char* kTargeting[] = { "Default", "Closest", "Distance", "Health" };
static const char* kKeyModes[]  = { "Hold", "Toggle", "Always" };
static const char* kAimModes[]  = { "Camera", "Mouse" };
static const char* kHitParts[]  = { "Head", "Torso", "Closest" };
static const char* kBoxStyles[] = { "Full", "Corners", "3D" };
static const char* kXhair[]     = { "Cross", "Dot", "Circle" };
static const char* kScales[]    = { "90%", "100%", "110%", "125%" };
static const char* kConfigs[]   = { "default", "legit", "rage", "hvh" };
static const char* kProfiles[]  = { "Auto", "Universal" };

static void PageAimingMain(Config& c)
{
    BeginSection("Main");
    Toggle("Enabled", "Masterswitch for the aimbot", &c.aimEnabled);
    Combo("Targeting Mode", "How the aimbot selects targets", &c.targeting, kTargeting, 4, 180);
    Keybind("Aimbot Key", "Activates the aimbot", &c.aimKey);
    Combo("Key Mode", "How the aimbot key behaves", &c.keyMode, kKeyModes, 3, 143);
    Toggle("Sticky", "Locks onto the current target until it dies", &c.sticky);
    Combo("Aim Mode", "How the aimbot moves your aim", &c.aimMode, kAimModes, 2, 143);
    Toggle("No Sleep", "Runs the aimbot at full speed with no delays for maximum precision", &c.noSleep);
    EndSection();

    BeginSection("Smoothing");
    Slider("Smoothness", "Higher values move your aim more gently", &c.smoothness, 0, 100, "%.0f%%");
    Slider("Field of View", "Radius around the crosshair to search", &c.fov, 10, 500, "%.0f");
    Toggle("Show FOV", "Draws the field of view circle", &c.showFov);
    EndSection();

    BeginSection("Targeting");
    Combo("Hit Part", "Which body part to aim at", &c.hitPart, kHitParts, 3, 143);
    Toggle("Prediction", "Leads moving targets", &c.prediction);
    Slider("Prediction Amount", "How far ahead to lead targets", &c.predAmount, 0, 0.5f, "%.3f");
    Toggle("Team Check", "Ignores players on your team", &c.teamCheck);
    Toggle("Wall Check", "Ignores targets behind walls", &c.wallCheck);
    Toggle("Alive Check", "Ignores knocked or dead players", &c.aliveCheck);
    EndSection();
}

static void PageAimingSilent(Config& c)
{
    BeginSection("Silent");
    Toggle("Enabled", "Masterswitch for silent aim", &c.silentEnabled);
    Keybind("Silent Key", "Activates silent aim", &c.silentKey);
    Slider("Hit Chance", "Chance for a shot to be redirected", &c.hitChance, 0, 100, "%.0f%%");
    Combo("Hit Part", "Which body part to redirect to", &c.silentPart, kHitParts, 3, 143);
    Slider("Field of View", "Radius around the crosshair to search", &c.silentFov, 10, 500, "%.0f");
    Toggle("Show FOV", "Draws the field of view circle", &c.silentShowFov);
    EndSection();
}

static void PageAimingTrigger(Config& c)
{
    BeginSection("Triggerbot");
    Toggle("Enabled", "Masterswitch for the triggerbot", &c.trigEnabled);
    Keybind("Trigger Key", "Activates the triggerbot", &c.trigKey);
    Combo("Key Mode", "How the trigger key behaves", &c.trigMode, kKeyModes, 3, 143);
    Slider("Delay", "Milliseconds to wait before shooting", &c.trigDelay, 0, 300, "%.0f ms");
    Toggle("Team Check", "Ignores players on your team", &c.trigTeam);
    EndSection();
}

static void PageVisualsPlayers(Config& c)
{
    BeginSection("Players");
    Toggle("Enabled", "Masterswitch for player visuals", &c.espEnabled);
    Toggle("Boxes", "Draws a box around players", &c.boxes);
    Combo("Box Style", "How boxes are drawn", &c.boxStyle, kBoxStyles, 3, 143);
    Toggle("Names", "Shows player names", &c.names);
    Toggle("Health Bars", "Shows player health", &c.health);
    Toggle("Tracers", "Draws lines to players", &c.tracers);
    Toggle("Chams", "Highlights players through walls", &c.chams);
    Slider("Max Distance", "Hides players further than this", &c.espDist, 100, 5000, "%.0f");
    EndSection();
}

static void PageVisualsWorld(Config& c)
{
    BeginSection("World");
    Toggle("Fullbright", "Lights up the whole map", &c.fullbright);
    Toggle("No Fog", "Removes distance fog", &c.noFog);
    Toggle("Custom Time", "Overrides the time of day", &c.customTime);
    Slider("Time of Day", "Clock time used when overriding", &c.timeOfDay, 0, 24, "%.1f h");
    EndSection();
}

static void PageVisualsCrosshair(Config& c)
{
    BeginSection("Crosshair");
    Toggle("Enabled", "Draws a custom crosshair", &c.xhair);
    Combo("Style", "Shape of the crosshair", &c.xhairStyle, kXhair, 3, 143);
    Slider("Size", "Crosshair size in pixels", &c.xhairSize, 2, 40, "%.0f px");
    Toggle("Spin", "Slowly rotates the crosshair", &c.xhairSpin);
    EndSection();
}

static void PageMiscMovement(Config& c)
{
    BeginSection("Movement");
    Toggle("Walkspeed", "Overrides your walk speed", &c.walkspeed);
    Slider("Speed", "Walk speed while enabled", &c.walkAmount, 16, 100, "%.0f");
    Toggle("Infinite Jump", "Jump again while in the air", &c.infJump);
    Toggle("Noclip", "Walk through walls", &c.noclip);
    Keybind("Noclip Key", "Toggles noclip", &c.noclipKey);
    EndSection();
}

static void PageMiscCharacter(Config& c)
{
    BeginSection("Character");
    Toggle("Anti AFK", "Prevents being kicked for idling", &c.antiAfk);
    Toggle("Auto Respawn", "Respawns instantly after dying", &c.autoRespawn);
    Slider("Camera FOV", "Your camera's field of view", &c.fovCamera, 50, 120, "%.0f");
    EndSection();
}

static void PageMiscMenu(Config& c)
{
    BeginSection("Menu");
    Keybind("Menu Key", "Opens and closes this menu", &c.menuKey);
    Toggle("Watermark", "Shows the neko watermark", &c.watermark);
    Toggle("Keybind List", "Shows active keybinds on screen", &c.keybindList);
    Combo("UI Scale", "Size of the menu", &c.uiScale, kScales, 4, 143);
    Button("Unload", "Removes neko from the game", "Unload");
    EndSection();
}

static void PageCloudConfigs(Config& c)
{
    BeginSection("Configs");
    Combo("Config", "The config to load or save", &c.config, kConfigs, 4, 180);
    Button("Load", "Applies the selected config", "Load");
    Button("Save", "Overwrites the selected config", "Save");
    Toggle("Autoload", "Loads the selected config on inject", &c.autoload);
    Toggle("Autosave", "Saves changes automatically", &c.autosave);
    EndSection();
}

static void PageCloudScripts(Config&)
{
    BeginSection("Scripts");
    Button("Browse", "Community scripts shared through the cloud", "Open");
    Button("Refresh", "Fetches the latest script list", "Refresh");
    EndSection();
}

static void PageGamesSupported(Config&)
{
    BeginSection("Supported");
    Button("Universal", "Works in every experience", "Active");
    Button("Arsenal", "Game specific features and checks", "View");
    Button("Da Hood", "Game specific features and checks", "View");
    Button("Rivals", "Game specific features and checks", "View");
    EndSection();
}

static void PageGamesSettings(Config& c)
{
    BeginSection("Settings");
    Toggle("Game Overrides", "Uses game specific settings when available", &c.gameOverrides);
    Combo("Profile", "Which settings profile to use", &c.gameProfile, kProfiles, 2, 143);
    EndSection();
}

static void DrawContent(ImDrawList* dl)
{
    PanelRect(dl, P(318, 108), P(1047, 677), 16.0f);

    ImGui::SetCursorScreenPos(P(318, 108));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    ImGui::BeginChild("##content", ImVec2(729, 567), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    g_childWin = ImGui::GetCurrentWindow();
    g_rowIdx = 0;
    g_secIdx = 0;

    // Smooth wheel scrolling with a rubber band past either end.
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID targetId = ImGui::GetID("##scrolltarget");
    const float maxY = ImGui::GetScrollMaxY();
    float target = st->GetFloat(targetId, 0.0f);
    // Content height is only known after the first frame on a page, so a
    // requested offset waits until there is something to scroll.
    if (g_pendingScroll >= 0.0f && (g_pendingScroll == 0.0f || maxY > 0.0f)) {
        target = ImMin(g_pendingScroll, maxY);
        ImGui::SetScrollY(target);
        g_pendingScroll = -1.0f;
    }
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
        target -= ImGui::GetIO().MouseWheel * 90.0f;
    const float clamped = ImClamp(target, 0.0f, maxY);
    float over = ImClamp(target - clamped, -80.0f, 80.0f);
    over *= 1.0f - ImSaturate(g_dt * 9.0f);                  // band relaxes back
    target = clamped + over;
    st->SetFloat(targetId, target);
    float next = ImLerp(win->Scroll.y, clamped, ImSaturate(g_dt * 14.0f));
    ImGui::SetScrollY(std::fabs(next - clamped) < 0.5f ? clamped : next);
    // Overscroll nudges the content, which then springs back.
    float shift = Spring(ImGui::GetID("##over"), -over * 0.6f, 16.0f, 0.45f);

    ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2(18, 22 + shift));
    Config& c = g_cfg;
    switch (g_tab * 3 + g_sub[g_tab]) {
    case 0:  PageAimingMain(c);      break;
    case 1:  PageAimingSilent(c);    break;
    case 2:  PageAimingTrigger(c);   break;
    case 3:  PageVisualsPlayers(c);  break;
    case 4:  PageVisualsWorld(c);    break;
    case 5:  PageVisualsCrosshair(c);break;
    case 6:  PageMiscMovement(c);    break;
    case 7:  PageMiscCharacter(c);   break;
    case 8:  PageMiscMenu(c);        break;
    case 9:  PageCloudConfigs(c);    break;
    case 10: PageCloudScripts(c);    break;
    case 12: PageGamesSupported(c);  break;
    case 13: PageGamesSettings(c);   break;
    }
    ImGui::Dummy(ImVec2(1, 4));

    // Edge fades hint that the list continues. Drawn inside the child so they
    // sit above the rows (the parent's draw list renders underneath).
    float scroll = ImGui::GetScrollY(), maxScroll = ImGui::GetScrollMaxY();
    float fb = Anim(ImGui::GetID("##fadeb"), scroll < maxScroll - 1.0f ? 1.0f : 0.0f, 10.0f);
    float ft = Anim(ImGui::GetID("##fadet"), scroll > 1.0f ? 1.0f : 0.0f, 10.0f);
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    if (fb > 0.0f)
        cdl->AddRectFilledMultiColor(P(336, 606), P(1030, 676),
                                     Fade(col::Card, 0.0f), Fade(col::Card, 0.0f),
                                     Fade(col::Card, 0.9f * fb), Fade(col::Card, 0.9f * fb));
    if (ft > 0.0f)
        cdl->AddRectFilledMultiColor(P(336, 108), P(1030, 150),
                                     Fade(col::Panel, 0.9f * ft), Fade(col::Panel, 0.9f * ft),
                                     Fade(col::Panel, 0.0f), Fade(col::Panel, 0.0f));

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
}

// ------------------------------------------------------------------ frame --

void Render(bool open)
{
    g_dt = ImMin(ImGui::GetIO().DeltaTime, 1.0f / 20.0f);
    g_time += g_dt;
    g_pageTime += g_dt;
    g_tabsTime += g_dt;

    // Pop in / out. x and y run on separate springs so the window wobbles like
    // jelly on the way in; closing is critically damped and quick.
    static float sx = 0.0f, vx = 0.0f, sy = 0.0f, vy = 0.0f, fade = 0.0f;
    static bool wasOpen = false;
    SpringStep(sx, vx, open ? 1.0f : 0.0f, open ? 14.0f : 20.0f, open ? 0.4f : 1.0f);
    SpringStep(sy, vy, open ? 1.0f : 0.0f, open ? 11.0f : 20.0f, open ? 0.36f : 1.0f);
    fade = ImLerp(fade, open ? 1.0f : 0.0f, ImSaturate(g_dt * (open ? 12.0f : 16.0f)));
    if (open && !wasOpen) { g_pageTime = 0.0f; g_tabsTime = 0.0f; }   // replay the intro
    if (!open && wasOpen) { ImGui::ClosePopupsExceptModals(); g_bindingId = 0; }
    wasOpen = open;
    if (!open && fade < 0.01f) {
        fade = sx = sy = vx = vy = 0.0f;
        return;
    }

    const ImVec2 size(1061, 689);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::Begin("##neko", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                 (open ? 0 : ImGuiWindowFlags_NoInputs));
    ImGui::PopStyleVar(3);

    g_origin = ImGui::GetWindowPos();
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    g_childWin = nullptr;

    // Window body, then the pink rim stroked along the top edge (drawn on top
    // rather than peeking out from underneath, so it fades cleanly), with a
    // soft highlight drifting along it.
    dl->AddRectFilled(P(0, 0), P(size.x, size.y), col::WindowBg, 16.0f);
    dl->PathArcTo(P(16, 16), 15.0f, IM_PI * 1.12f, IM_PI * 1.5f, 12);
    dl->PathArcTo(P(size.x - 16, 16), 15.0f, IM_PI * 1.5f, IM_PI * 1.88f, 12);
    dl->PathStroke(col::TopBorder, 0, 2.0f);
    {
        float cx = (std::fmod(g_time * 0.16f, 1.5f) - 0.25f) * size.x;
        dl->PushClipRect(P(14, 0), P(size.x - 14, 2), true);
        const ImU32 clear = IM_COL32(255, 235, 245, 0), glow = IM_COL32(255, 235, 245, 150);
        dl->AddRectFilledMultiColor(P(cx - 140, 0), P(cx, 2), clear, glow, glow, clear);
        dl->AddRectFilledMultiColor(P(cx, 0), P(cx + 140, 2), glow, clear, clear, glow);
        dl->PopClipRect();
    }

    DrawSidebar(dl);
    DrawTopBar(dl);
    DrawContent(dl);

    ImGui::End();

    const ImVec2 scale(0.86f + 0.14f * sx, 0.86f + 0.14f * sy);
    const float alpha = ImSaturate(fade);
    if (std::fabs(scale.x - 1.0f) > 1e-4f || std::fabs(scale.y - 1.0f) > 1e-4f || alpha < 1.0f) {
        const ImVec2 pivot = g_origin + size * ImVec2(0.5f, 0.45f);
        TransformDrawList(win->DrawList, pivot, scale, alpha);
        if (g_childWin) TransformDrawList(g_childWin->DrawList, pivot, scale, alpha);
    }
}

} // namespace neko
