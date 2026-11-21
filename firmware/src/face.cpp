#include "face.h"
#include "clawd_policy.h"
#include "theme.h"
#include "perf.h"
#include "field.h"
#include "face_motion.h"
#include "reminder_policy.h"
#include "hal/board_caps.h"
#include <lvgl.h>
#include <Arduino.h>
#include <string.h>
#include <math.h>

// Geometry is authored on a 480 px square (design system storyboards) and
// scaled by the panel's shorter side; the eye block is centred vertically.
//   eyes: 44 x 120 px, centres 160 px apart, top at y 150
//   arms (only while moving): 48 x 56 px nubs, top at y 240
//   squint: u = 12 px cells, a 5-row chevron per eye (> <)
//   glance: 20 px, far eye 1.5x
// The rise and the drop are face_motion.cpp: straight up and down with a flat
// top edge, arms on the rise, squash and stretch on the eyes, follow-through
// (docs/clawd-director.md, "The face's motion").
#define BLINK_MS   180
#define BLINK_GAP  140
#define GLANCE_MS  280

#define COL_FACE   theme_color(TH_CLAWD)      // Clawd orange by default
#define COL_EYE    theme_color(TH_EYE)

static Field     field;      // the body + arms: redraws only what changes
static lv_obj_t* root = NULL; // == field.obj: full screen, never moves; the eyes are its children
static lv_obj_t* eye_box[2];
static lv_obj_t* eye_open[2];
static lv_obj_t* squint[2][9];   // the cells of whatever shape the eye wears instead of the open eye

// What an eye shows: the open rectangle, or a shape built from the nine cells.
enum Glyph : uint8_t { GL_OPEN, GL_SQUINT, GL_HAPPY, GL_HEART, GL_CLOSED };

static int W = 480, H = 480, mind = 480;
static int eye_cx[2];        // eye centre x, px
static int eye_top = 150;    // eye block top, px (below the body's top edge)
static int eye_w0 = 44, eye_h0 = 120;
static int box_w0 = 52, box_h0 = 120;
static int arm_top = 240;    // arm nub top, px (below the body's top edge)
static int glance_px = 20;

static FaceGeom geom;
static FacePose pose;        // what's on screen
static FaceMove move_kind = FACE_MOVE_RISE;
static bool     settling = false;   // the rise reached rest; perf's settle window is open
static int      anim_var;           // lv_anim's handle for the motion

static bool moving = false;
static bool visible = false;
static bool resting = false;

// blink: 0 none, 1 closed, 2 open gap (double), 3 closed again
static uint8_t  blink_phase = 0;
static uint32_t blink_until = 0;
static bool     blink_twice = false;
// glance: 0 none, 1 out (anim), 2 hold, 3 back (anim)
static uint8_t  glance_phase = 0;
static uint32_t glance_until = 0;
static int      glance_target = 0;
static int      glance_cur = 0;
static uint32_t glance_hold = 0;      // 0: a resting glance's random hold
static uint32_t next_act_at = 0;
static int      interest_g = 0;       // curious about a tap: resting glances go back there (px)
static ClawdRng rng;

// The rarer expressions (clawd_policy.h FaceAct): a shape swap holds for
// face_expr_hold_ms; surprise and sleepy scale the open eye's height by the
// clock (face_expr_scale_pct). None while `expr_on` is false.
static bool     expr_on = false;
static FaceAct  expr_act = FACE_BLINK;
static uint32_t expr_start = 0;
static int      eye_scale = 100;      // the open eye's height in percent; wider too above 100

// Last geometry handed to LVGL per eye, so an unchanged eye costs nothing.
struct EyeBox { int x, y, w, h, ew, eh; bool solid; uint8_t g; };   // solid: the box is exactly the open eye
static EyeBox eye_last[2];
static uint8_t glyph[2] = { GL_OPEN, GL_OPEN };

// Burn-in drift (face_drift_for_step): the eyes, the glass and the sign sit
// this far off their places; one step round the ring every DRIFT_MS while up.
#define DRIFT_MS 60000
// The drift, the glass and the sign belong to the cola builds (-DCLAWD_COLA=1),
// whose face stays up for long stretches; other boards keep the face as it was.
#if CLAWD_COLA
#define FACE_EXTRAS 1
#else
#define FACE_EXTRAS 0
#endif
static int      drift_x = 0, drift_y = 0;
static unsigned drift_step = 0;
static uint32_t drift_at = 0;

// Drinking (a water reminder): eyes shut "‿", a glass with a straw in front of
// him, the water going down over the reminder. LVGL shapes on the face.
static lv_obj_t* cup = NULL;          // the glass: open-topped, white sides
static lv_obj_t* water = NULL;        // behind the glass's sides
static lv_obj_t* straw = NULL;        // a bent line up from the glass
static lv_point_precise_t straw_pts[3];
static int      cup_x = 0, cup_y = 0, cup_w = 0, cup_h = 0, cup_bw = 0;
static bool     drinking = false;
static uint32_t drink_start = 0, drink_ms = 0;
static int      water_shown = -1;

// The sign (the voice switch): a little white board on a stick, held up from
// in from the side with a speaker on it (a slash when muted, waves when on),
// held up in front of his face, then carried back out the same way.
// 0 none, 1 waiting for the face to be up and still, 2 up, 3 held, 4 down.
#define SIGN_UP_MS    760
#define SIGN_HOLD_MS  1600
#define SIGN_DOWN_MS  420
#define SIGN_WAIT_MS  4000
static lv_obj_t* sign = NULL;
static lv_obj_t* sign_slash = NULL;
static lv_obj_t* sign_wave[2] = { NULL, NULL };
static lv_obj_t* sign_box = NULL;
static lv_obj_t* sign_cone = NULL;
static lv_obj_t* sign_stick = NULL;                  // the stick: a line from the hand to the board
static lv_point_precise_t sign_stick_pts[2];
static lv_point_precise_t sign_cone_pts[5], sign_slash_pts[2], sign_wave_pts[2][3];
static uint8_t  sign_phase = 0;
static uint32_t sign_t0 = 0;
static int      sign_w = 0, sign_h = 0, sign_rest_y = 0;

static int S(int v) { return v * mind / 480; }

static void apply_eyes(void);

// Where the glyph's middle sits across the box (the squint's has always been
// the open eye's centre; the new shapes are centred in the box).
static int glyph_cx(uint8_t g) { return g == GL_SQUINT ? S(30) : box_w0 / 2; }

static void cell_at(int e, int i, int x, int y, int w, int h) {
    lv_obj_set_pos(squint[e][i], x, y);
    lv_obj_set_size(squint[e][i], w, h);
    lv_obj_clear_flag(squint[e][i], LV_OBJ_FLAG_HIDDEN);
}

// Places the nine cells for a shape (the unused ones hide) and shows them or
// the open eye.
static void layout_glyph(int e, uint8_t g) {
    const int u = S(12) > 0 ? S(12) : 1;
    for (int i = 0; i < 9; i++) lv_obj_add_flag(squint[e][i], LV_OBJ_FLAG_HIDDEN);
    if (g == GL_SQUINT) {
        const int dir = e == 0 ? +1 : -1;      // left eye ">", right eye "<"
        for (int i = 0; i < 5; i++) {
            const int dx = (dir > 0 ? i : 4 - i) * u;
            cell_at(e, i, dx, S(20) + i * u, 2 * u, u);
            if (i < 4) cell_at(e, 5 + i, dx, S(20) + (8 - i) * u, 2 * u, u);
        }
    } else if (g == GL_HAPPY) {                // "^": a 5-row arch, cells 2 wide, stepping 1
        const int h = S(7) > 0 ? S(7) : 1;
        const int x0 = (box_w0 - 10 * h) / 2, y0 = S(60) - 5 * h / 2;
        for (int i = 0; i < 5; i++) cell_at(e, i, x0 + i * h, y0 + (4 - i) * h, 2 * h, h);
        for (int i = 0; i < 4; i++) cell_at(e, 5 + i, x0 + (5 + i) * h, y0 + (1 + i) * h, 2 * h, h);
    } else if (g == GL_CLOSED) {               // "‿": the happy arch upside down, shut and content
        const int h = S(7) > 0 ? S(7) : 1;
        const int x0 = (box_w0 - 10 * h) / 2, y0 = S(60) - 5 * h / 2;
        for (int i = 0; i < 5; i++) cell_at(e, i, x0 + i * h, y0 + i * h, 2 * h, h);
        for (int i = 0; i < 4; i++) cell_at(e, 5 + i, x0 + (5 + i) * h, y0 + (3 - i) * h, 2 * h, h);
    } else if (g == GL_HEART) {                // a 7 x 6 pixel heart
        const int h = S(9) > 0 ? S(9) : 1;
        const int x0 = (box_w0 - 7 * h) / 2, y0 = S(60) - 3 * h;
        cell_at(e, 0, x0 + 1 * h, y0 + 0 * h, 2 * h, h);
        cell_at(e, 1, x0 + 4 * h, y0 + 0 * h, 2 * h, h);
        cell_at(e, 2, x0 + 0 * h, y0 + 1 * h, 7 * h, 2 * h);
        cell_at(e, 3, x0 + 1 * h, y0 + 3 * h, 5 * h, h);
        cell_at(e, 4, x0 + 2 * h, y0 + 4 * h, 3 * h, h);
        cell_at(e, 5, x0 + 3 * h, y0 + 5 * h, 1 * h, h);
    }
    if (g == GL_OPEN) lv_obj_clear_flag(eye_open[e], LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_add_flag(eye_open[e], LV_OBJ_FLAG_HIDDEN);
}

// One eye's shape. The box widens to hold a glyph before the glyph shows, so
// it never draws clipped.
static void set_glyph(int e, uint8_t g) {
    if (glyph[e] == g) return;
    glyph[e] = g;
    apply_eyes();
    layout_glyph(e, g);
}

static void set_squint(bool on) {
    set_glyph(0, on ? GL_SQUINT : GL_OPEN);
    set_glyph(1, on ? GL_SQUINT : GL_OPEN);
}

// Ends any expression and puts the eyes back to open and normal.
static void expr_clear(void) {
    expr_on = false;
    eye_scale = 100;
    set_glyph(0, GL_OPEN);
    set_glyph(1, GL_OPEN);
    apply_eyes();               // the scale back to normal even when both eyes were open already
}

static lv_obj_t* rect(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t c) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// The body's shape for a pose: a flat-topped rectangle, narrower than the
// panel by `margin` each side with an arm nub filling each gap.
static void shape_of(const FacePose* p, FieldShape* s) {
    field_shape_init(s);
    if (p->y >= H + geom.cell * 64) return;           // far below: nothing
    s->body.x1 = p->margin;
    s->body.x2 = W - 1 - p->margin;
    s->body.y1 = p->y;
    s->body.y2 = H - 1;
    if (p->margin <= 0) return;
    const int aw = face_arm_w(&geom), ah = face_arm_h(&geom);
    const int32_t xl = s->body.x1, xr = s->body.x2;
    const int32_t ya = p->y + arm_top + p->arm_dy;
    s->extra[0].x1 = xl - aw; s->extra[0].x2 = xl - 1; s->extra[0].y1 = ya; s->extra[0].y2 = ya + ah - 1;
    s->extra[1].x1 = xr + 1;  s->extra[1].x2 = xr + aw; s->extra[1].y1 = ya; s->extra[1].y2 = ya + ah - 1;
}

static EyeBox eye_next[2];

static void set_eye_objs(void* ctx) {
    (void)ctx;
    for (int e = 0; e < 2; e++) {
        const EyeBox& b = eye_next[e];
        const EyeBox& l = eye_last[e];
        if (b.w != l.w || b.h != l.h) lv_obj_set_size(eye_box[e], b.w, b.h);
        if (b.ew != l.ew || b.eh != l.eh) lv_obj_set_size(eye_open[e], b.ew, b.eh);
        if (b.g != l.g || b.ew != l.ew || b.eh != l.eh)
            lv_obj_set_pos(eye_open[e], b.g != GL_OPEN ? S(30) - b.ew / 2 : 0, 0);
        if (b.x != l.x || b.y != l.y) lv_obj_set_pos(eye_box[e], b.x, b.y);
    }
}

static lv_area_t eye_area(const EyeBox& b) {
    lv_area_t a = { b.x, b.y, b.x + b.w - 1, b.y + b.h - 1 };
    return a;
}

// The eyes ride the body, lag it by eye_dy (vertically), stretch or squash
// about their centre, and glance. An open eye's box is exactly the eye, a
// solid rectangle, so it moves with LVGL's invalidation paused and only what
// changed (old XOR new) is redrawn, a few rows or columns instead of the
// whole eye twice. A squint widens the box to hold the chevron and moves the
// ordinary way.
static void apply_eyes(void) {
    int dl, dr;
    face_eye_offsets(glance_cur, &dl, &dr);
    const int dx[2] = { dl, dr };
    const int st = pose.stretch;
    int h = eye_h0 * (1000 + st) / 1000;
    int w = eye_w0 * 1000 / (1000 + st);
    // Surprise and sleepy: the open eye's height by eye_scale, a surprised eye
    // a little wider too.
    h = h * eye_scale / 100;
    if (eye_scale > 100) w = w * (100 + (eye_scale - 100) / 2) / 100;
    if (h < 4) h = 4;
    h += h & 1; w += w & 1;
    for (int e = 0; e < 2; e++) {
        const int cx = eye_cx[e] + dx[e] + drift_x;
        const int cy = pose.y + eye_top + eye_h0 / 2 + pose.eye_dy + drift_y;
        EyeBox& b = eye_next[e];
        b.g = glyph[e];
        if (glyph[e] != GL_OPEN) { b.x = cx - glyph_cx(glyph[e]); b.y = cy - eye_h0 / 2; b.w = box_w0; b.h = box_h0; b.ew = eye_w0; b.eh = eye_h0; b.solid = false; }
        else                     { b.x = cx - w / 2; b.y = cy - h / 2;      b.w = w;      b.h = h;      b.ew = w;      b.eh = h;      b.solid = true; }
    }
    bool changed = false, quiet = true;
    for (int e = 0; e < 2; e++) {
        const EyeBox& b = eye_next[e];
        const EyeBox& l = eye_last[e];
        if (b.x != l.x || b.y != l.y || b.w != l.w || b.h != l.h || b.ew != l.ew || b.eh != l.eh || b.g != l.g)
            changed = true;
        if (!b.solid || !l.solid) quiet = false;
    }
    if (!changed) return;
    if (quiet) {
        field_quietly(root, set_eye_objs, NULL);
        for (int e = 0; e < 2; e++) {
            const lv_area_t o = eye_area(eye_last[e]), n = eye_area(eye_next[e]);
            field_invalidate_change(root, &o, &n);
        }
    } else {
        set_eye_objs(NULL);
    }
    eye_last[0] = eye_next[0];
    eye_last[1] = eye_next[1];
}

static void apply_pose(void) {
    FieldShape s;
    shape_of(&pose, &s);
    field_set_shape(&field, &s);
    apply_eyes();
}

static void place_eyes(int g) {
    glance_cur = g;
    apply_eyes();
}

static FacePose gone_pose(void) {
    FacePose p = {};
    p.y = H + geom.cell * 64;
    return p;
}

// ---- the glass and the sign ------------------------------------------------
static lv_obj_t* line(lv_obj_t* parent, const lv_point_precise_t* pts, uint32_t n, int w, lv_color_t c) {
    lv_obj_t* ln = lv_line_create(parent);
    lv_line_set_points(ln, pts, n);
    lv_obj_set_style_line_width(ln, w > 0 ? w : 1, 0);
    lv_obj_set_style_line_color(ln, c, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_obj_clear_flag(ln, LV_OBJ_FLAG_CLICKABLE);
    return ln;
}

static void make_cup(void) {
    cup_w = S(84); cup_h = S(116); cup_bw = S(5) > 0 ? S(5) : 1;
    cup_x = W / 2 - cup_w / 2;
    cup_y = eye_top + eye_h0 + S(36);
    water = rect(root, 0, 0, 1, 1, lv_color_make(0x8F, 0xD8, 0xFF));
    lv_obj_set_style_radius(water, S(4), 0);
    cup = lv_obj_create(root);
    lv_obj_remove_style_all(cup);
    lv_obj_set_size(cup, cup_w, cup_h);
    lv_obj_set_style_bg_color(cup, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cup, LV_OPA_20, 0);
    lv_obj_set_style_border_color(cup, lv_color_white(), 0);
    lv_obj_set_style_border_opa(cup, LV_OPA_90, 0);
    lv_obj_set_style_border_width(cup, cup_bw, 0);
    lv_obj_set_style_border_side(cup, (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM), 0);
    lv_obj_set_style_radius(cup, S(10), 0);
    lv_obj_clear_flag(cup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(cup, LV_OBJ_FLAG_SCROLLABLE);
    // The straw: from near the bottom of the glass, up past its rim, bent
    // toward the middle of his face.
    const int sx = W / 2 + S(14);
    straw_pts[0].x = sx;              straw_pts[0].y = cup_y + cup_h - S(26);
    straw_pts[1].x = sx;              straw_pts[1].y = cup_y - S(22);
    straw_pts[2].x = W / 2 - S(6);    straw_pts[2].y = cup_y - S(42);
    straw = line(root, straw_pts, 3, S(9), lv_color_make(0xFF, 0xF4, 0xEC));
    lv_obj_add_flag(water, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(straw, LV_OBJ_FLAG_HIDDEN);
}

// The water line for pct percent full (of the glass's inside, below a gap at
// the rim).
static void place_water(int pct) {
    const int inner = cup_h - cup_bw - S(14);
    int h = inner * pct / 100;
    if (h < 2) h = 2;
    lv_obj_set_pos(water, cup_x + drift_x + cup_bw, cup_y + drift_y + cup_h - cup_bw - h);
    lv_obj_set_size(water, cup_w - 2 * cup_bw, h);
    water_shown = pct;
}

static void place_cup(void) {
    if (!cup) return;
    lv_obj_set_pos(cup, cup_x + drift_x, cup_y + drift_y);
    lv_obj_set_pos(straw, drift_x, drift_y);
    if (water_shown >= 0) place_water(water_shown);
}

static void show_cup(bool on) {
    if (!cup) return;
    lv_obj_t* parts[] = { water, cup, straw };
    for (lv_obj_t* o : parts) {
        if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_pt(lv_point_precise_t* p, int x, int y) { p->x = x; p->y = y; }

static void make_sign(void) {
    sign_w = S(120); sign_h = S(92);
    const int stick_w = S(14);
    sign_rest_y = eye_top + eye_h0 + S(40);
    // The stick is a line from the hand (off the bottom, to the right) to the
    // board's bottom middle, drawn first so the board covers its end.
    sign_stick = line(root, sign_stick_pts, 2, stick_w, lv_color_make(0x8A, 0x5A, 0x3C));
    lv_obj_add_flag(sign_stick, LV_OBJ_FLAG_HIDDEN);
    sign = lv_obj_create(root);
    lv_obj_remove_style_all(sign);
    lv_obj_set_size(sign, sign_w, sign_h);
    lv_obj_clear_flag(sign, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(sign, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* board = rect(sign, 0, 0, sign_w, sign_h, lv_color_white());
    lv_obj_set_style_radius(board, S(14), 0);
    // A speaker in the eye colour, centred on the board (authored on 120 x 92).
    sign_box = rect(board, S(26), S(34), S(16), S(24), COL_EYE);
    static const int cone[5][2] = { {42, 34}, {60, 18}, {60, 74}, {42, 58}, {42, 34} };
    for (int i = 0; i < 5; i++) set_pt(&sign_cone_pts[i], S(cone[i][0]), S(cone[i][1]));
    sign_cone = line(board, sign_cone_pts, 5, S(6), COL_EYE);
    static const int waves[2][3][2] = { { {72, 34}, {78, 46}, {72, 58} }, { {84, 24}, {94, 46}, {84, 68} } };
    for (int w = 0; w < 2; w++) {
        for (int i = 0; i < 3; i++) set_pt(&sign_wave_pts[w][i], S(waves[w][i][0]), S(waves[w][i][1]));
        sign_wave[w] = line(board, sign_wave_pts[w], 3, S(6), COL_EYE);
    }
    set_pt(&sign_slash_pts[0], S(18), S(14));
    set_pt(&sign_slash_pts[1], S(102), S(78));
    sign_slash = line(board, sign_slash_pts, 2, S(8), lv_color_make(0xE0, 0x44, 0x3C));
    lv_obj_add_flag(sign, LV_OBJ_FLAG_HIDDEN);
}

// The sign is carried in by a hand off the bottom right: the stick pivots on
// the hand (it moves little), the board swings on the stick's end like a
// pendulum, "/" as it arrives, over to "\" and back, and rests held in front of
// him, the stick leaning back toward where it came from.
#define SIGN_LEAN_REST  (-0.35f)    // rad: "\" at rest (the board left of the hand)
#define SIGN_LEAN_AWAY  ( 0.60f)    // rad: "/" at the start and the end
static float sign_stick_len(void) { return (float)S(190); }

// Where the board rests: under his face, or beside the glass while he drinks.
static int sign_cx(void) { return (drinking ? W / 2 + S(118) : W / 2) + drift_x; }

// The hand when the sign is at rest, and how far right it starts and ends.
static float sign_hand_rest_x(void) { return sign_cx() - sign_stick_len() * sinf(SIGN_LEAN_REST); }
static float sign_hand_y(void) {
    return (float)(sign_rest_y + sign_h + drift_y) + sign_stick_len() * cosf(SIGN_LEAN_REST);
}
static float sign_hand_off_x(void) { return (float)W + S(30); }

// Draws the sign for a hand position and a lean (rad, + is "/").
static void sign_pose(float hand_x, float phi) {
    const float len = sign_stick_len();
    const float hy = sign_hand_y();
    const float bx = hand_x + len * sinf(phi);          // the board's bottom middle
    const float by = hy - len * cosf(phi);
    lv_obj_set_pos(sign, (int)bx - sign_w / 2, (int)by - sign_h);
    sign_stick_pts[0].x = bx;      sign_stick_pts[0].y = by - S(6);
    sign_stick_pts[1].x = hand_x;  sign_stick_pts[1].y = hy;
    lv_line_set_points(sign_stick, sign_stick_pts, 2);
}

static void sign_place_rest(void) { sign_pose(sign_hand_rest_x(), SIGN_LEAN_REST); }

static void sign_hide(void) {
    sign_phase = 0;
    if (sign) lv_obj_add_flag(sign, LV_OBJ_FLAG_HIDDEN);
    if (sign_stick) lv_obj_add_flag(sign_stick, LV_OBJ_FLAG_HIDDEN);
}

static void sign_start(uint32_t now) {
    sign_phase = 2;
    sign_t0 = now;
    sign_pose(sign_hand_off_x(), SIGN_LEAN_AWAY);
    lv_obj_move_foreground(sign_stick);
    lv_obj_move_foreground(sign);
    lv_obj_clear_flag(sign_stick, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sign, LV_OBJ_FLAG_HIDDEN);
}

// In: the hand eases in from the right while the board swings "/" -> "\" and
// back, settling at "\". Held. Out: the hand pulls away, speeding up, the
// board swinging back to "/".
static void sign_tick(uint32_t now) {
    if (!sign || sign_phase == 0) return;
    const uint32_t ms = now - sign_t0;
    if (sign_phase == 1) {
        if (visible && !moving) sign_start(now);
        else if (ms >= SIGN_WAIT_MS) sign_phase = 0;   // the face never came up
        return;
    }
    const float hx0 = sign_hand_off_x(), hx1 = sign_hand_rest_x();
    if (sign_phase == 2) {
        if (ms >= SIGN_UP_MS) { sign_phase = 3; sign_t0 = now; sign_place_rest(); return; }
        const float u = (float)ms / SIGN_UP_MS;
        const float ease = 1.0f - (1.0f - u) * (1.0f - u);                   // the hand: ease-out
        const float swing = (SIGN_LEAN_AWAY - SIGN_LEAN_REST) * (1.0f - u) * cosf(6.2832f * 1.25f * u);
        sign_pose(hx0 + (hx1 - hx0) * ease, SIGN_LEAN_REST + swing);
    } else if (sign_phase == 3) {
        if (ms >= SIGN_HOLD_MS) { sign_phase = 4; sign_t0 = now; }
    } else if (sign_phase == 4) {
        if (ms >= SIGN_DOWN_MS) { sign_hide(); return; }
        const float u = (float)ms / SIGN_DOWN_MS;
        sign_pose(hx1 + (hx0 - hx1) * u * u, SIGN_LEAN_REST + (SIGN_LEAN_AWAY - SIGN_LEAN_REST) * u);
    }
}

// Drinking ends: the glass goes, the eyes open.
static void drink_off(void) {
    if (!drinking) return;
    drinking = false;
    show_cup(false);
    water_shown = -1;
    set_glyph(0, GL_OPEN);
    set_glyph(1, GL_OPEN);
    if (sign_phase == 3) sign_place_rest();    // the sign moves back to the middle
}

void face_init(void) {
    const BoardCaps& c = board_caps();
    W = c.width; H = c.height;
    mind = W < H ? W : H;
    eye_cx[0] = W / 2 - S(80);
    eye_cx[1] = W / 2 + S(80);
    eye_top = (H - mind) / 2 + S(150);
    arm_top = (H - mind) / 2 + S(240);
    eye_w0 = S(44); eye_h0 = S(120);
    glance_px = S(20);

    geom.w = W; geom.h = H; geom.mind = mind;
    geom.cell = mind / 60 > 0 ? mind / 60 : 1;
    geom.start_y = H;

    field_init(&field, lv_layer_top(), W, H, COL_FACE);
    root = field.obj;
    const int u = S(12) > 0 ? S(12) : 1;
    // The box spans the open eye and the squint chevron (which reaches 2u
    // past the eye's right edge); its children are box-local. Kept tight:
    // moving it redraws its whole area every frame of a rise.
    box_w0 = S(52) > 6 * u ? S(52) : 6 * u;
    box_h0 = S(120) > S(20) + 9 * u ? S(120) : S(20) + 9 * u;
    for (int e = 0; e < 2; e++) {
        eye_box[e] = lv_obj_create(root);
        lv_obj_remove_style_all(eye_box[e]);
        lv_obj_set_size(eye_box[e], box_w0, box_h0);
        lv_obj_set_pos(eye_box[e], 0, H + S(200));
        lv_obj_clear_flag(eye_box[e], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(eye_box[e], LV_OBJ_FLAG_SCROLLABLE);
        eye_open[e] = rect(eye_box[e], S(8), 0, eye_w0, eye_h0, COL_EYE);
        for (int i = 0; i < 9; i++) squint[e][i] = rect(eye_box[e], 0, 0, 2 * u, u, COL_EYE);
        layout_glyph(e, GL_OPEN);              // the cells hide until a shape wants them
        eye_last[e] = { INT32_MIN, 0, box_w0, box_h0, -1, -1, false, GL_OPEN };   // forces the first placement
    }
    if (FACE_EXTRAS) {
        make_cup();
        make_sign();
    }
    pose = gone_pose();
    place_eyes(0);
    clawd_rng_seed(&rng, millis() ^ 0xFACE);
}

// Down and out of sight: hidden, so nothing of it can linger at the bottom
// edge. Its eyes sit below the panel, so hiding repaints nothing.
static void rest_hidden(void) {
    pose = gone_pose();
    apply_pose();
    visible = false;
    field_hide(&field);
}

static PerfWindow perf_window(void) {
    if (move_kind == FACE_MOVE_DROP) return PERF_FACE_DROP;
    return settling ? PERF_FACE_SETTLE : PERF_FACE_RISE;
}

static void motion_exec(void* var, int32_t v) {
    (void)var;
    if (move_kind == FACE_MOVE_RISE && !settling && (uint32_t)v >= face_motion_rest_ms()) {
        perf_end(PERF_FACE_RISE);
        settling = true;
        perf_begin(PERF_FACE_SETTLE);
    }
    pose = face_motion_pose(move_kind, (uint32_t)v, &geom);
    apply_pose();
}

static void motion_done(lv_anim_t* a) {
    (void)a;
    moving = false;
    perf_end(perf_window());
    if (move_kind == FACE_MOVE_DROP) rest_hidden();
}

static void start_motion(FaceMove m) {
    if (!root) return;
    lv_anim_delete(&anim_var, NULL);
    if (moving) perf_end(perf_window());
    if (!visible) {
        // Coming up from all the way down: nothing on screen yet, so it
        // shows and goes on top of the overlay layer without a redraw.
        pose = gone_pose();
        apply_pose();
        field_show(&field);
        field_to_front(&field);
        geom.start_y = H;
    } else {
        geom.start_y = pose.y < H ? pose.y : H;
    }
    move_kind = m;
    settling = false;
    visible = true;
    moving = true;
    perf_begin(perf_window());
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &anim_var);
    lv_anim_set_exec_cb(&a, motion_exec);
    const uint32_t ms = face_motion_duration(m);
    lv_anim_set_values(&a, 0, (int32_t)ms);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);   // the value is elapsed ms
    lv_anim_set_completed_cb(&a, motion_done);
    lv_anim_start(&a);
}

static void rise(int interest) {
    if (!root) return;
    interest_g = interest;
    blink_phase = 0; glance_phase = 0;
    drink_off();
    if (sign_phase >= 2) sign_hide();         // a sign still waiting (1) shows once it's up
    set_squint(false);
    expr_clear();
    lv_anim_delete(&glance_cur, NULL);
    glance_cur = 0;
    start_motion(FACE_MOVE_RISE);
    next_act_at = millis() + face_motion_duration(FACE_MOVE_RISE) + 1200;
}

void face_rise(void) { rise(0); }

// A peek: curious about where you tapped, so its resting glances keep going
// back there.
void face_rise_toward(int x) {
    const int g = peek_glance_px(x, W, glance_px * 3 / 2);
    rise(g != 0 ? g : 1);
}

void face_drop(void) {
    if (!root || !visible) return;
    resting = false;
    drink_off();
    sign_hide();
    // Eyes open and front-facing as it goes: he's looking at you, then away.
    blink_phase = 0;
    set_squint(false);
    expr_clear();
    lv_anim_delete(&glance_cur, NULL);
    glance_phase = 0; glance_cur = 0;
    interest_g = 0;
    start_motion(FACE_MOVE_DROP);
}

void face_hide_now(void) {
    if (!root) return;
    lv_anim_delete(&anim_var, NULL);
    lv_anim_delete(&glance_cur, NULL);
    if (moving) perf_end(perf_window());
    moving = false; resting = false;
    drink_off();
    sign_hide();
    blink_phase = 0; glance_phase = 0; glance_cur = 0; interest_g = 0;
    set_squint(false);
    expr_clear();
    rest_hidden();
}

bool face_visible(void) { return visible; }
bool face_moving(void)  { return moving; }
bool face_busy(void)    { return moving || blink_phase != 0 || glance_phase != 0 || expr_on; }

void face_blink(bool twice) {
    if (!visible || drinking) return;
    if (expr_on) expr_clear();                // the noticing beat cuts an expression short
    blink_twice = twice;
    blink_phase = 1;
    blink_until = millis() + BLINK_MS;
    set_squint(true);
}

void face_apply_theme(void) {
    if (!root) return;
    field.color = COL_FACE;
    for (int e = 0; e < 2; e++) {
        lv_obj_set_style_bg_color(eye_open[e], COL_EYE, 0);
        for (lv_obj_t* o : squint[e]) if (o) lv_obj_set_style_bg_color(o, COL_EYE, 0);
    }
    if (sign_box) {
        lv_obj_set_style_bg_color(sign_box, COL_EYE, 0);
        lv_obj_set_style_line_color(sign_cone, COL_EYE, 0);
        for (lv_obj_t* w : sign_wave) lv_obj_set_style_line_color(w, COL_EYE, 0);
    }
    lv_obj_invalidate(root);   // the body is drawn from field.color
}

void face_set_resting(bool on) {
    resting = on;
    if (on) next_act_at = millis() + 800;
}

static void glance_to(int target);

void face_look_at_x(int x, uint32_t hold_ms) {
    if (!visible) return;
    glance_hold = hold_ms ? hold_ms : 1;
    glance_phase = 1;
    glance_target = peek_glance_px(x, W, glance_px * 3 / 2);
    glance_to(glance_target);
}

static void glance_exec(void* var, int32_t v) {
    (void)var;
    place_eyes(v);
}

static void glance_done(lv_anim_t* a) {
    (void)a;
    if (glance_phase == 1) {
        glance_phase = 2;
        glance_until = millis() + (glance_hold ? glance_hold : clawd_rng_range(&rng, 700, 1600));
    } else {
        glance_phase = 0;
    }
}

static void glance_to(int target) {
    lv_anim_delete(&glance_cur, NULL);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &glance_cur);
    lv_anim_set_exec_cb(&a, glance_exec);
    lv_anim_set_values(&a, glance_cur, target);
    lv_anim_set_duration(&a, GLANCE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, glance_done);
    lv_anim_start(&a);
}

// Starts one of the rarer expressions: a shape swap (happy, heart, wink: the
// wink closes only the right eye) or a timed scale (surprise, sleepy).
static void expr_start_act(FaceAct a, uint32_t now) {
    expr_on = true;
    expr_act = a;
    expr_start = now;
    switch (a) {
    case FACE_HAPPY: set_glyph(0, GL_HAPPY); set_glyph(1, GL_HAPPY); break;
    case FACE_HEART: set_glyph(0, GL_HEART); set_glyph(1, GL_HEART); break;
    case FACE_WINK:  set_glyph(1, GL_SQUINT); break;
    default: break;                       // surprise, sleepy: expr_tick scales the eye
    }
}

static void expr_tick(uint32_t now) {
    const uint32_t ms = now - expr_start;
    const uint32_t hold = face_expr_hold_ms(expr_act);
    if (hold) {                           // a shape swap: back to open when the hold is up
        if (ms >= hold) expr_clear();
        return;
    }
    bool done = false;
    const int pct = face_expr_scale_pct(expr_act, ms, &done);
    if (done) { expr_clear(); return; }
    if (pct != eye_scale) {
        eye_scale = pct;
        apply_eyes();
    }
}

void face_tick(void) {
    if (!root) return;
    const uint32_t now = millis();
    sign_tick(now);
    if (!visible) return;

    if (FACE_EXTRAS && !moving && (uint32_t)(now - drift_at) >= DRIFT_MS) {
        drift_at = now;
        drift_step++;
        const int unit = geom.cell / 2 > 0 ? geom.cell / 2 : 1;
        face_drift_for_step(drift_step, unit, &drift_x, &drift_y);
        apply_eyes();
        place_cup();
        if (sign_phase == 3) sign_place_rest();
    }
    if (drinking) {
        const int pct = reminder_water_pct(now - drink_start, drink_ms);
        if (pct != water_shown) place_water(pct);
    }

    if (blink_phase && (int32_t)(now - blink_until) >= 0) {
        if (blink_phase == 1 && blink_twice) {
            blink_phase = 2; blink_until = now + BLINK_GAP; set_squint(false);
        } else if (blink_phase == 2) {
            blink_phase = 3; blink_until = now + BLINK_MS; set_squint(true);
        } else {
            blink_phase = 0; set_squint(false);
        }
    }
    if (glance_phase == 2 && (int32_t)(now - glance_until) >= 0) {
        glance_phase = 3;
        glance_to(0);
    }

    if (expr_on) expr_tick(now);
    if (!resting || moving || blink_phase || glance_phase || expr_on || drinking) return;
    if ((int32_t)(now - next_act_at) < 0) return;
    const FacePlan p = face_plan_next(&rng, &clawd_tuning());
    switch (p.act) {
    case FACE_BLINK:         face_blink(false); break;
    case FACE_DOUBLE_BLINK:  face_blink(true); break;
    case FACE_GLANCE_LEFT:
    case FACE_GLANCE_RIGHT:
        glance_hold = 0;
        glance_phase = 1;
        glance_target = face_glance_px(p.act, glance_px, interest_g, &rng, &clawd_tuning());
        glance_to(glance_target);
        break;
    default:                 expr_start_act(p.act, now); break;
    }
    next_act_at = now + p.gap_ms;
}

bool face_debug(const char* what) {
    if (!root) return false;
    if (strcmp(what, "sign on") == 0 || strcmp(what, "sign off") == 0) {   // QA: the voice sign
        if (!sign || !visible) return false;
        face_show_sign(what[6] == 'n');
        return true;
    }
    FaceAct a;
    if      (strcmp(what, "happy") == 0)    a = FACE_HAPPY;
    else if (strcmp(what, "heart") == 0)    a = FACE_HEART;
    else if (strcmp(what, "wink") == 0)     a = FACE_WINK;
    else if (strcmp(what, "surprise") == 0) a = FACE_SURPRISE;
    else if (strcmp(what, "sleepy") == 0)   a = FACE_SLEEPY;
    else return false;
    if (!visible || moving || blink_phase || glance_phase) return false;
    expr_start_act(a, millis());
    return true;
}

void face_drink(bool on, uint32_t ms) {
    if (!root) return;
    if (!on) { drink_off(); return; }
    if (!cup || !visible || moving) return;
    blink_phase = 0;
    expr_clear();
    lv_anim_delete(&glance_cur, NULL);
    glance_phase = 0;
    place_eyes(0);
    drinking = true;
    drink_start = millis();
    drink_ms = ms;
    set_glyph(0, GL_CLOSED);
    set_glyph(1, GL_CLOSED);
    place_cup();
    place_water(100);
    show_cup(true);
    if (sign_phase == 3) sign_place_rest();    // a sign already up steps aside
}

bool face_drinking(void) { return drinking; }

void face_show_sign(bool sound_on) {
    if (!sign) return;
    if (sound_on) {
        lv_obj_add_flag(sign_slash, LV_OBJ_FLAG_HIDDEN);
        for (lv_obj_t* w : sign_wave) lv_obj_clear_flag(w, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(sign_slash, LV_OBJ_FLAG_HIDDEN);
        for (lv_obj_t* w : sign_wave) lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    }
    const uint32_t now = millis();
    if (visible && !moving) sign_start(now);
    else { sign_phase = 1; sign_t0 = now; }
}
