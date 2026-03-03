# AXS15231B QSPI Display — Graphics Engine Notes

## The Core Constraint: Full-Frame Writes Only

The AXS15231B in QSPI mode **ignores RASET (Row Address Set / 0x2B)**. Only CASET
(Column Address Set / 0x2A) is honored. This means:

- You cannot do partial screen updates (e.g., redraw just one text label)
- Every RAMWR (0x2C) starts writing at row 0 and fills sequentially
- Arduino_GFX's `fillRect`, `drawChar`, etc. all call `writeAddrWindow` internally,
  which sets both CASET and RASET — but since RASET is ignored, those partial writes
  corrupt the display

This was discovered empirically and confirmed by analyzing the Waveshare factory code,
which always sets `full_refresh = 1` and redraws the entire 172×640 frame.

## The Solution: Canvas Framebuffer + flush()

Use `Arduino_Canvas` as an in-memory framebuffer:

```cpp
Arduino_Canvas *gfx = new Arduino_Canvas(172, 640, display);
```

All drawing operations (fillRect, print, drawLine, etc.) write to RAM. Then
`gfx->flush()` sends the entire 172×640 buffer to the display in one shot,
which works correctly because it writes sequentially from row 0.

**Every function that draws to the screen must call `gfx->flush()` at the end.**

## Landscape Mode (640×172 bar layout)

The native panel resolution is 172×640 (portrait). For landscape:

1. Create the Canvas in **native portrait** dimensions: `Arduino_Canvas(172, 640, display)`
2. After `gfx->begin()`, call `gfx->setRotation(1)`
3. Drawing coordinates are now remapped to 640×172 (landscape)
4. The underlying buffer stays 172×640, so `flush()` sends data in QSPI-correct order

**Do NOT pass rotation to the Canvas or display constructors.** Constructor rotation
changes the buffer layout, which breaks the QSPI full-frame write. Only use
`setRotation()` after `begin()`.

## Display Initialization

The default Arduino_GFX init blob (`axs15231b_180640_init_operations`) doesn't match
this specific panel. A minimal init sequence works:

```cpp
static const uint8_t panel_init[] = {
  BEGIN_WRITE, WRITE_COMMAND_8, 0x11, END_WRITE,  // Sleep out
  DELAY, 200,
  BEGIN_WRITE, WRITE_C8_D8, 0x3A, 0x55, END_WRITE,  // COLMOD: RGB565
  BEGIN_WRITE, WRITE_COMMAND_8, 0x29, END_WRITE,  // Display on
  DELAY, 50,
};
```

Pass this to the `Arduino_AXS15231B` constructor as the custom init sequence.

## QSPI Command Framing

The Arduino_ESP32QSPI bus uses two command prefixes:
- **0x02** (single-line SPI): Used for sending commands/parameters
- **0x32** (quad-line SPI): Used for sending pixel data

The address field in the SPI transaction encodes the MIPI DCS command. For pixel
writes, the bus sends 0x32 with address 0x003C00 (RAMWRC / write memory continue).

`writeRepeat()` works (used by fillRect, fillScreen) but individual `write16()` calls
don't produce visible output — the QSPI framing is different for bulk vs single-pixel
writes.

## Performance Considerations

- Full-frame flush of 172×640×2 bytes = 220,160 bytes per frame
- At QSPI speeds this takes roughly 10-20ms — fast enough for 1 Hz clock updates
- The Canvas uses ~220KB of RAM (allocated from PSRAM on this board)
- Avoid calling flush() more than necessary — the main loop redraws once per second
