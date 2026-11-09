// On-screen touch controls for the Android build (drawn with raylib).
#if defined(__ANDROID__)

#include "runtime/ps2_pad.h"
#include "ps2_host_backend.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr uint16_t PAD_LEFT = 0x0080u;
    constexpr uint16_t PAD_DOWN = 0x0040u;
    constexpr uint16_t PAD_RIGHT = 0x0020u;
    constexpr uint16_t PAD_UP = 0x0010u;
    constexpr uint16_t PAD_START = 0x0008u;
    constexpr uint16_t PAD_SELECT = 0x0001u;
    constexpr uint16_t PAD_SQUARE = 0x8000u;
    constexpr uint16_t PAD_CROSS = 0x4000u;
    constexpr uint16_t PAD_CIRCLE = 0x2000u;
    constexpr uint16_t PAD_TRIANGLE = 0x1000u;
    constexpr uint16_t PAD_R1 = 0x0800u;
    constexpr uint16_t PAD_L1 = 0x0400u;
    constexpr uint16_t PAD_R2 = 0x0200u;
    constexpr uint16_t PAD_L2 = 0x0100u;

    struct Stick
    {
        int id = -1;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Layout
    {
        float u;
        float stickR;
        Vector2 leftStick;
        Vector2 rightStick;
        Vector2 dpad;
        Vector2 face;
        Rectangle l1, l2, r1, r2, startBtn, selectBtn;
    };

    Stick g_left;
    Stick g_right;
    std::atomic<uint16_t> g_pressed{0};
    std::atomic<int> g_lx{128}, g_ly{128}, g_rx{128}, g_ry{128};
    std::atomic<bool> g_active{false};

    const Color kLine{255, 255, 255, 140};

    Color shade(bool down)
    {
        return Color{255, 255, 255, static_cast<unsigned char>(down ? 150 : 70)};
    }

    Layout makeLayout(float w, float h)
    {
        Layout L{};
        const float u = std::min(w, h) / 10.0f;
        L.u = u;
        L.stickR = 1.5f * u;
        L.leftStick = Vector2{2.2f * u, h - 2.1f * u};
        L.dpad = Vector2{2.2f * u, h - 5.5f * u};
        L.rightStick = Vector2{w - 2.2f * u, h - 2.1f * u};
        L.face = Vector2{w - 2.2f * u, h - 5.5f * u};
        L.l2 = Rectangle{0.4f * u, 0.3f * u, 2.2f * u, 0.9f * u};
        L.l1 = Rectangle{0.4f * u, 1.3f * u, 2.2f * u, 0.9f * u};
        L.r2 = Rectangle{w - 2.6f * u, 0.3f * u, 2.2f * u, 0.9f * u};
        L.r1 = Rectangle{w - 2.6f * u, 1.3f * u, 2.2f * u, 0.9f * u};
        L.selectBtn = Rectangle{w * 0.5f - 2.0f * u, h - 1.1f * u, 1.7f * u, 0.7f * u};
        L.startBtn = Rectangle{w * 0.5f + 0.3f * u, h - 1.1f * u, 1.7f * u, 0.7f * u};
        return L;
    }

    bool inCircle(Vector2 p, Vector2 c, float r)
    {
        const float dx = p.x - c.x;
        const float dy = p.y - c.y;
        return dx * dx + dy * dy <= r * r;
    }

    bool inRect(Vector2 p, Rectangle r)
    {
        return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
    }

    Vector2 shifted(Vector2 c, float dx, float dy)
    {
        return Vector2{c.x + dx, c.y + dy};
    }

    void updateStick(Stick &s, Vector2 c, float r, Vector2 p)
    {
        float dx = (p.x - c.x) / r;
        float dy = (p.y - c.y) / r;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f)
        {
            dx /= len;
            dy /= len;
        }
        if (len < 0.08f)
        {
            dx = 0.0f;
            dy = 0.0f;
        }
        s.x = dx;
        s.y = dy;
    }

    int toAxis(float v)
    {
        return std::clamp(128 + static_cast<int>(v * 127.0f), 0, 255);
    }

    void updateTouch(const Layout &L)
    {
        const int count = GetTouchPointCount();
        auto stillDown = [count](int id)
        {
            for (int i = 0; i < count; ++i)
            {
                if (GetTouchPointId(i) == id)
                    return true;
            }
            return false;
        };
        if (g_left.id != -1 && !stillDown(g_left.id))
            g_left = Stick{};
        if (g_right.id != -1 && !stillDown(g_right.id))
            g_right = Stick{};

        uint16_t pressed = 0;
        const float u = L.u;
        const float hit = 0.85f * u;
        const float f = 0.95f * u;

        for (int i = 0; i < count; ++i)
        {
            const Vector2 p = GetTouchPosition(i);
            const int id = GetTouchPointId(i);

            if (id == g_left.id)
            {
                updateStick(g_left, L.leftStick, L.stickR, p);
                continue;
            }
            if (id == g_right.id)
            {
                updateStick(g_right, L.rightStick, L.stickR, p);
                continue;
            }
            if (g_left.id == -1 && inCircle(p, L.leftStick, L.stickR * 1.25f))
            {
                g_left.id = id;
                updateStick(g_left, L.leftStick, L.stickR, p);
                continue;
            }
            if (g_right.id == -1 && inCircle(p, L.rightStick, L.stickR * 1.25f))
            {
                g_right.id = id;
                updateStick(g_right, L.rightStick, L.stickR, p);
                continue;
            }

            if (inCircle(p, shifted(L.dpad, 0, -u), hit)) pressed |= PAD_UP;
            if (inCircle(p, shifted(L.dpad, 0, u), hit)) pressed |= PAD_DOWN;
            if (inCircle(p, shifted(L.dpad, -u, 0), hit)) pressed |= PAD_LEFT;
            if (inCircle(p, shifted(L.dpad, u, 0), hit)) pressed |= PAD_RIGHT;

            if (inCircle(p, shifted(L.face, 0, -f), hit)) pressed |= PAD_TRIANGLE;
            if (inCircle(p, shifted(L.face, 0, f), hit)) pressed |= PAD_CROSS;
            if (inCircle(p, shifted(L.face, -f, 0), hit)) pressed |= PAD_SQUARE;
            if (inCircle(p, shifted(L.face, f, 0), hit)) pressed |= PAD_CIRCLE;

            if (inRect(p, L.l1)) pressed |= PAD_L1;
            if (inRect(p, L.l2)) pressed |= PAD_L2;
            if (inRect(p, L.r1)) pressed |= PAD_R1;
            if (inRect(p, L.r2)) pressed |= PAD_R2;
            if (inRect(p, L.startBtn)) pressed |= PAD_START;
            if (inRect(p, L.selectBtn)) pressed |= PAD_SELECT;
        }

        g_pressed.store(pressed);
        g_lx.store(toAxis(g_left.x));
        g_ly.store(toAxis(g_left.y));
        g_rx.store(toAxis(g_right.x));
        g_ry.store(toAxis(g_right.y));
    }

    void drawStick(Vector2 c, float r, const Stick &s, float u)
    {
        DrawCircleV(c, r, Color{255, 255, 255, 25});
        DrawRing(c, r - 0.06f * u, r, 0.0f, 360.0f, 48, kLine);
        const Vector2 k{c.x + s.x * r * 0.55f, c.y + s.y * r * 0.55f};
        DrawCircleV(k, 0.7f * u, shade(s.id != -1));
    }

    void drawLabelRect(Rectangle r, const char *text, bool down, float u)
    {
        DrawRectangleRounded(r, 0.4f, 8, shade(down));
        const int fs = static_cast<int>(u * 0.42f);
        const int tw = MeasureText(text, fs);
        DrawText(text,
                 static_cast<int>(r.x + (r.width - static_cast<float>(tw)) * 0.5f),
                 static_cast<int>(r.y + (r.height - static_cast<float>(fs)) * 0.5f),
                 fs, Color{255, 255, 255, 230});
    }

    void drawOverlay(const Layout &L, uint16_t pressed)
    {
        const float u = L.u;
        const Color white{255, 255, 255, 230};

        drawStick(L.leftStick, L.stickR, g_left, u);
        drawStick(L.rightStick, L.stickR, g_right, u);

        // D-pad
        struct Dir
        {
            float dx, dy, rot;
            uint16_t bit;
        };
        const Dir dirs[4] = {
            {0.0f, -1.0f, -90.0f, PAD_UP},
            {0.0f, 1.0f, 90.0f, PAD_DOWN},
            {-1.0f, 0.0f, 180.0f, PAD_LEFT},
            {1.0f, 0.0f, 0.0f, PAD_RIGHT}};
        for (const Dir &d : dirs)
        {
            const Vector2 c{L.dpad.x + d.dx * u, L.dpad.y + d.dy * u};
            DrawCircleV(c, 0.7f * u, shade((pressed & d.bit) != 0));
            DrawPoly(c, 3, 0.3f * u, d.rot, white);
        }

        // Face buttons
        const float f = 0.95f * u;
        const Vector2 tri{L.face.x, L.face.y - f};
        const Vector2 crs{L.face.x, L.face.y + f};
        const Vector2 sqr{L.face.x - f, L.face.y};
        const Vector2 cir{L.face.x + f, L.face.y};
        DrawCircleV(tri, 0.72f * u, shade((pressed & PAD_TRIANGLE) != 0));
        DrawCircleV(crs, 0.72f * u, shade((pressed & PAD_CROSS) != 0));
        DrawCircleV(sqr, 0.72f * u, shade((pressed & PAD_SQUARE) != 0));
        DrawCircleV(cir, 0.72f * u, shade((pressed & PAD_CIRCLE) != 0));

        DrawPolyLinesEx(tri, 3, 0.4f * u, -90.0f, 0.09f * u, white);
        DrawLineEx(Vector2{crs.x - 0.28f * u, crs.y - 0.28f * u},
                   Vector2{crs.x + 0.28f * u, crs.y + 0.28f * u}, 0.09f * u, white);
        DrawLineEx(Vector2{crs.x - 0.28f * u, crs.y + 0.28f * u},
                   Vector2{crs.x + 0.28f * u, crs.y - 0.28f * u}, 0.09f * u, white);
        DrawRectangleLinesEx(Rectangle{sqr.x - 0.3f * u, sqr.y - 0.3f * u, 0.6f * u, 0.6f * u},
                             0.09f * u, white);
        DrawRing(cir, 0.28f * u, 0.38f * u, 0.0f, 360.0f, 32, white);

        // Shoulders, start, select
        drawLabelRect(L.l2, "L2", (pressed & PAD_L2) != 0, u);
        drawLabelRect(L.l1, "L1", (pressed & PAD_L1) != 0, u);
        drawLabelRect(L.r2, "R2", (pressed & PAD_R2) != 0, u);
        drawLabelRect(L.r1, "R1", (pressed & PAD_R1) != 0, u);
        drawLabelRect(L.selectBtn, "SELECT", (pressed & PAD_SELECT) != 0, u);
        drawLabelRect(L.startBtn, "START", (pressed & PAD_START) != 0, u);
    }
} // namespace

// Called every frame between BeginDrawing() and EndDrawing().
void ps2x_touch_overlay_draw()
{
    if (IsGamepadAvailable(0))
    {
        g_active.store(false);
        return;
    }
    const Layout L = makeLayout(static_cast<float>(GetScreenWidth()),
                                static_cast<float>(GetScreenHeight()));
    updateTouch(L);
    g_active.store(true);
    drawOverlay(L, g_pressed.load());
}

// Called by the pad backend when the guest reads the controller.
void ps2x_touch_apply(uint16_t &btns, uint8_t *data)
{
    if (!g_active.load())
        return;
    btns = static_cast<uint16_t>(btns & ~g_pressed.load());
    data[6] = static_cast<uint8_t>(g_lx.load());
    data[7] = static_cast<uint8_t>(g_ly.load());
    data[4] = static_cast<uint8_t>(g_rx.load());
    data[5] = static_cast<uint8_t>(g_ry.load());
}

#endif // __ANDROID__
