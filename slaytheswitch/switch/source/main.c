#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#define SCREEN_W 1280
#define SCREEN_H 720

#define BASE_CARD_COUNT 14
#define CARD_COUNT 28
#define HAND_SIZE 4
#define REWARD_SIZE 3
#define SHOP_SIZE 8
#define PLAYER_DECK_MAX 64

#define MAX_MAP_NODE_COUNT 64
#define MAX_MAP_EDGE_COUNT 160
#define MAP_LAYERS 14
#define MAP_WORLD_H 1980

#define MAX_ENEMIES 3
#define NORMAL_ENEMY_SPRITE_COUNT 10
#define ELITE_ENEMY_SPRITE_COUNT 11
#define BOSS_ENEMY_SPRITE_COUNT 3
#define CRAWLER_COUNT 2
#define BATTLE_BACKGROUND_COUNT 12
#define BATTLE_BACKGROUND_SET_SIZE 4

typedef enum {
    SCREEN_TITLE,
    SCREEN_CRAWLER,
    SCREEN_HOW_TO_PLAY,
    SCREEN_MAP,
    SCREEN_COMBAT,
    SCREEN_REWARD,
    SCREEN_CAMPFIRE,
    SCREEN_UPGRADE,
    SCREEN_SHOP,
    SCREEN_DEATH,
    SCREEN_WIN
} GameScreen;

typedef enum {
    NODE_START,
    NODE_COMBAT,
    NODE_ELITE,
    NODE_CAMPFIRE,
    NODE_SHOP,
    NODE_BOSS
} NodeType;

typedef struct {
    const char* name;
    const char* type;
    const char* effect1;
    const char* effect2;
    const char* imagePath;
    int cost;
    int damage;
    int block;
    int heal;
    int energyGain;
    int vulnerable;
    int weak;
    int affectsAll;
    int nextTurnBlock;
    int rare;
    int upgraded;
    unsigned char* img;
    int w;
    int h;
} Card;

typedef struct {
    int x;
    int y;
    NodeType type;
    int layer;
} MapNode;

typedef struct {
    int from;
    int to;
} MapEdge;

typedef struct {
    const char* name;
    int hp;
    int maxHp;
    int block;
    int intent;
    int lastIntent;
    int intentValue;
    int intentDebuff; // 0 = Weak, 1 = Vulnerable
    int aiType; // 0 = balanced, 1 = bruiser, 2 = guardian, 3 = hexer
    int vulnerable;
    int weak;
    unsigned char* img;
    int w;
    int h;
    int alive;
} Enemy;

typedef struct {
    const char* name;
    const char* imagePath;
    int maxHp;
    int startingBlock;
    int startingStrength;
    unsigned char* img;
    int w;
    int h;
} Crawler;

typedef struct {
    unsigned char* img;
    int w;
    int h;
} Background;

typedef struct {
    int bestFloors;
    int bestCoins;
    int bestElites;
    int bestBosses;
} SaveStats;

static Crawler crawlers[CRAWLER_COUNT] = {
    {"Knight", "romfs:/crawlers/knight.png", 80, 0, 1, NULL, 0, 0},
    {"Cleric", "romfs:/crawlers/cleric.png", 60, 2, 0, NULL, 0, 0}
};

static Background mainMenuBg = {NULL, 0, 0};
static Background howToBg = {NULL, 0, 0};
static Background battleBgs[BATTLE_BACKGROUND_COUNT];

static Card deck[CARD_COUNT] = {
    // Base cards. These are the normal cards the player can start with, buy, or earn.
    {"Slash", "ATTACK", "Deal 7 damage.", "", "romfs:/cards/slash.png", 1, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0},
    {"Shield", "SKILL", "Gain 6 Block.", "", "romfs:/cards/shield.png", 1, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0},
    {"Energy Orb", "SKILL", "Gain 1 Energy.", "", "romfs:/cards/energy-orb.png", 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, NULL, 0, 0},
    {"Thunder", "ATTACK", "Deal 4 damage.", "Apply 2 Vulnerable and 1 Weak.", "romfs:/cards/thunder.png", 1, 4, 0, 0, 0, 2, 1, 0, 0, 0, 0, NULL, 0, 0},
    {"Battle Axe", "ATTACK", "Deal 8 damage.", "Apply 1 Weak.", "romfs:/cards/axe.png", 1, 8, 0, 0, 0, 0, 1, 0, 0, 0, 0, NULL, 0, 0},
    {"Punch", "ATTACK", "Deal 5 damage.", "", "romfs:/cards/punch.png", 1, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0},
    {"Void Orb", "ATTACK", "Deal 7 damage.", "Heal 3.", "romfs:/cards/void-orb.png", 1, 7, 0, 3, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0},
    {"Fortify", "SKILL", "Gain 6 Block.", "Gain 1 Block next turn.", "romfs:/cards/fortify.png", 1, 0, 6, 0, 0, 0, 0, 0, 1, 0, 0, NULL, 0, 0},
    {"Stance", "SKILL", "Gain 3 Block.", "Apply 2 Weak to all enemies.", "romfs:/cards/stance.png", 1, 0, 3, 0, 0, 0, 2, 1, 0, 0, 0, NULL, 0, 0},
    {"Smoke Screen", "SKILL", "Apply 3 Weak to all enemies.", "", "romfs:/cards/smokescreen.png", 1, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, NULL, 0, 0},
    {"Mark", "SKILL", "Apply 3 Vulnerable to all enemies.", "", "romfs:/cards/mark.png", 1, 0, 0, 0, 0, 3, 0, 1, 0, 0, 0, NULL, 0, 0},
    {"Barrier", "SKILL", "Gain 14 Block.", "", "romfs:/cards/barrier.png", 2, 0, 14, 0, 0, 0, 0, 0, 0, 1, 0, NULL, 0, 0},
    {"Demon Claw", "ATTACK", "Deal 10 damage.", "Apply 4 Weak.", "romfs:/cards/demon-claw.png", 3, 10, 0, 0, 0, 0, 4, 0, 0, 1, 0, NULL, 0, 0},
    {"Power Up", "SKILL", "Gain 3 Strength.", "", "romfs:/cards/power-up.png", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0},

    // Upgraded copies. These are not separate shop/reward cards. Campfires replace one owned
    // base card with the matching upgraded card, so duplicate base cards upgrade one at a time.
    {"Slash+", "ATTACK", "Deal 10 damage.", "", "romfs:/cards/slashplus.png", 1, 10, 0, 0, 0, 0, 0, 0, 0, 0, 1, NULL, 0, 0},
    {"Shield+", "SKILL", "Gain 9 Block.", "", "romfs:/cards/shieldplus.png", 1, 0, 9, 0, 0, 0, 0, 0, 0, 0, 1, NULL, 0, 0},
    {"Energy Orb+", "SKILL", "Gain 4 Energy.", "", "romfs:/cards/energy-orbplus.png", 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 1, NULL, 0, 0},
    {"Thunder+", "ATTACK", "Deal 7 damage.", "Apply 3 Vulnerable and 4 Weak.", "romfs:/cards/thunderplus.png", 1, 7, 0, 0, 0, 3, 4, 0, 0, 0, 1, NULL, 0, 0},
    {"Battle Axe+", "ATTACK", "Deal 10 damage.", "Apply 3 Weak.", "romfs:/cards/axeplus.png", 1, 10, 0, 0, 0, 0, 3, 0, 0, 0, 1, NULL, 0, 0},
    {"Punch+", "ATTACK", "Deal 7 damage.", "", "romfs:/cards/punchplus.png", 1, 7, 0, 0, 0, 0, 0, 0, 0, 0, 1, NULL, 0, 0},
    {"Void Orb+", "ATTACK", "Deal 10 damage.", "Heal 5.", "romfs:/cards/void-orbplus.png", 1, 10, 0, 5, 0, 0, 0, 0, 0, 0, 1, NULL, 0, 0},
    {"Fortify+", "SKILL", "Gain 8 Block.", "Gain 3 Block next turn.", "romfs:/cards/fortifyplus.png", 1, 0, 8, 0, 0, 0, 0, 0, 3, 0, 1, NULL, 0, 0},
    {"Stance+", "SKILL", "Gain 8 Block.", "Apply 5 Weak to all enemies.", "romfs:/cards/stanceplus.png", 1, 0, 8, 0, 0, 0, 5, 1, 0, 0, 1, NULL, 0, 0},
    {"Smoke Screen+", "SKILL", "Apply 6 Weak to all enemies.", "", "romfs:/cards/smokescreenplus.png", 1, 0, 0, 0, 0, 0, 6, 1, 0, 0, 1, NULL, 0, 0},
    {"Mark+", "SKILL", "Apply 6 Vulnerable to all enemies.", "", "romfs:/cards/markplus.png", 1, 0, 0, 0, 0, 6, 0, 1, 0, 0, 1, NULL, 0, 0},
    {"Barrier+", "SKILL", "Gain 17 Block.", "", "romfs:/cards/barrierplus.png", 2, 0, 17, 0, 0, 0, 0, 0, 0, 1, 1, NULL, 0, 0},
    {"Demon Claw+", "ATTACK", "Deal 13 damage.", "Apply 7 Weak.", "romfs:/cards/demon-clawplus.png", 3, 13, 0, 0, 0, 0, 7, 0, 0, 1, 1, NULL, 0, 0},
    {"Power Up+", "SKILL", "Gain 6 Strength.", "", "romfs:/cards/power-upplus.png", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, NULL, 0, 0}
};

static unsigned char* normalEnemySprites[NORMAL_ENEMY_SPRITE_COUNT];
static int normalEnemyW[NORMAL_ENEMY_SPRITE_COUNT];
static int normalEnemyH[NORMAL_ENEMY_SPRITE_COUNT];

static unsigned char* eliteEnemySprites[ELITE_ENEMY_SPRITE_COUNT];
static int eliteEnemyW[ELITE_ENEMY_SPRITE_COUNT];
static int eliteEnemyH[ELITE_ENEMY_SPRITE_COUNT];

static unsigned char* bossEnemySprites[BOSS_ENEMY_SPRITE_COUNT];
static int bossEnemyW[BOSS_ENEMY_SPRITE_COUNT];
static int bossEnemyH[BOSS_ENEMY_SPRITE_COUNT];

static MapNode mapNodes[MAX_MAP_NODE_COUNT];
static MapEdge mapEdges[MAX_MAP_EDGE_COUNT];
static int mapNodeCount = 0;
static int mapEdgeCount = 0;
static int layerStart[MAP_LAYERS];
static int layerCount[MAP_LAYERS];

static int rand_range(int min, int max) {
    if (max <= min) return min;
    return min + (rand() % (max - min + 1));
}

static NodeType random_node_type(int layer) {
    int roll = rand() % 100;

    // First real row should be simple and readable.
    if (layer == 1) {
        if (roll < 88) return NODE_COMBAT;
        return NODE_CAMPFIRE;
    }

    // The full row before the boss is forced to campfires in generate_map().
    // Keep this fallback here in case the map rules are changed later.
    if (layer == MAP_LAYERS - 2) {
        return NODE_CAMPFIRE;
    }

    if (roll < 15) return NODE_ELITE;
    if (roll < 29) return NODE_CAMPFIRE;
    if (roll < 41) return NODE_SHOP;

    return NODE_COMBAT;
}

static void add_map_edge(int from, int to) {
    if (mapEdgeCount >= MAX_MAP_EDGE_COUNT) return;

    for (int i = 0; i < mapEdgeCount; i++)
        if (mapEdges[i].from == from && mapEdges[i].to == to)
            return;

    mapEdges[mapEdgeCount].from = from;
    mapEdges[mapEdgeCount].to = to;
    mapEdgeCount++;
}

static int orientation(int ax, int ay, int bx, int by, int cx, int cy) {
    long long v = (long long)(by - ay) * (cx - bx) - (long long)(bx - ax) * (cy - by);
    if (v == 0) return 0;
    return v > 0 ? 1 : 2;
}

static int edges_cross(int a, int b, int c, int d) {
    if (a == c || a == d || b == c || b == d) return 0;

    int o1 = orientation(mapNodes[a].x, mapNodes[a].y, mapNodes[b].x, mapNodes[b].y, mapNodes[c].x, mapNodes[c].y);
    int o2 = orientation(mapNodes[a].x, mapNodes[a].y, mapNodes[b].x, mapNodes[b].y, mapNodes[d].x, mapNodes[d].y);
    int o3 = orientation(mapNodes[c].x, mapNodes[c].y, mapNodes[d].x, mapNodes[d].y, mapNodes[a].x, mapNodes[a].y);
    int o4 = orientation(mapNodes[c].x, mapNodes[c].y, mapNodes[d].x, mapNodes[d].y, mapNodes[b].x, mapNodes[b].y);

    return o1 != o2 && o3 != o4;
}

static void add_map_edge_no_cross(int from, int to) {
    // Only block crossings between different nodes on neighboring rows.
    for (int i = 0; i < mapEdgeCount; i++) {
        if (mapNodes[mapEdges[i].from].layer == mapNodes[from].layer &&
            mapNodes[mapEdges[i].to].layer == mapNodes[to].layer &&
            edges_cross(from, to, mapEdges[i].from, mapEdges[i].to)) {
            return;
        }
    }

    add_map_edge(from, to);
}

static int nearest_node_in_layer_by_x(int layer, int x) {
    int best = layerStart[layer];
    int bestDist = abs(mapNodes[best].x - x);

    for (int i = 1; i < layerCount[layer]; i++) {
        int idx = layerStart[layer] + i;
        int d = abs(mapNodes[idx].x - x);
        if (d < bestDist) {
            best = idx;
            bestDist = d;
        }
    }

    return best;
}

static int has_incoming_edge(int node) {
    for (int i = 0; i < mapEdgeCount; i++)
        if (mapEdges[i].to == node)
            return 1;
    return 0;
}

static int has_outgoing_edge(int node) {
    for (int i = 0; i < mapEdgeCount; i++)
        if (mapEdges[i].from == node)
            return 1;
    return 0;
}

static int layer_has_type(int layer, NodeType type) {
    if (layer < 0 || layer >= MAP_LAYERS) return 0;
    if (layerCount[layer] <= 0) return 0;

    for (int i = 0; i < layerCount[layer]; i++) {
        int idx = layerStart[layer] + i;
        if (idx >= 0 && idx < mapNodeCount && mapNodes[idx].type == type)
            return 1;
    }

    return 0;
}

static int layer_should_not_random_campfire(int layer) {
    // Prevent back-to-back campfire rows. The final pre-boss row and the midpoint safety
    // campfire are intentional, so nearby random campfires are blocked instead.
    if (layer <= 1) return 0;
    if (layer == MAP_LAYERS - 3) return 1;        // row before guaranteed pre-boss campfires
    if (layer == MAP_LAYERS / 2 - 1) return 1;    // row before midpoint campfire
    if (layer == MAP_LAYERS / 2 + 1) return 1;    // row after midpoint campfire
    if (layer_has_type(layer - 1, NODE_CAMPFIRE)) return 1;
    return 0;
}

static void generate_map(int act) {
    (void)act;
    mapNodeCount = 0;
    mapEdgeCount = 0;

    for (int i = 0; i < MAP_LAYERS; i++) {
        layerStart[i] = 0;
        layerCount[i] = 0;
    }

    layerCount[0] = 1;
    for (int layer = 1; layer < MAP_LAYERS - 1; layer++) {
        // Keep row sizes controlled so the map reads like paths instead of spaghetti.
        int roll = rand() % 100;
        if (roll < 22) layerCount[layer] = 2;
        else if (roll < 82) layerCount[layer] = 3;
        else layerCount[layer] = 4;
    }
    layerCount[MAP_LAYERS - 1] = 1;

    int lastShopLayer = -99;
    int laneX[5] = {260, 430, 640, 850, 1020};

    for (int layer = 0; layer < MAP_LAYERS; layer++) {
        layerStart[layer] = mapNodeCount;
        int count = layerCount[layer];
        int shopUsedThisLayer = 0;
        int usedLane[5] = {0, 0, 0, 0, 0};

        for (int slot = 0; slot < count; slot++) {
            int lane = 2;

            if (layer == 0 || layer == MAP_LAYERS - 1) {
                lane = 2;
            } else {
                // Pick lanes left-to-right, but with variety each act.
                int minLane = 0;
                int maxLane = 4;
                int remaining = count - slot - 1;
                int pickable[5];
                int pickCount = 0;

                for (int l = minLane; l <= maxLane; l++) {
                    if (usedLane[l]) continue;

                    int freeAfter = 0;
                    for (int ll = l + 1; ll <= maxLane; ll++)
                        if (!usedLane[ll]) freeAfter++;

                    if (freeAfter >= remaining)
                        pickable[pickCount++] = l;
                }

                lane = pickable[rand_range(0, pickCount - 1)];
                usedLane[lane] = 1;
            }

            int x = laneX[lane];
            if (layer > 0 && layer < MAP_LAYERS - 1)
                x += rand_range(-24, 24);

            int y = 100 + layer * 140;

            NodeType type = NODE_COMBAT;
            if (layer == 0) {
                type = NODE_START;
            } else if (layer == MAP_LAYERS - 1) {
                type = NODE_BOSS;
            } else if (layer == MAP_LAYERS - 2) {
                // Final row before the boss: every path gets a guaranteed campfire.
                type = NODE_CAMPFIRE;
            } else if (layer == MAP_LAYERS / 2 && slot == count / 2) {
                // Mid-act safety campfire: at least one readable recovery node around halfway.
                type = NODE_CAMPFIRE;
            } else {
                type = random_node_type(layer);

                // Do not let random campfires chain together. Forced campfires still exist:
                // one around the midpoint and the full row before the boss.
                if (type == NODE_CAMPFIRE && layer_should_not_random_campfire(layer)) {
                    type = NODE_COMBAT;
                }

                // Shops should be occasional, not several rows in a row. If a shop is blocked,
                // fall back to combat instead of campfire so this cannot create CF streaks.
                if (type == NODE_SHOP) {
                    if (shopUsedThisLayer || layer - lastShopLayer <= 1) {
                        type = NODE_COMBAT;
                    } else {
                        shopUsedThisLayer = 1;
                        lastShopLayer = layer;
                    }
                }
            }

            mapNodes[mapNodeCount].x = x;
            mapNodes[mapNodeCount].y = y;
            mapNodes[mapNodeCount].type = type;
            mapNodes[mapNodeCount].layer = layer;
            mapNodeCount++;
        }
    }

    // Connect each row in readable, mostly vertical/diagonal paths. Any edge that would cross an
    // existing edge on the same row pair is skipped.
    for (int layer = 0; layer < MAP_LAYERS - 1; layer++) {
        int curStart = layerStart[layer];
        int nextStart = layerStart[layer + 1];
        int curCount = layerCount[layer];
        int nextCount = layerCount[layer + 1];

        for (int i = 0; i < curCount; i++) {
            int from = curStart + i;
            int primary = nearest_node_in_layer_by_x(layer + 1, mapNodes[from].x);
            add_map_edge_no_cross(from, primary);

            // Add a second branch sometimes, but only to nearby choices and never through another path.
            if (nextCount > 1 && rand() % 100 < 42) {
                int best = -1;
                int bestDist = 9999;

                for (int j = 0; j < nextCount; j++) {
                    int to = nextStart + j;
                    if (to == primary) continue;
                    int d = abs(mapNodes[to].x - mapNodes[from].x);
                    if (d < bestDist) {
                        best = to;
                        bestDist = d;
                    }
                }

                if (best >= 0 && bestDist <= 310)
                    add_map_edge_no_cross(from, best);
            }
        }

        // Make sure every next-row node is reachable. Use the closest previous node so this stays clean.
        for (int j = 0; j < nextCount; j++) {
            int to = nextStart + j;
            if (!has_incoming_edge(to)) {
                int from = nearest_node_in_layer_by_x(layer, mapNodes[to].x);
                add_map_edge_no_cross(from, to);
            }
        }

        // Make sure every current-row node has at least one outgoing edge.
        for (int i = 0; i < curCount; i++) {
            int from = curStart + i;
            if (!has_outgoing_edge(from)) {
                int to = nearest_node_in_layer_by_x(layer + 1, mapNodes[from].x);
                add_map_edge_no_cross(from, to);
            }
        }
    }
}

static void put_pixel(u32* fb, u32 stride, int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    fb[y * (stride / 4) + x] = color;
}

static void fill_rect(u32* fb, u32 stride, int x, int y, int w, int h, u32 color) {
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            put_pixel(fb, stride, x + xx, y + yy, color);
}

static void draw_rect(u32* fb, u32 stride, int x, int y, int w, int h, u32 color) {
    for (int i = 0; i < w; i++) {
        put_pixel(fb, stride, x + i, y, color);
        put_pixel(fb, stride, x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        put_pixel(fb, stride, x, y + i, color);
        put_pixel(fb, stride, x + w - 1, y + i, color);
    }
}

static void draw_thick_rect(u32* fb, u32 stride, int x, int y, int w, int h, int t, u32 color) {
    for (int i = 0; i < t; i++)
        draw_rect(fb, stride, x - i, y - i, w + i * 2, h + i * 2, color);
}

static void draw_line(u32* fb, u32 stride, int x0, int y0, int x1, int y1, u32 color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        put_pixel(fb, stride, x0, y0, color);
        put_pixel(fb, stride, x0 + 1, y0, color);
        put_pixel(fb, stride, x0, y0 + 1, color);
        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void fill_circle(u32* fb, u32 stride, int cx, int cy, int r, u32 color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r)
                put_pixel(fb, stride, cx + x, cy + y, color);
}

static void draw_centered_text(u32* fb, u32 stride, int cx, int y, const char* text, int scale, u32 color);

static void draw_image_cover(u32* fb, u32 stride, unsigned char* img, int iw, int ih) {
    if (!img || iw <= 0 || ih <= 0) {
        fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);
        draw_centered_text(fb, stride, SCREEN_W / 2, SCREEN_H / 2, "BACKGROUND MISSING", 3, 0xFFFF4040);
        return;
    }

    // Cover the whole screen without stretching. This is for backgrounds, not sprites.
    float ir = (float)iw / (float)ih;
    float sr = (float)SCREEN_W / (float)SCREEN_H;

    int sw = iw;
    int sh = ih;
    int sx0 = 0;
    int sy0 = 0;

    if (ir > sr) {
        sw = (int)((float)ih * sr);
        sx0 = (iw - sw) / 2;
    } else {
        sh = (int)((float)iw / sr);
        sy0 = (ih - sh) / 2;
    }

    if (sw <= 0) sw = iw;
    if (sh <= 0) sh = ih;

    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int sx = sx0 + x * sw / SCREEN_W;
            int sy = sy0 + y * sh / SCREEN_H;
            int i = (sy * iw + sx) * 4;

            unsigned char r = img[i];
            unsigned char g = img[i + 1];
            unsigned char b = img[i + 2];
            unsigned char a = img[i + 3];

            if (a > 10)
                put_pixel(fb, stride, x, y, (a << 24) | (b << 16) | (g << 8) | r);
            else
                put_pixel(fb, stride, x, y, 0xFF101018);
        }
    }
}
static void draw_image_fit(u32* fb, u32 stride, unsigned char* img, int iw, int ih, int bx, int by, int bw, int bh) {
    if (!img || iw <= 0 || ih <= 0) return;

    float ir = (float)iw / (float)ih;
    float br = (float)bw / (float)bh;

    int dw = bw;
    int dh = bh;

    if (ir > br) dh = (int)(bw / ir);
    else dw = (int)(bh * ir);

    int dx = bx + (bw - dw) / 2;
    int dy = by + (bh - dh) / 2;

    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int sx = x * iw / dw;
            int sy = y * ih / dh;
            int i = (sy * iw + sx) * 4;

            unsigned char r = img[i];
            unsigned char g = img[i + 1];
            unsigned char b = img[i + 2];
            unsigned char a = img[i + 3];

            if (a > 10)
                put_pixel(fb, stride, dx + x, dy + y, (a << 24) | (b << 16) | (g << 8) | r);
        }
    }
}


static void draw_image_cover_rect(u32* fb, u32 stride, unsigned char* img, int iw, int ih, int bx, int by, int bw, int bh) {
    if (!img || iw <= 0 || ih <= 0) {
        fill_rect(fb, stride, bx, by, bw, bh, 0xFF263033);
        draw_thick_rect(fb, stride, bx, by, bw, bh, 3, 0xFFFFA530);
        return;
    }

    float ir = (float)iw / (float)ih;
    float br = (float)bw / (float)bh;

    int sw = iw;
    int sh = ih;
    int sx0 = 0;
    int sy0 = 0;

    if (ir > br) {
        sw = (int)(ih * br);
        sx0 = (iw - sw) / 2;
    } else {
        sh = (int)(iw / br);
        sy0 = (ih - sh) / 2;
    }

    for (int y = 0; y < bh; y++) {
        for (int x = 0; x < bw; x++) {
            int sx = sx0 + x * sw / bw;
            int sy = sy0 + y * sh / bh;
            int i = (sy * iw + sx) * 4;

            unsigned char r = img[i];
            unsigned char g = img[i + 1];
            unsigned char b = img[i + 2];
            unsigned char a = img[i + 3];

            if (a > 10)
                put_pixel(fb, stride, bx + x, by + y, (a << 24) | (b << 16) | (g << 8) | r);
            else
                put_pixel(fb, stride, bx + x, by + y, 0xFF101018);
        }
    }
}

static void draw_char(u32* fb, u32 stride, int x, int y, char c, int scale, u32 color) {
    unsigned char rows[7] = {0};
    if (c >= 'a' && c <= 'z') c -= 32;

    switch (c) {
        case 'A': { unsigned char r[7]={14,17,17,31,17,17,17}; memcpy(rows,r,7); } break;
        case 'B': { unsigned char r[7]={30,17,17,30,17,17,30}; memcpy(rows,r,7); } break;
        case 'C': { unsigned char r[7]={14,17,16,16,16,17,14}; memcpy(rows,r,7); } break;
        case 'D': { unsigned char r[7]={30,17,17,17,17,17,30}; memcpy(rows,r,7); } break;
        case 'E': { unsigned char r[7]={31,16,16,30,16,16,31}; memcpy(rows,r,7); } break;
        case 'F': { unsigned char r[7]={31,16,16,30,16,16,16}; memcpy(rows,r,7); } break;
        case 'G': { unsigned char r[7]={14,17,16,23,17,17,14}; memcpy(rows,r,7); } break;
        case 'H': { unsigned char r[7]={17,17,17,31,17,17,17}; memcpy(rows,r,7); } break;
        case 'I': { unsigned char r[7]={14,4,4,4,4,4,14}; memcpy(rows,r,7); } break;
        case 'J': { unsigned char r[7]={7,2,2,2,18,18,12}; memcpy(rows,r,7); } break;
        case 'K': { unsigned char r[7]={17,18,20,24,20,18,17}; memcpy(rows,r,7); } break;
        case 'L': { unsigned char r[7]={16,16,16,16,16,16,31}; memcpy(rows,r,7); } break;
        case 'M': { unsigned char r[7]={17,27,21,21,17,17,17}; memcpy(rows,r,7); } break;
        case 'N': { unsigned char r[7]={17,25,21,19,17,17,17}; memcpy(rows,r,7); } break;
        case 'O': { unsigned char r[7]={14,17,17,17,17,17,14}; memcpy(rows,r,7); } break;
        case 'P': { unsigned char r[7]={30,17,17,30,16,16,16}; memcpy(rows,r,7); } break;
        case 'R': { unsigned char r[7]={30,17,17,30,20,18,17}; memcpy(rows,r,7); } break;
        case 'S': { unsigned char r[7]={15,16,16,14,1,1,30}; memcpy(rows,r,7); } break;
        case 'T': { unsigned char r[7]={31,4,4,4,4,4,4}; memcpy(rows,r,7); } break;
        case 'U': { unsigned char r[7]={17,17,17,17,17,17,14}; memcpy(rows,r,7); } break;
        case 'V': { unsigned char r[7]={17,17,17,17,17,10,4}; memcpy(rows,r,7); } break;
        case 'W': { unsigned char r[7]={17,17,17,21,21,21,10}; memcpy(rows,r,7); } break;
        case 'X': { unsigned char r[7]={17,17,10,4,10,17,17}; memcpy(rows,r,7); } break;
        case 'Y': { unsigned char r[7]={17,17,10,4,4,4,4}; memcpy(rows,r,7); } break;
        case 'Z': { unsigned char r[7]={31,1,2,4,8,16,31}; memcpy(rows,r,7); } break;
        case '0': { unsigned char r[7]={14,17,19,21,25,17,14}; memcpy(rows,r,7); } break;
        case '1': { unsigned char r[7]={4,12,4,4,4,4,14}; memcpy(rows,r,7); } break;
        case '2': { unsigned char r[7]={14,17,1,2,4,8,31}; memcpy(rows,r,7); } break;
        case '3': { unsigned char r[7]={30,1,1,14,1,1,30}; memcpy(rows,r,7); } break;
        case '4': { unsigned char r[7]={2,6,10,18,31,2,2}; memcpy(rows,r,7); } break;
        case '5': { unsigned char r[7]={31,16,16,30,1,1,30}; memcpy(rows,r,7); } break;
        case '6': { unsigned char r[7]={14,16,16,30,17,17,14}; memcpy(rows,r,7); } break;
        case '7': { unsigned char r[7]={31,1,2,4,8,8,8}; memcpy(rows,r,7); } break;
        case '8': { unsigned char r[7]={14,17,17,14,17,17,14}; memcpy(rows,r,7); } break;
        case '9': { unsigned char r[7]={14,17,17,15,1,1,14}; memcpy(rows,r,7); } break;
        case '+': { unsigned char r[7]={0,4,4,31,4,4,0}; memcpy(rows,r,7); } break;
        case '.': { unsigned char r[7]={0,0,0,0,0,6,6}; memcpy(rows,r,7); } break;
        case '/': { unsigned char r[7]={1,1,2,4,8,16,16}; memcpy(rows,r,7); } break;
        case ':': { unsigned char r[7]={0,6,6,0,6,6,0}; memcpy(rows,r,7); } break;
        case '-': { unsigned char r[7]={0,0,0,31,0,0,0}; memcpy(rows,r,7); } break;
        case '!': { unsigned char r[7]={4,4,4,4,4,0,4}; memcpy(rows,r,7); } break;
        default: break;
    }

    for (int yy = 0; yy < 7; yy++)
        for (int xx = 0; xx < 5; xx++)
            if (rows[yy] & (1 << (4 - xx)))
                fill_rect(fb, stride, x + xx * scale, y + yy * scale, scale, scale, color);
}

static void draw_text(u32* fb, u32 stride, int x, int y, const char* text, int scale, u32 color) {
    int cx = x;
    for (int i = 0; text[i]; i++) {
        if (text[i] == ' ') {
            cx += 4 * scale;
            continue;
        }
        draw_char(fb, stride, cx, y, text[i], scale, color);
        cx += 6 * scale;
    }
}

static int text_width(const char* text, int scale) {
    int w = 0;
    for (int i = 0; text[i]; i++)
        w += (text[i] == ' ') ? 4 * scale : 6 * scale;
    return w;
}

static void draw_centered_text(u32* fb, u32 stride, int cx, int y, const char* text, int scale, u32 color) {
    draw_text(fb, stride, cx - text_width(text, scale) / 2, y, text, scale, color);
}


static void load_backgrounds(void) {
    // Main menu background. Primary path matches: romfs/backgrounds/main-menu/mm-bg.png
    mainMenuBg.img = stbi_load("romfs:/backgrounds/main-menu/mm-bg.png", &mainMenuBg.w, &mainMenuBg.h, NULL, 4);

    // Fallbacks are harmless and make it easier to test alternate folder layouts.
    if (!mainMenuBg.img)
        mainMenuBg.img = stbi_load("romfs:/backgrounds/mm-bg.png", &mainMenuBg.w, &mainMenuBg.h, NULL, 4);
    if (!mainMenuBg.img)
        mainMenuBg.img = stbi_load("romfs:/backgrounds/main-menu.png", &mainMenuBg.w, &mainMenuBg.h, NULL, 4);
    if (!mainMenuBg.img)
        mainMenuBg.img = stbi_load("romfs:/backgrounds/main_menu/mm-bg.png", &mainMenuBg.w, &mainMenuBg.h, NULL, 4);

    if (!mainMenuBg.img) {
        mainMenuBg.w = 0;
        mainMenuBg.h = 0;
    }

    // How To Play image. User-made transparent PNG with Joy-Con/controller controls.
    // Expected path: romfs/backgrounds/main-menu/how-to-play.png
    howToBg.img = stbi_load("romfs:/backgrounds/main-menu/how-to-play.png", &howToBg.w, &howToBg.h, NULL, 4);
    if (!howToBg.img) {
        howToBg.w = 0;
        howToBg.h = 0;
    }

    for (int i = 0; i < BATTLE_BACKGROUND_COUNT; i++) {
        char path[96];
        snprintf(path, sizeof(path), "romfs:/backgrounds/dungeon%d.png", i + 1);
        battleBgs[i].img = stbi_load(path, &battleBgs[i].w, &battleBgs[i].h, NULL, 4);
        if (!battleBgs[i].img) {
            battleBgs[i].w = 0;
            battleBgs[i].h = 0;
        }
    }
}

static void free_backgrounds(void) {
    if (mainMenuBg.img) {
        stbi_image_free(mainMenuBg.img);
        mainMenuBg.img = NULL;
    }

    if (howToBg.img) {
        stbi_image_free(howToBg.img);
        howToBg.img = NULL;
    }

    for (int i = 0; i < BATTLE_BACKGROUND_COUNT; i++) {
        if (battleBgs[i].img) {
            stbi_image_free(battleBgs[i].img);
            battleBgs[i].img = NULL;
        }
    }
}

static Background* choose_battle_background_for_act(int act) {
    int theme = (act - 1) % 3;
    if (theme < 0) theme = 0;

    int start = theme * BATTLE_BACKGROUND_SET_SIZE;
    int offset = rand() % BATTLE_BACKGROUND_SET_SIZE;
    Background* bg = &battleBgs[start + offset];

    if (!bg->img) {
        for (int i = 0; i < BATTLE_BACKGROUND_COUNT; i++) {
            if (battleBgs[i].img) return &battleBgs[i];
        }
        return NULL;
    }

    return bg;
}

static const char* save_stats_path(void) {
    return "sdmc:/switch/slaytheswitch/stats.dat";
}

static void load_best_stats(int* bestFloors, int* bestCoins, int* bestElites, int* bestBosses) {
    *bestFloors = 0;
    *bestCoins = 0;
    *bestElites = 0;
    *bestBosses = 0;

    FILE* f = fopen(save_stats_path(), "rb");
    if (!f) return;

    SaveStats stats;
    size_t readCount = fread(&stats, sizeof(SaveStats), 1, f);
    fclose(f);

    if (readCount != 1) return;

    if (stats.bestFloors >= 0) *bestFloors = stats.bestFloors;
    if (stats.bestCoins >= 0) *bestCoins = stats.bestCoins;
    if (stats.bestElites >= 0) *bestElites = stats.bestElites;
    if (stats.bestBosses >= 0) *bestBosses = stats.bestBosses;
}

static void save_best_stats(int bestFloors, int bestCoins, int bestElites, int bestBosses) {
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/slaytheswitch", 0777);

    FILE* f = fopen(save_stats_path(), "wb");
    if (!f) return;

    SaveStats stats;
    stats.bestFloors = bestFloors;
    stats.bestCoins = bestCoins;
    stats.bestElites = bestElites;
    stats.bestBosses = bestBosses;

    fwrite(&stats, sizeof(SaveStats), 1, f);
    fclose(f);
}

static void load_enemy_sprite_pool(const char* prefix, int count, unsigned char* imgs[], int widths[], int heights[]) {
    for (int i = 0; i < count; i++) {
        char path[96];
        snprintf(path, sizeof(path), "romfs:/enemies/%s%d.png", prefix, i + 1);
        imgs[i] = stbi_load(path, &widths[i], &heights[i], NULL, 4);
        if (!imgs[i]) {
            widths[i] = 0;
            heights[i] = 0;
        }
    }
}

static void free_enemy_sprite_pool(int count, unsigned char* imgs[]) {
    for (int i = 0; i < count; i++) {
        if (imgs[i]) {
            stbi_image_free(imgs[i]);
            imgs[i] = NULL;
        }
    }
}

static void load_crawler_sprites(void) {
    for (int i = 0; i < CRAWLER_COUNT; i++) {
        crawlers[i].img = stbi_load(crawlers[i].imagePath, &crawlers[i].w, &crawlers[i].h, NULL, 4);
        if (!crawlers[i].img) {
            crawlers[i].w = 0;
            crawlers[i].h = 0;
        }
    }
}

static void free_crawler_sprites(void) {
    for (int i = 0; i < CRAWLER_COUNT; i++) {
        if (crawlers[i].img) {
            stbi_image_free(crawlers[i].img);
            crawlers[i].img = NULL;
        }
    }
}

static void assign_enemy_sprite(Enemy* e, NodeType type) {
    if (!e) return;

    e->img = NULL;
    e->w = 0;
    e->h = 0;

    if (type == NODE_BOSS) {
        int idx = rand() % BOSS_ENEMY_SPRITE_COUNT;
        e->img = bossEnemySprites[idx];
        e->w = bossEnemyW[idx];
        e->h = bossEnemyH[idx];
        return;
    }

    if (type == NODE_ELITE) {
        int idx = rand() % ELITE_ENEMY_SPRITE_COUNT;
        e->img = eliteEnemySprites[idx];
        e->w = eliteEnemyW[idx];
        e->h = eliteEnemyH[idx];
        return;
    }

    int idx = rand() % NORMAL_ENEMY_SPRITE_COUNT;
    e->img = normalEnemySprites[idx];
    e->w = normalEnemyW[idx];
    e->h = normalEnemyH[idx];
}

static void draw_card_name(u32* fb, u32 stride, Card* card, int x, int y, int w) {
    if (!card || !card->name) return;

    // Leave space for the top-left cost badge, then center the card name in the remaining top area.
    int nameAreaX = x + 42;
    int nameAreaW = w - 48;
    int scale = 2;

    if (text_width(card->name, scale) > nameAreaW)
        scale = 1;

    int cx = nameAreaX + nameAreaW / 2;
    int ty = y + (scale == 2 ? 12 : 15);

    // Small shadow so the name stays readable on top of your custom card image.
    draw_centered_text(fb, stride, cx + 2, ty + 2, card->name, scale, 0xDD000000);
    draw_centered_text(fb, stride, cx, ty, card->name, scale, 0xFFFFFFFF);
}

static void draw_card(u32* fb, u32 stride, Card* card, int x, int y, int selected) {
    if (!card) return;

    int w = 170;
    int h = 250;

    draw_image_cover_rect(fb, stride, card->img, card->w, card->h, x, y, w, h);

    // Dynamic overlays that are not baked into the custom card art.
    draw_card_name(fb, stride, card, x, y, w);

    char cost[8];
    snprintf(cost, sizeof(cost), "%d", card->cost);
    fill_rect(fb, stride, x + 6, y + 6, 32, 30, 0xDD101018);
    draw_thick_rect(fb, stride, x + 6, y + 6, 32, 30, 2, 0xFFFFD060);
    draw_text(fb, stride, x + 17, y + 13, cost, 2, 0xFFFFFFFF);

    if (selected)
        draw_thick_rect(fb, stride, x - 8, y - 8, w + 16, h + 16, 5, 0xFFFFFF40);
}

static int count_available_nodes(int currentNode) {
    int c = 0;
    for (int i = 0; i < mapEdgeCount; i++)
        if (mapEdges[i].from == currentNode)
            c++;
    return c;
}

static void get_available_nodes_sorted(int currentNode, int outNodes[8], int* outCount) {
    *outCount = 0;

    for (int i = 0; i < mapEdgeCount && *outCount < 8; i++) {
        if (mapEdges[i].from == currentNode) {
            outNodes[*outCount] = mapEdges[i].to;
            (*outCount)++;
        }
    }

    for (int i = 0; i < *outCount - 1; i++) {
        for (int j = i + 1; j < *outCount; j++) {
            if (mapNodes[outNodes[j]].x < mapNodes[outNodes[i]].x) {
                int t = outNodes[i];
                outNodes[i] = outNodes[j];
                outNodes[j] = t;
            }
        }
    }
}

static int get_available_node(int currentNode, int slot) {
    int nodes[8];
    int count = 0;
    get_available_nodes_sorted(currentNode, nodes, &count);

    if (count <= 0) return currentNode;
    if (slot < 0) slot = 0;
    if (slot >= count) slot = count - 1;

    return nodes[slot];
}

static int is_available_node(int currentNode, int nodeIdx, int* outSlot) {
    int nodes[8];
    int count = 0;
    get_available_nodes_sorted(currentNode, nodes, &count);

    for (int i = 0; i < count; i++) {
        if (nodes[i] == nodeIdx) {
            if (outSlot) *outSlot = i;
            return 1;
        }
    }

    return 0;
}

static const char* node_label(NodeType type) {
    if (type == NODE_START) return "ST";
    if (type == NODE_COMBAT) return "EN";
    if (type == NODE_ELITE) return "EL";
    if (type == NODE_CAMPFIRE) return "CF";
    if (type == NODE_SHOP) return "SH";
    return "BO";
}

static void draw_map_screen(u32* fb, u32 stride, int act, int currentNode, int selectedSlot, int coins) {
    fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF203722);

    int scrollY = mapNodes[currentNode].y - 330;
    if (scrollY < 0) scrollY = 0;
    if (scrollY > MAP_WORLD_H - SCREEN_H) scrollY = MAP_WORLD_H - SCREEN_H;

    for (int i = 0; i < mapEdgeCount; i++) {
        int from = mapEdges[i].from;
        int to = mapEdges[i].to;
        u32 lineColor = mapNodes[from].layer < mapNodes[currentNode].layer ? 0xFF8A5A20 : 0xFFFF8A00;
        draw_line(fb, stride,
            mapNodes[from].x, mapNodes[from].y - scrollY,
            mapNodes[to].x, mapNodes[to].y - scrollY,
            lineColor);
    }

    for (int i = 0; i < mapNodeCount; i++) {
        int drawY = mapNodes[i].y - scrollY;
        if (drawY < -80 || drawY > SCREEN_H + 80) continue;

        int slot = -1;
        int available = is_available_node(currentNode, i, &slot);
        int active = available && slot == selectedSlot;
        int current = i == currentNode;
        int past = mapNodes[i].layer < mapNodes[currentNode].layer;

        int r = active ? 52 : (current ? 42 : 34);
        u32 col = active ? 0xFF401010 : (current ? 0xFF204070 : (past ? 0xFF1E5A24 : 0xAA102010));

        if (mapNodes[i].type == NODE_START && !active && !current)
            col = 0xFF203050;

        fill_circle(fb, stride, mapNodes[i].x, drawY, r, col);
        draw_thick_rect(fb, stride, mapNodes[i].x - r, drawY - r, r * 2, r * 2, 2, active ? 0xFFFFFF40 : 0xFFFF8A00);
        draw_text(fb, stride, mapNodes[i].x - 20, drawY - 12, node_label(mapNodes[i].type), 2, 0xFFFFE8FF);
    }

    fill_rect(fb, stride, 0, 0, SCREEN_W, 158, 0xDD203722);

    char title[32];
    snprintf(title, sizeof(title), "ACT %d MAP", act);
    draw_text(fb, stride, 40, 30, title, 4, 0xFFFFD060);

    char coinText[32];
    snprintf(coinText, sizeof(coinText), "COINS %d", coins);
    draw_text(fb, stride, 1000, 35, coinText, 3, 0xFFFFD060);

    draw_text(fb, stride, 40, 90, "LEFT RIGHT SELECT   A ENTER", 2, 0xFFFFE8C0);
    draw_text(fb, stride, 40, 130, "TOP DOWN PATH   ONLY CONNECTED NODES CAN BE CHOSEN", 2, 0xFFFFE8C0);
}


static int roll_intent(int previous, int avoidRepeat) {
    int roll = rand() % 100;
    int next;

    // Weighted enemy AI: mostly attacks, sometimes buffs/heals, sometimes debuffs.
    // This makes fights easier to read and prevents bosses from feeling like pure debuff spam.
    if (roll < 58) next = 0;       // attack
    else if (roll < 78) next = 1;  // buff/heal
    else next = 2;                 // debuff

    if (avoidRepeat && previous >= 0) {
        int guard = 0;
        while (next == previous && guard < 10) {
            roll = rand() % 100;
            if (roll < 58) next = 0;
            else if (roll < 78) next = 1;
            else next = 2;
            guard++;
        }
        if (next == previous)
            next = (previous + 1 + (rand() % 2)) % 3;
    }

    return next;
}

static int planned_enemy_attack_damage(NodeType nodeType) {
    int dmg = 6 + rand() % 6;
    if (nodeType == NODE_ELITE) dmg += 3;
    if (nodeType == NODE_BOSS) dmg += 7;
    return dmg;
}

static void set_enemy_next_intent(Enemy* e, NodeType nodeType, int avoidRepeat) {
    if (!e) return;

    int roll = rand() % 100;
    int attackWeight = 58;
    int buffWeight = 20;

    // Enemy behavior variety. These are still simple, but normal enemies no longer
    // all feel identical: bruisers hit more, guardians heal/block more, hexers debuff more.
    if (e->aiType == 1) { attackWeight = 76; buffWeight = 12; }
    else if (e->aiType == 2) { attackWeight = 42; buffWeight = 40; }
    else if (e->aiType == 3) { attackWeight = 42; buffWeight = 14; }

    if (nodeType == NODE_ELITE) {
        attackWeight += 6;
        if (attackWeight > 82) attackWeight = 82;
    }
    if (nodeType == NODE_BOSS) {
        attackWeight = 58;
        buffWeight = 20;
    }

    int next;
    if (roll < attackWeight) next = 0;
    else if (roll < attackWeight + buffWeight) next = 1;
    else next = 2;

    if (avoidRepeat && e->lastIntent >= 0) {
        int guard = 0;
        while (next == e->lastIntent && guard < 10) {
            roll = rand() % 100;
            if (roll < attackWeight) next = 0;
            else if (roll < attackWeight + buffWeight) next = 1;
            else next = 2;
            guard++;
        }
        if (next == e->lastIntent)
            next = (e->lastIntent + 1 + (rand() % 2)) % 3;
    }

    e->intent = next;
    e->intentValue = 0;
    e->intentDebuff = 0;

    if (e->intent == 0) {
        e->intentValue = planned_enemy_attack_damage(nodeType);
        if (e->aiType == 1) e->intentValue += 2;
        if (e->aiType == 3 && e->intentValue > 2) e->intentValue -= 1;
    } else if (e->intent == 1) {
        e->intentValue = nodeType == NODE_BOSS ? 8 : 4;
        if (e->aiType == 2) e->intentValue += 3;
    } else {
        e->intentValue = nodeType == NODE_BOSS ? 3 : 2;
        if (e->aiType == 3) e->intentValue += 1;
        e->intentDebuff = rand() % 2;
    }
}

static int apply_damage_status_modifiers(int baseDamage, int attackerWeak, int defenderVulnerable);

static void draw_status_line(u32* fb, u32 stride, int x, int y, const char* label, int value, u32 color) {
    if (value <= 0) return;

    char text[32];
    snprintf(text, sizeof(text), "%s %d", label, value);
    fill_rect(fb, stride, x - 4, y - 4, 92, 20, 0xAA101018);
    draw_text(fb, stride, x, y, text, 1, color);
}

static void draw_enemy(u32* fb, u32 stride, Enemy* e, int x, int y, int selected, int compact, int player_vulnerable) {
    if (!e->alive) return;

    int w = compact ? 150 : 180;
    int h = compact ? 118 : 145;
    int barW = compact ? 165 : 200;
    int spriteBoxW = compact ? 126 : 150;
    int spriteBoxH = compact ? 78 : 96;

    fill_rect(fb, stride, x, y, w, h, selected ? 0xFF703020 : 0xCC202028);
    draw_thick_rect(fb, stride, x, y, w, h, selected ? 4 : 2, selected ? 0xFFFFFF40 : 0xFFFF6060);

    draw_centered_text(fb, stride, x + w / 2, y + 8, e->name, compact ? 1 : 2, 0xFFFFE8C0);

    int spriteX = x + (w - spriteBoxW) / 2;
    int spriteY = y + (compact ? 28 : 36);
    if (e->img)
        draw_image_fit(fb, stride, e->img, e->w, e->h, spriteX, spriteY, spriteBoxW, spriteBoxH);
    else
        fill_circle(fb, stride, x + w / 2, spriteY + spriteBoxH / 2, compact ? 28 : 36, 0xFF703020);

    char intentText[32];
    if (e->intent == 0) {
        int previewDmg = apply_damage_status_modifiers(e->intentValue, e->weak, player_vulnerable);
        snprintf(intentText, sizeof(intentText), "ATK %d", previewDmg);
    } else if (e->intent == 1) {
        snprintf(intentText, sizeof(intentText), "HEAL %d", e->intentValue);
    } else if (e->intentDebuff == 0) {
        snprintf(intentText, sizeof(intentText), "WEAK %d", e->intentValue);
    } else {
        snprintf(intentText, sizeof(intentText), "VULN %d", e->intentValue);
    }
    u32 intentBg = 0xFF301018;
    u32 intentBorder = 0xFFFF6060;
    if (e->intent == 1) { intentBg = 0xFF103020; intentBorder = 0xFF80FF90; }
    if (e->intent == 2) { intentBg = 0xFF241030; intentBorder = 0xFFC080FF; }
    fill_rect(fb, stride, x + w - 78, y + h - 29, 72, 22, intentBg);
    draw_thick_rect(fb, stride, x + w - 78, y + h - 29, 72, 22, 1, intentBorder);
    draw_centered_text(fb, stride, x + w - 42, y + h - 23, intentText, 1, 0xFFFFFFFF);

    fill_rect(fb, stride, x - 8, y + h + 12, barW, 22, 0xDD301010);
    int hpw = e->hp * barW / e->maxHp;
    fill_rect(fb, stride, x - 8, y + h + 12, hpw, 22, 0xFFFF3030);
    draw_thick_rect(fb, stride, x - 8, y + h + 12, barW, 22, 2, 0xFFFFFFFF);

    char hp[32];
    snprintf(hp, sizeof(hp), "%d/%d", e->hp, e->maxHp);
    draw_centered_text(fb, stride, x - 8 + barW / 2, y + h + 17, hp, compact ? 1 : 2, 0xFFFFFFFF);

    int statusY = y + h + 42;
    draw_status_line(fb, stride, x - 6, statusY, "VULN", e->vulnerable, 0xFFFFC040);
    draw_status_line(fb, stride, x + 84, statusY, "WEAK", e->weak, 0xFFC0EFFF);
}

static void draw_card_list_panel(u32* fb, u32 stride, int x, int y, int w, int h, const char* title, Card* cards[PLAYER_DECK_MAX], int count) {
    fill_rect(fb, stride, x, y, w, h, 0xAA101018);
    draw_thick_rect(fb, stride, x, y, w, h, 2, 0xFFFFD060);

    char header[48];
    snprintf(header, sizeof(header), "%s %d", title, count);
    draw_centered_text(fb, stride, x + w / 2, y + 10, header, 1, 0xFFFFE8C0);

    int maxRows = (h - 38) / 18;
    if (maxRows > count) maxRows = count;

    for (int i = 0; i < maxRows; i++) {
        Card* card = cards[count - 1 - i];
        if (!card) continue;
        draw_text(fb, stride, x + 8, y + 34 + i * 18, card->name, 1, card->upgraded ? 0xFFC0EFFF : 0xFFFFFFFF);
    }

    if (count > maxRows)
        draw_text(fb, stride, x + 8, y + h - 18, "...", 1, 0xFFFFE8C0);
}


static void draw_player_box(u32* fb, u32 stride, Crawler* crawler, int x, int y, int player_hp, int max_hp, int player_block, int player_vulnerable, int player_weak, int player_strength) {
    int w = 190;
    int h = 150;
    int spriteBoxW = 150;
    int spriteBoxH = 96;
    int barW = 210;

    fill_rect(fb, stride, x, y, w, h, 0xCC202840);
    draw_thick_rect(fb, stride, x, y, w, h, 3, 0xFFC0EFFF);

    const char* name = crawler ? crawler->name : "CRAWLER";
    draw_centered_text(fb, stride, x + w / 2, y + 8, name, 2, 0xFFFFE8C0);

    int spriteX = x + (w - spriteBoxW) / 2;
    int spriteY = y + 38;
    if (crawler && crawler->img)
        draw_image_fit(fb, stride, crawler->img, crawler->w, crawler->h, spriteX, spriteY, spriteBoxW, spriteBoxH);
    else
        fill_circle(fb, stride, x + w / 2, spriteY + spriteBoxH / 2, 42, 0xFF204070);

    fill_rect(fb, stride, x - 10, y + h + 12, barW, 24, 0xDD301010);
    int hpw = 0;
    if (max_hp > 0) hpw = player_hp * barW / max_hp;
    if (hpw < 0) hpw = 0;
    if (hpw > barW) hpw = barW;
    fill_rect(fb, stride, x - 10, y + h + 12, hpw, 24, 0xFFFF3030);
    draw_thick_rect(fb, stride, x - 10, y + h + 12, barW, 24, 2, 0xFFFFFFFF);

    char hpText[32];
    snprintf(hpText, sizeof(hpText), "%d/%d", player_hp, max_hp);
    draw_centered_text(fb, stride, x - 10 + barW / 2, y + h + 18, hpText, 2, 0xFFFFFFFF);

    int statusY = y + h + 46;
    int leftX = x - 6;
    int rightX = x + 92;
    draw_status_line(fb, stride, leftX, statusY, "BLOCK", player_block, 0xFFC0EFFF);
    draw_status_line(fb, stride, rightX, statusY, "STR", player_strength, 0xFFFFD060);
    statusY += 24;
    draw_status_line(fb, stride, leftX, statusY, "VULN", player_vulnerable, 0xFFFFC040);
    draw_status_line(fb, stride, rightX, statusY, "WEAK", player_weak, 0xFFC0EFFF);
}

static void draw_combat_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, Card* hand[HAND_SIZE], int handCount, int selectedCard, Enemy enemies[MAX_ENEMIES], int enemyCount, int selectedEnemy, int player_hp, int max_hp, int player_block, int player_vulnerable, int player_weak, int player_strength, int energy, int act, int coins, Card* drawPile[PLAYER_DECK_MAX], int drawPileCount, Card* discardPile[PLAYER_DECK_MAX], int discardCount, int exhaustCount, Crawler* crawler) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    else fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);

    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.

    char title[32];
    snprintf(title, sizeof(title), "ACT %d", act);
    draw_text(fb, stride, 20, 20, title, 2, 0xFFFFE8C0);
    draw_text(fb, stride, 1050, 20, "X END TURN", 2, 0xFFFFE8C0);
    char coinText[32];
    snprintf(coinText, sizeof(coinText), "COINS %d", coins);
    draw_text(fb, stride, 20, 50, coinText, 2, 0xFFFFD060);

    draw_player_box(fb, stride, crawler, 190, 100, player_hp, max_hp, player_block, player_vulnerable, player_weak, player_strength);

    for (int i = 0; i < enemyCount; i++) {
        int compact = enemyCount >= 3;
        int spacing = compact ? 170 : 220;
        int startX = 735 - ((enemyCount - 1) * spacing) / 2;
        draw_enemy(fb, stride, &enemies[i], startX + i * spacing, compact ? 95 : 100, i == selectedEnemy, compact, player_vulnerable);
    }

    fill_rect(fb, stride, 30, 270, 70, 70, 0xFF0A6A5A);
    draw_thick_rect(fb, stride, 30, 270, 70, 70, 3, 0xFFC0FFF0);

    char en[8];
    snprintf(en, sizeof(en), "%d/3", energy);
    draw_text(fb, stride, 45, 292, en, 3, 0xFFFFFFFF);

    draw_text(fb, stride, 20, 675, "L/R TARGET   LEFT RIGHT CARD   A PLAY   X END", 2, 0xFFFFE8C0);

    draw_card_list_panel(fb, stride, 12, 370, 175, 235, "DRAW", drawPile, drawPileCount);
    draw_card_list_panel(fb, stride, 1092, 370, 175, 235, "DISCARD", discardPile, discardCount);

    fill_rect(fb, stride, 1092, 615, 175, 42, 0xFF101018);
    draw_thick_rect(fb, stride, 1092, 615, 175, 42, 2, 0xFFFF6060);
    char exhaustText[48];
    snprintf(exhaustText, sizeof(exhaustText), "EXHAUSTED: %d", exhaustCount);
    draw_centered_text(fb, stride, 1092 + 87, 629, exhaustText, 1, 0xFFFF8080);

    for (int i = 0; i < handCount; i++) {
        int x = 300 + i * 175;
        int y = (i == selectedCard) ? 355 : 385;
        draw_card(fb, stride, hand[i], x, y, i == selectedCard);
    }
}

static void draw_reward_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, Card* rewards[REWARD_SIZE], int selectedReward, int coins, int lastGoldGain) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);

    draw_centered_text(fb, stride, SCREEN_W / 2, 30, "VICTORY!", 6, 0xFFFFC040);
    draw_centered_text(fb, stride, SCREEN_W / 2, 105, "CHOOSE A CARD OR SKIP", 3, 0xFFFFE8C0);

    char coinText[64];
    snprintf(coinText, sizeof(coinText), "COINS %d   GAINED %d", coins, lastGoldGain);
    draw_centered_text(fb, stride, SCREEN_W / 2, 150, coinText, 2, 0xFFFFD060);

    for (int i = 0; i < REWARD_SIZE; i++) {
        int selected = i == selectedReward;
        int x = 255 + i * 250;
        int y = selected ? 180 : 215;
        draw_card(fb, stride, rewards[i], x, y, selected);
        if (!selected)
            draw_thick_rect(fb, stride, x + 4, y + 4, 162, 242, 1, 0xFF404040);
    }

    int skipX = SCREEN_W - 245;
    int skipY = SCREEN_H - 82;
    int skipW = 205;
    int skipH = 44;
    fill_rect(fb, stride, skipX, skipY, skipW, skipH, selectedReward == REWARD_SIZE ? 0xFFC0EFFF : 0xAA203040);
    draw_thick_rect(fb, stride, skipX, skipY, skipW, skipH, 3, selectedReward == REWARD_SIZE ? 0xFFFFFFFF : 0xFFFFD060);
    draw_centered_text(fb, stride, skipX + skipW / 2, skipY + 13, "SKIP", 2, selectedReward == REWARD_SIZE ? 0xFF203040 : 0xFFFFE8C0);

    fill_rect(fb, stride, 360, 622, 560, 48, 0xFFC0EFFF);
    draw_thick_rect(fb, stride, 360, 622, 560, 48, 3, 0xFFFFFFFF);
    draw_centered_text(fb, stride, SCREEN_W / 2, 638, "A CONFIRM   B SKIP", 2, 0xFF406070);
}

static void draw_campfire_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, int selected) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.

    draw_text(fb, stride, 500, 80, "CAMPFIRE", 6, 0xFFFFC040);
    draw_text(fb, stride, 250, 180, "REST HEALS 30 PERCENT OF MAX HP.", 2, 0xFFFFE8C0);

    const char* opts[3] = {"REST", "UPGRADE CARD", "LEAVE"};

    for (int i = 0; i < 3; i++) {
        int x = 380;
        int y = 285 + i * 78;
        fill_rect(fb, stride, x, y, 520, 54, i == selected ? 0xFFC0EFFF : 0xFF90BCCC);
        draw_thick_rect(fb, stride, x, y, 520, 54, 3, 0xFFFFFFFF);
        draw_centered_text(fb, stride, x + 260, y + 19, opts[i], 2, 0xFFFFFFFF);
    }
}

static int count_upgradeable_cards(Card* playerDeck[PLAYER_DECK_MAX], int playerDeckCount) {
    int count = 0;
    for (int i = 0; i < playerDeckCount; i++) {
        if (playerDeck[i] && !playerDeck[i]->upgraded)
            count++;
    }
    return count;
}

static int get_upgradeable_card_index(Card* playerDeck[PLAYER_DECK_MAX], int playerDeckCount, int upgradeSlot) {
    int count = 0;
    for (int i = 0; i < playerDeckCount; i++) {
        if (playerDeck[i] && !playerDeck[i]->upgraded) {
            if (count == upgradeSlot)
                return i;
            count++;
        }
    }
    return -1;
}

static void draw_upgrade_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, Card* playerDeck[PLAYER_DECK_MAX], int playerDeckCount, int selected) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.

    draw_centered_text(fb, stride, SCREEN_W / 2, 35, "UPGRADE CARD", 5, 0xFFFFC040);
    draw_centered_text(fb, stride, SCREEN_W / 2, 96, "LEFT RIGHT SELECT OWNED CARD   A UPGRADE ONE COPY   B BACK", 2, 0xFFFFE8C0);

    int upgradeableCount = count_upgradeable_cards(playerDeck, playerDeckCount);
    if (upgradeableCount <= 0) {
        draw_centered_text(fb, stride, SCREEN_W / 2, 330, "NO UNUPGRADED CARDS", 4, 0xFFFFE8C0);
        return;
    }

    if (selected < 0) selected = 0;
    if (selected >= upgradeableCount) selected = upgradeableCount - 1;

    int pageStart = (selected / 5) * 5;
    int pageEnd = pageStart + 5;
    if (pageEnd > upgradeableCount) pageEnd = upgradeableCount;

    char pageText[64];
    snprintf(pageText, sizeof(pageText), "CARD %d/%d", selected + 1, upgradeableCount);
    draw_centered_text(fb, stride, SCREEN_W / 2, 130, pageText, 2, 0xFFFFD060);

    for (int i = pageStart; i < pageEnd; i++) {
        int actualIndex = get_upgradeable_card_index(playerDeck, playerDeckCount, i);
        if (actualIndex < 0) continue;

        int slot = i - pageStart;
        int x = 210 + slot * 175;
        int y = 220;
        draw_card(fb, stride, playerDeck[actualIndex], x, y, i == selected);
    }

    draw_centered_text(fb, stride, SCREEN_W / 2, 585, "UPGRADED COPIES ARE REMOVED FROM THIS LIST", 3, 0xFFC0EFFF);
}


static void draw_shop_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, Card* shopCards[SHOP_SIZE], int shopCosts[SHOP_SIZE], int shopPurchased[SHOP_SIZE], int selectedShop, int coins) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    else fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);

    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.

    draw_text(fb, stride, 500, 25, "SHOP", 6, 0xFFFFC040);

    char coinText[64];
    snprintf(coinText, sizeof(coinText), "COINS %d", coins);
    draw_text(fb, stride, 40, 35, coinText, 3, 0xFFFFD060);
    draw_text(fb, stride, 305, 92, "A BUY   B LEAVE   SELECT BACK TO MAP TO EXIT", 2, 0xFFFFE8C0);

    for (int i = 0; i < SHOP_SIZE; i++) {
        int col = i % 4;
        int row = i / 4;
        int x = 120 + col * 260;
        int y = 130 + row * 250;
        draw_card(fb, stride, shopCards[i], x, y, selectedShop == i);

        char price[32];
        if (shopPurchased[i]) snprintf(price, sizeof(price), "SOLD");
        else snprintf(price, sizeof(price), "%d COINS", shopCosts[i]);

        fill_rect(fb, stride, x + 16, y + 218, 136, 24, shopPurchased[i] ? 0xFF505050 : 0xFF0A6A5A);
        draw_text(fb, stride, x + 28, y + 225, price, 1, 0xFFFFFFFF);
    }

    int bx = 470;
    int by = 635;
    fill_rect(fb, stride, bx, by, 340, 45, selectedShop == SHOP_SIZE ? 0xFFC0EFFF : 0xAA203040);
    draw_thick_rect(fb, stride, bx, by, 340, 45, 3, selectedShop == SHOP_SIZE ? 0xFFFFFFFF : 0xFFFFD060);
    draw_text(fb, stride, bx + 85, by + 14, "BACK TO MAP", 2, selectedShop == SHOP_SIZE ? 0xFF203040 : 0xFFFFE8C0);
}


static void draw_title_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, int selected, int bestFloors, int bestCoins, int bestElites, int bestBosses) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    else fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);

    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.

    draw_centered_text(fb, stride, SCREEN_W / 2, 130, "SLAY THE SWITCH", 7, 0xFFFFD060);
    draw_centered_text(fb, stride, SCREEN_W / 2, 188, "INSPIRED BY SLAY THE SPIRE", 2, 0xFFDDDDDD);

    fill_rect(fb, stride, 36, 210, 360, 170, 0x99203040);
    draw_thick_rect(fb, stride, 36, 210, 360, 170, 2, 0xFFFFD060);
    draw_text(fb, stride, 60, 232, "ENDLESS BEST", 2, 0xFFFFD060);

    char statLine[96];
    snprintf(statLine, sizeof(statLine), "FLOORS %d", bestFloors);
    draw_text(fb, stride, 60, 268, statLine, 2, 0xFFFFE8C0);
    snprintf(statLine, sizeof(statLine), "COINS %d", bestCoins);
    draw_text(fb, stride, 60, 300, statLine, 2, 0xFFFFE8C0);
    snprintf(statLine, sizeof(statLine), "ELITES %d", bestElites);
    draw_text(fb, stride, 60, 332, statLine, 2, 0xFFFFE8C0);
    snprintf(statLine, sizeof(statLine), "BOSSES %d", bestBosses);
    draw_text(fb, stride, 60, 364, statLine, 2, 0xFFFFE8C0);

    const char* opts[5] = {"PLAY", "ENDLESS", "CRAWLER", "HOW TO PLAY", "EXIT"};
    for (int i = 0; i < 5; i++) {
        int x = 500;
        int y = 272 + i * 58;
        int w = 280;
        int h = 46;
        fill_rect(fb, stride, x, y, w, h, i == selected ? 0xFFC0EFFF : 0xAA203040);
        draw_thick_rect(fb, stride, x, y, w, h, 3, i == selected ? 0xFFFFFFFF : 0xFFFFD060);
        draw_centered_text(fb, stride, x + w / 2, y + 15, opts[i], 2, i == selected ? 0xFF203040 : 0xFFFFE8C0);
    }

    draw_centered_text(fb, stride, SCREEN_W / 2, 620, "UP DOWN SELECT   A CONFIRM   TOUCH SUPPORTED", 2, 0xFFFFE8C0);
}

static void draw_crawler_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, int selectedCrawler) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    else fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);

    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.

    draw_centered_text(fb, stride, SCREEN_W / 2, 65, "CHOOSE CRAWLER", 5, 0xFFFFD060);
    draw_centered_text(fb, stride, SCREEN_W / 2, 122, "LEFT RIGHT SELECT   A CONFIRM   B BACK", 2, 0xFFFFE8C0);

    for (int i = 0; i < CRAWLER_COUNT; i++) {
        int w = 340;
        int h = 410;
        int x = 250 + i * 440;
        int y = 190;

        fill_rect(fb, stride, x, y, w, h, i == selectedCrawler ? 0xDD203040 : 0xAA101018);
        draw_thick_rect(fb, stride, x, y, w, h, 4, i == selectedCrawler ? 0xFFFFFF40 : 0xFFFFD060);

        draw_centered_text(fb, stride, x + w / 2, y + 30, crawlers[i].name, 4, 0xFFFFE8C0);

        if (crawlers[i].img)
            draw_image_fit(fb, stride, crawlers[i].img, crawlers[i].w, crawlers[i].h, x + 90, y + 90, 160, 190);
        else
            fill_circle(fb, stride, x + w / 2, y + 185, 50, 0xFF505070);

        char stat[64];
        snprintf(stat, sizeof(stat), "MAX HP %d", crawlers[i].maxHp);
        draw_centered_text(fb, stride, x + w / 2, y + 310, stat, 2, 0xFFFFFFFF);

        if (crawlers[i].startingStrength > 0) {
            snprintf(stat, sizeof(stat), "START STRENGTH %d", crawlers[i].startingStrength);
            draw_centered_text(fb, stride, x + w / 2, y + 345, stat, 2, 0xFFFFC040);
        } else if (crawlers[i].startingBlock > 0) {
            snprintf(stat, sizeof(stat), "START BLOCK %d", crawlers[i].startingBlock);
            draw_centered_text(fb, stride, x + w / 2, y + 345, stat, 2, 0xFFC0EFFF);
        }
    }
}

static void draw_how_to_play_popup(u32* fb, u32 stride) {
    int boxW = 1040;
    int boxH = 520;
    int boxX = (SCREEN_W - boxW) / 2;
    int boxY = (SCREEN_H - boxH) / 2;

    // Popup only. The title screen/menu remains behind this overlay.
    fill_rect(fb, stride, boxX, boxY, boxW, boxH, 0xFF181820);
    draw_thick_rect(fb, stride, boxX, boxY, boxW, boxH, 4, 0xFFFFFFFF);
    draw_thick_rect(fb, stride, boxX + 8, boxY + 8, boxW - 16, boxH - 16, 2, 0xFFFFD060);

    draw_centered_text(fb, stride, SCREEN_W / 2, boxY + 28, "HOW TO PLAY", 5, 0xFFFFC040);

    int leftX = boxX + 72;
    int textX = leftX + 170;
    int y = boxY + 110;
    int rowGap = 42;

    draw_text(fb, stride, leftX, y, "A", 3, 0xFFFFD060);
    draw_text(fb, stride, textX, y + 4, "CONFIRM / PLAY CARD / TAKE REWARD", 2, 0xFFFFFFFF);

    y += rowGap;
    draw_text(fb, stride, leftX, y, "B", 3, 0xFFFFD060);
    draw_text(fb, stride, textX, y + 4, "BACK / SKIP REWARD", 2, 0xFFFFFFFF);

    y += rowGap;
    draw_text(fb, stride, leftX, y, "X", 3, 0xFFFFD060);
    draw_text(fb, stride, textX, y + 4, "END TURN", 2, 0xFFFFFFFF);

    y += rowGap + 10;
    draw_text(fb, stride, leftX, y, "D-PAD", 2, 0xFFFFD060);
    draw_text(fb, stride, textX, y, "MOVE SELECTION", 2, 0xFFFFFFFF);

    y += rowGap;
    draw_text(fb, stride, leftX, y, "UP/DOWN", 2, 0xFFFFD060);
    draw_text(fb, stride, textX, y, "CHANGE ENEMY TARGET", 2, 0xFFFFFFFF);

    y += rowGap;
    draw_text(fb, stride, leftX, y, "L/R", 2, 0xFFFFD060);
    draw_text(fb, stride, textX, y, "CYCLE ENEMY TARGET", 2, 0xFFFFFFFF);

    y += rowGap;
    draw_text(fb, stride, leftX, y, "ZL/ZR", 2, 0xFFFFD060);
    draw_text(fb, stride, textX, y, "CYCLE ENEMY TARGET", 2, 0xFFFFFFFF);

    draw_centered_text(fb, stride, SCREEN_W / 2, boxY + boxH - 92, "TOUCH SCREEN - TAP CARDS, ENEMIES, BUTTONS, OR MENU OPTIONS", 2, 0xFFC0EFFF);
    draw_centered_text(fb, stride, SCREEN_W / 2, boxY + boxH - 44, "B BACK", 3, 0xFFFFFFFF);
}

static void draw_win_screen(u32* fb, u32 stride) {
    fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);
    draw_text(fb, stride, 360, 260, "RUN COMPLETE", 7, 0xFFFFD060);
    draw_text(fb, stride, 390, 370, "PLUS TO EXIT", 4, 0xFFFFE8C0);
}

static void draw_death_screen(u32* fb, u32 stride, unsigned char* bg, int bgw, int bgh, int selected, int runFloors, int coins, int runElites, int runBosses) {
    if (bg) draw_image_cover(fb, stride, bg, bgw, bgh);
    else fill_rect(fb, stride, 0, 0, SCREEN_W, SCREEN_H, 0xFF101018);

    // Background is already drawn; do not cover it with a fake-alpha full-screen fill.
    draw_centered_text(fb, stride, SCREEN_W / 2, 180, "YOU DIED", 8, 0xFFFF4040);
    draw_centered_text(fb, stride, SCREEN_W / 2, 265, "THE RUN HAS FAILED", 3, 0xFFFFE8C0);

    char statLine[96];
    snprintf(statLine, sizeof(statLine), "THIS RUN FLOORS %d   COINS %d", runFloors, coins);
    draw_centered_text(fb, stride, SCREEN_W / 2, 310, statLine, 2, 0xFFFFD060);
    snprintf(statLine, sizeof(statLine), "ELITES %d   BOSSES %d", runElites, runBosses);
    draw_centered_text(fb, stride, SCREEN_W / 2, 340, statLine, 2, 0xFFFFE8C0);

    const char* opts[2] = {"RESTART", "MAIN MENU"};
    for (int i = 0; i < 2; i++) {
        int w = 330;
        int h = 62;
        int x = SCREEN_W / 2 - w / 2;
        int y = 405 + i * 86;
        fill_rect(fb, stride, x, y, w, h, i == selected ? 0xFFC0EFFF : 0xFF203040);
        draw_thick_rect(fb, stride, x, y, w, h, 3, 0xFFFFFFFF);
        draw_centered_text(fb, stride, SCREEN_W / 2, y + 22, opts[i], 2, i == selected ? 0xFF203040 : 0xFFFFE8C0);
    }

    draw_centered_text(fb, stride, SCREEN_W / 2, 620, "UP DOWN SELECT   A CONFIRM", 2, 0xFFFFE8C0);
}

static void draw_new_hand(Card* hand[HAND_SIZE], int* handCount, Card* playerDeck[PLAYER_DECK_MAX], int playerDeckCount) {
    for (int i = 0; i < HAND_SIZE; i++)
        hand[i] = NULL;

    if (playerDeckCount <= 0) {
        *handCount = HAND_SIZE;
        for (int i = 0; i < HAND_SIZE; i++)
            hand[i] = &deck[rand() % BASE_CARD_COUNT];
        return;
    }

    int drawCount = playerDeckCount < HAND_SIZE ? playerDeckCount : HAND_SIZE;
    int order[PLAYER_DECK_MAX];

    for (int i = 0; i < playerDeckCount; i++)
        order[i] = i;

    // Shuffle the player's deck order, then draw the first 5. This keeps duplicate cards
    // possible only when the deck actually contains multiple copies, instead of allowing
    // one copy of Energy Orb to fill an entire hand.
    for (int i = playerDeckCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }

    *handCount = drawCount;
    for (int i = 0; i < drawCount; i++)
        hand[i] = playerDeck[order[i]];
}

static void remove_card_from_hand(Card* hand[HAND_SIZE], int* handCount, int* selectedCard) {
    if (*handCount <= 0) return;

    for (int i = *selectedCard; i < *handCount - 1; i++)
        hand[i] = hand[i + 1];

    hand[*handCount - 1] = NULL;
    (*handCount)--;

    if (*selectedCard >= *handCount)
        *selectedCard = *handCount - 1;

    if (*selectedCard < 0)
        *selectedCard = 0;
}

static Card* random_card_from_pool(NodeType sourceType, int forceRare) {
    int candidates[CARD_COUNT];
    int count = 0;

    for (int i = 0; i < BASE_CARD_COUNT; i++) {
        if (forceRare) {
            if (deck[i].rare) candidates[count++] = i;
        } else if (sourceType == NODE_ELITE) {
            // Elites can drop rare cards sometimes, but usually still give normal cards.
            if (!deck[i].rare || rand() % 100 < 35) candidates[count++] = i;
        } else {
            if (!deck[i].rare) candidates[count++] = i;
        }
    }

    if (count <= 0)
        return &deck[rand() % BASE_CARD_COUNT];

    return &deck[candidates[rand() % count]];
}

static void draw_rewards(Card* rewards[REWARD_SIZE], NodeType sourceType) {
    int forceRare = sourceType == NODE_BOSS;

    for (int i = 0; i < REWARD_SIZE; i++)
        rewards[i] = random_card_from_pool(sourceType, forceRare);
}

static void init_player_deck(Card* playerDeck[PLAYER_DECK_MAX], int* playerDeckCount) {
    *playerDeckCount = 0;

    // Starter deck:
    // 4x Slash, 4x Shield, 1x Energy Orb.
    for (int i = 0; i < 4 && *playerDeckCount < PLAYER_DECK_MAX; i++)
        playerDeck[(*playerDeckCount)++] = &deck[0];

    for (int i = 0; i < 4 && *playerDeckCount < PLAYER_DECK_MAX; i++)
        playerDeck[(*playerDeckCount)++] = &deck[1];

    if (*playerDeckCount < PLAYER_DECK_MAX)
        playerDeck[(*playerDeckCount)++] = &deck[2];
}

static void add_card_to_player_deck(Card* playerDeck[PLAYER_DECK_MAX], int* playerDeckCount, Card* card) {
    if (*playerDeckCount >= PLAYER_DECK_MAX || card == NULL) return;
    playerDeck[*playerDeckCount] = card;
    (*playerDeckCount)++;
}

static int card_index(Card* card) {
    for (int i = 0; i < CARD_COUNT; i++)
        if (card == &deck[i])
            return i;
    return -1;
}

static Card* upgraded_version(Card* card) {
    int idx = card_index(card);
    if (idx < 0 || idx >= BASE_CARD_COUNT)
        return card;
    return &deck[idx + BASE_CARD_COUNT];
}

static int upgrade_owned_card(Card* playerDeck[PLAYER_DECK_MAX], int playerDeckCount, int selected) {
    if (selected < 0 || selected >= playerDeckCount) return 0;
    Card* current = playerDeck[selected];
    if (!current || current->upgraded) return 0;
    playerDeck[selected] = upgraded_version(current);
    return 1;
}

static void shuffle_cards(Card* cards[PLAYER_DECK_MAX], int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card* t = cards[i];
        cards[i] = cards[j];
        cards[j] = t;
    }
}

static void start_combat_draw_pile(Card* drawPile[PLAYER_DECK_MAX], int* drawPileCount, Card* discardPile[PLAYER_DECK_MAX], int* discardCount, Card* playerDeck[PLAYER_DECK_MAX], int playerDeckCount) {
    *drawPileCount = 0;
    *discardCount = 0;

    for (int i = 0; i < playerDeckCount && i < PLAYER_DECK_MAX; i++)
        drawPile[(*drawPileCount)++] = playerDeck[i];

    shuffle_cards(drawPile, *drawPileCount);
    for (int i = 0; i < PLAYER_DECK_MAX; i++)
        discardPile[i] = NULL;
}

static void add_to_discard(Card* discardPile[PLAYER_DECK_MAX], int* discardCount, Card* card) {
    if (!card || *discardCount >= PLAYER_DECK_MAX) return;
    discardPile[(*discardCount)++] = card;
}

static Card* draw_one_card(Card* drawPile[PLAYER_DECK_MAX], int* drawPileCount, Card* discardPile[PLAYER_DECK_MAX], int* discardCount) {
    if (*drawPileCount <= 0 && *discardCount > 0) {
        for (int i = 0; i < *discardCount; i++)
            drawPile[i] = discardPile[i];
        *drawPileCount = *discardCount;
        *discardCount = 0;
        shuffle_cards(drawPile, *drawPileCount);
    }

    if (*drawPileCount <= 0)
        return NULL;

    (*drawPileCount)--;
    Card* card = drawPile[*drawPileCount];
    drawPile[*drawPileCount] = NULL;
    return card;
}

static void draw_hand_from_piles(Card* hand[HAND_SIZE], int* handCount, Card* drawPile[PLAYER_DECK_MAX], int* drawPileCount, Card* discardPile[PLAYER_DECK_MAX], int* discardCount) {
    *handCount = 0;
    for (int i = 0; i < HAND_SIZE; i++) {
        Card* card = draw_one_card(drawPile, drawPileCount, discardPile, discardCount);
        hand[i] = card;
        if (card) (*handCount)++;
    }
}

static void discard_hand(Card* hand[HAND_SIZE], int* handCount, Card* discardPile[PLAYER_DECK_MAX], int* discardCount) {
    for (int i = 0; i < *handCount; i++) {
        add_to_discard(discardPile, discardCount, hand[i]);
        hand[i] = NULL;
    }
    *handCount = 0;
}

static void setup_shop(Card* shopCards[SHOP_SIZE], int shopCosts[SHOP_SIZE], int shopPurchased[SHOP_SIZE], int act) {
    for (int i = 0; i < SHOP_SIZE; i++) {
        int wantRare = rand() % 100 < 22;
        shopCards[i] = random_card_from_pool(NODE_SHOP, wantRare);

        if (shopCards[i]->rare)
            shopCosts[i] = rand_range(62, 86) + (act - 1) * 8;
        else
            shopCosts[i] = rand_range(28, 52) + (act - 1) * 5;

        shopPurchased[i] = 0;
    }
}

static int coin_reward_for_node(NodeType type) {
    if (type == NODE_ELITE) return rand_range(23, 34);
    if (type == NODE_BOSS) return rand_range(40, 55);
    return rand_range(10, 20);
}

static int alive_enemy_count(Enemy enemies[MAX_ENEMIES], int enemyCount) {
    int c = 0;
    for (int i = 0; i < enemyCount; i++)
        if (enemies[i].alive)
            c++;
    return c;
}

static int first_alive_enemy(Enemy enemies[MAX_ENEMIES], int enemyCount) {
    for (int i = 0; i < enemyCount; i++)
        if (enemies[i].alive)
            return i;
    return 0;
}

static int next_alive_enemy_from(Enemy enemies[MAX_ENEMIES], int enemyCount, int current, int dir) {
    if (enemyCount <= 0) return 0;
    for (int step = 1; step <= enemyCount; step++) {
        int idx = current + dir * step;
        while (idx < 0) idx += enemyCount;
        while (idx >= enemyCount) idx -= enemyCount;
        if (enemies[idx].alive) return idx;
    }
    return first_alive_enemy(enemies, enemyCount);
}

static int point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void setup_enemies(Enemy enemies[MAX_ENEMIES], int* enemyCount, NodeType type, int act) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].alive = 0;
        enemies[i].hp = 0;
        enemies[i].maxHp = 0;
        enemies[i].block = 0;
        enemies[i].intent = 0;
        enemies[i].lastIntent = -1;
        enemies[i].intentValue = 0;
        enemies[i].intentDebuff = 0;
        enemies[i].aiType = 0;
        enemies[i].vulnerable = 0;
        enemies[i].weak = 0;
        enemies[i].img = NULL;
        enemies[i].w = 0;
        enemies[i].h = 0;
        enemies[i].name = "Enemy";
    }

    if (type == NODE_BOSS) {
        *enemyCount = 1;
        int hp = 150;
        for (int i = 1; i < act; i++)
            hp = hp + hp / 8;

        enemies[0].name = "Boss";
        enemies[0].aiType = 0;
        enemies[0].hp = hp;
        enemies[0].maxHp = hp;
        enemies[0].alive = 1;
        set_enemy_next_intent(&enemies[0], NODE_BOSS, 1);
        assign_enemy_sprite(&enemies[0], NODE_BOSS);
        return;
    }

    if (type == NODE_ELITE) {
        *enemyCount = 1;
        int hp = 40 + (rand() % 21) + (act - 1) * 8;
        enemies[0].aiType = rand() % 4;
        const char* eliteNames[4] = {"Elite", "Brute", "Guard", "Hexer"};
        enemies[0].name = eliteNames[enemies[0].aiType];
        enemies[0].hp = hp;
        enemies[0].maxHp = hp;
        enemies[0].alive = 1;
        set_enemy_next_intent(&enemies[0], NODE_ELITE, 1);
        assign_enemy_sprite(&enemies[0], NODE_ELITE);
        return;
    }

    *enemyCount = 1 + rand() % 3;

    for (int i = 0; i < *enemyCount; i++) {
        int hp = 18 + rand() % 16 + (act - 1) * 5;
        enemies[i].aiType = rand() % 4;
        const char* normalNames[4] = {"Enemy", "Bruiser", "Guard", "Hexer"};
        enemies[i].name = normalNames[enemies[i].aiType];
        enemies[i].hp = hp;
        enemies[i].maxHp = hp;
        enemies[i].alive = 1;
        set_enemy_next_intent(&enemies[i], NODE_COMBAT, 1);
        assign_enemy_sprite(&enemies[i], NODE_COMBAT);
    }
}

static int apply_damage_status_modifiers(int baseDamage, int attackerWeak, int defenderVulnerable) {
    // Custom SlayTheSwitch stacking:
    // Each Vulnerable stack adds +50% of the ORIGINAL/base damage.
    // Each Weak stack removes -25% of the ORIGINAL/base damage.
    // The stacks do not recursively boost/reduce the already modified value.
    // Example: 7 damage + 2 Vulnerable = 7 + 3 + 3 = 13.
    // Example: 7 damage + 1 Weak = 7 - 2 = 5.
    int dmg = baseDamage;

    if (defenderVulnerable > 0)
        dmg += (baseDamage * defenderVulnerable) / 2;

    if (attackerWeak > 0)
        dmg -= (baseDamage * attackerWeak + 3) / 4;

    if (dmg < 1) dmg = 1;
    return dmg;
}

static void tick_enemy_statuses(Enemy enemies[MAX_ENEMIES], int enemyCount) {
    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].alive) continue;
        if (enemies[i].vulnerable > 0) enemies[i].vulnerable--;
        if (enemies[i].weak > 0) enemies[i].weak--;
    }
}

static void enemy_turn(Enemy enemies[MAX_ENEMIES], int enemyCount, int* player_hp, int* player_block, int* player_vulnerable, int* player_weak, NodeType nodeType) {
    int avoidRepeat = 1;

    for (int i = 0; i < enemyCount; i++) {
        if (!enemies[i].alive) continue;

        if (enemies[i].intent == 0) {
            int dmg = apply_damage_status_modifiers(enemies[i].intentValue, enemies[i].weak, *player_vulnerable);

            if (*player_block >= dmg) {
                *player_block -= dmg;
            } else {
                dmg -= *player_block;
                *player_block = 0;
                *player_hp -= dmg;
                if (*player_hp < 0) *player_hp = 0;
            }
        } else if (enemies[i].intent == 1) {
            int heal = enemies[i].intentValue;
            enemies[i].hp += heal;
            if (enemies[i].hp > enemies[i].maxHp)
                enemies[i].hp = enemies[i].maxHp;
        } else {
            if (enemies[i].intentDebuff == 0)
                *player_weak += enemies[i].intentValue;
            else
                *player_vulnerable += enemies[i].intentValue;
        }

        enemies[i].lastIntent = enemies[i].intent;
        set_enemy_next_intent(&enemies[i], nodeType, avoidRepeat);
    }

    tick_enemy_statuses(enemies, enemyCount);

    if (*player_vulnerable > 0) (*player_vulnerable)--;
    if (*player_weak > 0) (*player_weak)--;
}


static int card_exhausts(Card* c) {
    if (!c) return 0;
    return strcmp(c->name, "Power Up") == 0 ||
           strcmp(c->name, "Power Up+") == 0 ||
           strcmp(c->name, "Demon Claw") == 0 ||
           strcmp(c->name, "Demon Claw+") == 0 ||
           strcmp(c->name, "Barrier") == 0 ||
           strcmp(c->name, "Barrier+") == 0;
}

static void update_best_stats(int endlessMode, int runFloors, int coins, int runElites, int runBosses, int* bestFloors, int* bestCoins, int* bestElites, int* bestBosses) {
    if (!endlessMode) return;

    int changed = 0;

    if (runFloors > *bestFloors) { *bestFloors = runFloors; changed = 1; }
    if (coins > *bestCoins) { *bestCoins = coins; changed = 1; }
    if (runElites > *bestElites) { *bestElites = runElites; changed = 1; }
    if (runBosses > *bestBosses) { *bestBosses = runBosses; changed = 1; }

    if (changed)
        save_best_stats(*bestFloors, *bestCoins, *bestElites, *bestBosses);
}

static void record_completed_node(NodeType type, int* runFloors, int* runElites, int* runBosses) {
    (*runFloors)++;
    if (type == NODE_ELITE) (*runElites)++;
    if (type == NODE_BOSS) (*runBosses)++;
}

static void clear_battle_statuses(int* player_block, int* player_next_turn_block, int* player_vulnerable, int* player_weak, int* player_strength) {
    *player_block = 0;
    *player_next_turn_block = 0;
    *player_vulnerable = 0;
    *player_weak = 0;
    *player_strength = 0;
}

static void advance_room(GameScreen* screen, int* act, int* currentNode, int* selectedSlot, int destinationNode, int endlessMode) {
    *currentNode = destinationNode;
    *selectedSlot = 0;

    if (mapNodes[*currentNode].type == NODE_BOSS) {
        (*act)++;
        if (!endlessMode && *act > 3) {
            *screen = SCREEN_WIN;
            return;
        }

        generate_map(*act);
        *currentNode = 0;
        *selectedSlot = 0;
    }

    *screen = SCREEN_MAP;
}

int main(int argc, char **argv) {
    romfsInit();
    srand((unsigned int)svcGetSystemTick());

    load_backgrounds();

    load_enemy_sprite_pool("enemy", NORMAL_ENEMY_SPRITE_COUNT, normalEnemySprites, normalEnemyW, normalEnemyH);
    load_enemy_sprite_pool("elite", ELITE_ENEMY_SPRITE_COUNT, eliteEnemySprites, eliteEnemyW, eliteEnemyH);
    load_enemy_sprite_pool("boss", BOSS_ENEMY_SPRITE_COUNT, bossEnemySprites, bossEnemyW, bossEnemyH);
    load_crawler_sprites();

    for (int i = 0; i < CARD_COUNT; i++) {
        deck[i].img = stbi_load(deck[i].imagePath, &deck[i].w, &deck[i].h, NULL, 4);
        // If the image is missing, draw_card will render a simple placeholder instead of crashing.
        if (!deck[i].img) {
            deck[i].w = 0;
            deck[i].h = 0;
        }
    }

    Card* hand[HAND_SIZE];
    Card* rewards[REWARD_SIZE];
    Card* shopCards[SHOP_SIZE];
    int shopCosts[SHOP_SIZE];
    int shopPurchased[SHOP_SIZE];

    Card* drawPile[PLAYER_DECK_MAX];
    int drawPileCount = 0;
    Card* discardPile[PLAYER_DECK_MAX];
    int discardCount = 0;
    int exhaustCount = 0;

    Card* playerDeck[PLAYER_DECK_MAX];
    int playerDeckCount = 0;
    init_player_deck(playerDeck, &playerDeckCount);

    int handCount = 0;
    draw_new_hand(hand, &handCount, playerDeck, playerDeckCount);
    draw_rewards(rewards, NODE_COMBAT);
    setup_shop(shopCards, shopCosts, shopPurchased, 1);

    Enemy enemies[MAX_ENEMIES];
    int enemyCount = 0;
    int selectedEnemy = 0;

    GameScreen screen = SCREEN_TITLE;

    int act = 1;
    generate_map(act);

    int currentNode = 0;
    int selectedSlot = 0;
    int pendingNode = 0;

    int selectedTitle = 0;
    int showHowToPopup = 0;
    int selectedCrawler = 0;
    int selectedDeath = 0;
    int selectedCard = 0;
    int selectedReward = 0;
    int selectedCampfire = 0;
    int selectedUpgrade = 0;
    int selectedShop = 0;

    int endlessMode = 0;
    int runFloors = 0;
    int runElites = 0;
    int runBosses = 0;
    int bestFloors = 0;
    int bestCoins = 0;
    int bestElites = 0;
    int bestBosses = 0;
    load_best_stats(&bestFloors, &bestCoins, &bestElites, &bestBosses);

    int coins = 0;
    int lastGoldGain = 0;
    Background* currentBattleBg = choose_battle_background_for_act(act);

    int playerMaxHp = 72;
    int player_hp = playerMaxHp;
    int player_block = 0;
    int player_next_turn_block = 0;
    int player_vulnerable = 0;
    int player_weak = 0;
    int player_strength = 0;
    int energy = 3;

    NodeType currentNodeType = NODE_COMBAT;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    int touchWasDown = 0;

    NWindow *win = nwindowGetDefault();
    Framebuffer fb;
    framebufferCreate(&fb, win, SCREEN_W, SCREEN_H, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);

        HidTouchScreenState touchState;
        memset(&touchState, 0, sizeof(touchState));
        int touchCount = hidGetTouchScreenStates(&touchState, 1);
        int touchDownNow = (touchCount > 0 && touchState.count > 0);
        int touchPressed = touchDownNow && !touchWasDown;
        int touchX = touchDownNow ? touchState.touches[0].x : -1;
        int touchY = touchDownNow ? touchState.touches[0].y : -1;
        touchWasDown = touchDownNow;

        if (down & HidNpadButton_Plus) break;

        if (screen == SCREEN_TITLE) {
            if (showHowToPopup) {
                if ((down & HidNpadButton_B) || (down & HidNpadButton_A) || touchPressed) {
                    showHowToPopup = 0;
                }
            } else {
                if ((down & HidNpadButton_Up) && selectedTitle > 0) selectedTitle--;
                if ((down & HidNpadButton_Down) && selectedTitle < 4) selectedTitle++;

                if (touchPressed) {
                    for (int i = 0; i < 5; i++) {
                        int x = 500;
                        int y = 272 + i * 58;
                        int w = 280;
                        int h = 46;
                        if (point_in_rect(touchX, touchY, x, y, w, h)) {
                            selectedTitle = i;
                            down |= HidNpadButton_A;
                        }
                    }
                }

                if (down & HidNpadButton_A) {
                    if (selectedTitle == 0 || selectedTitle == 1) {
                        endlessMode = (selectedTitle == 1);
                        act = 1;
                        generate_map(act);
                        currentBattleBg = choose_battle_background_for_act(act);
                        currentNode = 0;
                        selectedSlot = 0;
                        pendingNode = 0;
                        playerMaxHp = crawlers[selectedCrawler].maxHp;
                        player_hp = playerMaxHp;
                        player_block = 0;
                        player_next_turn_block = 0;
                        player_vulnerable = 0;
                        player_weak = 0;
                        player_strength = 0;
                        energy = 3;
                        coins = 0;
                        lastGoldGain = 0;
                        runFloors = 0;
                        runElites = 0;
                        runBosses = 0;
                        init_player_deck(playerDeck, &playerDeckCount);
                        draw_new_hand(hand, &handCount, playerDeck, playerDeckCount);
                        draw_rewards(rewards, NODE_COMBAT);
                        setup_shop(shopCards, shopCosts, shopPurchased, act);
                        screen = SCREEN_MAP;
                    } else if (selectedTitle == 2) {
                        screen = SCREEN_CRAWLER;
                    } else if (selectedTitle == 3) {
                        showHowToPopup = 1;
                    } else {
                        break;
                    }
                }
            }
        } else if (screen == SCREEN_CRAWLER) {
            if ((down & HidNpadButton_Left) && selectedCrawler > 0) selectedCrawler--;
            if ((down & HidNpadButton_Right) && selectedCrawler < CRAWLER_COUNT - 1) selectedCrawler++;

            if (down & HidNpadButton_A)
                screen = SCREEN_TITLE;
            if (down & HidNpadButton_B)
                screen = SCREEN_TITLE;
        } else if (screen == SCREEN_DEATH) {
            if ((down & HidNpadButton_Up) && selectedDeath > 0) selectedDeath--;
            if ((down & HidNpadButton_Down) && selectedDeath < 1) selectedDeath++;

            if (down & HidNpadButton_A) {
                if (selectedDeath == 0) {
                    act = 1;
                    generate_map(act);
                    currentBattleBg = choose_battle_background_for_act(act);
                    currentNode = 0;
                    selectedSlot = 0;
                    pendingNode = 0;
                    playerMaxHp = crawlers[selectedCrawler].maxHp;
                    player_hp = playerMaxHp;
                    player_block = 0;
                    player_next_turn_block = 0;
                    player_vulnerable = 0;
                    player_weak = 0;
                    player_strength = 0;
                    energy = 3;
                    coins = 0;
                    lastGoldGain = 0;
                    runFloors = 0;
                    runElites = 0;
                    runBosses = 0;
                    init_player_deck(playerDeck, &playerDeckCount);
                    draw_new_hand(hand, &handCount, playerDeck, playerDeckCount);
                    draw_rewards(rewards, NODE_COMBAT);
                    setup_shop(shopCards, shopCosts, shopPurchased, act);
                    screen = SCREEN_MAP;
                } else {
                    selectedTitle = 0;
                    screen = SCREEN_TITLE;
                }
            }
        } else if (screen == SCREEN_MAP) {
            int availableCount = count_available_nodes(currentNode);

            if ((down & HidNpadButton_Left) && availableCount > 0)
                selectedSlot = (selectedSlot + availableCount - 1) % availableCount;
            if ((down & HidNpadButton_Right) && availableCount > 0)
                selectedSlot = (selectedSlot + 1) % availableCount;

            if (selectedSlot >= availableCount) selectedSlot = availableCount - 1;
            if (selectedSlot < 0) selectedSlot = 0;

            if ((down & HidNpadButton_A) && availableCount > 0) {
                pendingNode = get_available_node(currentNode, selectedSlot);
                currentNodeType = mapNodes[pendingNode].type;

                if (currentNodeType == NODE_CAMPFIRE) {
                    screen = SCREEN_CAMPFIRE;
                    selectedCampfire = 0;
                } else if (currentNodeType == NODE_SHOP) {
                    setup_shop(shopCards, shopCosts, shopPurchased, act);
                    selectedShop = 0;
                    screen = SCREEN_SHOP;
                } else {
                    currentBattleBg = choose_battle_background_for_act(act);
                    setup_enemies(enemies, &enemyCount, currentNodeType, act);
                    selectedEnemy = first_alive_enemy(enemies, enemyCount);

                    energy = 3;
                    player_block = player_next_turn_block + crawlers[selectedCrawler].startingBlock;
                    player_next_turn_block = 0;
                    player_strength += crawlers[selectedCrawler].startingStrength;
                    selectedCard = 0;
                    start_combat_draw_pile(drawPile, &drawPileCount, discardPile, &discardCount, playerDeck, playerDeckCount);
                    exhaustCount = 0;
                    draw_hand_from_piles(hand, &handCount, drawPile, &drawPileCount, discardPile, &discardCount);

                    screen = SCREEN_COMBAT;
                }
            }
        } else if (screen == SCREEN_COMBAT) {
            if ((down & HidNpadButton_Left) && selectedCard > 0) selectedCard--;
            if ((down & HidNpadButton_Right) && selectedCard < handCount - 1) selectedCard++;

            if (down & (HidNpadButton_L | HidNpadButton_ZL))
                selectedEnemy = next_alive_enemy_from(enemies, enemyCount, selectedEnemy, -1);
            if (down & (HidNpadButton_R | HidNpadButton_ZR))
                selectedEnemy = next_alive_enemy_from(enemies, enemyCount, selectedEnemy, 1);

            if ((down & HidNpadButton_Up) && selectedEnemy > 0) selectedEnemy--;
            if ((down & HidNpadButton_Down) && selectedEnemy < enemyCount - 1) selectedEnemy++;

            if (!enemies[selectedEnemy].alive)
                selectedEnemy = first_alive_enemy(enemies, enemyCount);

            if (touchPressed) {
                for (int i = 0; i < enemyCount; i++) {
                    int compact = enemyCount >= 3;
                    int spacing = compact ? 170 : 220;
                    int startX = 735 - ((enemyCount - 1) * spacing) / 2;
                    int ex = startX + i * spacing;
                    int ey = compact ? 95 : 100;
                    int ew = compact ? 150 : 180;
                    int eh = compact ? 118 : 145;
                    if (enemies[i].alive && point_in_rect(touchX, touchY, ex - 12, ey - 12, ew + 24, eh + 90))
                        selectedEnemy = i;
                }

                for (int i = 0; i < handCount; i++) {
                    int cx = 300 + i * 175;
                    int cy = (i == selectedCard) ? 355 : 385;
                    if (point_in_rect(touchX, touchY, cx - 10, cy - 10, 190, 270)) {
                        selectedCard = i;
                        down |= HidNpadButton_A;
                    }
                }
            }

            if ((down & HidNpadButton_A) && handCount > 0 && energy >= hand[selectedCard]->cost) {
                Card* c = hand[selectedCard];

                energy -= c->cost;

                int targetEnemy = selectedEnemy;

                if (c->damage > 0 && enemies[targetEnemy].alive) {
                    int dmg = apply_damage_status_modifiers(c->damage, player_weak, enemies[targetEnemy].vulnerable);

                    if (enemies[targetEnemy].block >= dmg) {
                        enemies[targetEnemy].block -= dmg;
                    } else {
                        dmg -= enemies[targetEnemy].block;
                        enemies[targetEnemy].block = 0;
                        enemies[targetEnemy].hp -= dmg;
                    }

                    if (player_strength > 0 && enemies[targetEnemy].alive)
                        enemies[targetEnemy].vulnerable += 1;
                }

                if (c->vulnerable > 0 || c->weak > 0) {
                    if (c->affectsAll) {
                        // Applies to enemies only. The player should never receive their own all-enemy debuffs.
                        for (int ei = 0; ei < enemyCount; ei++) {
                            if (!enemies[ei].alive) continue;
                            enemies[ei].vulnerable += c->vulnerable;
                            enemies[ei].weak += c->weak;
                        }
                    } else if (enemies[targetEnemy].alive) {
                        enemies[targetEnemy].vulnerable += c->vulnerable;
                        enemies[targetEnemy].weak += c->weak;
                    }
                }

                if (enemies[targetEnemy].hp <= 0) {
                    enemies[targetEnemy].hp = 0;
                    enemies[targetEnemy].alive = 0;
                    selectedEnemy = first_alive_enemy(enemies, enemyCount);
                }

                player_block += c->block;
                player_next_turn_block += c->nextTurnBlock;
                if (strcmp(c->name, "Power Up") == 0) player_strength += 3;
                if (strcmp(c->name, "Power Up+") == 0) player_strength += 6;
                player_hp += c->heal;
                energy += c->energyGain;

                if (player_hp > playerMaxHp) player_hp = playerMaxHp;

                if (card_exhausts(c)) {
                    exhaustCount++;
                } else {
                    add_to_discard(discardPile, &discardCount, c);
                }
                remove_card_from_hand(hand, &handCount, &selectedCard);

                if (alive_enemy_count(enemies, enemyCount) == 0) {
                    record_completed_node(currentNodeType, &runFloors, &runElites, &runBosses);
                    lastGoldGain = coin_reward_for_node(currentNodeType);
                    coins += lastGoldGain;
                    update_best_stats(endlessMode, runFloors, coins, runElites, runBosses, &bestFloors, &bestCoins, &bestElites, &bestBosses);
                    clear_battle_statuses(&player_block, &player_next_turn_block, &player_vulnerable, &player_weak, &player_strength);
                    draw_rewards(rewards, currentNodeType);
                    selectedReward = 0;
                    screen = SCREEN_REWARD;
                }
            }

            if (down & HidNpadButton_X) {
                enemy_turn(enemies, enemyCount, &player_hp, &player_block, &player_vulnerable, &player_weak, currentNodeType);

                if (player_hp <= 0) {
                    selectedDeath = 0;
                    screen = SCREEN_DEATH;
                } else {
                    energy = 3;
                    player_block = player_next_turn_block;
                    player_next_turn_block = 0;
                    if (player_strength > 0) player_strength--;
                    selectedCard = 0;
                    discard_hand(hand, &handCount, discardPile, &discardCount);
                    draw_hand_from_piles(hand, &handCount, drawPile, &drawPileCount, discardPile, &discardCount);
                }
            }
        } else if (screen == SCREEN_REWARD) {
            if ((down & HidNpadButton_Left) && selectedReward > 0) selectedReward--;
            if ((down & HidNpadButton_Right) && selectedReward < REWARD_SIZE) selectedReward++;

            if (touchPressed) {
                for (int i = 0; i < REWARD_SIZE; i++) {
                    int cardY = (selectedReward == i) ? 170 : 190;
                    int cardX = 310 + i * 250;
                    if (point_in_rect(touchX, touchY, cardX - 10, cardY - 10, 190, 270)) {
                        selectedReward = i;
                        down |= HidNpadButton_A;
                    }
                }

                int skipX = SCREEN_W - 245;
                int skipY = SCREEN_H - 82;
                int skipW = 205;
                int skipH = 44;
                if (point_in_rect(touchX, touchY, skipX, skipY, skipW, skipH)) {
                    selectedReward = REWARD_SIZE;
                    down |= HidNpadButton_A;
                }
            }

            if (down & HidNpadButton_A) {
                if (selectedReward < REWARD_SIZE)
                    add_card_to_player_deck(playerDeck, &playerDeckCount, rewards[selectedReward]);
                advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
            }

            if (down & HidNpadButton_B) {
                advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
            }
        } else if (screen == SCREEN_CAMPFIRE) {
            if ((down & HidNpadButton_Up) && selectedCampfire > 0) selectedCampfire--;
            if ((down & HidNpadButton_Down) && selectedCampfire < 2) selectedCampfire++;

            if (down & HidNpadButton_A) {
                if (selectedCampfire == 0) {
                    int restHeal = playerMaxHp * 30 / 100;
                    if (restHeal < 1) restHeal = 1;
                    player_hp += restHeal;
                    if (player_hp > playerMaxHp) player_hp = playerMaxHp;
                    record_completed_node(currentNodeType, &runFloors, &runElites, &runBosses);
                    update_best_stats(endlessMode, runFloors, coins, runElites, runBosses, &bestFloors, &bestCoins, &bestElites, &bestBosses);
                    advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
                } else if (selectedCampfire == 1) {
                    screen = SCREEN_UPGRADE;
                    selectedUpgrade = 0;
                } else {
                    record_completed_node(currentNodeType, &runFloors, &runElites, &runBosses);
                    update_best_stats(endlessMode, runFloors, coins, runElites, runBosses, &bestFloors, &bestCoins, &bestElites, &bestBosses);
                    advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
                }
            }
        } else if (screen == SCREEN_SHOP) {
            if ((down & HidNpadButton_Left) && selectedShop > 0 && selectedShop < SHOP_SIZE) selectedShop--;
            if ((down & HidNpadButton_Right) && selectedShop < SHOP_SIZE) selectedShop++;

            if ((down & HidNpadButton_Up) && selectedShop >= 4 && selectedShop < SHOP_SIZE) selectedShop -= 4;
            else if ((down & HidNpadButton_Up) && selectedShop == SHOP_SIZE) selectedShop = 6;

            if ((down & HidNpadButton_Down) && selectedShop < 4) selectedShop += 4;
            else if ((down & HidNpadButton_Down) && selectedShop >= 4 && selectedShop < SHOP_SIZE) selectedShop = SHOP_SIZE;

            if (selectedShop < 0) selectedShop = 0;
            if (selectedShop > SHOP_SIZE) selectedShop = SHOP_SIZE;

            if (down & HidNpadButton_A) {
                if (selectedShop == SHOP_SIZE) {
                    record_completed_node(currentNodeType, &runFloors, &runElites, &runBosses);
                    update_best_stats(endlessMode, runFloors, coins, runElites, runBosses, &bestFloors, &bestCoins, &bestElites, &bestBosses);
                    advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
                } else if (!shopPurchased[selectedShop] && coins >= shopCosts[selectedShop]) {
                    coins -= shopCosts[selectedShop];
                    shopPurchased[selectedShop] = 1;
                    add_card_to_player_deck(playerDeck, &playerDeckCount, shopCards[selectedShop]);
                }
            }

            if (down & HidNpadButton_B) {
                record_completed_node(currentNodeType, &runFloors, &runElites, &runBosses);
                update_best_stats(endlessMode, runFloors, coins, runElites, runBosses, &bestFloors, &bestCoins, &bestElites, &bestBosses);
                advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
            }
        } else if (screen == SCREEN_UPGRADE) {
            int upgradeableCount = count_upgradeable_cards(playerDeck, playerDeckCount);

            if ((down & HidNpadButton_Left) && selectedUpgrade > 0) selectedUpgrade--;
            if ((down & HidNpadButton_Right) && selectedUpgrade < upgradeableCount - 1) selectedUpgrade++;

            if (selectedUpgrade < 0) selectedUpgrade = 0;
            if (selectedUpgrade >= upgradeableCount) selectedUpgrade = upgradeableCount - 1;

            if (down & HidNpadButton_A) {
                int actualIndex = get_upgradeable_card_index(playerDeck, playerDeckCount, selectedUpgrade);
                if (actualIndex >= 0 && upgrade_owned_card(playerDeck, playerDeckCount, actualIndex)) {
                    record_completed_node(currentNodeType, &runFloors, &runElites, &runBosses);
                    update_best_stats(endlessMode, runFloors, coins, runElites, runBosses, &bestFloors, &bestCoins, &bestElites, &bestBosses);
                    advance_room(&screen, &act, &currentNode, &selectedSlot, pendingNode, endlessMode);
                }
            }

            if (down & HidNpadButton_B) {
                screen = SCREEN_CAMPFIRE;
            }
        }

        u32 stride;
        u32* framebuf = framebufferBegin(&fb, &stride);

        if (screen == SCREEN_TITLE)
            draw_title_screen(framebuf, stride, mainMenuBg.img, mainMenuBg.w, mainMenuBg.h, selectedTitle, bestFloors, bestCoins, bestElites, bestBosses);
        else if (screen == SCREEN_CRAWLER)
            draw_crawler_screen(framebuf, stride, mainMenuBg.img, mainMenuBg.w, mainMenuBg.h, selectedCrawler);
        else if (screen == SCREEN_MAP)
            draw_map_screen(framebuf, stride, act, currentNode, selectedSlot, coins);
        else if (screen == SCREEN_COMBAT)
            draw_combat_screen(framebuf, stride, currentBattleBg ? currentBattleBg->img : NULL, currentBattleBg ? currentBattleBg->w : 0, currentBattleBg ? currentBattleBg->h : 0, hand, handCount, selectedCard, enemies, enemyCount, selectedEnemy, player_hp, playerMaxHp, player_block, player_vulnerable, player_weak, player_strength, energy, act, coins, drawPile, drawPileCount, discardPile, discardCount, exhaustCount, &crawlers[selectedCrawler]);
        else if (screen == SCREEN_REWARD)
            draw_reward_screen(framebuf, stride, currentBattleBg ? currentBattleBg->img : NULL, currentBattleBg ? currentBattleBg->w : 0, currentBattleBg ? currentBattleBg->h : 0, rewards, selectedReward, coins, lastGoldGain);
        else if (screen == SCREEN_CAMPFIRE)
            draw_campfire_screen(framebuf, stride, currentBattleBg ? currentBattleBg->img : NULL, currentBattleBg ? currentBattleBg->w : 0, currentBattleBg ? currentBattleBg->h : 0, selectedCampfire);
        else if (screen == SCREEN_UPGRADE)
            draw_upgrade_screen(framebuf, stride, currentBattleBg ? currentBattleBg->img : NULL, currentBattleBg ? currentBattleBg->w : 0, currentBattleBg ? currentBattleBg->h : 0, playerDeck, playerDeckCount, selectedUpgrade);
        else if (screen == SCREEN_SHOP)
            draw_shop_screen(framebuf, stride, currentBattleBg ? currentBattleBg->img : NULL, currentBattleBg ? currentBattleBg->w : 0, currentBattleBg ? currentBattleBg->h : 0, shopCards, shopCosts, shopPurchased, selectedShop, coins);
        else if (screen == SCREEN_DEATH)
            draw_death_screen(framebuf, stride, mainMenuBg.img, mainMenuBg.w, mainMenuBg.h, selectedDeath, runFloors, coins, runElites, runBosses);
        else
            draw_win_screen(framebuf, stride);

        if (screen == SCREEN_TITLE && showHowToPopup)
            draw_how_to_play_popup(framebuf, stride);

        framebufferEnd(&fb);
    }

    framebufferClose(&fb);

    free_backgrounds();

    for (int i = 0; i < CARD_COUNT; i++)
        if (deck[i].img) stbi_image_free(deck[i].img);

    free_enemy_sprite_pool(NORMAL_ENEMY_SPRITE_COUNT, normalEnemySprites);
    free_enemy_sprite_pool(ELITE_ENEMY_SPRITE_COUNT, eliteEnemySprites);
    free_enemy_sprite_pool(BOSS_ENEMY_SPRITE_COUNT, bossEnemySprites);
    free_crawler_sprites();

    romfsExit();
    return 0;
}