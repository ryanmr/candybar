#pragma once

// =============================================================
// Audio — ES8311 codec + I2S click feedback
// Critical: I2S must start BEFORE ES8311 init (codec needs MCLK
// running to configure its PLL). Stereo mode required for correct
// BCLK/LRCK ratio. Register sequence from Waveshare reference.
// =============================================================
#if TOUCH_SOUND_ENABLED
#include <ESP_I2S.h>

I2SClass i2sOut;
bool audioReady = false;

void es8311WriteReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(ES8311_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

bool es8311Init() {
  // Verify codec is present
  Wire1.beginTransmission(ES8311_ADDR);
  if (Wire1.endTransmission() != 0) {
    Serial.println("[Audio] ES8311 not found");
    return false;
  }
  Serial.println("[Audio] ES8311 found at 0x18");

  // Reset all registers
  es8311WriteReg(0x00, 0x1F);
  delay(20);
  es8311WriteReg(0x00, 0x00);
  delay(20);

  // I2C noise immunity (per Waveshare reference — write twice)
  es8311WriteReg(0x44, 0x08);
  es8311WriteReg(0x44, 0x08);

  // -- es8311_open: initial clock + system setup --
  es8311WriteReg(0x01, 0x30);  // Initial clock config (partial)
  es8311WriteReg(0x02, 0x00);  // MCLK source = from pin
  es8311WriteReg(0x03, 0x10);  // ADC OSR
  es8311WriteReg(0x16, 0x24);  // ADC mic gain
  es8311WriteReg(0x04, 0x10);  // DAC OSR
  es8311WriteReg(0x05, 0x00);  // CLK dividers

  // System power
  es8311WriteReg(0x0B, 0x00);  // System normal
  es8311WriteReg(0x0C, 0x00);  // System normal
  es8311WriteReg(0x10, 0x1F);  // Analog power: VREF + all bias on
  es8311WriteReg(0x11, 0x7F);  // Analog power: DAC + headphone on

  // Power on in slave mode
  es8311WriteReg(0x00, 0x80);
  delay(10);

  // Enable ALL clocks (0x3F, not 0x30) — includes DAC clock domain
  es8311WriteReg(0x01, 0x3F);

  // HP driver + system config
  es8311WriteReg(0x13, 0x10);  // HP driver enable
  es8311WriteReg(0x1B, 0x0A);  // ADC HPF
  es8311WriteReg(0x1C, 0x6A);  // ADC EQ

  // Internal DAC reference
  es8311WriteReg(0x44, 0x58);

  // -- Clock coefficients for 16kHz, MCLK=256*16000=4,096,000 --
  // From ES8311 coefficient table: {4096000, 16000, ...}
  es8311WriteReg(0x02, 0x00);  // pre_div=1, pre_mult=0
  es8311WriteReg(0x05, 0x00);  // adc_div=1, dac_div=1
  es8311WriteReg(0x03, 0x10);  // fs_mode=0, adc_osr=0x10
  es8311WriteReg(0x04, 0x20);  // dac_osr=0x20
  es8311WriteReg(0x07, 0x00);  // LRCK high
  es8311WriteReg(0x08, 0xFF);  // LRCK low
  es8311WriteReg(0x06, 0x03);  // BCLK divider

  // I2S format: standard I2S (Philips), 16-bit, SDP enabled (bit 6 = 0)
  es8311WriteReg(0x09, 0x0C);  // DAC SDP: I2S 16-bit, not tri-stated
  es8311WriteReg(0x0A, 0x0C);  // ADC SDP: I2S 16-bit

  // -- es8311_start: activate the DAC output path --
  es8311WriteReg(0x00, 0x80);  // Power on, slave mode
  es8311WriteReg(0x01, 0x3F);  // All clocks enabled

  es8311WriteReg(0x17, 0xBF);  // ADC volume (needed even for DAC mode)
  es8311WriteReg(0x0E, 0x02);  // Power up PGA + ADC analog
  es8311WriteReg(0x12, 0x00);  // Enable DAC
  es8311WriteReg(0x14, 0x1A);  // Analog PGA gain
  es8311WriteReg(0x0D, 0x01);  // Power up system
  es8311WriteReg(0x15, 0x40);  // ADC ramp rate
  es8311WriteReg(0x37, 0x08);  // DAC ramp rate
  es8311WriteReg(0x45, 0x00);  // GP control

  // DAC volume
  es8311WriteReg(0x32, 0xBF);  // ~75% volume

  delay(50);
  Serial.println("[Audio] ES8311 initialized");
  return true;
}

// Click waveform buffer — filled once at startup
// 48 frames of tone + 208 frames of silence = 256 total (~16ms at 16kHz)
// Large silence pad ensures the full DMA buffer is flushed to a clean
// state after every click, preventing variance between taps.
#define CLICK_FRAMES 48
#define CLICK_PAD    208
#define CLICK_TOTAL  (CLICK_FRAMES + CLICK_PAD)
static int16_t clickBuf[CLICK_TOTAL * 2];  // stereo pairs

void generateClick() {
  // Soft sine "tick": 1200Hz tone with fast exponential decay (~3ms)
  const float freq = 1200.0f;
  const float rate = 16000.0f;
  const float amp  = 14000.0f;
  const float decay = 9.0f;

  for (int i = 0; i < CLICK_FRAMES; i++) {
    float t = (float)i / (float)CLICK_FRAMES;
    float angle = 2.0f * PI * freq * (float)i / rate;
    float envelope = expf(-decay * t);
    int16_t s = (int16_t)(amp * sinf(angle) * envelope);
    clickBuf[i * 2]     = s;  // L
    clickBuf[i * 2 + 1] = s;  // R
  }
  // Silence pad to flush DMA pipeline
  for (int i = CLICK_FRAMES; i < CLICK_TOTAL; i++) {
    clickBuf[i * 2]     = 0;
    clickBuf[i * 2 + 1] = 0;
  }
}

void playClick() {
  if (!audioReady) return;
  i2sOut.write((uint8_t*)clickBuf, sizeof(clickBuf));
}

void audioSetup() {
  // Step 1: Start I2S FIRST — ES8311 needs MCLK running to configure PLL
  i2sOut.setPins(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_SDIN, I2S_MCLK);
  if (!i2sOut.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[Audio] I2S begin failed");
    return;
  }
  Serial.println("[Audio] I2S started (16kHz stereo)");
  delay(50);  // Let MCLK stabilize

  // Step 2: Init ES8311 codec (now that MCLK is running)
  if (!es8311Init()) return;

  // Step 3: Enable speaker amplifier via TCA9554 pin 7 (after codec to avoid pop)
  tca9554SetPin(TCA9554_PA_EN, true);
  delay(10);

  // Pre-compute the click waveform
  generateClick();

  audioReady = true;
  Serial.println("[Audio] Audio ready");
}

// Startup tune — warm ascending arpeggio (~1s)
// Heap-allocated, generated once, played, then freed.
#define TUNE_RATE    16000
#define TUNE_MS      1100
#define TUNE_FRAMES  (TUNE_RATE * TUNE_MS / 1000)   // 17600
#define TUNE_PAD     256
#define TUNE_TOTAL   (TUNE_FRAMES + TUNE_PAD)

struct TuneNote {
  float freq;       // Hz
  int   startFrame; // when this note begins
  int   frames;     // how long it lasts
  float amplitude;
  int   attackFrames;
  float decayRate;  // exponential decay steepness
};

void playStartupTune() {
  if (!audioReady) return;

  int16_t* buf = (int16_t*)ps_malloc(TUNE_TOTAL * 4);  // stereo 16-bit
  if (!buf) {
    Serial.println("[Audio] Tune alloc failed");
    return;
  }
  memset(buf, 0, TUNE_TOTAL * 4);

  // D major arpeggio — overlapping notes create a warm cascading chime
  //                freq      start   dur    amp   atk  decay
  TuneNote notes[] = {
    {  587.33f,       0, 5600, 3500.0f, 240, 3.5f },  // D5  0–350ms
    {  739.99f,    2880, 5600, 3500.0f, 240, 3.5f },  // F#5 180–530ms
    {  880.00f,    5760, 5600, 3500.0f, 240, 3.5f },  // A5  360–710ms
    { 1174.66f,    8640, 8000, 3800.0f, 320, 2.5f },  // D6  540–1040ms (lingers)
  };
  const int numNotes = 4;

  for (int n = 0; n < numNotes; n++) {
    TuneNote &note = notes[n];
    for (int i = 0; i < note.frames; i++) {
      int frame = note.startFrame + i;
      if (frame >= TUNE_FRAMES) break;

      float angle = 2.0f * PI * note.freq * (float)i / (float)TUNE_RATE;

      // Envelope: smooth attack then exponential decay
      float env;
      if (i < note.attackFrames) {
        env = (float)i / (float)note.attackFrames;
      } else {
        float dt = (float)(i - note.attackFrames) / (float)(note.frames - note.attackFrames);
        env = expf(-note.decayRate * dt);
      }

      // Bell-like timbre: fundamental + 2nd and 3rd harmonics
      float tone = sinf(angle)
                 + sinf(angle * 2.0f) * 0.3f
                 + sinf(angle * 3.0f) * 0.08f;
      int16_t s = (int16_t)(note.amplitude * tone * env / 1.38f);

      // Mix (add) so overlapping notes blend
      int32_t L = (int32_t)buf[frame * 2]     + s;
      int32_t R = (int32_t)buf[frame * 2 + 1] + s;
      if (L >  32767) L =  32767; if (L < -32768) L = -32768;
      if (R >  32767) R =  32767; if (R < -32768) R = -32768;
      buf[frame * 2]     = (int16_t)L;
      buf[frame * 2 + 1] = (int16_t)R;
    }
  }

  i2sOut.write((uint8_t*)buf, TUNE_TOTAL * 4);
  free(buf);
  Serial.println("[Audio] Startup tune played");
}
#endif // TOUCH_SOUND_ENABLED
