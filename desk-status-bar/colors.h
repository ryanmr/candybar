#pragma once

// =============================================================
// Color Palette — dark theme, easy on the eyes at a desk
// =============================================================
#define BG_COLOR       gfx->color565(10, 10, 14)      // near-black
#define PANEL_COLOR    gfx->color565(55, 55, 75)      // visible card
#define ACCENT_COLOR   gfx->color565(99, 140, 255)    // soft blue
#define TEXT_PRIMARY   gfx->color565(230, 230, 240)   // off-white
#define TEXT_SECONDARY gfx->color565(140, 140, 160)   // muted
#define TEXT_DIM       gfx->color565(100, 100, 120)   // muted but readable
#define GOOD_COLOR     gfx->color565(80, 200, 120)    // green
#define WARN_COLOR     gfx->color565(255, 180, 60)    // amber
#define ERR_COLOR      gfx->color565(255, 80, 80)     // red
#define CRITTER_BODY      gfx->color565(130, 185, 230)  // soft sky blue
#define CRITTER_HIGHLIGHT gfx->color565(180, 215, 245)  // lighter blue belly
#define CRITTER_EYE       gfx->color565(30, 30, 40)     // near-black
#define CRITTER_CHEEK     gfx->color565(255, 160, 160)  // pink blush
#define CRITTER_BEAK      gfx->color565(240, 180, 70)   // warm orange
#define CRITTER_WING      gfx->color565(100, 155, 210)  // deeper blue
