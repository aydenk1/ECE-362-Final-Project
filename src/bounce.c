#include "stm32f0xx.h"
#include <stdio.h>      // for sprintf()
#include <stdint.h>
#include <stdlib.h>
#include "lcd.h"

#include "extrafunctions.h"
//Globals
//=============================================================================
extern const Picture background;// A 240x320 background image
extern const Picture enemyship; // A 32x32 spaceship
extern const Picture railgun;   // A 20x68 gun
extern const Picture laser;     // A 4x50 laser beam
extern const Picture explosion; // A 39x33 background image

//KEY SELECTION
const char left = '1';
const char right = '*';
const char fire = '5';

const int border = 10;
const int fireDly = 300;        //Recharge delay
const int strtAmmo = 3;         //Starting ammo
const int strOffset = 20;       //String offset
int xmin; // Farthest to the left the center of the ball can go
int xmax; // Farthest to the right the center of the ball can go
int ymin; // Farthest to the top the center of the ball can go
int ymax; // Farthest to the bottom the center of the ball can go
int x,y; // Center of ball
int vx,vy; // Velocity components of ball



int px;             // Center of paddle offset
int newpx;          // New center of paddle
int firing;
int fireRdy;        // Allows a shot
int ammo;           // Ammo counter
int lasery;
int laserx;
int laserymax;
int score = 0;
int level = 1;
char lvlstr[] = "Level 1 ";
int rnd = 1;
char rndstr[] = "Round 1 ";
//=============================================================================
void firingMechanics();
void shipLocationMechanics();
void updateLvlRnd();
void initKeys();


// Copy a subset of a large source picture into a smaller destination.
// sx,sy are the offset into the source picture.
void pic_subset(Picture *dst, const Picture *src, int sx, int sy)
{
    int dw = dst->width;
    int dh = dst->height;
    if (dw + sx > src->width)
        for(;;)
            ;
    if (dh + sy > src->height)
        for(;;)
            ;
    for(int y=0; y<dh; y++)
        for(int x=0; x<dw; x++)
            dst->pix2[dw * y + x] = src->pix2[src->width * (y+sy) + x + sx];
}

// Overlay a picture onto a destination picture.
// xoffset,yoffset are the offset into the destination picture that the
// source picture is placed.
// Any pixel in the source that is the 'transparent' color will not be
// copied.  This defines a color in the source that can be used as a
// transparent color.
void pic_overlay(Picture *dst, int xoffset, int yoffset, const Picture *src, int transparent)
{
    for(int y=0; y<src->height; y++) {
        int dy = y+yoffset;
        if (dy < 0 || dy >= dst->height)
            continue;
        for(int x=0; x<src->width; x++) {
            int dx = x+xoffset;
            if (dx < 0 || dx >= dst->width)
                continue;
            unsigned short int p = src->pix2[y*src->width + x];
            if (p != transparent)
                dst->pix2[dy*dst->width + dx] = p;
        }
    }
}

// Called after a bounce, update the x,y velocity components in a
// pseudo random way.  (+/- 1)
void perturb(int *vx, int *vy)
{
    int lvlmax = level + 2;

    if (*vx > 0) {
        *vx += (random()%3) - 1;
        if (*vx > lvlmax)
            *vx = lvlmax;
        if (*vx == 0)
            *vx = level;
    } else {
        *vx += (random()%3) - 1;
        if (*vx < -lvlmax)
            *vx = -lvlmax;
        if (*vx == 0)
            *vx = -level;
    }
    if (*vy > 0) {
        *vy += (random()%3) - 1;
        if (*vy > lvlmax)
            *vy = lvlmax;
        if (*vy == 0)
            *vy = level;
    } else {
        *vy += (random()%3) - 1;
        if (*vy < -lvlmax)
            *vy = -lvlmax;
        if (*vy == 0)
            *vy = -level;
    }
}


// This C macro will create an array of Picture elements.
// Really, you'll just use it as a pointer to a single Picture
// element with an internal pix2[] array large enough to hold
// an image of the specified size.
// BE CAREFUL HOW LARGE OF A PICTURE YOU TRY TO CREATE:
// A 100x100 picture uses 20000 bytes.  You have 32768 bytes of SRAM.
#define TempPicturePtr(name,width,height) Picture name[(width)*(height)/6+2] = { {width,height,2} }

// Create a 29x29 object to hold the ship plus padding
TempPicturePtr(object,42,42);


void firingMechanics()
{
    int rightLx = laserx + (laser.width/2);
    int leftLx = laserx - (laser.width/2);

    TempPicturePtr(tmp,4,60);                                 //Laser is 4x50, 5 pixels of vert buffer
    pic_subset(tmp, &background, laserx-tmp->width/2, lasery);//Copy the background
    pic_overlay(tmp, 0, 5, &laser, 0xffff);
    LCD_DrawPicture(laserx - tmp->width/2, lasery, tmp);


    //Hit Detection
    //=========================================================================
    if ((rightLx >= (x - (enemyship.width/2))) && (leftLx <= (x + (enemyship.width/2))))
    {
        if (lasery <= y)
        {
            //Hit the ship
            //=================================================================
            firing = 0;

            TempPicturePtr(tmp,42,42);// Create a temporary 42x24 image.
            pic_subset(tmp, &background, x-tmp->width/2, y-tmp->height/2);// Copy the background
            pic_overlay(tmp, 0,0, &explosion, 0xffff);// Overlay the object
            LCD_DrawPicture(x-tmp->width/2,y-tmp->height/2, tmp);

            score = score  + (((ammo + 1) * 50) * rnd);//Update score
            updateScore(score);
            ammo = strtAmmo;                        //Update ammo
            nano_wait(500000000);                   //Bask in glory

            updateAmmoLEDs(ammo);
            updateLvlRnd();
            px = -1;                                //Force redraw of ship
        }
    }
    else if (lasery <= (laserymax)) {
            //Missed the ship
            //=================================================================
            firing = 0;
            LCD_DrawString(rightLx + 5,lasery + 10,  WHITE, BLACK, "MISS!", 16, 0);
            nano_wait(500000000);                   //Feel shame

            LCD_DrawPicture(0,0,&background);       //Redraw background to clear text
            px = -1;
    }
    lasery = lasery - 2;
}

void shipLocationMechanics()
{
    x += vx;                //Update Ship Velocities
    y += vy;
    if (x <= xmin) {        //Ship hit left wall
        vx = - vx;
        if (x < xmin)
            x += vx;
        perturb(&vx,&vy);
    }
    if (x >= xmax) {        //Ship hit the right wall
        vx = -vx;
        if (x > xmax)
            x += vx;
        perturb(&vx,&vy);
    }
    if (y <= ymin) {        //Ship hit the top wall.
        vy = - vy;
        if (y < ymin)
            y += vy;
        perturb(&vx,&vy);
    }
    if (y >= ymax) {        //Ship hit the bottom "wall".
        vy = - vy;
        if (y > ymax)
            y += vy;
        perturb(&vx,&vy);
    }
}

void updateLvlRnd()
{
    ++level;

    if (level > 3) {
        ++rnd;                                      //Increment round

        char rndNum[3];
        snprintf(rndNum, 3, "%d", rnd);
        rndstr[6] = rndNum[0];
        rndstr[7] = rndNum[1];
        LCD_DrawString(background.width/2 - strOffset,background.height/2 - 20,  WHITE, BLACK, rndstr, 16, 0);
        LCD_DrawString(background.width/2 - strOffset,background.height/2,  WHITE, BLACK, "Level 1", 16, 0);
        nano_wait(1000000000);

        level = 1;                                  //Reset level
        LCD_DrawPicture(0,0,&background);           //Redraw background to clear text
    }
    else
    {
        char lvlNum[2];
        snprintf(lvlNum, 2, "%d", level);
        lvlstr[6] = lvlNum[0];
        LCD_DrawString(background.width/2 - strOffset,background.height/2,  WHITE, BLACK, lvlstr, 16, 0);
        nano_wait(1000000000);

        LCD_DrawPicture(0,0,&background);           //Redraw background to clear text
    }
}

void initKeys()
{
    LCD_DrawPicture(0,0,&background);
    LCD_DrawString(background.width/2 - strOffset - 40,background.height/2,  WHITE, BLACK, "Press 1 to Start", 16, 0);

    while(getkey() != '1');

    LCD_DrawPicture(0,0,&background);
}

void TIM17_IRQHandler(void)
{
    TIM17->SR &= ~TIM_SR_UIF;
    //Shot was fired
    //=========================================================================
    if (firing == 1)
    {
        firingMechanics();
    }
    else if(ammo == 0)
    {
        LCD_DrawString(background.width/2 - strOffset,background.height/2,  WHITE, BLACK, "YOU LOSE!", 16, 0);
    }
    else
    {
    //Normal play
    //=========================================================================
        char key = check_key(left, right, fire);
        if ((key == fire) && (fireRdy == fireDly) && (ammo > 0))
        {
            firing = 1;
            fireRdy = 0;
            --ammo;
            updateAmmoLEDs(ammo);
            laserx = px + 1;                                //Center of laser beam
            lasery = background.height - (border + 12 + 60);//Set the top left of laser image, 12 is the gun offset and 60 for the height of the tmp
        }
        else if (key == '1')
        {
            newpx -= 2;
        }
        else if (key == '*')
        {
            newpx += 2;
        }

        //Update railgun location
        //=====================================================================
        if (newpx - railgun.width/2 <= border || newpx + railgun.width/2 >= 240-border) {
            newpx = px;
        }
        if (newpx != px) {
            px = newpx;
            TempPicturePtr(tmp,24,68);
            pic_subset(tmp, &background, px-tmp->width/2, background.height-border-tmp->height); // Copy the background
            pic_overlay(tmp, 2, 0, &railgun, 0xffff);
            LCD_DrawPicture(px - tmp->width/2, background.height - border - tmp->height, tmp);
        }
        //Update ship location and display
        //=====================================================================
        shipLocationMechanics();

        TempPicturePtr(tmp,42,42); // Create a temporary 29x29 image.
        pic_subset(tmp, &background, x-tmp->width/2, y-tmp->height/2); // Copy the background
        pic_overlay(tmp, 0,0, object, 0xffff); // Overlay the object
        pic_overlay(tmp, (px-railgun.width/2) - (x-tmp->width/2),
                (background.height-border-railgun.height) - (y-tmp->height/2),
                &railgun, 0xffff); // Draw the paddle into the image
        LCD_DrawPicture(x-tmp->width/2,y-tmp->height/2, tmp); // Re-draw it to the screen
        // The object has a 5-pixel border around it that holds the background
        // image.  As long as the object does not move more than 5 pixels (in any
        // direction) from it's previous location, it will clear the old object.
    }

    if ((fireRdy < fireDly) && (firing == 0) && (ammo != 0))
    {
        ++fireRdy;
        updateOLED(fireRdy);
    }

}

int main(void)
{
    enable_keypad_display();
    setup_spi1_displayTFT();
    setupOLED();
    LCD_Init();

    // Draw the background and intitialize levels
    initKeys();
    initOLED();

    LCD_DrawString(background.width/2 - strOffset,background.height/2 - 20,  WHITE, BLACK, "Round 1", 16, 0);
    LCD_DrawString(background.width/2 - strOffset,background.height/2,  WHITE, BLACK, "Level 1", 16, 0);
    nano_wait(1000000000);
    LCD_DrawPicture(0,0,&background);

    setPercentTable(fireDly);

    // Set all pixels in the object to white.
    for(int i=0; i<42*42; i++)
        object->pix2[i] = 0xffff;

    // Center the 19x19 ball into center of the 29x29 object.
    // Now, the 19x19 ball has 5-pixel white borders in all directions.
    pic_overlay(object,5,5,&enemyship,0xffff);

    // Initialize the game state.
    xmin = border + enemyship.width/2;
    xmax = background.width - border - enemyship.width/2;
    ymin = border + enemyship.height/2;
    ymax = background.height/2 - border - enemyship.height/2;
    x = (xmin+xmax)/2; // Center of ball
    y = ymin;
    vx = 1; // Velocity components of ball
    vy = 0;
    laserymax = border + 5;
    fireRdy = fireDly;
    ammo = strtAmmo;


    px = -1; // Center of paddle offset (invalid initial value to force update)
    newpx = (xmax+xmin)/2; // New center of paddle

    //Initialize Peripherals
    updateScore(0);
    setupAmmoLEDs();
    updateAmmoLEDs(ammo);

    setup_tim17();
}
