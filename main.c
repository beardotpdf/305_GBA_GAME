#include "stars.h"
#include "stars2.h"
#include "sky.h"
#include "ground.h"
#include "LunarLanderTiles.h"

#include "sprites.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800

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
// Buttons used for testing background scroll
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)

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

    sprites[index].attribute0 = y |            
        (0 << 8) |        
        (0 << 10) |         
        (0 << 12) |      
        (1 << 13) |         
        (shape_bits << 14);

    sprites[index].attribute1 = x |            
        (0 << 9) |       
        (h << 12) |        
        (v << 13) |         
        (size_bits << 14); 

    sprites[index].attribute2 = tile_index |   
        (priority << 10) | 
        (0 << 12);         

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
    memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) sprites_palette, PALETTE_SIZE);
    memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) sprites_data, (sprites_width * sprites_height) / 2);
}

// Function to set up background for the game
void setup_background() {

    for (int i = 0; i < PALETTE_SIZE; i++) {
        bg_palette[i] = LunarLanderTiles_palette[i];
    }

    volatile unsigned short* dest = char_block(0);
    unsigned short* image = (unsigned short*) LunarLanderTiles_data;
    for (int i = 0; i < ((LunarLanderTiles_width * LunarLanderTiles_height) / 2); i++) {
        dest[i] = image[i];
    }

    *bg0_control = 0 |    
        (0 << 2)  |       
        (0 << 6)  |       
        (1 << 7)  |       
        (16 << 8) |       
        (1 << 13) |       
        (0 << 14);    
    *bg1_control = 1 |    
        (0 << 2)  |       
        (0 << 6)  |       
        (1 << 7)  |       
        (17 << 8) |       
        (1 << 13) |      
        (0 << 14);        
    *bg2_control = 2 |    
        (0 << 2)  |       
        (0 << 6)  |       
        (1 << 7)  |       
        (18 << 8) |       
        (1 << 13) |      
        (0 << 14);        
    *bg3_control = 3 |    
        (0 << 2)  |       
        (0 << 6)  |       
        (1 << 7)  |       
        (19 << 8) |       
        (1 << 13) |      
        (0 << 14);       
    // Background for the ground
    dest = screen_block(16);
    for (int i = 0; i < (ground_width * ground_height); i++) {
        dest[i] = ground[i];
    }
    // Background for stars
    dest = screen_block(17);
    for (int i = 0; i < (stars_width * stars_height); i++) {
        dest[i] = stars[i];
    }
    // Background for stars2
    dest = screen_block(18);
    for (int i = 0; i < (stars2_width * stars2_height); i++) {
        dest[i] = stars2[i];
    }
    // Background for sky
    dest = screen_block(19);
    for (int i = 0; i < (sky_width * sky_height); i++) {
        dest[i] = sky[i];
    }
}


// Struct for the lander
struct Lander {
    struct Sprite* sprite;
    int x, y;
    int xvel;
    int yvel;
    int gravity;
    int landed;
    int fuel;
    int score;
    int frame;
    int border;
};

// Struct for Vertical thrust
struct VerticalThrust {
    struct Sprite* sprite;
    int x, y;
    int xoffset, yoffset;
    int frame; // which frame of animation is the thrust on
    int animation_delay; // num of frames to wait before flipping
    int counter; // how many frames until we flip
    int active; // whether the thrust is active
};

// Struct for Left Thrust (on left side of lander)
struct LeftThrust {
    struct Sprite* sprite;
    int x, y;
    int xoffset, yoffset;
    int frame; // which frame of animation is the thrust on
    int animation_delay; // num of frames to wait before flipping
    int counter; // how many frames until we flip
    int active; // whether the thrust is active
};

// Struct for Right Thrust (on right side of lander)
struct RightThrust {
    struct Sprite* sprite;
    int x, y;
    int xoffset, yoffset;
    int frame; // which frame of animation is the thrust on
    int animation_delay; // num of frames to wait before flipping
    int counter; // how many frames until we flip
    int active; // whether the thrust is active
};

// Struct for characters
struct Character {
    struct Sprite* sprite;
    int x, y;
    int frame;
};

// Initializes a character sprite
void character_init(struct Character* character, int x, int y, int frame) {
    character->x = x;
    character->y = y;
    character->frame = frame;
    character->sprite = sprite_init(x, y, SIZE_8_8, 0, 0, character->frame, 0);
}

// Struct for the UI
struct UI {
    int x, y;
    struct Character fuel1, fuel2, fuel3, fuel4;
    struct Character score1, score2, score3, score4;
};

// Initializes the UI which shows the amount of fuel left and score
void UI_init(struct UI* ui, int x, int y, struct Lander* lander) {
    ui->x = x;
    ui->y = y;

    struct Character f, u, e1, l, colon1, s, c, o, r, e2, colon2;
    struct Character fuel1, fuel2, fuel3, fuel4;
    struct Character score1, score2, score3, score4;
    // Letter offset used to get where the letter sprite frames start
    int letter_offset = 18;
    // Initialize letter character sprites
    character_init(&f, x + 8, y, 2 * (letter_offset + 6));
    character_init(&u, x + 16, y, 2 * (letter_offset + 21));
    character_init(&e1, x + 24, y, 2 * (letter_offset + 5));
    character_init(&l, x + 32, y, 2 * (letter_offset + 12));
    character_init(&colon1, x + 40, y, 2 * 46);
    character_init(&s, x, y + 9, 2 * (letter_offset + 19));
    character_init(&c, x + 8, y + 9, 2 * (letter_offset + 3));
    character_init(&o, x + 16, y + 9, 2 * (letter_offset + 15));
    character_init(&r, x + 24, y + 9, 2 * (letter_offset + 18));
    character_init(&e2, x + 32, y + 9, 2 * (letter_offset + 5));
    character_init(&colon2, x + 40, y + 9, 2 * 46);

    // Digit offset used to get to where the digit sprite frames start
    int digit_offset = 9;
    // Digit sprites used to display the amount of fuel left
    int fuel = lander->fuel;
    int score = lander->score;
    // Initialize digit character sprites
    character_init(&fuel1, x + 48, y, 2 * (digit_offset + (fuel / 10 / 10 / 10) % 10));
    character_init(&fuel2, x + 56, y, 2 * (digit_offset + (fuel / 10 / 10) % 10));
    character_init(&fuel3, x + 64, y, 2 * (digit_offset + (fuel / 10) % 10));
    character_init(&fuel4, x + 72, y, 2 * (digit_offset + (fuel % 10)));
    character_init(&score1, x + 48, y + 9, 2 * (digit_offset + (score / 10 / 10 / 10) % 10));
    character_init(&score2, x + 56, y + 9, 2 * (digit_offset + (score / 10 / 10) % 10));
    character_init(&score3, x + 64, y + 9, 2 * (digit_offset + (score / 10) % 10));
    character_init(&score4, x + 72, y + 9, 2 * (digit_offset + (score % 10)));
    // Assign the initialized digit character sprites to the UI struct variables
    ui->fuel1 = fuel1;
    ui->fuel2 = fuel2;
    ui->fuel3 = fuel3;
    ui->fuel4 = fuel4;
    ui->score1 = score1;
    ui->score2 = score2;
    ui->score3 = score3;
    ui->score4 = score4;
};


// Initialize lander
void lander_init(struct Lander* lander) {
    lander->x = 120;
    lander->y = 20;
    lander->xvel = 0;
    lander->yvel = 0;
    lander->gravity = 20;
    lander->landed = 0;
    lander->fuel = 3000;
    lander->score = 0;
    lander->frame = 0;
    // Initialize sprite of lander and its properties
    /*
     * Lander sprite is currently a placeholder and is a 8 by 8 size, if lander sprite is changed to something bigger 
     * change the SIZE_8_8 value in the third argument to whatever the new sprite size is.
     */
    lander->sprite = sprite_init(lander->x, lander->y, SIZE_8_8, 0, 0, lander->frame, 1);
}

void landerReset(struct Lander* lander) {
    lander->x = 120;
    lander->y = 20;
    lander->xvel = 0;
    lander->yvel = 0;
    lander->gravity = 20;
    lander->landed = 0;
    lander->frame = 0;
}

void thrust_init(struct VerticalThrust* verticalThrust, struct LeftThrust* leftThrust, struct RightThrust* rightThrust, struct Lander* lander) {
    
    verticalThrust->xoffset = 0;
    verticalThrust->yoffset = 8;
    verticalThrust->x = lander->x;
    verticalThrust->y = lander->y + verticalThrust->yoffset;
    verticalThrust->frame = 0;
    verticalThrust->animation_delay = 8; 
    verticalThrust->counter = 0;
    verticalThrust->active = 0;

    verticalThrust->sprite = sprite_init(verticalThrust->x, verticalThrust->y, SIZE_8_8, 0, 0, 8,1);

    leftThrust->xoffset = -8;
    leftThrust->yoffset = 0;
    leftThrust->x = lander->x + leftThrust->xoffset;
    leftThrust->y = lander->y + leftThrust->yoffset;
    leftThrust->frame = 0;
    leftThrust->animation_delay = 8;
    leftThrust->counter = 0;
    leftThrust->active = 0; 

    leftThrust->sprite = sprite_init(leftThrust->x, leftThrust->y, SIZE_8_8, 1, 0, 8, 1);

    rightThrust->xoffset = 8;
    rightThrust->yoffset = 0;
    rightThrust->x = lander->x + rightThrust->xoffset;
    rightThrust->y = lander->y + rightThrust->yoffset;
    rightThrust->frame = 0;
    rightThrust->animation_delay = 8;
    rightThrust->counter = 0;
    rightThrust->active = 0;

    rightThrust->sprite = sprite_init(rightThrust->x, rightThrust->y, SIZE_8_8, 0, 0, 8, 1);

}

// Checks if the lander is at the bottom of the map, if so return true
int lander_at_bounds(struct Lander* lander, int* yscroll) {
    // 96 is the lowest yscroll value before it transitions to the next copy of the background below
    if (*yscroll >= 96 && lander->y >= 20) {
	// Change yscroll to 96 in case it goes under
	*yscroll = 96;
	return 1;
    // 0 is the highest yscroll value before it transitions to the next copy of the background above
    } else if (*yscroll <= 0 && lander->y <= 20) {
	// Change yscroll to 0 in case it goes over
	*yscroll = 0;
	return 1;
    } else {
	// Set lander y to 20 if false
	lander->y = 20;
	return 0;
    }
}

// Decreases lander y velocity to allow it to move it up
void lander_ascend(struct Lander* lander) {
    if (!lander->landed && lander->fuel > 0) {
	lander->yvel += -40;
	lander->fuel -= 1;
    }
}

// updates lander's x velocity and decrements remaining fuel
void updateLanderXvel(int* xvel, int right, int* fuel);

// Increases or decreases lander x velocity to move it left or right
void lander_side(struct Lander* lander, int right) {
    if (!lander->landed && lander->fuel > 0) {
	// If right is true, increase xvel; If false, decrease xvel

        updateLanderXvel(&(lander->xvel), right, &(lander->fuel));

    }
}

// asm function
// returns index of bg tile that contains pixel (x, y) 
int getIndex(int x, int y);

// determines if the lander is colliding with the surface 
// by checking the bottom left and bottom right corners of its sprite
// overlap with ground tiles
// returns 2 when landing on both feet, 0 when no collision happens, and 1 for any other collision
int checkCollision(struct Lander* lander, int* xscroll, int* yscroll) {
    int collision = 0;

    // define lander hitbox
    int left = *xscroll + lander->x;
    int right = left + 7;

    int top = *yscroll + lander->y;
    int bottom = top + 7;

    // check bottom left tile

    if (ground[getIndex(left, bottom)]) {
        collision += 1;
    }

    // check bottom right tile

    if (ground[getIndex(right, bottom)]) {
        collision += 1;
    }


    // don't check top left or top right; it can be assumed that the lander won't fly up into the ground

    return collision;

}

// Updates the lander
void lander_update(struct Lander* lander, int* yscroll , int* xscroll, struct VerticalThrust* verticalThrust, struct LeftThrust* leftThrust, struct RightThrust* rightThrust) {
    // Update position of lander
    if (!lander->landed) {
      	// If lander at bottom or top of background, stop scrolling and move lander by changing y value. If false, continue scrolling.
	if (lander_at_bounds(lander, yscroll)) {
	    lander->y += (lander->yvel >> 8);
	} else {
            *yscroll += (lander->yvel >> 8);
	}
	// Add gravity to lander y velocity so it falls
	lander->yvel += lander->gravity;
	// Scroll background left or right depending on the x velocity of the lander
	*xscroll += (lander->xvel >> 8);
    
        int collision = checkCollision(lander, xscroll, yscroll);

        if (collision == 2  && lander->xvel >> 9 == 0 && lander->yvel >> 8 <= 1) {
            // successful landing on both feet with max 1px/frame movement on each axis
            
            lander->y--; // move sprite to ground level
            
            lander->score += 250;
            
            lander->landed = 1;
        }
        else if (collision) {
            // run crash landing sequence here
            lander->landed = 1;    
        }
    }

    // increment reset timer after landing
    if (lander->landed) {
        lander->landed++;
    }

    if (lander->landed > 60) {
        // TODO: reset lander position, velocity, landed
        landerReset(lander);
        thrust_init(&lander, &verticalThrust, &leftThrust, &rightThrust);

        *xscroll = lander->x;
        *yscroll = lander->y - 20;
    }
    
    // Set lander sprite on the screen position
    sprite_position(lander->sprite, lander->x, lander->y);
}

// Update thrust sprites
void thrust_update(struct Lander* lander, struct VerticalThrust* verticalThrust, struct LeftThrust* leftThrust, struct RightThrust* rightThrust) {
    sprite_position(verticalThrust->sprite, lander->x, (lander->y + verticalThrust->yoffset));
    sprite_position(leftThrust->sprite, (lander->x + leftThrust->xoffset), lander->y);
    sprite_position(rightThrust->sprite, (lander->x + rightThrust->xoffset), lander->y);
    
    if(button_pressed(BUTTON_A)) {
        verticalThrust->counter++;
        if (verticalThrust->counter >= verticalThrust->animation_delay) {   
            verticalThrust->frame = verticalThrust->frame + 16;
            if (verticalThrust->frame > 16) {
                verticalThrust->frame = 0;
            }
            sprite_set_offset(verticalThrust->sprite, 14); 
            verticalThrust->counter = 0;
        }
    }

    if (button_pressed(BUTTON_RIGHT)) {
        leftThrust->counter++;
        if (leftThrust->counter >= leftThrust->animation_delay) {
            leftThrust->frame = leftThrust->frame + 16;
            if (leftThrust->frame > 16) {
                leftThrust->frame = 0;
            }
            sprite_set_offset(leftThrust->sprite, 10); 
            leftThrust->counter = 0;
        }
    }

    if (button_pressed(BUTTON_LEFT)) {
        rightThrust->counter++;
        if (rightThrust->counter >= rightThrust->animation_delay) {
            rightThrust->frame = rightThrust->frame + 16;
            if (rightThrust->frame > 16) {
                rightThrust->frame = 0;
            }
            sprite_set_offset(rightThrust->sprite, 10); 
            rightThrust->counter = 0;
        }
    }
}


// Updates the UI by changing the tile offsets of the digit character sprites
void UI_update(struct UI* ui, struct Lander* lander) {
    int digit_offset = 9;
    // Get fuel and score of lander
    int fuel = lander->fuel;
    int score = lander->score;
    // Change frames for the digit character sprites based on score and amoutn of fuel left
    ui->fuel1.frame = 2 * (digit_offset + (fuel / 10 / 10 / 10) % 10);
    ui->fuel2.frame = 2 * (digit_offset + (fuel / 10 / 10) % 10);
    ui->fuel3.frame = 2 * (digit_offset + (fuel / 10) % 10);
    ui->fuel4.frame = 2 * (digit_offset + (fuel % 10));
    ui->score1.frame = 2 * (digit_offset + (score / 10 / 10 / 10) % 10);
    ui->score2.frame = 2 * (digit_offset + (score / 10 / 10) % 10);
    ui->score3.frame = 2 * (digit_offset + (score / 10) % 10);
    ui->score4.frame = 2 * (digit_offset + (score % 10));
    // Set the offset for each of the digit character sprites to update them
    sprite_set_offset(ui->fuel1.sprite, ui->fuel1.frame);
    sprite_set_offset(ui->fuel2.sprite, ui->fuel2.frame);
    sprite_set_offset(ui->fuel3.sprite, ui->fuel3.frame);
    sprite_set_offset(ui->fuel4.sprite, ui->fuel4.frame);
    sprite_set_offset(ui->score1.sprite, ui->score1.frame);
    sprite_set_offset(ui->score2.sprite, ui->score2.frame);
    sprite_set_offset(ui->score3.sprite, ui->score3.frame);
    sprite_set_offset(ui->score4.sprite, ui->score4.frame);
}


int main() {
    *display_control = MODE0 | BG0_ENABLE | BG1_ENABLE | BG2_ENABLE | BG3_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;
    // Set up background and the sprites
    setup_background();
    setup_sprite_image();
    sprite_clear();

    // Initialize lander
    struct Lander lander;
    lander_init(&lander);

    //Initialize Thruster
    struct VerticalThrust verticalThrust;
    struct LeftThrust leftThrust;
    struct RightThrust rightThrust;
    thrust_init(&verticalThrust, &leftThrust, &rightThrust, &lander);

    // Initialize UI
    struct UI ui;
    UI_init(&ui, 1, 1, &lander);

    // Set initial scroll to lander x and y position
    int xscroll = lander.x;
    int yscroll = lander.y - 20;

    while (1) {
	// Update UI
	UI_update(&ui, &lander);
	// Update the lander
	lander_update(&lander, &yscroll, &xscroll, &verticalThrust, &leftThrust, &rightThrust);
    	thrust_update(&lander, &verticalThrust, &leftThrust, &rightThrust);
	// Move lander up if A button is pressed
	if (button_pressed(BUTTON_A)) {
	    lander_ascend(&lander);
	}
        // Move lander left or right if LEFT or RIGHT button is pressed	
	if (button_pressed(BUTTON_RIGHT)) {
	    lander_side(&lander, 1);
	}
	if (button_pressed(BUTTON_LEFT)) {
	    lander_side(&lander, 0);
	}

	// Wait for vblank period before doing anything else
	wait_vblank();
	// Scroll the backgrounds
	*bg0_x_scroll = xscroll;
	*bg0_y_scroll = yscroll;
	*bg1_x_scroll = xscroll / 7;
	*bg1_y_scroll = yscroll / 7;
	*bg2_x_scroll = xscroll / 15;
	*bg2_y_scroll = yscroll / 15;
	// Update sprites on screen
	sprite_update_all();
	// Delay so lander doesn't move too fast
	delay(3000);
    }
}
