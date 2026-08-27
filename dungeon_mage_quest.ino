/*
  DUNGEON MAGE QUEST
  A motion-controlled dungeon crawl for the M5StickC Plus2, seen from a
  first-person corridor view. You walk automatically - all you do is
  swing when something appears in front of you.

  THE STORY
  ---------
  Each dungeon picks a random cursed name (a title card shows which one).
  You step in with nothing but your wand, walk its torchlit stone
  corridor, and along the way you'll hit forks where the path splits -
  pick BtnA (left) or BtnB (right) and live with what you find. Beat the
  boss at the end and you're offered a choice of the next dungeon to
  push into - deeper, harder, worth more. Run out of health and the
  dungeon claims you.

  HOW TO PLAY
  -----------
  Hold the StickC Plus2 like a wand: screen facing you, USB-C port toward
  the bottom, big front button under your thumb. Press BtnA to start.

  You advance down the corridor on your own - just watch the torchlit
  stone tunnel scroll past. When a shape grows in the distance, something
  has appeared:

    - A mossy BOULDER blocks the way - that's an obstacle. The screen
      names a gesture (ATTACK / BLOCK / PARRY) - perform THAT swing
      before the timer runs out to get past it, or take a hit (with a
      chance to just graze it for half damage instead).
    - A pair of glowing eyes in the dark is a monster, closing in until
      it's drawn as what it actually is - a squat SLIME, a winged BAT, a
      whiskered RAT, an armed GOBLIN, a rattling SKELETON, or one of two
      rare elites, a blazing PHOENIX or a sharp-taloned GRIFFIN - or, at
      the end of the dungeon, one of two bosses: the three-headed HYDRA
      or the cursed-gaze MEDUSA, whose eyes glow magenta while still a
      shadow. Every species has its own personality - some favor Block,
      some favor Parry, some hit harder, some come out swinging faster.
      Swing sideways (or forward) to Attack, landing enough damage to
      win, with a chance of a bonus CRITICAL hit. Fill the mana bar for a
      screen-flashing Ultimate. The monster attacks back periodically,
      randomly demanding BLOCK (lift the wand) or PARRY (push it down),
      biased by its species - read the prompt each time. Hits/defenses
      in a row build a combo that boosts your damage; a failed defense
      might still graze for half damage. Landing a hit - or a monster
      going down - flashes a quick shimmer of magic; a clean hit against
      you flashes red across the screen.
    - A GOLD diamond is the treasure at the end - any swing grabs it,
      then it's straight to the boss.

  Beat the boss and the dungeon is cleared: you're shown your score and a
  choice of two dungeons to push into next. Push deeper and monsters hit
  harder and come in tougher varieties, but you're also worth more each
  time. There's no ceiling - you go until you fall.

  At a FORK, the corridor splits - the screen describes both paths; press
  BtnA for the left path or BtnB for the right. One tends to be quieter,
  the other livelier - it nudges your odds of running into something on
  the next stretch, for better or worse.

  You'll also occasionally run into something extra mid-walk, at random:
  a glowing HEALTH HERB that heals you on the spot, a sudden TRAP that
  needs a quick reflex swing, or an AMBUSH - a weaker monster jumping out
  with no warning. None of these are guaranteed every walk.

  Your health carries across every dungeon you push into - clearing one
  heals you a little, but there's no full reset except the occasional
  herb. Hit zero and it's game over; your final score is compared
  against your best and saved on the device.

  CONTROLS
  --------
  - Lift the wand sharply UP    -> BLOCK  (stops a BLOCK-flagged attack)
  - Push the wand sharply DOWN  -> PARRY  (stops a PARRY-flagged attack)
  - Swing side to side (or jab forward/back) -> ATTACK

  LIBRARIES NEEDED (Arduino Library Manager)
  -------------------------------------------
    - "M5Unified" by M5Stack
  (Preferences.h ships with the ESP32 Arduino core, nothing extra to add.)

  BOARD SETUP
  ------------
  Tools > Board > M5Stack > M5StickCPlus2  (see README.md for the full
  board-manager setup if you haven't installed the M5Stack package yet).

  CALIBRATING YOUR GRIP
  ----------------------
  If a swing ever feels backwards (e.g. lifting the wand registers as a
  Parry instead of a Block), flip BLOCK_IS_POSITIVE_Y below and re-upload.
  Holding BtnB while powering on drops you into a serial calibration mode
  that streams live accelerometer values (Serial Monitor, 115200 baud) so
  you can watch which axis moves for each swing.
*/

#include <M5Unified.h>
#include <Preferences.h>

// ---------------------------------------------------------------------
// Tuning constants
// ---------------------------------------------------------------------
static const float    TRIGGER_THRESHOLD_G = 0.95f;
static const uint32_t GESTURE_COOLDOWN_MS = 260;
static const bool BLOCK_IS_POSITIVE_Y = true;

static const int PLAYER_MAX_HP     = 100;
static const int OBSTACLE_FAIL_DMG = 10;
static const int OBSTACLE_WINDOW_MS = 1100;
static const int OBSTACLE_JITTER_MS = 200;

static const int BOSS_BASE_HP      = 130;
static const int BOSS_HP_PER_DEPTH = 22; // each dungeon you push into makes the boss tougher
static const float MONSTER_HP_DEPTH_SCALE = 0.12f; // +12% regular-monster HP per depth beyond the first
static const int MANA_MAX        = 100;
static const int MANA_PER_CAST   = 25;
static const int ATTACK_DMG   = 15;
static const int ULTIMATE_DMG = 45;
static const int BLOCK_REFLECT_DMG = 6;
static const int MAX_COMBO = 5;
static const float DAMAGE_VARIANCE_MIN = 0.85f;
static const float DAMAGE_VARIANCE_MAX = 1.15f;

static const int ATTACK_INTERVAL_START_MS = 3000;
static const int ATTACK_INTERVAL_FLOOR_MS = 1000;
static const int ATTACK_INTERVAL_SCALE_MS = 1800;
static const int ATTACK_INTERVAL_JITTER_MS = 500;
static const int TELEGRAPH_WINDOW_START_MS = 1000;
static const int TELEGRAPH_WINDOW_FLOOR_MS = 400;
static const int TELEGRAPH_WINDOW_SCALE_MS = 500;
static const int BOSS_HIT_DMG_BASE  = 12;
static const int BOSS_HIT_DMG_SCALE = 14;
static const float FLURRY_HP_FRAC_THRESHOLD = 0.4f; // boss-only trait
static const int   FLURRY_CHANCE_PERCENT    = 25;

// --- crits, grazes ---
static const int CRIT_CHANCE_PERCENT  = 15;   // player attacks only
static const float CRIT_MULTIPLIER    = 1.75f;
static const int GRAZE_CHANCE_PERCENT = 25;   // any failed defense/obstacle/trap

// --- monster species: each behaves (and looks) a little differently, and
// unlocks at a minimum dungeon depth. Elites (Phoenix/Griffin) are rarer. ---
const char* MONSTER_NAMES[] = { "SLIME", "BAT", "RAT", "GOBLIN", "SKELETON", "PHOENIX", "GRIFFIN" };
static const int MONSTER_NAME_COUNT = 7;
static const int  SPECIES_HP[7]              = {  50,  38,  46,  60,  72,  55,  58 };
static const int  SPECIES_INTERVAL_MOD_MS[7] = { 700,-450,   0,-150, 200,-350,-300 };
static const int  SPECIES_WINDOW_MOD_MS[7]   = { 250,-220,   0, -80, 100,-200,-180 };
static const int  SPECIES_DMG_MOD[7]         = {  -2,   3,   0,   5,   6,   7,   8 };
static const int  SPECIES_BLOCK_BIAS_PCT[7]  = {  80,  20,  50,  65,  55,  30,  45 }; // vs PARRY
static const int  SPECIES_TELE_HZ[7]         = { 260, 700, 450, 380, 220, 820, 600 };
static const int  SPECIES_TELE_MS[7]         = { 240, 110, 170, 160, 260, 100, 130 };
static const int  SPECIES_MIN_DEPTH[7]       = {   1,   1,   1,   2,   3,   4,   4 };
static const bool SPECIES_IS_ELITE[7]        = { false,false,false,false,false, true, true };

// --- the two end-of-dungeon bosses ---
const char* BOSS_NAMES[] = { "HYDRA", "MEDUSA" };

// --- score awarded for clearing each kind of encounter ---
static const int SCORE_OBSTACLE      = 15;
static const int SCORE_TREASURE      = 40;
static const int SCORE_MONSTER_PER_HP = 2;   // times the monster's max HP
static const int SCORE_BOSS          = 300;
static const int SCORE_DEPTH_BONUS   = 50;   // times dungeonDepth, added on a boss kill
static const int DUNGEON_CLEAR_HEAL  = 15;   // small heal granted when you push into the next dungeon

// --- the run itself ---
static const int NUM_OBSTACLES = 3;
static const int NUM_MONSTERS  = 2;   // regular monsters, not counting the boss
static const int NUM_FORKS     = 2;
static const int TOTAL_ENCOUNTERS = NUM_OBSTACLES + NUM_MONSTERS + NUM_FORKS + 2; // + boss + treasure

static const int WALK_DURATION_BASE_MS = 1700;
static const int WALK_JITTER_MS        = 400;
static const float WALK_APPEAR_FRAC    = 0.5f; // shape starts growing halfway through the walk
static const int TREASURE_WINDOW_MS    = 2500; // generous - times out into an auto-grab

// --- random mid-walk side events (on top of the main obstacle/monster run) ---
static const int WALK_EVENT_HERB_PCT   = 25; // base chance per walk segment
static const int WALK_EVENT_TRAP_PCT   = 12;
static const int WALK_EVENT_AMBUSH_PCT = 8;  // remaining % is no event at all
static const int TRAP_FAIL_DMG   = 6;
static const int TRAP_WINDOW_MS  = 650;
static const int FLAVOR_LINE_CHANCE_PCT = 30; // chance of an ambient story line during a walk

// ---------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------
enum GestureType : uint8_t { GESTURE_NONE = 0, GESTURE_ATTACK, GESTURE_BLOCK, GESTURE_PARRY };
enum DefenseType : uint8_t { DEF_BLOCK = 0, DEF_PARRY };
enum BossType : uint8_t { BOSS_HYDRA = 0, BOSS_MEDUSA };
enum EncounterKind : uint8_t { ENC_OBSTACLE, ENC_MONSTER, ENC_BOSS, ENC_TREASURE, ENC_FORK };
enum EventKind : uint8_t { EVENT_NONE = 0, EVENT_HERB, EVENT_TRAP, EVENT_AMBUSH };
enum GameMode { MODE_TITLE, MODE_WALK, MODE_OBSTACLE, MODE_TRAP, MODE_COMBAT, MODE_TREASURE, MODE_FORK, MODE_DUNGEON_CLEAR, MODE_DEATH };
GameMode mode = MODE_TITLE;

// ---------------------------------------------------------------------
// Story text pools
// ---------------------------------------------------------------------
const char* DUNGEON_TITLE[]  = { "SUNKEN VAULT", "FORGOTTEN CRYPT", "WYRM'S HOARD", "BONE CATACOMBS" };
const char* DUNGEON_FLAVOR[] = { "None who enter return", "Old magic stirs below", "Guarded by teeth and claw", "The dead don't rest here" };
// A second beat of lore, shown on its own card right after the first -
// index-matched to DUNGEON_TITLE/DUNGEON_FLAVOR.
const char* DUNGEON_LORE2[]  = { "Flooded halls, old bones", "Sealed for a thousand years", "Its hoard calls the greedy", "Every step wakes something" };
// A closing line shown on the dungeon-clear screen, also index-matched.
const char* DUNGEON_EPILOGUE[] = { "The water settles behind you", "The crypt seals once more", "The hoard's curse lifts", "The bones fall still" };
static const int DUNGEON_COUNT = 4;

const char* FLAVOR_LINES[] = {
  "Water drips in the dark",
  "The walls seem to breathe",
  "Something watches you",
  "Your torchlight flickers",
  "Bones crunch underfoot",
  "A cold wind stirs",
  "Distant chanting echoes",
  "The air reeks of decay",
};
static const int FLAVOR_LINE_COUNT = 8;

// Shown instead of the above, more often, once you're well into a run -
// a sense that the dungeon (and the boss) is closing in.
const char* FLAVOR_LINES_LATE[] = {
  "The air grows colder",
  "You sense you're close",
  "Your torch burns lower",
  "The passage narrows",
};
static const int FLAVOR_LINE_LATE_COUNT = 4;

const char* FORK_LEFT[]  = { "A NARROW CRACK", "A MOSSY TUNNEL", "A SILENT HALL", "A COLD DRAFT" };
const char* FORK_RIGHT[] = { "A WIDE HALL", "A TORCHLIT PATH", "A DISTANT GROWL", "A WARM GLOW" };
static const int FORK_COUNT = 4;

const char* DEATH_LINES[] = { "THE DARK CLAIMS YOU", "YOUR LIGHT FADES", "SILENCE FALLS" };
static const int DEATH_LINE_COUNT = 3;

// A dramatic beat shown right before the boss fight begins.
const char* BOSS_LINES[] = { "THE GROUND SHAKES", "SOMETHING HUGE STIRS", "THE AIR GROWS COLD", "A ROAR ECHOES" };
static const int BOSS_LINE_COUNT = 4;

// A simple square-wave startup fanfare, in the spirit of an old 8-bit RPG
// boot chime. {hz, ms} pairs; hz==0 is a rest.
struct JingleNote { int hz; int ms; };
const JingleNote STARTUP_JINGLE[] = {
  {523,150},{659,150},{784,150},{1047,420},{0,120},{784,150},{880,150},{1047,520}
};
static const int STARTUP_JINGLE_COUNT = 8;

// ---------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------
EncounterKind sequence[TOTAL_ENCOUNTERS];
int sequenceIndex = 0;

int playerHP;
uint32_t runScore = 0;
uint32_t bestScore = 0;
int dungeonDepth = 1;
int dungeonIndex = 0;
int nextDungeonOptionA = 0, nextDungeonOptionB = 0;

uint32_t walkStartedAt = 0;
uint32_t walkDurationMs = 0;
uint32_t lastWalkDraw = 0;
int lastStepIndex = -1;
EventKind walkEventKind = EVENT_NONE;
uint32_t walkEventAtMs = 0;
bool walkEventFired = false;
bool showFlavorThisWalk = false;
int flavorLineIndex = 0;
bool flavorIsLate = false; // true once a walk's flavor caption is drawn from the "late run" pool
float nextWalkEventChanceMultiplier = 1.0f; // set by a fork choice, consumed by the next beginWalk()

// Obstacle state
GestureType obstacleTarget = GESTURE_ATTACK;
uint32_t obstacleStartedAt = 0;
uint32_t obstacleWindowMs = 0;
uint32_t lastObstacleDraw = 0;

// Trap state (a random mid-walk side event)
GestureType trapTarget = GESTURE_ATTACK;
uint32_t trapStartedAt = 0;
uint32_t trapWindowMs = 0;
uint32_t lastTrapDraw = 0;

// Treasure state
uint32_t treasureStartedAt = 0;
uint32_t lastTreasureDraw = 0;

// Fork state
int forkPromptIndex = 0;

// Combat state (scoped to whichever monster is currently being fought)
int monsterHP = 0, monsterMaxHP = 0;
String monsterName = "";
bool isBossFight = false;
BossType currentBossType = BOSS_HYDRA;
bool combatIsSideEvent = false; // true for an ambush - doesn't advance the main run
int currentSpeciesIndex = 0;
int manaCurrent = 0;
bool manaFull = false;
int comboLevel = 0;
bool telegraphActive = false;
DefenseType currentDefense = DEF_BLOCK;
uint32_t telegraphStartedAt = 0;
uint32_t telegraphWindowMs = 0;
uint32_t nextAttackAt = 0;
uint32_t lastTelegraphDraw = 0;

// Shared swing-gesture classifier baseline
uint32_t coolingDownUntil = 0;
float baseAx = 0, baseAy = 0, baseAz = 0;
bool baselineInit = false;

String msgMain = "";
String msgSub = "";
uint16_t msgColor = TFT_WHITE;

// Dungeon color palette (computed once in setup(), since color565 needs the display)
uint16_t COLOR_STONE = TFT_NAVY;
uint16_t COLOR_TORCH = TFT_ORANGE;
uint16_t COLOR_ROCK  = TFT_ORANGE;
uint16_t COLOR_LEATHER = TFT_ORANGE;
uint16_t COLOR_STEEL   = TFT_LIGHTGREY;
uint16_t COLOR_MOSS    = TFT_DARKGREEN;
uint16_t COLOR_MAGE_ROBE = TFT_PURPLE;
uint16_t COLOR_MAGE_ROBE_HI = TFT_PURPLE;
uint16_t COLOR_MAGE_HAT = TFT_PURPLE;
uint16_t COLOR_MAGE_HAND = TFT_ORANGE;
uint16_t COLOR_MAGE_STAFF = TFT_OLIVE;
uint16_t COLOR_MEDUSA_ROBE = TFT_PURPLE;

Preferences prefs;

// Off-screen frame buffer everything is drawn into; pushed to the real
// display in one shot at the end of each frame (see setup()) to avoid
// flicker/tearing from drawing straight to the LCD.
M5Canvas canvas(&M5.Lcd);

// ---------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------
void beginWalk(bool allowEvent);
void startEncounter();
void advanceRun();
void triggerDeath();
void enterDungeonClear();
void drawTelegraphBar(float remainingFrac);
void enterCombat(bool boss, bool sideEvent);
void drawCorridorBackdrop();
void drawBoulderSprite(int cx, int cy, int r, uint16_t color);
void enterBossFight();

// ---------------------------------------------------------------------
// Sound
// ---------------------------------------------------------------------
void beepAttack()   { M5.Speaker.tone(1500, 60); }
void beepCrit()     { M5.Speaker.tone(2500, 45); }
void beepUltimate(){
  M5.Speaker.tone(900, 70);  delay(60);
  M5.Speaker.tone(1300, 70); delay(60);
  M5.Speaker.tone(1700, 70); delay(60);
  M5.Speaker.tone(2200, 140);
}
void beepTelegraph(int hz, int ms) { M5.Speaker.tone(hz, ms); }
void beepDefendSuccess() { M5.Speaker.tone(2200, 90); }
void beepPlayerHit()     { M5.Speaker.tone(150, 260); }
void beepGood()          { M5.Speaker.tone(1800, 70); }
void beepEncounter()     { M5.Speaker.tone(700, 120); }
void beepFootstep()      { M5.Speaker.tone(300, 20); }
void beepTrapSpring() {
  M5.Speaker.tone(900, 45); delay(35);
  M5.Speaker.tone(650, 55);
}
void beepAmbush() {
  M5.Speaker.tone(200, 45); delay(35);
  M5.Speaker.tone(200, 45);
}
void beepVictory() {
  M5.Speaker.tone(1200, 90); delay(100);
  M5.Speaker.tone(1500, 90); delay(100);
  M5.Speaker.tone(1900, 200);
}
void beepDefeat() { M5.Speaker.tone(220, 500); }

void playStartupJingle() {
  for (int i = 0; i < STARTUP_JINGLE_COUNT; i++) {
    if (STARTUP_JINGLE[i].hz > 0) M5.Speaker.tone(STARTUP_JINGLE[i].hz, STARTUP_JINGLE[i].ms);
    delay(STARTUP_JINGLE[i].ms + 20);
  }
}

// ---------------------------------------------------------------------
// Random helpers
// ---------------------------------------------------------------------
float damageVariance() {
  int span = (int)((DAMAGE_VARIANCE_MAX - DAMAGE_VARIANCE_MIN) * 1000);
  return DAMAGE_VARIANCE_MIN + random(0, span + 1) / 1000.0f;
}

// A failed defense/obstacle/trap has a chance to only "graze" for half
// damage instead of landing clean - reported back via wasGraze.
int applyGraze(int fullDmg, bool &wasGraze) {
  if (random(0, 100) < GRAZE_CHANCE_PERCENT) {
    wasGraze = true;
    int half = fullDmg / 2;
    return half < 1 ? 1 : half;
  }
  wasGraze = false;
  return fullDmg;
}

const char* gestureName(GestureType g) {
  switch (g) {
    case GESTURE_ATTACK: return "ATTACK";
    case GESTURE_BLOCK:  return "BLOCK";
    case GESTURE_PARRY:  return "PARRY";
    default: return "?";
  }
}

// Picks a regular-monster species available at the current dungeon depth,
// weighting the rare elites (Phoenix/Griffin) much lower than the rest.
int pickSpeciesForDepth() {
  int candidates[MONSTER_NAME_COUNT];
  int weights[MONSTER_NAME_COUNT];
  int cnt = 0, totalWeight = 0;
  for (int i = 0; i < MONSTER_NAME_COUNT; i++) {
    if (SPECIES_MIN_DEPTH[i] <= dungeonDepth) {
      int w = SPECIES_IS_ELITE[i] ? 1 : 4;
      candidates[cnt] = i; weights[cnt] = w; totalWeight += w; cnt++;
    }
  }
  int roll = random(0, totalWeight);
  int acc = 0;
  for (int i = 0; i < cnt; i++) {
    acc += weights[i];
    if (roll < acc) return candidates[i];
  }
  return candidates[0];
}

// A tiny, fast integer hash - used for deterministic-but-irregular timing
// (e.g. the walking screen's water drips) without needing float sin().
uint32_t hashU32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// ---------------------------------------------------------------------
// IMU: sharp-swing classifier (same technique as the earlier sketches) -
// a slowly adapting "at rest" baseline, and a spike on any axis past
// TRIGGER_THRESHOLD_G is classified by which axis moved most and which way.
// Vertical motion (up/down) reads as Block/Parry; any horizontal or
// forward/back motion reads as a single unified Attack.
// ---------------------------------------------------------------------
bool sampleAndClassify(GestureType &out, uint32_t now) {
  float ax, ay, az;
  if (!M5.Imu.getAccelData(&ax, &ay, &az)) return false;

  if (!baselineInit) { baseAx = ax; baseAy = ay; baseAz = az; baselineInit = true; }
  if (now < coolingDownUntil) return false;

  float dx = ax - baseAx, dy = ay - baseAy, dz = az - baseAz;
  float adx = fabsf(dx), ady = fabsf(dy), adz = fabsf(dz);
  float peak = max(adx, max(ady, adz));

  if (peak < TRIGGER_THRESHOLD_G) {
    const float alpha = 0.02f;
    baseAx += (ax - baseAx) * alpha;
    baseAy += (ay - baseAy) * alpha;
    baseAz += (az - baseAz) * alpha;
    return false;
  }

  if (ady >= adx && ady >= adz) {
    bool positive = (dy > 0);
    out = (positive == BLOCK_IS_POSITIVE_Y) ? GESTURE_BLOCK : GESTURE_PARRY;
  } else {
    out = GESTURE_ATTACK; // any horizontal or forward/back swing
  }
  return true;
}

// ---------------------------------------------------------------------
// Drawing helpers (shared)
// ---------------------------------------------------------------------
void drawBar(int x, int y, int w, int h, float frac, uint16_t fg, uint16_t bg) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  canvas.fillRect(x, y, w, h, bg);
  canvas.fillRect(x, y, (int)(w * frac), h, fg);
  canvas.drawRect(x, y, w, h, TFT_DARKGREY);
}

uint8_t fitSize(const String &s, int maxWidthPx, uint8_t maxSize) {
  int len = s.length();
  if (len == 0) return 1;
  for (int sz = maxSize; sz >= 1; sz--) {
    if (len * 6 * sz <= maxWidthPx) return (uint8_t)sz;
  }
  return 1;
}

void drawFitAt(const String &text, textdatum_t datum, int x, int y, uint8_t maxSize, uint16_t color) {
  if (text.length() == 0) return;
  int W = canvas.width();
  uint8_t sz = fitSize(text, W - 10, maxSize);
  canvas.setTextDatum(datum);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.setTextSize(sz);
  canvas.drawString(text, x, y);
}

void showMessage(const String &main, const String &sub, uint16_t color) {
  msgMain = main; msgSub = sub; msgColor = color;
}

void flashScreen(uint16_t color, uint32_t ms) {
  canvas.fillScreen(color);
  canvas.pushSprite(&M5.Lcd, 0, 0);
  delay(ms);
}

// A quick multi-frame red splatter, used whenever a clean (non-grazed) hit
// lands on the player - obstacles, traps, and monster attacks alike.
void playHitSplash() {
  int W = canvas.width(), H = canvas.height();
  uint32_t seed = millis();
  for (int f = 0; f < 2; f++) {
    canvas.fillScreen(f == 0 ? TFT_RED : shadeColor565(TFT_RED, 0.35f));
    uint32_t rnd = seed + (uint32_t)f * 977u;
    for (int i = 0; i < 9; i++) {
      rnd = rnd * 9301u + 49297u; int bx = (int)(rnd % (uint32_t)W);
      rnd = rnd * 9301u + 49297u; int by = (int)(rnd % (uint32_t)H);
      rnd = rnd * 9301u + 49297u; int br = 6 + (int)(rnd % 16u);
      canvas.fillCircle(bx, by, br, shadeColor565(TFT_MAROON, 0.65f));
    }
    canvas.pushSprite(&M5.Lcd, 0, 0);
    delay(f == 0 ? 65 : 55);
  }
}

// ---------------------------------------------------------------------
// First-person wireframe stone corridor
// ---------------------------------------------------------------------
static const int TUNNEL_RINGS = 4;
static const int TUNNEL_CYCLE_MS = 480; // time for one ring-step, sets walking/footstep pace

// Darkens a 565 color toward black by (1-factor); factor is clamped to [0,1].
// Used to fade the far end of the corridor into shadow, like torchlight
// falling off with distance.
uint16_t shadeColor565(uint16_t c, float factor) {
  if (factor < 0) factor = 0;
  if (factor > 1) factor = 1;
  uint8_t r = (c >> 11) & 0x1F;
  uint8_t g = (c >> 5) & 0x3F;
  uint8_t b = c & 0x1F;
  r = (uint8_t)(r * factor);
  g = (uint8_t)(g * factor);
  b = (uint8_t)(b * factor);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void drawCorridor(float phase, uint16_t color, int rings) {
  int W = canvas.width(), H = canvas.height();
  float bob = sinf(phase * 6.2831853f) * 2.0f; // small vertical sway synced to the footstep pace
  int cx = W / 2, cy = (int)(H * 0.40f + bob);
  int maxHalfW = (W - 10) / 2;
  int maxHalfH = (int)(H * 0.60f) / 2;

  // Solid floor and ceiling bands so the corridor reads as an enclosed
  // stone passage, not just floating wireframe lines.
  int topY = 10, botY = H - 12;
  int ceilH = (cy - maxHalfH) - topY;
  int floorH = botY - (cy + maxHalfH);
  if (ceilH > 0)  canvas.fillRect(0, topY, W, ceilH, shadeColor565(color, 0.30f));
  if (floorH > 0) canvas.fillRect(0, cy + maxHalfH, W, floorH, shadeColor565(color, 0.55f));

  int halfW[TUNNEL_RINGS], halfH[TUNNEL_RINGS];
  float depth[TUNNEL_RINGS];
  for (int i = 0; i < rings; i++) {
    float t = (i + phase) / rings;
    if (t > 1) t = 1;
    float scale = 1.0f - t * 0.90f;
    halfW[i] = (int)(maxHalfW * scale);
    halfH[i] = (int)(maxHalfH * scale);
    depth[i] = 1.0f - t * 0.65f; // farther rings dim, like light falling off into the dark
    canvas.drawRect(cx - halfW[i], cy - halfH[i], halfW[i] * 2, halfH[i] * 2, shadeColor565(color, depth[i]));
  }
  for (int i = 0; i < rings - 1; i++) {
    uint16_t lineColor = shadeColor565(color, depth[i]);
    canvas.drawLine(cx - halfW[i], cy - halfH[i], cx - halfW[i + 1], cy - halfH[i + 1], lineColor);
    canvas.drawLine(cx + halfW[i], cy - halfH[i], cx + halfW[i + 1], cy - halfH[i + 1], lineColor);
    canvas.drawLine(cx - halfW[i], cy + halfH[i], cx - halfW[i + 1], cy + halfH[i + 1], lineColor);
    canvas.drawLine(cx + halfW[i], cy + halfH[i], cx + halfW[i + 1], cy + halfH[i + 1], lineColor);
  }
}

// A wall-mounted torch with a flickering flame - purely cosmetic.
void drawTorch(int x, int y, int size) {
  int flick = random(0, 3);
  uint16_t flameColor = (flick == 0) ? TFT_YELLOW : (flick == 1) ? TFT_ORANGE : COLOR_TORCH;
  canvas.fillRect(x - 2, y, 4, size, TFT_DARKGREY); // sconce/bracket
  canvas.fillTriangle(x, y - size - flick, x - size / 3, y, x + size / 3, y, flameColor);
}

// A row of offset stone bricks - used to frame the top/bottom of every screen.
void drawBrickStrip(int y, int h) {
  int W = canvas.width();
  const int brickW = 14, brickH = 6;
  int rows = max(1, h / brickH);
  for (int row = 0; row < rows; row++) {
    int offset = (row % 2 == 0) ? 0 : brickW / 2;
    int by = y + row * brickH;
    for (int bx = -offset; bx < W; bx += brickW) {
      canvas.fillRect(bx, by, brickW - 2, brickH - 1, COLOR_STONE);
      canvas.drawRect(bx, by, brickW - 2, brickH - 1, TFT_BLACK);
    }
  }
}

// A small clump of wall moss - cosmetic dungeon-atmosphere detail.
void drawMossPatch(int x, int y, int size) {
  for (int i = 0; i < 5; i++) {
    int ox = (i * 37) % (size * 2) - size;
    int oy = (i * 23) % size - size / 2;
    canvas.fillCircle(x + ox, y + oy, max(1, size / 4), COLOR_MOSS);
  }
}

// A jagged crack in the floor - cosmetic dungeon-atmosphere detail.
void drawFloorCrack(int x, int y, int len) {
  int cx = x, cy = y;
  const int segs = 4;
  for (int i = 0; i < segs; i++) {
    int nx = cx + ((i % 2 == 0) ? 3 : -4);
    int ny = cy + len / segs;
    canvas.drawLine(cx, cy, nx, ny, TFT_BLACK);
    cx = nx; cy = ny;
  }
}

// Moss, floor cracks, and side support beams, layered on top of a corridor
// to make it read as an old, worn dungeon rather than a bare wireframe.
void drawDungeonAtmosphere() {
  int W = canvas.width(), H = canvas.height();
  drawMossPatch((int)(W * 0.10f), 14, 9);
  drawMossPatch((int)(W * 0.90f), H - 16, 8);
  drawFloorCrack((int)(W * 0.35f), (int)(H * 0.78f), (int)(H * 0.16f));
  drawFloorCrack((int)(W * 0.62f), (int)(H * 0.85f), (int)(H * 0.12f));
  uint16_t beamColor = shadeColor565(TFT_DARKGREY, 0.45f);
  canvas.fillRect(4, (int)(H * 0.32f), 3, H - (int)(H * 0.32f) - 14, beamColor);
  canvas.fillRect(W - 7, (int)(H * 0.32f), 3, H - (int)(H * 0.32f) - 14, beamColor);
}

// A dim stone tunnel + brick frame + atmosphere, used as a backdrop behind
// encounter HUDs.
void drawCorridorBackdrop() {
  drawCorridor(0.3f, COLOR_STONE, 2);
  drawBrickStrip(0, 10);
  drawBrickStrip(canvas.height() - 12, 10);
  drawDungeonAtmosphere();
}

// A stone plaque strip spanning the screen width, with centered text -
// the shared "UI chrome" used for titles and prompts on every encounter
// screen so they read as one consistent interface instead of floating text.
void drawBanner(int y, int h, const String &text, uint8_t size, uint16_t textColor) {
  int W = canvas.width();
  canvas.fillRect(0, y, W, h, shadeColor565(COLOR_STONE, 0.7f));
  canvas.drawRect(0, y, W, h, TFT_BLACK);
  drawFitAt(text, middle_center, W / 2, y + h / 2, size, textColor);
}

// A small cosmetic cobweb tucked into a corner - flipX mirrors it for the
// opposite side of the screen.
void drawCobweb(int x, int y, int size, bool flipX) {
  int dir = flipX ? -1 : 1;
  canvas.drawLine(x, y, x + dir * size, y, TFT_DARKGREY);
  canvas.drawLine(x, y, x, y + size, TFT_DARKGREY);
  canvas.drawLine(x, y, x + dir * size, y + size, TFT_DARKGREY);
  canvas.drawLine(x + dir * (size / 2), y, x + dir * (size / 2), y + size / 2, TFT_DARKGREY);
  canvas.drawLine(x, y + size / 2, x + dir * size / 2, y + size, TFT_DARKGREY);
}

// A primitive-drawn mage figure for the title screen - robe, pointed hat
// with a gem band, a raised hand, and a glowing staff orb.
void drawMageSprite(int cx, int cy, float s) {
  canvas.fillTriangle(cx, cy - (int)(30 * s), cx - (int)(16 * s), cy + (int)(8 * s), cx + (int)(16 * s), cy + (int)(8 * s), COLOR_MAGE_ROBE);
  canvas.fillTriangle(cx, cy - (int)(30 * s), cx - (int)(14 * s), cy + (int)(2 * s), cx + (int)(14 * s), cy + (int)(2 * s), COLOR_MAGE_ROBE_HI);
  canvas.fillTriangle(cx, cy - (int)(52 * s), cx - (int)(14 * s), cy - (int)(28 * s), cx + (int)(14 * s), cy - (int)(28 * s), COLOR_MAGE_HAT);
  canvas.fillCircle(cx, cy - (int)(24 * s), max(1, (int)(3 * s)), TFT_GOLD);
  canvas.fillCircle(cx - (int)(16 * s), cy - (int)(4 * s), max(1, (int)(6 * s)), COLOR_MAGE_HAND);
  canvas.drawLine(cx - (int)(16 * s), cy + (int)(2 * s), cx - (int)(16 * s), cy - (int)(40 * s), COLOR_MAGE_STAFF);
  canvas.fillCircle(cx - (int)(16 * s), cy - (int)(42 * s), max(1, (int)(4 * s)), TFT_CYAN);
  canvas.fillCircle(cx - (int)(16 * s), cy - (int)(42 * s), max(1, (int)(2 * s)), TFT_WHITE);
}

// ---------------------------------------------------------------------
// Walking (auto-advance between encounters, with a chance of a random
// side event or a bit of scene-setting flavor text along the way)
// ---------------------------------------------------------------------
// A single dripping crack: irregular period, irregular fall-duration, and a
// chance of skipping a cycle entirely, so it never falls on a metronome.
void drawDrip(int dx, uint32_t now, uint32_t slotBase, uint32_t seedOffset) {
  uint32_t slot = now / slotBase;
  uint32_t h = hashU32(slot * 2654435761u + seedOffset);
  if ((h % 100u) < 40u) return; // ~40% of slots stay dry
  uint32_t h2 = hashU32(slot * 40503u + seedOffset + 17u);
  uint32_t fallDur = (uint32_t)(slotBase * (0.5f + (h2 % 100u) / 100.0f * 0.35f));
  uint32_t phase = now - slot * slotBase;
  if (phase < fallDur) {
    int H = canvas.height();
    int dy = (int)(H * 0.14f) + (int)(((float)phase / fallDur) * (H * 0.20f));
    canvas.fillCircle(dx, dy, 2, TFT_CYAN);
  }
}

void drawWalkFrame(uint32_t elapsedMs) {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);

  float phase = fmodf(elapsedMs / (float)TUNNEL_CYCLE_MS, 1.0f);
  drawCorridor(phase, COLOR_TORCH, TUNNEL_RINGS);
  drawDungeonAtmosphere();
  drawTorch((int)(W * 0.14f), (int)(H * 0.30f), 10);
  drawTorch((int)(W * 0.86f), (int)(H * 0.30f), 10);

  // Two independent dripping cracks with irregular timing (period,
  // duration, and a chance to skip a cycle) so the drips feel organic.
  uint32_t now32 = millis();
  drawDrip((int)(W * 0.28f), now32, 780, 0);
  drawDrip((int)(W * 0.68f), now32, 930, 50);

  float frac = elapsedMs / (float)walkDurationMs;
  if (frac > WALK_APPEAR_FRAC) {
    float grow = (frac - WALK_APPEAR_FRAC) / (1.0f - WALK_APPEAR_FRAC);
    if (grow > 1) grow = 1;
    int size = (int)(grow * 30) + 4;
    int cx = W / 2, cy = (int)(H * 0.40f);
    EncounterKind next = sequence[sequenceIndex];
    if (next == ENC_OBSTACLE) {
      drawBoulderSprite(cx, cy, size / 2, COLOR_ROCK);
    } else if (next == ENC_TREASURE) {
      canvas.fillTriangle(cx, cy - size / 2, cx - size / 2, cy, cx, cy + size / 2, TFT_GOLD);
      canvas.fillTriangle(cx, cy - size / 2, cx + size / 2, cy, cx, cy + size / 2, TFT_GOLD);
    } else if (next == ENC_MONSTER || next == ENC_BOSS) {
      // Still too far to make out - just a shadowed shape with eyes that
      // catch the torchlight, until you're close enough to see it clearly.
      uint16_t eyeColor = (next == ENC_BOSS) ? TFT_MAGENTA : TFT_RED;
      canvas.fillCircle(cx, cy, size / 2, TFT_BLACK);
      canvas.drawCircle(cx, cy, size / 2, TFT_DARKGREY);
      canvas.fillCircle(cx - size / 5, cy - size / 8, max(1, size / 8), eyeColor);
      canvas.fillCircle(cx + size / 5, cy - size / 8, max(1, size / 8), eyeColor);
    }
  }

  if (showFlavorThisWalk && elapsedMs < 1100) {
    canvas.fillRect(0, H - 48, W, 14, TFT_BLACK);
    const char* line = flavorIsLate ? FLAVOR_LINES_LATE[flavorLineIndex] : FLAVOR_LINES[flavorLineIndex];
    drawFitAt(line, bottom_center, W / 2, H - 38, 2, TFT_LIGHTGREY);
  }

  canvas.fillRect(0, H - 26, W, 26, TFT_BLACK);
  drawFitAt("HP", top_left, 4, H - 24, 2, TFT_WHITE);
  drawBar(24, H - 24, W - 30, 10, playerHP / (float)PLAYER_MAX_HP, TFT_GREEN, TFT_DARKGREEN);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void beginWalk(bool allowEvent) {
  mode = MODE_WALK;
  walkStartedAt = millis();
  walkDurationMs = WALK_DURATION_BASE_MS + random(-WALK_JITTER_MS, WALK_JITTER_MS);
  if (walkDurationMs < 500) walkDurationMs = 500;
  lastWalkDraw = 0;
  lastStepIndex = -1;
  walkEventFired = false;
  walkEventKind = EVENT_NONE;

  showFlavorThisWalk = random(0, 100) < FLAVOR_LINE_CHANCE_PCT;
  if (showFlavorThisWalk) {
    // Well into the run, lean toward "getting close" lines instead of the
    // general ambiance pool, for a sense of progress through the dungeon.
    float progress = (float)sequenceIndex / (float)TOTAL_ENCOUNTERS;
    flavorIsLate = (progress > 0.6f) && (random(0, 100) < 50);
    flavorLineIndex = flavorIsLate ? random(0, FLAVOR_LINE_LATE_COUNT) : random(0, FLAVOR_LINE_COUNT);
  }

  if (allowEvent) {
    float mult = nextWalkEventChanceMultiplier;
    int herbPct = (int)(WALK_EVENT_HERB_PCT * mult);
    int trapPct = (int)(WALK_EVENT_TRAP_PCT * mult);
    int ambushPct = (int)(WALK_EVENT_AMBUSH_PCT * mult);
    int total = herbPct + trapPct + ambushPct;
    if (total > 90) {
      float scale = 90.0f / total;
      herbPct = (int)(herbPct * scale);
      trapPct = (int)(trapPct * scale);
      ambushPct = (int)(ambushPct * scale);
    }
    int roll = random(0, 100);
    if (roll < herbPct) walkEventKind = EVENT_HERB;
    else if (roll < herbPct + trapPct) walkEventKind = EVENT_TRAP;
    else if (roll < herbPct + trapPct + ambushPct) walkEventKind = EVENT_AMBUSH;

    if (walkEventKind != EVENT_NONE) {
      uint32_t minOff = (uint32_t)(walkDurationMs * 0.20f);
      uint32_t maxOff = (uint32_t)(walkDurationMs * 0.45f);
      if (maxOff <= minOff) maxOff = minOff + 50;
      walkEventAtMs = minOff + random(0, maxOff - minOff);
    }
  }
  nextWalkEventChanceMultiplier = 1.0f; // a fork's nudge only ever applies to the very next walk
}

void triggerWalkEvent(EventKind k);

void updateWalk(uint32_t now) {
  uint32_t elapsed = now - walkStartedAt;

  int stepIdx = (int)(elapsed / (uint32_t)TUNNEL_CYCLE_MS);
  if (stepIdx != lastStepIndex) { lastStepIndex = stepIdx; beepFootstep(); }

  if (!walkEventFired && walkEventKind != EVENT_NONE && elapsed >= walkEventAtMs) {
    walkEventFired = true;
    triggerWalkEvent(walkEventKind);
    return;
  }

  if (elapsed >= walkDurationMs) {
    startEncounter();
    return;
  }
  if (now - lastWalkDraw > 40) {
    drawWalkFrame(elapsed);
    lastWalkDraw = now;
  }
}

// ---------------------------------------------------------------------
// Random mid-walk side events - bonus content that doesn't touch the
// main obstacle/monster sequence, just happens (or doesn't) along the way.
// ---------------------------------------------------------------------
void triggerHerbEvent() {
  int heal = random(8, 16);
  playerHP += heal;
  if (playerHP > PLAYER_MAX_HP) playerHP = PLAYER_MAX_HP;
  beepGood();

  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  drawFitAt("HEALTH HERB!", middle_center, W / 2, H / 2 - 14, 2, TFT_GREEN);
  char buf[8]; snprintf(buf, sizeof(buf), "+%d HP", heal);
  drawFitAt(buf, middle_center, W / 2, H / 2 + 16, 3, TFT_GREEN);
  canvas.pushSprite(&M5.Lcd, 0, 0);
  delay(700);

  beginWalk(false);
}

// A row of sprung spikes bursting up from a trigger plate - cosmetic only.
void drawTrapSprite(int cx, int cy, int r) {
  canvas.fillRect(cx - r, cy + r / 3, r * 2, r / 2, TFT_DARKGREY);
  canvas.drawRect(cx - r, cy + r / 3, r * 2, r / 2, TFT_BLACK);
  for (int i = -2; i <= 2; i++) {
    int sx = cx + i * (r / 2);
    canvas.fillTriangle(sx, cy - r, sx - r / 6, cy + r / 3, sx + r / 6, cy + r / 3, TFT_LIGHTGREY);
  }
}

void drawTrapScreen(float remainingFrac) {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  drawBanner(4, 20, "TRAP!", 2, TFT_RED);
  drawTrapSprite(W / 2, (int)(H * 0.325f), (int)(min(W, H) * 0.15f));
  drawFitAt(gestureName(trapTarget), middle_center, W / 2, (int)(H * 0.49f), 4, TFT_YELLOW);
  drawBar(4, H - 46, W - 8, 10, remainingFrac, TFT_RED, TFT_BLACK);
  drawBanner(H - 20, 16, "quick, react!", 2, TFT_WHITE);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void enterTrap() {
  mode = MODE_TRAP;
  trapTarget = (GestureType)(1 + random(0, 3));
  trapWindowMs = TRAP_WINDOW_MS + random(-100, 100);
  if (trapWindowMs < 350) trapWindowMs = 350;
  trapStartedAt = millis();
  coolingDownUntil = 0;
  baselineInit = false;
  beepTrapSpring();
  drawTrapScreen(1.0f);
  lastTrapDraw = millis();
}

void resolveTrap(bool success) {
  if (success) {
    beepGood();
    flashScreen(TFT_DARKGREEN, 80);
  } else {
    bool grazed;
    int dmg = applyGraze(TRAP_FAIL_DMG, grazed);
    playerHP -= dmg;
    if (playerHP < 0) playerHP = 0;
    beepPlayerHit();
    if (grazed) flashScreen(TFT_ORANGE, 90);
    else playHitSplash();
  }
  if (playerHP <= 0) { triggerDeath(); return; }
  beginWalk(false);
}

void updateTrap(uint32_t now) {
  GestureType g;
  uint32_t elapsed = now - trapStartedAt;
  if (sampleAndClassify(g, now)) {
    coolingDownUntil = now + GESTURE_COOLDOWN_MS;
    resolveTrap(g == trapTarget);
  } else if (elapsed >= trapWindowMs) {
    resolveTrap(false);
  } else if (now - lastTrapDraw > 50) {
    drawTrapScreen(1.0f - (float)elapsed / (float)trapWindowMs);
    lastTrapDraw = now;
  }
}

void triggerAmbushEvent() {
  beepAmbush();
  flashScreen(TFT_RED, 100);
  enterCombat(false, true); // sideEvent = true: doesn't advance the main run
}

void triggerWalkEvent(EventKind k) {
  switch (k) {
    case EVENT_HERB:   triggerHerbEvent();   break;
    case EVENT_TRAP:   enterTrap();          break;
    case EVENT_AMBUSH: triggerAmbushEvent(); break;
    default: break;
  }
}

// ---------------------------------------------------------------------
// Forks - the corridor splits; the player picks a path with BtnA/BtnB.
// The choice doesn't change WHICH encounters remain, just nudges the
// odds of a random side event on the very next stretch of corridor.
// ---------------------------------------------------------------------
void drawForkScreen() {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  drawBanner(4, 20, "THE PATH SPLITS", 2, TFT_GOLD);

  int boxY1 = (int)(H * 0.28f), boxH = (int)(H * 0.22f);
  canvas.fillRect(4, boxY1, W - 8, boxH, shadeColor565(COLOR_STONE, 0.6f));
  canvas.drawRect(4, boxY1, W - 8, boxH, TFT_CYAN);
  drawFitAt("A: LEFT", top_center, W / 2, boxY1 + 4, 2, TFT_CYAN);
  drawFitAt(FORK_LEFT[forkPromptIndex], middle_center, W / 2, boxY1 + boxH / 2 + 6, 2, TFT_WHITE);

  int boxY2 = boxY1 + boxH + 8;
  canvas.fillRect(4, boxY2, W - 8, boxH, shadeColor565(COLOR_STONE, 0.6f));
  canvas.drawRect(4, boxY2, W - 8, boxH, TFT_YELLOW);
  drawFitAt("B: RIGHT", top_center, W / 2, boxY2 + 4, 2, TFT_YELLOW);
  drawFitAt(FORK_RIGHT[forkPromptIndex], middle_center, W / 2, boxY2 + boxH / 2 + 6, 2, TFT_WHITE);

  drawBanner(H - 22, 18, "A=left  B=right", 2, TFT_DARKGREY);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void enterFork() {
  mode = MODE_FORK;
  forkPromptIndex = random(0, FORK_COUNT);
  drawForkScreen();
}

void resolveFork(bool leftChosen) {
  nextWalkEventChanceMultiplier = leftChosen ? 0.5f : 1.6f;
  beepEncounter();

  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  drawFitAt(leftChosen ? "YOU GO LEFT..." : "YOU GO RIGHT...", middle_center, W / 2, H / 2, 2, TFT_WHITE);
  canvas.pushSprite(&M5.Lcd, 0, 0);
  delay(700);

  advanceRun();
}

// ---------------------------------------------------------------------
// Obstacles
// ---------------------------------------------------------------------
// A cracked, moss-patched boulder blocking the corridor.
void drawBoulderSprite(int cx, int cy, int r, uint16_t color) {
  uint16_t dark = shadeColor565(color, 0.5f);
  canvas.fillCircle(cx, cy, r, color);
  canvas.fillCircle(cx - (int)(r * 0.5f), cy + (int)(r * 0.35f), (int)(r * 0.6f), color);
  canvas.fillCircle(cx + (int)(r * 0.55f), cy + (int)(r * 0.3f), (int)(r * 0.55f), color);
  canvas.fillCircle(cx - (int)(r * 0.1f), cy - (int)(r * 0.4f), (int)(r * 0.5f), color);
  canvas.drawLine(cx - r / 3, cy - r / 2, cx - r / 8, cy, dark);
  canvas.drawLine(cx - r / 8, cy, cx, cy + r / 3, dark);
  canvas.drawLine(cx + r / 4, cy - r / 3, cx + r / 6, cy + r / 4, dark);
  canvas.fillCircle(cx - (int)(r * 0.4f), cy - (int)(r * 0.15f), max(1, r / 8), TFT_DARKGREEN);
  canvas.fillCircle(cx + (int)(r * 0.35f), cy + (int)(r * 0.45f), max(1, r / 10), TFT_DARKGREEN);
  canvas.fillCircle(cx - r / 3, cy - r / 2, max(1, r / 10), TFT_LIGHTGREY); // torchlit highlight edge
}

void drawObstacleScreen(float remainingFrac) {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  drawBanner(4, 20, "OBSTACLE!", 2, TFT_ORANGE);
  drawBoulderSprite(W / 2, (int)(H * 0.325f), (int)(min(W, H) * 0.15f), COLOR_ROCK);
  drawFitAt(gestureName(obstacleTarget), middle_center, W / 2, (int)(H * 0.49f), 4, TFT_CYAN);
  drawBar(4, H - 46, W - 8, 10, remainingFrac, TFT_RED, TFT_BLACK);
  drawBanner(H - 20, 16, "swing to cross!", 2, TFT_WHITE);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void enterObstacle() {
  mode = MODE_OBSTACLE;
  obstacleTarget = (GestureType)(1 + random(0, 3));
  obstacleWindowMs = OBSTACLE_WINDOW_MS + random(-OBSTACLE_JITTER_MS, OBSTACLE_JITTER_MS);
  if (obstacleWindowMs < 400) obstacleWindowMs = 400;
  obstacleStartedAt = millis();
  coolingDownUntil = 0;
  baselineInit = false;
  beepEncounter();
  drawObstacleScreen(1.0f);
  lastObstacleDraw = millis();
}

void resolveObstacle(bool success) {
  if (success) {
    runScore += SCORE_OBSTACLE;
    beepGood();
    flashScreen(TFT_DARKGREEN, 90);
  } else {
    bool grazed;
    int dmg = applyGraze(OBSTACLE_FAIL_DMG, grazed);
    playerHP -= dmg;
    if (playerHP < 0) playerHP = 0;
    beepPlayerHit();
    if (grazed) flashScreen(TFT_ORANGE, 90);
    else playHitSplash();
  }
  if (playerHP <= 0) { triggerDeath(); return; }
  advanceRun();
}

void updateObstacle(uint32_t now) {
  GestureType g;
  uint32_t elapsed = now - obstacleStartedAt;
  if (sampleAndClassify(g, now)) {
    coolingDownUntil = now + GESTURE_COOLDOWN_MS;
    resolveObstacle(g == obstacleTarget);
  } else if (elapsed >= obstacleWindowMs) {
    resolveObstacle(false);
  } else if (now - lastObstacleDraw > 60) {
    drawObstacleScreen(1.0f - (float)elapsed / (float)obstacleWindowMs);
    lastObstacleDraw = now;
  }
}

// ---------------------------------------------------------------------
// Treasure (always resolves - a swing grabs it, or it's auto-collected
// if you don't; there's no fail state at the finish line)
// ---------------------------------------------------------------------
void drawTreasureScreen(float remainingFrac) {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  drawBanner(4, 20, "TREASURE!", 2, TFT_GOLD);
  int cx = W / 2, cy = H / 2 - 10, size = 26;
  canvas.fillTriangle(cx, cy - size / 2, cx - size / 2, cy, cx, cy + size / 2, TFT_GOLD);
  canvas.fillTriangle(cx, cy - size / 2, cx + size / 2, cy, cx, cy + size / 2, TFT_GOLD);
  drawBar(4, H - 46, W - 8, 10, remainingFrac, TFT_GOLD, TFT_BLACK);
  drawBanner(H - 20, 16, "swing to grab it!", 2, TFT_WHITE);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void enterTreasure() {
  mode = MODE_TREASURE;
  coolingDownUntil = 0;
  baselineInit = false;
  treasureStartedAt = millis();
  beepEncounter();
  drawTreasureScreen(1.0f);
  lastTreasureDraw = millis();
}

void updateTreasure(uint32_t now) {
  GestureType g;
  uint32_t elapsed = now - treasureStartedAt;
  if (sampleAndClassify(g, now)) {
    coolingDownUntil = now + GESTURE_COOLDOWN_MS;
    runScore += SCORE_TREASURE;
    beepGood();
    flashScreen(TFT_GOLD, 120);
    advanceRun();
  } else if (elapsed >= TREASURE_WINDOW_MS) {
    runScore += SCORE_TREASURE;
    beepGood();
    advanceRun();
  } else if (now - lastTreasureDraw > 70) {
    drawTreasureScreen(1.0f - (float)elapsed / (float)TREASURE_WINDOW_MS);
    lastTreasureDraw = now;
  }
}

// ---------------------------------------------------------------------
// Combat. Regular monsters have species-specific behavior AND appearance;
// the two bosses (Hydra, Medusa) are always tougher, more imposing fights
// that scale further with how many dungeons deep you've pushed.
// ---------------------------------------------------------------------
void drawSlimeSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillCircle(cx, cy, (int)(r * 0.75f), color);
  canvas.fillCircle(cx - (int)(r * 0.55f), cy + (int)(r * 0.25f), (int)(r * 0.45f), color);
  canvas.fillCircle(cx + (int)(r * 0.55f), cy + (int)(r * 0.25f), (int)(r * 0.45f), color);
  canvas.fillRect(cx - (int)(r * 0.9f), cy, (int)(r * 1.8f), (int)(r * 0.55f), color); // flattens it into a puddle base
  canvas.fillCircle(cx - r / 3, cy, r / 6, TFT_WHITE);
  canvas.fillCircle(cx + r / 3, cy, r / 6, TFT_WHITE);
  canvas.fillCircle(cx - r / 3, cy, r / 10, TFT_RED);   // glowing pupils, not friendly
  canvas.fillCircle(cx + r / 3, cy, r / 10, TFT_RED);
  canvas.fillCircle(cx - r / 2, cy - r / 2, max(1, r / 8), TFT_WHITE); // shine highlight
  // a wide, dripping mouth full of little fangs
  canvas.fillRect(cx - (int)(r * 0.35f), cy + (int)(r * 0.32f), (int)(r * 0.7f), r / 8, TFT_BLACK);
  for (int i = -2; i <= 2; i++) {
    int fx = cx + i * (r / 6);
    canvas.fillTriangle(fx, cy + (int)(r * 0.32f), fx - r / 12, cy + (int)(r * 0.32f), fx - r / 20, cy + (int)(r * 0.32f) + r / 8, TFT_WHITE);
  }
}

void drawBatSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillTriangle(cx - (int)(r * 0.5f), cy, cx - (int)(r * 1.5f), cy - (int)(r * 0.6f), cx - (int)(r * 0.4f), cy + (int)(r * 0.5f), color);
  canvas.fillTriangle(cx + (int)(r * 0.5f), cy, cx + (int)(r * 1.5f), cy - (int)(r * 0.6f), cx + (int)(r * 0.4f), cy + (int)(r * 0.5f), color);
  canvas.fillCircle(cx, cy, (int)(r * 0.55f), color); // body, drawn over the wing roots
  canvas.fillTriangle(cx - (int)(r * 0.35f), cy - (int)(r * 0.45f), cx - (int)(r * 0.2f), cy - (int)(r * 0.9f), cx - (int)(r * 0.05f), cy - (int)(r * 0.45f), color);
  canvas.fillTriangle(cx + (int)(r * 0.05f), cy - (int)(r * 0.45f), cx + (int)(r * 0.2f), cy - (int)(r * 0.9f), cx + (int)(r * 0.35f), cy - (int)(r * 0.45f), color);
  canvas.fillCircle(cx - (int)(r * 0.2f), cy - (int)(r * 0.1f), max(1, (int)(r * 0.09f)), TFT_RED);
  canvas.fillCircle(cx + (int)(r * 0.2f), cy - (int)(r * 0.1f), max(1, (int)(r * 0.09f)), TFT_RED);
}

// Redesigned for clarity: pointed snout, big round ears, a curled tail,
// whiskers - reads as a rat rather than a generic blob.
void drawRatSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillCircle(cx - (int)(r * 0.1f), cy + (int)(r * 0.2f), (int)(r * 0.55f), color);
  uint16_t tailDark = shadeColor565(color, 0.7f);
  canvas.drawLine(cx - (int)(r * 0.55f), cy + (int)(r * 0.5f), cx - (int)(r * 0.95f), cy + (int)(r * 0.6f), tailDark);
  canvas.drawLine(cx - (int)(r * 0.95f), cy + (int)(r * 0.6f), cx - (int)(r * 1.15f), cy + (int)(r * 0.05f), tailDark);
  canvas.fillTriangle(cx + (int)(r * 0.25f), cy - (int)(r * 0.05f), cx + (int)(r * 0.95f), cy + (int)(r * 0.18f), cx + (int)(r * 0.3f), cy + (int)(r * 0.42f), color);
  canvas.fillCircle(cx - (int)(r * 0.4f), cy - (int)(r * 0.38f), (int)(r * 0.24f), color);
  canvas.fillCircle(cx - (int)(r * 0.4f), cy - (int)(r * 0.38f), (int)(r * 0.13f), shadeColor565(color, 0.55f));
  canvas.fillCircle(cx + (int)(r * 0.05f), cy - (int)(r * 0.42f), (int)(r * 0.2f), color);
  canvas.fillCircle(cx + (int)(r * 0.05f), cy - (int)(r * 0.42f), (int)(r * 0.1f), shadeColor565(color, 0.55f));
  canvas.fillCircle(cx + (int)(r * 0.3f), cy + (int)(r * 0.06f), max(1, (int)(r * 0.08f)), TFT_RED);
  canvas.fillCircle(cx + (int)(r * 0.85f), cy + (int)(r * 0.22f), max(1, (int)(r * 0.07f)), TFT_BLACK);
  canvas.drawLine(cx + (int)(r * 0.7f), cy + (int)(r * 0.18f), cx + (int)(r * 1.15f), cy + (int)(r * 0.02f), TFT_WHITE);
  canvas.drawLine(cx + (int)(r * 0.7f), cy + (int)(r * 0.26f), cx + (int)(r * 1.15f), cy + (int)(r * 0.32f), TFT_WHITE);
  canvas.fillCircle(cx - (int)(r * 0.3f), cy + (int)(r * 0.65f), max(1, (int)(r * 0.14f)), shadeColor565(color, 0.8f));
  canvas.fillCircle(cx + (int)(r * 0.15f), cy + (int)(r * 0.65f), max(1, (int)(r * 0.14f)), shadeColor565(color, 0.8f));
}

// New: an armed goblin - pointed ears, a leather cap, narrowed eyes, a
// crooked toothy grin, and a dagger arm.
void drawGoblinSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillTriangle(cx - (int)(r * 0.55f), cy - (int)(r * 0.05f), cx - (int)(r * 1.05f), cy - (int)(r * 0.35f), cx - (int)(r * 0.35f), cy - (int)(r * 0.35f), color);
  canvas.fillTriangle(cx + (int)(r * 0.55f), cy - (int)(r * 0.05f), cx + (int)(r * 1.05f), cy - (int)(r * 0.35f), cx + (int)(r * 0.35f), cy - (int)(r * 0.35f), color);
  canvas.fillCircle(cx, cy + (int)(r * 0.05f), (int)(r * 0.68f), color);
  canvas.fillTriangle(cx - (int)(r * 0.42f), cy - (int)(r * 0.32f), cx, cy - (int)(r * 0.68f), cx + (int)(r * 0.42f), cy - (int)(r * 0.32f), COLOR_LEATHER);
  canvas.fillRect(cx - (int)(r * 0.42f), cy - (int)(r * 0.34f), (int)(r * 0.84f), (int)(r * 0.1f), shadeColor565(COLOR_LEATHER, 0.6f));
  canvas.fillRect(cx - (int)(r * 0.38f), cy - (int)(r * 0.22f), (int)(r * 0.76f), (int)(r * 0.1f), shadeColor565(color, 0.6f));
  canvas.fillCircle(cx - (int)(r * 0.22f), cy - (int)(r * 0.05f), max(1, (int)(r * 0.1f)), TFT_YELLOW);
  canvas.fillCircle(cx + (int)(r * 0.22f), cy - (int)(r * 0.05f), max(1, (int)(r * 0.1f)), TFT_YELLOW);
  canvas.fillCircle(cx - (int)(r * 0.22f), cy - (int)(r * 0.05f), max(1, (int)(r * 0.04f)), TFT_BLACK);
  canvas.fillCircle(cx + (int)(r * 0.22f), cy - (int)(r * 0.05f), max(1, (int)(r * 0.04f)), TFT_BLACK);
  canvas.fillTriangle(cx - (int)(r * 0.07f), cy + (int)(r * 0.08f), cx + (int)(r * 0.07f), cy + (int)(r * 0.08f), cx, cy + (int)(r * 0.2f), shadeColor565(color, 0.7f));
  canvas.fillRect(cx - (int)(r * 0.3f), cy + (int)(r * 0.27f), (int)(r * 0.6f), (int)(r * 0.15f), TFT_BLACK);
  for (int i = 0; i < 3; i++) {
    int tx = cx - (int)(r * 0.25f) + i * (int)(r * 0.22f);
    canvas.fillTriangle(tx, cy + (int)(r * 0.27f), tx + (int)(r * 0.11f), cy + (int)(r * 0.27f), tx + (int)(r * 0.05f), cy + (int)(r * 0.4f), TFT_WHITE);
  }
  canvas.fillRect(cx + (int)(r * 0.55f), cy + (int)(r * 0.14f), (int)(r * 0.15f), (int)(r * 0.5f), COLOR_LEATHER);
  canvas.fillTriangle(cx + (int)(r * 0.48f), cy - (int)(r * 0.02f), cx + (int)(r * 0.77f), cy - (int)(r * 0.02f), cx + (int)(r * 0.625f), cy - (int)(r * 0.32f), COLOR_STEEL);
}

// New: a skull-and-ribcage skeleton, with reaching arm bones.
void drawSkeletonSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillCircle(cx, cy - (int)(r * 0.35f), (int)(r * 0.5f), color);
  canvas.fillCircle(cx - (int)(r * 0.2f), cy - (int)(r * 0.38f), (int)(r * 0.13f), TFT_BLACK);
  canvas.fillCircle(cx + (int)(r * 0.2f), cy - (int)(r * 0.38f), (int)(r * 0.13f), TFT_BLACK);
  canvas.fillTriangle(cx - (int)(r * 0.06f), cy - (int)(r * 0.22f), cx + (int)(r * 0.06f), cy - (int)(r * 0.22f), cx, cy - (int)(r * 0.1f), TFT_BLACK);
  canvas.fillRect(cx - (int)(r * 0.3f), cy - (int)(r * 0.02f), (int)(r * 0.6f), (int)(r * 0.14f), color);
  for (int i = 0; i < 4; i++) {
    int lx = cx - (int)(r * 0.24f) + i * (int)(r * 0.16f);
    canvas.drawLine(lx, cy - (int)(r * 0.02f), lx, cy + (int)(r * 0.12f), TFT_BLACK);
  }
  canvas.fillRect(cx - (int)(r * 0.06f), cy + (int)(r * 0.12f), (int)(r * 0.12f), (int)(r * 0.18f), color);
  canvas.fillRect(cx - (int)(r * 0.42f), cy + (int)(r * 0.3f), (int)(r * 0.84f), (int)(r * 0.5f), shadeColor565(color, 0.92f));
  for (int i = 0; i < 4; i++) {
    int ly = cy + (int)(r * 0.36f) + i * (int)(r * 0.11f);
    canvas.drawLine(cx - (int)(r * 0.38f), ly, cx + (int)(r * 0.38f), ly, TFT_BLACK);
  }
  canvas.drawLine(cx, cy + (int)(r * 0.3f), cx, cy + (int)(r * 0.8f), TFT_BLACK);
  canvas.drawLine(cx - (int)(r * 0.42f), cy + (int)(r * 0.4f), cx - (int)(r * 0.7f), cy + (int)(r * 0.7f), color);
  canvas.drawLine(cx - (int)(r * 0.7f), cy + (int)(r * 0.7f), cx - (int)(r * 0.62f), cy + (int)(r * 0.88f), color);
  canvas.drawLine(cx + (int)(r * 0.42f), cy + (int)(r * 0.4f), cx + (int)(r * 0.7f), cy + (int)(r * 0.7f), color);
  canvas.drawLine(cx + (int)(r * 0.7f), cy + (int)(r * 0.7f), cx + (int)(r * 0.62f), cy + (int)(r * 0.88f), color);
}

// New elite: a blazing phoenix - wingspread, crest feathers, flame tail.
void drawPhoenixSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillTriangle(cx - (int)(r * 0.1f), cy - (int)(r * 0.1f), cx - (int)(r * 1.3f), cy - (int)(r * 0.5f), cx - (int)(r * 0.3f), cy + (int)(r * 0.3f), color);
  canvas.fillTriangle(cx + (int)(r * 0.1f), cy - (int)(r * 0.1f), cx + (int)(r * 1.3f), cy - (int)(r * 0.5f), cx + (int)(r * 0.3f), cy + (int)(r * 0.3f), color);
  canvas.fillTriangle(cx - (int)(r * 0.15f), cy - (int)(r * 0.05f), cx - (int)(r * 0.85f), cy - (int)(r * 0.12f), cx - (int)(r * 0.25f), cy + (int)(r * 0.12f), TFT_YELLOW);
  canvas.fillTriangle(cx + (int)(r * 0.15f), cy - (int)(r * 0.05f), cx + (int)(r * 0.85f), cy - (int)(r * 0.12f), cx + (int)(r * 0.25f), cy + (int)(r * 0.12f), TFT_YELLOW);
  canvas.fillCircle(cx, cy, (int)(r * 0.42f), color);
  canvas.fillCircle(cx, cy - (int)(r * 0.5f), (int)(r * 0.24f), color);
  canvas.fillTriangle(cx - (int)(r * 0.05f), cy - (int)(r * 0.44f), cx + (int)(r * 0.1f), cy - (int)(r * 0.44f), cx, cy - (int)(r * 0.3f), TFT_YELLOW);
  canvas.fillTriangle(cx - (int)(r * 0.1f), cy - (int)(r * 0.72f), cx, cy - (int)(r * 0.98f), cx + (int)(r * 0.03f), cy - (int)(r * 0.68f), TFT_YELLOW);
  canvas.fillTriangle(cx + (int)(r * 0.05f), cy - (int)(r * 0.68f), cx + (int)(r * 0.16f), cy - (int)(r * 0.9f), cx + (int)(r * 0.14f), cy - (int)(r * 0.62f), TFT_RED);
  canvas.fillCircle(cx - (int)(r * 0.06f), cy - (int)(r * 0.52f), max(1, (int)(r * 0.06f)), TFT_BLACK);
  canvas.fillTriangle(cx - (int)(r * 0.15f), cy + (int)(r * 0.35f), cx - (int)(r * 0.04f), cy + (int)(r * 0.92f), cx + (int)(r * 0.1f), cy + (int)(r * 0.4f), TFT_RED);
  canvas.fillTriangle(cx, cy + (int)(r * 0.4f), cx + (int)(r * 0.1f), cy + (int)(r * 1.0f), cx + (int)(r * 0.25f), cy + (int)(r * 0.4f), TFT_YELLOW);
  canvas.fillTriangle(cx + (int)(r * 0.15f), cy + (int)(r * 0.35f), cx + (int)(r * 0.3f), cy + (int)(r * 0.85f), cx + (int)(r * 0.35f), cy + (int)(r * 0.35f), TFT_RED);
}

// New elite: a griffin - eagle head, lion body, taloned legs.
void drawGriffinSprite(int cx, int cy, int r, uint16_t color) {
  uint16_t wingColor = shadeColor565(color, 0.75f);
  canvas.fillCircle(cx, cy + (int)(r * 0.2f), (int)(r * 0.55f), color);
  canvas.fillTriangle(cx - (int)(r * 0.2f), cy - (int)(r * 0.1f), cx - (int)(r * 1.1f), cy - (int)(r * 0.55f), cx - (int)(r * 0.3f), cy + (int)(r * 0.15f), wingColor);
  canvas.fillTriangle(cx + (int)(r * 0.2f), cy - (int)(r * 0.1f), cx + (int)(r * 1.1f), cy - (int)(r * 0.55f), cx + (int)(r * 0.3f), cy + (int)(r * 0.15f), wingColor);
  canvas.fillCircle(cx, cy - (int)(r * 0.35f), (int)(r * 0.32f), COLOR_LEATHER);
  canvas.fillTriangle(cx - (int)(r * 0.28f), cy - (int)(r * 0.32f), cx - (int)(r * 0.55f), cy - (int)(r * 0.22f), cx - (int)(r * 0.28f), cy - (int)(r * 0.15f), TFT_YELLOW);
  canvas.fillCircle(cx - (int)(r * 0.08f), cy - (int)(r * 0.4f), max(1, (int)(r * 0.07f)), TFT_WHITE);
  canvas.fillCircle(cx - (int)(r * 0.08f), cy - (int)(r * 0.4f), max(1, (int)(r * 0.035f)), TFT_BLACK);
  canvas.fillTriangle(cx - (int)(r * 0.1f), cy - (int)(r * 0.6f), cx - (int)(r * 0.02f), cy - (int)(r * 0.78f), cx + (int)(r * 0.06f), cy - (int)(r * 0.58f), COLOR_LEATHER);
  canvas.fillRect(cx - (int)(r * 0.35f), cy + (int)(r * 0.6f), (int)(r * 0.16f), (int)(r * 0.3f), color);
  canvas.fillRect(cx + (int)(r * 0.2f), cy + (int)(r * 0.6f), (int)(r * 0.16f), (int)(r * 0.3f), color);
  canvas.fillCircle(cx - (int)(r * 0.27f), cy + (int)(r * 0.92f), max(1, (int)(r * 0.1f)), TFT_YELLOW);
  canvas.fillCircle(cx + (int)(r * 0.28f), cy + (int)(r * 0.92f), max(1, (int)(r * 0.1f)), TFT_YELLOW);
  canvas.drawLine(cx + (int)(r * 0.5f), cy + (int)(r * 0.4f), cx + (int)(r * 0.9f), cy + (int)(r * 0.1f), wingColor);
}

// Boss: Hydra - three necks and heads on a coiled body.
void drawHydraSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillCircle(cx, cy + (int)(r * 0.4f), (int)(r * 0.6f), color);
  canvas.fillRect(cx - (int)(r * 0.5f), cy + (int)(r * 0.2f), r, (int)(r * 0.5f), color);
  const float necks[3] = { -0.55f, 0.0f, 0.55f };
  for (int i = 0; i < 3; i++) {
    float nx = necks[i];
    int hx = cx + (int)(nx * r), hy = cy - (int)(r * 0.5f) - (int)(fabsf(nx) * r * 0.15f);
    uint16_t hc = shadeColor565(color, (i == 1) ? 1.0f : 0.82f);
    int bx = cx + (int)(nx * r * 0.5f), by = cy + (int)(r * 0.1f);
    canvas.drawLine(bx, by, hx, hy + (int)(r * 0.2f), hc);
    canvas.fillCircle(hx, hy, (int)(r * 0.22f), hc);
    canvas.fillCircle(hx - (int)(r * 0.08f), hy - (int)(r * 0.04f), max(1, (int)(r * 0.05f)), TFT_YELLOW);
    canvas.fillCircle(hx + (int)(r * 0.08f), hy - (int)(r * 0.04f), max(1, (int)(r * 0.05f)), TFT_YELLOW);
    canvas.fillTriangle(hx - (int)(r * 0.1f), hy + (int)(r * 0.15f), hx + (int)(r * 0.1f), hy + (int)(r * 0.15f), hx, hy + (int)(r * 0.3f), TFT_RED);
  }
}

// Boss: Medusa - a robed figure with a crown of snakes and a cursed gaze.
void drawMedusaSprite(int cx, int cy, int r, uint16_t color) {
  canvas.fillTriangle(cx, cy - (int)(r * 0.1f), cx - (int)(r * 0.5f), cy + (int)(r * 0.9f), cx + (int)(r * 0.5f), cy + (int)(r * 0.9f), COLOR_MEDUSA_ROBE);
  canvas.fillCircle(cx, cy - (int)(r * 0.25f), (int)(r * 0.35f), color);
  for (int i = -2; i <= 2; i++) {
    float ang = i * 0.35f;
    int bx = cx + (int)(sinf(ang) * r * 0.35f), by = cy - (int)(r * 0.5f);
    int hx = bx + (int)(sinf(ang + 0.6f) * r * 0.25f), hy = by - (int)(r * 0.25f);
    canvas.drawLine(cx + (int)(sinf(ang) * r * 0.2f), cy - (int)(r * 0.4f), hx, hy, TFT_DARKGREEN);
    canvas.fillCircle(hx, hy, max(1, (int)(r * 0.08f)), TFT_DARKGREEN);
    canvas.fillCircle(hx - (int)(r * 0.03f), hy, max(1, (int)(r * 0.025f)), TFT_YELLOW);
  }
  canvas.fillCircle(cx - (int)(r * 0.14f), cy - (int)(r * 0.28f), max(1, (int)(r * 0.07f)), TFT_YELLOW);
  canvas.fillCircle(cx + (int)(r * 0.14f), cy - (int)(r * 0.28f), max(1, (int)(r * 0.07f)), TFT_YELLOW);
  canvas.fillCircle(cx - (int)(r * 0.14f), cy - (int)(r * 0.28f), max(1, (int)(r * 0.03f)), TFT_RED);
  canvas.fillCircle(cx + (int)(r * 0.14f), cy - (int)(r * 0.28f), max(1, (int)(r * 0.03f)), TFT_RED);
  canvas.fillRect(cx - (int)(r * 0.1f), cy - (int)(r * 0.08f), (int)(r * 0.2f), max(1, (int)(r * 0.05f)), TFT_BLACK);
}

void drawMonsterSprite(uint16_t bodyColor) {
  int W = canvas.width(), H = canvas.height();
  int cx = W / 2, cy = (int)(H * 0.325f);
  int r = (int)(min(W, H) * 0.15f);
  if (isBossFight) {
    if (currentBossType == BOSS_HYDRA) drawHydraSprite(cx, cy, r, bodyColor);
    else drawMedusaSprite(cx, cy, r, bodyColor);
  } else {
    switch (currentSpeciesIndex) {
      case 0: drawSlimeSprite(cx, cy, r, bodyColor);    break;
      case 1: drawBatSprite(cx, cy, r, bodyColor);      break;
      case 2: drawRatSprite(cx, cy, r, bodyColor);      break;
      case 3: drawGoblinSprite(cx, cy, r, bodyColor);   break;
      case 4: drawSkeletonSprite(cx, cy, r, bodyColor); break;
      case 5: drawPhoenixSprite(cx, cy, r, bodyColor);  break;
      default: drawGriffinSprite(cx, cy, r, bodyColor); break;
    }
  }
}

void renderCombatHUD() {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();

  drawBanner(0, 26, monsterName, 3, TFT_WHITE);
  drawBar(4, 32, W - 8, 12, monsterHP / (float)monsterMaxHP, TFT_RED, TFT_MAROON);
  drawMonsterSprite(telegraphActive ? TFT_RED : (isBossFight ? TFT_MAGENTA : TFT_NAVY));

  canvas.fillRect(0, 104, W, 66, shadeColor565(COLOR_STONE, 0.6f));
  canvas.drawRect(0, 104, W, 66, TFT_BLACK);
  drawFitAt(msgMain, middle_center, W / 2, 118, 3, msgColor);
  drawFitAt(msgSub, middle_center, W / 2, 150, 4, msgColor);

  drawBar(4, 176, W - 8, 10, manaCurrent / (float)MANA_MAX,
          manaFull ? TFT_GOLD : TFT_SKYBLUE, TFT_NAVY);

  drawFitAt("HP", top_left, 4, 192, 2, TFT_WHITE);
  drawBar(4, 210, W - 8, 12, playerHP / (float)PLAYER_MAX_HP, TFT_GREEN, TFT_DARKGREEN);

  char buf[16];
  snprintf(buf, sizeof(buf), "x%d COMBO", comboLevel);
  drawFitAt(buf, middle_right, W - 4, 228, 2, TFT_YELLOW);

  if (telegraphActive) drawTelegraphBar(1.0f);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void drawTelegraphBar(float remainingFrac) {
  int W = canvas.width();
  drawBar(4, 100, W - 8, 6, remainingFrac, TFT_RED, TFT_BLACK);
}

// A quick magic "shine" - the monster flashes white with a few radiating
// sparkle lines - played whenever you land a hit on it, whether by
// attacking or by successfully blocking/parrying for reflect damage.
void flashMonsterHit() {
  int W = canvas.width(), H = canvas.height();
  int cx = W / 2, cy = (int)(H * 0.325f);
  int r = (int)(min(W, H) * 0.15f);
  for (int f = 0; f < 2; f++) {
    drawMonsterSprite(TFT_WHITE);
    for (int i = 0; i < 5; i++) {
      float ang = (i / 5.0f) * 6.2831853f + 1.4f;
      int x1 = cx + (int)(cosf(ang) * r), y1 = cy + (int)(sinf(ang) * r);
      int x2 = cx + (int)(cosf(ang) * (r + 14)), y2 = cy + (int)(sinf(ang) * (r + 14));
      canvas.drawLine(x1, y1, x2, y2, TFT_WHITE);
    }
    canvas.pushSprite(&M5.Lcd, 0, 0);
    delay(f == 0 ? 55 : 45);
  }
  drawMonsterSprite(telegraphActive ? TFT_RED : (isBossFight ? TFT_MAGENTA : TFT_NAVY));
  // no push here - every caller redraws and pushes the full HUD right after this returns
}

// A gold sparkle burst radiating from the monster, played right before the
// "DEFEATED!" HUD text - the "kill shine" magic effect.
void playKillShine() {
  int W = canvas.width(), H = canvas.height();
  int cx = W / 2, cy = (int)(H * 0.325f);
  for (int f = 0; f < 4; f++) {
    canvas.fillScreen(TFT_BLACK);
    drawCorridorBackdrop();
    float p = f / 3.0f;
    float burst = (p < 0.5f) ? (p / 0.5f) : (1.0f - (p - 0.5f) / 0.5f);
    canvas.fillCircle(cx, cy, (int)(26 * burst * 0.4f) + 2, TFT_GOLD);
    for (int i = 0; i < 8; i++) {
      float ang = (i / 8.0f) * 6.2831853f + p * 1.2f;
      int len = 14 + (int)(burst * 30);
      int x1 = cx + (int)(cosf(ang) * 6), y1 = cy + (int)(sinf(ang) * 6);
      int x2 = cx + (int)(cosf(ang) * len), y2 = cy + (int)(sinf(ang) * len);
      canvas.drawLine(x1, y1, x2, y2, (i % 2 == 0) ? TFT_GOLD : TFT_WHITE);
    }
    canvas.pushSprite(&M5.Lcd, 0, 0);
    delay(65);
  }
}

void scheduleNextMonsterAttack() {
  float frac = monsterHP / (float)monsterMaxHP;
  int interval = (int)(ATTACK_INTERVAL_START_MS - (1.0f - frac) * ATTACK_INTERVAL_SCALE_MS);
  if (!isBossFight) interval += SPECIES_INTERVAL_MOD_MS[currentSpeciesIndex];
  if (interval < ATTACK_INTERVAL_FLOOR_MS) interval = ATTACK_INTERVAL_FLOOR_MS;
  interval += (int)random(-ATTACK_INTERVAL_JITTER_MS, ATTACK_INTERVAL_JITTER_MS);
  if (interval < 300) interval = 300;
  nextAttackAt = millis() + interval;
}

void startMonsterTelegraph() {
  telegraphActive = true;
  telegraphStartedAt = millis();

  if (isBossFight) {
    currentDefense = (random(0, 2) == 0) ? DEF_BLOCK : DEF_PARRY;
  } else {
    int bias = SPECIES_BLOCK_BIAS_PCT[currentSpeciesIndex];
    currentDefense = (random(0, 100) < bias) ? DEF_BLOCK : DEF_PARRY;
  }

  float frac = monsterHP / (float)monsterMaxHP;
  uint32_t win = (uint32_t)(TELEGRAPH_WINDOW_START_MS - (1.0f - frac) * TELEGRAPH_WINDOW_SCALE_MS);
  if (win < (uint32_t)TELEGRAPH_WINDOW_FLOOR_MS) win = TELEGRAPH_WINDOW_FLOOR_MS;
  if (!isBossFight) win += SPECIES_WINDOW_MOD_MS[currentSpeciesIndex];
  if (win < 300) win = 300; // hard floor regardless of species
  telegraphWindowMs = win;

  if (isBossFight) beepTelegraph(400, 180);
  else beepTelegraph(SPECIES_TELE_HZ[currentSpeciesIndex], SPECIES_TELE_MS[currentSpeciesIndex]);

  uint16_t color = (currentDefense == DEF_BLOCK) ? TFT_SKYBLUE : TFT_RED;
  showMessage("INCOMING!", (currentDefense == DEF_BLOCK) ? "BLOCK!" : "PARRY!", color);
  renderCombatHUD();
  lastTelegraphDraw = millis();
}

void monsterDefeated() {
  uint32_t gained = isBossFight
    ? (uint32_t)(SCORE_BOSS + dungeonDepth * SCORE_DEPTH_BONUS)
    : (uint32_t)(monsterMaxHP * SCORE_MONSTER_PER_HP);
  runScore += gained;
  playKillShine();
  showMessage("DEFEATED!", monsterName, TFT_GOLD);
  renderCombatHUD();
  beepGood();
  delay(500);
  if (combatIsSideEvent) beginWalk(false);
  else advanceRun();
}

void resolveMonsterTelegraph(bool correct) {
  telegraphActive = false;
  if (correct) {
    monsterHP -= BLOCK_REFLECT_DMG;
    if (monsterHP < 0) monsterHP = 0;
    comboLevel = min(comboLevel + 1, MAX_COMBO);
    beepDefendSuccess();
    flashMonsterHit();
    char buf[8]; snprintf(buf, sizeof(buf), "-%d", BLOCK_REFLECT_DMG);
    showMessage(currentDefense == DEF_BLOCK ? "BLOCKED" : "PARRIED", buf,
                currentDefense == DEF_BLOCK ? TFT_SKYBLUE : TFT_RED);
  } else {
    float frac = monsterHP / (float)monsterMaxHP;
    int dmg = (int)((BOSS_HIT_DMG_BASE + (1.0f - frac) * BOSS_HIT_DMG_SCALE) * damageVariance());
    if (!isBossFight) dmg += SPECIES_DMG_MOD[currentSpeciesIndex];
    if (dmg < 1) dmg = 1;
    bool grazed;
    dmg = applyGraze(dmg, grazed);
    playerHP -= dmg;
    if (playerHP < 0) playerHP = 0;
    comboLevel = grazed ? max(0, comboLevel - 1) : 0;
    beepPlayerHit();
    if (grazed) flashScreen(TFT_ORANGE, 90);
    else playHitSplash();
    char buf[8]; snprintf(buf, sizeof(buf), "-%d", dmg);
    showMessage(grazed ? "GRAZED!" : "HIT!", buf, grazed ? TFT_ORANGE : TFT_RED);
  }
  renderCombatHUD();

  if (playerHP <= 0) { triggerDeath(); return; }
  if (monsterHP <= 0) { monsterDefeated(); return; }

  // Flurry is a boss-only trait - regular monsters never chain attacks.
  float frac = monsterHP / (float)monsterMaxHP;
  bool startFlurry = isBossFight &&
                      (frac < FLURRY_HP_FRAC_THRESHOLD && random(0, 100) < FLURRY_CHANCE_PERCENT);
  if (startFlurry) { delay(280); startMonsterTelegraph(); }
  else scheduleNextMonsterAttack();
}

void castSpellAtMonster(GestureType g) {
  if (g != GESTURE_ATTACK) {
    showMessage("...", "", TFT_DARKGREY);
    renderCombatHUD();
    return;
  }

  float multiplier = (1.0f + comboLevel * 0.1f) * damageVariance();
  int dmg; const char* label; uint16_t color;
  bool isCrit = false;

  if (manaFull) {
    dmg = (int)(ULTIMATE_DMG * multiplier);
    label = "ULTIMATE"; color = TFT_GOLD;
    manaCurrent = 0; manaFull = false;
    beepUltimate();
  } else {
    dmg = (int)(ATTACK_DMG * multiplier);
    label = "ATTACK"; color = TFT_CYAN;
    beepAttack();
    if (random(0, 100) < CRIT_CHANCE_PERCENT) {
      isCrit = true;
      dmg = (int)(dmg * CRIT_MULTIPLIER);
      beepCrit();
    }
    manaCurrent += MANA_PER_CAST;
    if (manaCurrent >= MANA_MAX) { manaCurrent = MANA_MAX; manaFull = true; }
  }
  if (dmg < 1) dmg = 1;

  monsterHP -= dmg;
  if (monsterHP < 0) monsterHP = 0;
  comboLevel = min(comboLevel + 1, MAX_COMBO);

  flashMonsterHit();
  char labelBuf[16];
  if (isCrit) snprintf(labelBuf, sizeof(labelBuf), "CRIT %s", label);
  else snprintf(labelBuf, sizeof(labelBuf), "%s", label);
  char buf[8]; snprintf(buf, sizeof(buf), "-%d", dmg);
  showMessage(labelBuf, buf, isCrit ? TFT_GOLD : color);
  renderCombatHUD();

  if (monsterHP <= 0) monsterDefeated();
}

void enterCombat(bool boss, bool sideEvent) {
  mode = MODE_COMBAT;
  isBossFight = boss;
  combatIsSideEvent = sideEvent;

  if (boss) {
    monsterMaxHP = BOSS_BASE_HP + (dungeonDepth - 1) * BOSS_HP_PER_DEPTH;
    monsterName = String(BOSS_NAMES[currentBossType]);
  } else {
    currentSpeciesIndex = pickSpeciesForDepth();
    monsterName = String(MONSTER_NAMES[currentSpeciesIndex]);
    monsterMaxHP = (int)(SPECIES_HP[currentSpeciesIndex] * (1.0f + (dungeonDepth - 1) * MONSTER_HP_DEPTH_SCALE));
    if (sideEvent) {
      monsterMaxHP = (int)(monsterMaxHP * 0.6f);
      if (monsterMaxHP < 15) monsterMaxHP = 15;
    }
  }
  monsterHP = monsterMaxHP;
  manaCurrent = 0; manaFull = false; comboLevel = 0;
  telegraphActive = false;
  coolingDownUntil = 0; baselineInit = false;

  beepEncounter();
  String headline = sideEvent ? String("AMBUSH!") : (boss ? String(BOSS_NAMES[currentBossType]) : String("A WILD"));
  String subline = boss ? String("BLOCKS THE WAY") : (monsterName + String(sideEvent ? " JUMPS OUT" : " APPEARS"));
  showMessage(headline, subline, TFT_WHITE);
  renderCombatHUD();
  scheduleNextMonsterAttack();
}

void updateCombat(uint32_t now) {
  GestureType g;
  if (telegraphActive) {
    uint32_t elapsed = now - telegraphStartedAt;
    if (sampleAndClassify(g, now)) {
      coolingDownUntil = now + GESTURE_COOLDOWN_MS;
      bool correct = (currentDefense == DEF_BLOCK && g == GESTURE_BLOCK) ||
                      (currentDefense == DEF_PARRY && g == GESTURE_PARRY);
      resolveMonsterTelegraph(correct);
    } else if (elapsed >= telegraphWindowMs) {
      resolveMonsterTelegraph(false);
    } else if (now - lastTelegraphDraw > 70) {
      drawTelegraphBar(1.0f - (float)elapsed / (float)telegraphWindowMs);
      canvas.pushSprite(&M5.Lcd, 0, 0);
      lastTelegraphDraw = now;
    }
  } else {
    if (now >= nextAttackAt) {
      startMonsterTelegraph();
    } else if (sampleAndClassify(g, now)) {
      coolingDownUntil = now + GESTURE_COOLDOWN_MS;
      castSpellAtMonster(g);
    }
  }
}

// ---------------------------------------------------------------------
// Run sequencing
// ---------------------------------------------------------------------
void buildSequence() {
  const int n = NUM_OBSTACLES + NUM_MONSTERS;
  EncounterKind core[n];
  int k = 0;
  for (int i = 0; i < NUM_OBSTACLES; i++) core[k++] = ENC_OBSTACLE;
  for (int i = 0; i < NUM_MONSTERS; i++) core[k++] = ENC_MONSTER;
  for (int i = n - 1; i > 0; i--) { // Fisher-Yates shuffle of everything placed so far
    int j = random(0, i + 1);
    EncounterKind t = core[i]; core[i] = core[j]; core[j] = t;
  }

  // Sprinkle two forks roughly a third and two-thirds of the way through.
  int insertAfter1 = max(0, n / 3 - 1);
  int insertAfter2 = max(insertAfter1 + 1, (2 * n) / 3 - 1);
  if (insertAfter2 >= n - 1) insertAfter2 = n - 2;
  if (insertAfter2 <= insertAfter1) insertAfter2 = insertAfter1 + 1;
  if (insertAfter2 >= n) insertAfter2 = n - 1;

  int out = 0;
  for (int i = 0; i < n; i++) {
    sequence[out++] = core[i];
    if (i == insertAfter1 || i == insertAfter2) sequence[out++] = ENC_FORK;
  }
  sequence[out++] = ENC_BOSS;     // always the final fight of a dungeon
  sequence[out++] = ENC_TREASURE; // always the last step
}

void startEncounter() {
  switch (sequence[sequenceIndex]) {
    case ENC_OBSTACLE: enterObstacle(); break;
    case ENC_MONSTER:  enterCombat(false, false); break;
    case ENC_BOSS:      enterBossFight(); break;
    case ENC_TREASURE: enterTreasure(); break;
    case ENC_FORK:      enterFork(); break;
  }
}

void advanceRun() {
  EncounterKind justDone = sequence[sequenceIndex];
  sequenceIndex++;
  if (justDone == ENC_TREASURE) { enterDungeonClear(); return; }
  beginWalk(true);
}

// ---------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------
void drawTitleScreen() {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridor(0.2f, COLOR_STONE, 3);
  drawBrickStrip(0, 10);
  drawBrickStrip(H - 12, 10);
  drawDungeonAtmosphere();
  drawCobweb(2, 12, 22, false);
  drawCobweb(W - 2, 12, 22, true);
  drawMageSprite((int)(W * 0.60f), (int)(H * 0.62f), 0.55f);

  drawFitAt("DUNGEON", middle_center, W / 2, (int)(H * 0.09f), 3, TFT_MAGENTA);
  drawFitAt("MAGE QUEST", middle_center, W / 2, (int)(H * 0.145f), 3, TFT_MAGENTA);

  drawFitAt("A cursed dungeon", middle_center, W / 2, (int)(H * 0.25f), 2, TFT_LIGHTGREY);
  drawFitAt("awaits your wand...", middle_center, W / 2, (int)(H * 0.32f), 2, TFT_LIGHTGREY);

  drawFitAt("SIDE = ATTACK", middle_center, W / 2, (int)(H * 0.40f), 1, TFT_WHITE);
  drawFitAt("UP/DOWN = DEFEND", middle_center, W / 2, (int)(H * 0.44f), 1, TFT_WHITE);

  drawFitAt("PRESS A", middle_center, W / 2, (int)(H * 0.85f), 3, TFT_YELLOW);
  drawFitAt("to begin", middle_center, W / 2, (int)(H * 0.915f), 2, TFT_YELLOW);

  char buf[24];
  if (bestScore > 0) snprintf(buf, sizeof(buf), "Best Score %lu", (unsigned long)bestScore);
  else snprintf(buf, sizeof(buf), "Best Score --");
  drawFitAt(buf, bottom_center, W / 2, H - 4, 1, TFT_DARKGREY);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void drawIntroStory() {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridor(0.2f, COLOR_TORCH, 3);
  drawCobweb(2, 12, 18, false);
  drawCobweb(W - 2, 12, 18, true);
  drawFitAt(DUNGEON_TITLE[dungeonIndex], middle_center, W / 2, (int)(H * 0.35f), 2, TFT_GOLD);
  drawFitAt(DUNGEON_FLAVOR[dungeonIndex], middle_center, W / 2, (int)(H * 0.50f), 2, TFT_WHITE);
  drawFitAt("...", middle_center, W / 2, (int)(H * 0.62f), 2, TFT_DARKGREY);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

// A second beat of lore, shown right after drawIntroStory() for a bit
// more of a story build-up before the walk begins.
void drawIntroLore() {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridor(0.25f, COLOR_TORCH, 3);
  drawFitAt(DUNGEON_LORE2[dungeonIndex], middle_center, W / 2, (int)(H * 0.42f), 2, TFT_WHITE);
  drawFitAt("your torch is all you have...", middle_center, W / 2, (int)(H * 0.58f), 2, TFT_LIGHTGREY);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

// Shown after the boss falls: your score, and a choice of two dungeons to
// push into next (BtnA/BtnB) - the run keeps going until you die.
void drawDungeonClearScreen() {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCorridorBackdrop();
  char title[24]; snprintf(title, sizeof(title), "%s SLAIN!", BOSS_NAMES[currentBossType]);
  drawBanner(4, 20, title, 2, TFT_GOLD);
  drawFitAt(DUNGEON_EPILOGUE[dungeonIndex], middle_center, W / 2, (int)(H * 0.21f), 1, TFT_DARKGREY);

  drawFitAt("SCORE", middle_center, W / 2, (int)(H * 0.29f), 2, TFT_LIGHTGREY);
  char scoreBuf[16]; snprintf(scoreBuf, sizeof(scoreBuf), "%lu", (unsigned long)runScore);
  drawFitAt(scoreBuf, middle_center, W / 2, (int)(H * 0.38f), 4, TFT_GOLD);
  drawFitAt("choose next dungeon", middle_center, W / 2, (int)(H * 0.48f), 1, TFT_LIGHTGREY);

  int cardW = (W - 16) / 2, cardY = (int)(H * 0.54f), cardH = (int)(H * 0.30f);
  canvas.fillRect(6, cardY, cardW, cardH, shadeColor565(COLOR_STONE, 1.0f));
  canvas.drawRect(6, cardY, cardW, cardH, TFT_BLACK);
  canvas.fillRect(10 + cardW, cardY, cardW, cardH, shadeColor565(COLOR_STONE, 1.0f));
  canvas.drawRect(10 + cardW, cardY, cardW, cardH, TFT_BLACK);
  drawFitAt("A", middle_center, 6 + cardW / 2, cardY + 15, 2, TFT_YELLOW);
  drawFitAt(DUNGEON_TITLE[nextDungeonOptionA], middle_center, 6 + cardW / 2, cardY + (int)(cardH * 0.62f), 1, TFT_WHITE);
  drawFitAt("B", middle_center, 10 + cardW + cardW / 2, cardY + 15, 2, TFT_YELLOW);
  drawFitAt(DUNGEON_TITLE[nextDungeonOptionB], middle_center, 10 + cardW + cardW / 2, cardY + (int)(cardH * 0.62f), 1, TFT_WHITE);

  char bestBuf[24]; snprintf(bestBuf, sizeof(bestBuf), "Best Score %lu", (unsigned long)bestScore);
  drawFitAt(bestBuf, bottom_center, W / 2, H - 4, 1, TFT_DARKGREY);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

void drawDeathScreen(bool record) {
  int W = canvas.width(), H = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  drawCobweb(2, 4, 16, false);
  drawCobweb(W - 2, 4, 16, true);
  drawFitAt("YOU DIED", middle_center, W / 2, (int)(H * 0.20f), 3, TFT_RED);
  drawFitAt(DEATH_LINES[random(0, DEATH_LINE_COUNT)], middle_center, W / 2, (int)(H * 0.36f), 2, TFT_DARKGREY);

  char scoreBuf[24]; snprintf(scoreBuf, sizeof(scoreBuf), "SCORE %lu", (unsigned long)runScore);
  drawFitAt(scoreBuf, middle_center, W / 2, (int)(H * 0.48f), 2, TFT_WHITE);
  char depthBuf[24]; snprintf(depthBuf, sizeof(depthBuf), "DEPTH %d REACHED", dungeonDepth);
  drawFitAt(depthBuf, middle_center, W / 2, (int)(H * 0.56f), 1, TFT_LIGHTGREY);
  if (record) drawFitAt("NEW BEST SCORE!", middle_center, W / 2, (int)(H * 0.65f), 2, TFT_YELLOW);

  drawFitAt("PRESS A", middle_center, W / 2, (int)(H * 0.80f), 3, TFT_YELLOW);
  drawFitAt("to try again", middle_center, W / 2, (int)(H * 0.885f), 2, TFT_YELLOW);
  canvas.pushSprite(&M5.Lcd, 0, 0);
}

// A dramatic beat shown right before a boss fight begins.
void enterBossFight() {
  int W = canvas.width(), H = canvas.height();
  currentBossType = (BossType)random(0, 2);
  canvas.fillScreen(TFT_BLACK);
  drawCorridor(0.15f, COLOR_TORCH, 2);
  drawDungeonAtmosphere();
  drawFitAt(BOSS_LINES[random(0, BOSS_LINE_COUNT)], middle_center, W / 2, (int)(H * 0.38f), 2, TFT_RED);
  char line2[28]; snprintf(line2, sizeof(line2), "THE %s AWAKENS", BOSS_NAMES[currentBossType]);
  drawFitAt(line2, middle_center, W / 2, (int)(H * 0.52f), 2, TFT_ORANGE);
  canvas.pushSprite(&M5.Lcd, 0, 0);
  beepTelegraph(180, 260);
  delay(1100);
  enterCombat(true, false);
}

// ---------------------------------------------------------------------
// Run/state transitions
// ---------------------------------------------------------------------
void startNewRun() {
  dungeonDepth = 1;
  runScore = 0;
  playerHP = PLAYER_MAX_HP;
  dungeonIndex = random(0, DUNGEON_COUNT);
  buildSequence();
  sequenceIndex = 0;

  drawIntroStory();
  delay(1300);
  drawIntroLore();
  delay(1100);

  beginWalk(true);
}

// Called after a boss falls and the player picks a next-dungeon card -
// pushes one dungeon deeper, grants a small heal, and keeps the run going.
void chooseNextDungeon(bool pickA) {
  dungeonIndex = pickA ? nextDungeonOptionA : nextDungeonOptionB;
  dungeonDepth++;
  playerHP = min(PLAYER_MAX_HP, playerHP + DUNGEON_CLEAR_HEAL);
  buildSequence();
  sequenceIndex = 0;

  drawIntroStory();
  delay(1200);
  drawIntroLore();
  delay(1000);

  beginWalk(true);
}

void enterDungeonClear() {
  mode = MODE_DUNGEON_CLEAR;
  nextDungeonOptionA = random(0, DUNGEON_COUNT);
  do {
    nextDungeonOptionB = random(0, DUNGEON_COUNT);
  } while (nextDungeonOptionB == nextDungeonOptionA && DUNGEON_COUNT > 1);
  beepVictory();
  drawDungeonClearScreen();
}

void triggerDeath() {
  mode = MODE_DEATH;
  bool record = false;
  if (runScore > bestScore) {
    bestScore = runScore;
    prefs.putUInt("bestscore", bestScore);
    record = true;
  }
  beepDefeat();
  drawDeathScreen(record);
}

// ---------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Lcd.setRotation(0);
  randomSeed((uint32_t)esp_random());

  // Draw every frame into an off-screen canvas the same size as the
  // display, then blit it in one shot with pushSprite(). Drawing straight
  // to the LCD (the old approach) sends dozens of separate small SPI
  // writes per frame, so a partially-drawn frame is visible for a moment -
  // that's what caused flicker and garbled-looking busy areas during
  // walking/combat. Compositing off-screen first and pushing once fixes
  // both, and also gives us headroom for the extra atmosphere/VFX detail
  // below since those draws are cheap RAM writes, not SPI traffic.
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Lcd.width(), M5.Lcd.height());

  COLOR_STONE        = canvas.color565(60, 55, 52);
  COLOR_TORCH        = canvas.color565(255, 130, 40);
  COLOR_ROCK         = canvas.color565(120, 108, 92);
  COLOR_LEATHER      = canvas.color565(90, 74, 42);
  COLOR_STEEL        = canvas.color565(184, 184, 184);
  COLOR_MOSS         = canvas.color565(62, 90, 46);
  COLOR_MAGE_ROBE    = canvas.color565(91, 79, 168);
  COLOR_MAGE_ROBE_HI = canvas.color565(122, 111, 203);
  COLOR_MAGE_HAT     = canvas.color565(62, 52, 128);
  COLOR_MAGE_HAND    = canvas.color565(231, 201, 166);
  COLOR_MAGE_STAFF   = canvas.color565(138, 106, 60);
  COLOR_MEDUSA_ROBE  = canvas.color565(62, 58, 94);

  prefs.begin("dungeonmagequest", false);
  bestScore = prefs.getUInt("bestscore", 0);

  M5.update();
  if (M5.BtnB.isPressed()) {
    Serial.begin(115200);
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.setTextSize(1);
    canvas.drawString("Calibration mode", canvas.width() / 2, canvas.height() / 2 - 8);
    canvas.drawString("See Serial Monitor", canvas.width() / 2, canvas.height() / 2 + 8);
    canvas.pushSprite(&M5.Lcd, 0, 0);
    while (true) {
      float ax, ay, az;
      if (M5.Imu.getAccelData(&ax, &ay, &az)) {
        Serial.printf("ax=%+.2f  ay=%+.2f  az=%+.2f\n", ax, ay, az);
      }
      delay(80);
    }
  }

  playStartupJingle();
  drawTitleScreen();
}

void loop() {
  M5.update();
  uint32_t now = millis();

  switch (mode) {
    case MODE_TITLE:
      if (M5.BtnA.wasPressed()) startNewRun();
      break;

    case MODE_WALK:
      updateWalk(now);
      break;

    case MODE_OBSTACLE:
      updateObstacle(now);
      break;

    case MODE_TRAP:
      updateTrap(now);
      break;

    case MODE_COMBAT:
      updateCombat(now);
      break;

    case MODE_TREASURE:
      updateTreasure(now);
      break;

    case MODE_FORK:
      if (M5.BtnA.wasPressed()) resolveFork(true);
      else if (M5.BtnB.wasPressed()) resolveFork(false);
      break;

    case MODE_DUNGEON_CLEAR:
      if (M5.BtnA.wasPressed()) chooseNextDungeon(true);
      else if (M5.BtnB.wasPressed()) chooseNextDungeon(false);
      break;

    case MODE_DEATH:
      if (M5.BtnA.wasPressed()) startNewRun();
      break;
  }

  delay(5);
}
