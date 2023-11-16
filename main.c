#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x300
#define BG3_ENABLE 0x400

#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000

#define PALETTE_SIZE 256
#define NUM_SPRITES 128

// Pointers to registers of the 4 tile layers
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;

// Pointers to scrolling registers for the background
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;
volatile short* bg2_x_scroll = (unsigned short*) 0x4000018;
volatile short* bg2_y_scroll = (unsigned short*) 0x400001a;
volatile short* bg3_x_scroll = (unsigned short*) 0x400001c;
volatile short* bg3_y_scroll = (unsigned short*) 0x400001e;

// Pointers to registers used by program
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;
volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

// Bit positions for each button used by program
#define BUTTON_A (1 << 0)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)

// Wait for screen to be drawn
void wait_vblank() {
    while (*scanline_counter < 160) { }
}

// Check button input
unsigned char button_pressed(unsigned short button) {
    unsigned short pressed = *buttons & button;
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}

// Return pointer to one of 4 character blocks
volatile unsigned short* char_block(unsigned long block) {
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

// Return pointer to one of 32 screen blocks
volatile unsigned short* screen_block(unsigned long block) {
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}

// Flags for enabling DMA and sizes to transfer
#define DMA_ENABLE 0x80000000
#define DMA_16 0x00000000
#define DMA_32 0x04000000

// Pointers to location of DMA source and location
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;
// Pointer to DMA count
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

// Use DMA to copy data
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
    *dma_source = (unsigned int) source;
    *dma_destination = (unsigned int) dest;
    *dma_count = amount | DMA_16 | DMA_ENABLE;
}

// Delay time
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

// Struct for storing the 4 sprite attributes
struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};

// Array of sprites in GBA
struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

// Sprite sizes
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};

// Initialize a sprite and return the pointer
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
        int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    int index = next_sprite_index++;

    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    sprites[index].attribute0 = y |             /* y coordinate */
        (0 << 8) |          /* rendering mode */
        (0 << 10) |         /* gfx mode */
        (0 << 12) |         /* mosaic */
        (1 << 13) |         /* color mode, 0:16, 1:256 */
        (shape_bits << 14); /* shape */

    sprites[index].attribute1 = x |             /* x coordinate */
        (0 << 9) |          /* affine flag */
        (h << 12) |         /* horizontal flip flag */
        (v << 13) |         /* vertical flip flag */
        (size_bits << 14);  /* size */

    sprites[index].attribute2 = tile_index |   // tile index */
        (priority << 10) | // priority */
        (0 << 12);         // palette bank (only 16 color)*/

    return &sprites[index];
}

// Update all sprites on screen
void sprite_update_all() {
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

// Set up sprites and splace offscreen
void sprite_clear() {
    next_sprite_index = 0;

    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = SCREEN_HEIGHT;
        sprites[i].attribute1 = SCREEN_WIDTH;
    }
}

// Set sprite position
void sprite_position(struct Sprite* sprite, int x, int y) {
    sprite->attribute0 &= 0xff00;
    sprite->attribute0 |= (y & 0xff);
    sprite->attribute1 &= 0xfe00;
    sprite->attribute1 |= (x & 0x1ff);
}

// Move a sprite
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    int y = sprite->attribute0 & 0xff;
    int x = sprite->attribute1 & 0x1ff;
    sprite_position(sprite, x + dx, y + dy);
}

// Change tile offset for a sprite
void sprite_set_offset(struct Sprite* sprite, int offset) {
    sprite->attribute2 &= 0xfc00;
    sprite->attribute2 |= (offset & 0x03ff);
}

// Set up sprite image and the palette
void setup_sprite_image() {
}

// Struct for the lander
struct Lander {
    int x, y;
    int yvel;
    int gravity;
    int landed;
    int fuel;
};

// struct for keeping track of the score
struct Score {
    int points;
}

// Initialize lander
void lander_init(struct Lander* lander) {
}


int main() {
    *display_control = MODE0 | BG0_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

    struct Lander lander;
    lander_init(&lander);

    int xscroll = 0;

    while (1) {
    }
}

