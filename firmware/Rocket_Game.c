/*
 * ATmega328P Rocket_Game / Alien Shooter
 * Clock: internal 1 MHz
 * Display: 1.8-inch ST7735/ST7735S 128x160, BLACKTAB-compatible
 * Programmer: USBasp
 *
 * TFT:
 *   RST -> PB0, physical pin 14
 *   D/C -> PB1, physical pin 15
 *   CS  -> PB2, physical pin 16
 *   DIN -> PB3/MOSI, physical pin 17
 *   CLK -> PB5/SCK, physical pin 19
 *
 * Joystick:
 *   VRx -> PC0/ADC0, physical pin 23
 *   VRy -> PC1/ADC1, physical pin 24
 *
 * Buttons:
 *   START/RESTART -> PC2, physical pin 25, button to GND
 *   SHOOT         -> PC3, physical pin 26, button to GND
 *
 * Passive buzzer:
 *   PD3, physical pin 5 -> 100 ohm -> buzzer +, buzzer - -> GND
 */

#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define TFT_RST_LOW()    (PORTB &= ~(1 << PB0))
#define TFT_RST_HIGH()   (PORTB |=  (1 << PB0))
#define TFT_DC_LOW()     (PORTB &= ~(1 << PB1))
#define TFT_DC_HIGH()    (PORTB |=  (1 << PB1))
#define TFT_CS_LOW()     (PORTB &= ~(1 << PB2))
#define TFT_CS_HIGH()    (PORTB |=  (1 << PB2))

#define BUZZER_LOW()     (PORTD &= ~(1 << PD3))
#define BUZZER_HIGH()    (PORTD |=  (1 << PD3))

#define ST7735_SWRESET   0x01
#define ST7735_SLPOUT    0x11
#define ST7735_NORON     0x13
#define ST7735_INVOFF    0x20
#define ST7735_DISPON    0x29
#define ST7735_CASET     0x2A
#define ST7735_RASET     0x2B
#define ST7735_RAMWR     0x2C
#define ST7735_MADCTL    0x36
#define ST7735_COLMOD    0x3A
#define ST7735_FRMCTR1   0xB1
#define ST7735_FRMCTR2   0xB2
#define ST7735_FRMCTR3   0xB3
#define ST7735_INVCTR    0xB4
#define ST7735_PWCTR1    0xC0
#define ST7735_PWCTR2    0xC1
#define ST7735_PWCTR3    0xC2
#define ST7735_PWCTR4    0xC3
#define ST7735_PWCTR5    0xC4
#define ST7735_VMCTR1    0xC5
#define ST7735_GMCTRP1   0xE0
#define ST7735_GMCTRN1   0xE1

#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    160
#define HUD_HEIGHT       14
#define PLAY_TOP         16
#define PLAY_BOTTOM      159

#define BLACK            0x0000
#define WHITE            0xFFFF
#define RED              0xF800
#define GREEN            0x07E0
#define BLUE             0x001F
#define YELLOW           0xFFE0
#define CYAN             0x07FF
#define MAGENTA          0xF81F
#define ORANGE           0xFD20
#define PURPLE           0x8010
#define NAVY             0x000F
#define SKY              0x867D
#define DARK_GRAY        0x4208
#define LIGHT_GRAY       0xC618
#define LIME             0x87E0
#define PINK             0xF81F

#define PLAYER_W         11
#define PLAYER_H          9
#define PLAYER_MIN_Y     105
#define PLAYER_MAX_Y     149
#define MAX_PLAYER_BULLETS 3
#define MAX_ALIENS        8
#define MAX_ENEMY_BULLETS 2
#define ALIEN_W           9
#define ALIEN_H           7
#define JOY_LOW          380
#define JOY_HIGH         644

typedef struct {
    int16_t x, y, old_x, old_y;
    uint8_t active;
} Bullet;

typedef struct {
    int16_t x, y, old_x, old_y;
    uint8_t active;
    uint8_t health;
    uint8_t type;
} Alien;

static int16_t player_x, player_y, old_player_x, old_player_y;
static Bullet player_bullets[MAX_PLAYER_BULLETS];
static Bullet enemy_bullets[MAX_ENEMY_BULLETS];
static Alien aliens[MAX_ALIENS];

static uint16_t score;
static uint8_t lives;
static uint8_t level;
static uint8_t game_over;
static uint8_t shoot_cooldown;
static uint8_t enemy_fire_counter;
static uint8_t alien_move_counter;
static int8_t alien_direction;
static uint16_t random_state = 0xACE1u;

static const uint8_t digit_font[10][5] = {
    {0x07,0x05,0x05,0x05,0x07},{0x02,0x06,0x02,0x02,0x07},
    {0x07,0x01,0x07,0x04,0x07},{0x07,0x01,0x07,0x01,0x07},
    {0x05,0x05,0x07,0x01,0x01},{0x07,0x04,0x07,0x01,0x07},
    {0x07,0x04,0x07,0x05,0x07},{0x07,0x01,0x01,0x01,0x01},
    {0x07,0x05,0x07,0x05,0x07},{0x07,0x05,0x07,0x01,0x07}
};

/* 3x5 uppercase letters needed by the screens. Each row uses 3 bits. */
typedef struct { char c; uint8_t row[5]; } Glyph;
static const Glyph glyphs[] = {
    {'A',{2,5,7,5,5}}, {'C',{7,4,4,4,7}}, {'D',{6,5,5,5,6}},
    {'E',{7,4,6,4,7}}, {'F',{7,4,6,4,4}}, {'G',{7,4,5,5,7}},
    {'I',{7,2,2,2,7}}, {'L',{4,4,4,4,7}}, {'M',{5,7,7,5,5}},
    {'N',{5,7,7,7,5}}, {'O',{7,5,5,5,7}}, {'P',{6,5,6,4,4}},
    {'R',{6,5,6,5,5}}, {'S',{7,4,7,1,7}}, {'T',{7,2,2,2,2}},
    {'V',{5,5,5,5,2}}, {'Y',{5,5,2,2,2}}, {'X',{5,5,2,5,5}},
    {'U',{5,5,5,5,7}}, {'H',{5,5,7,5,5}}, {'B',{6,5,6,5,6}},
    {'K',{5,5,6,5,5}}, {' ',{0,0,0,0,0}}, {'-',{0,0,7,0,0}}
};

static void spi_init(void) {
    DDRB |= (1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB5);
    DDRB &= ~(1<<PB4);
    TFT_RST_HIGH(); TFT_DC_HIGH(); TFT_CS_HIGH();
    SPCR = (1<<SPE)|(1<<MSTR);
    SPSR = (1<<SPI2X);
}

static void spi_write(uint8_t v) {
    SPDR = v;
    while (!(SPSR & (1<<SPIF))) { }
}

static void tft_command(uint8_t c) {
    TFT_CS_LOW(); TFT_DC_LOW(); spi_write(c); TFT_CS_HIGH();
}

static void tft_data(uint8_t d) {
    TFT_CS_LOW(); TFT_DC_HIGH(); spi_write(d); TFT_CS_HIGH();
}

static void tft_reset(void) {
    TFT_RST_HIGH(); _delay_ms(100);
    TFT_RST_LOW();  _delay_ms(100);
    TFT_RST_HIGH(); _delay_ms(200);
}

static void tft_init(void) {
    spi_init(); tft_reset();
    tft_command(ST7735_SWRESET); _delay_ms(150);
    tft_command(ST7735_SLPOUT);  _delay_ms(500);
    tft_command(ST7735_FRMCTR1); tft_data(0x01); tft_data(0x2C); tft_data(0x2D);
    tft_command(ST7735_FRMCTR2); tft_data(0x01); tft_data(0x2C); tft_data(0x2D);
    tft_command(ST7735_FRMCTR3); tft_data(0x01); tft_data(0x2C); tft_data(0x2D); tft_data(0x01); tft_data(0x2C); tft_data(0x2D);
    tft_command(ST7735_INVCTR); tft_data(0x07);
    tft_command(ST7735_PWCTR1); tft_data(0xA2); tft_data(0x02); tft_data(0x84);
    tft_command(ST7735_PWCTR2); tft_data(0xC5);
    tft_command(ST7735_PWCTR3); tft_data(0x0A); tft_data(0x00);
    tft_command(ST7735_PWCTR4); tft_data(0x8A); tft_data(0x2A);
    tft_command(ST7735_PWCTR5); tft_data(0x8A); tft_data(0xEE);
    tft_command(ST7735_VMCTR1); tft_data(0x0E);
    tft_command(ST7735_INVOFF);
    tft_command(ST7735_MADCTL); tft_data(0xC8);
    tft_command(ST7735_COLMOD); tft_data(0x05); _delay_ms(10);
    tft_command(ST7735_GMCTRP1);
    { const uint8_t g1[16]={0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10}; uint8_t i; for(i=0;i<16;i++) tft_data(g1[i]); }
    tft_command(ST7735_GMCTRN1);
    { const uint8_t g2[16]={0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10}; uint8_t i; for(i=0;i<16;i++) tft_data(g2[i]); }
    tft_command(ST7735_NORON); _delay_ms(10);
    tft_command(ST7735_DISPON); _delay_ms(100);
}

static void tft_set_window(uint8_t x0,uint8_t y0,uint8_t x1,uint8_t y1) {
    tft_command(ST7735_CASET); tft_data(0); tft_data(x0); tft_data(0); tft_data(x1);
    tft_command(ST7735_RASET); tft_data(0); tft_data(y0); tft_data(0); tft_data(y1);
    tft_command(ST7735_RAMWR);
}

static void tft_fill_rect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color) {
    uint32_t n; uint8_t hi,lo;
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;}
    if(x+w>SCREEN_WIDTH) w=SCREEN_WIDTH-x;
    if(y+h>SCREEN_HEIGHT) h=SCREEN_HEIGHT-y;
    if(w<=0||h<=0) return;
    hi=(uint8_t)(color>>8); lo=(uint8_t)color;
    tft_set_window((uint8_t)x,(uint8_t)y,(uint8_t)(x+w-1),(uint8_t)(y+h-1));
    n=(uint32_t)w*(uint32_t)h;
    TFT_CS_LOW(); TFT_DC_HIGH();
    while(n--){spi_write(hi);spi_write(lo);} TFT_CS_HIGH();
}

static void tft_fill_screen(uint16_t c){ tft_fill_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,c); }

static void draw_digit(uint8_t d,int16_t x,int16_t y,uint8_t s,uint16_t c) {
    uint8_t r,col; if(d>9)return;
    for(r=0;r<5;r++) for(col=0;col<3;col++) if(digit_font[d][r]&(1<<(2-col))) tft_fill_rect(x+col*s,y+r*s,s,s,c);
}

static const uint8_t *glyph_for(char c) {
    uint8_t i;
    for(i=0;i<(uint8_t)(sizeof(glyphs)/sizeof(glyphs[0]));i++) if(glyphs[i].c==c) return glyphs[i].row;
    return glyphs[23].row;
}

static void draw_char(char ch,int16_t x,int16_t y,uint8_t s,uint16_t c) {
    const uint8_t *g=glyph_for(ch); uint8_t r,col;
    for(r=0;r<5;r++) for(col=0;col<3;col++) if(g[r]&(1<<(2-col))) tft_fill_rect(x+col*s,y+r*s,s,s,c);
}

static void draw_text(const char *text,int16_t x,int16_t y,uint8_t s,uint16_t c) {
    while(*text){ draw_char(*text,x,y,s,c); x += 4*s; text++; }
}

static void adc_init(void) {
    ADMUX=(1<<REFS0);
    ADCSRA=(1<<ADEN)|(1<<ADPS1)|(1<<ADPS0);
}

static uint16_t adc_read(uint8_t ch) {
    ADMUX=(1<<REFS0)|(ch&0x0F); _delay_us(20);
    ADCSRA|=(1<<ADSC); while(ADCSRA&(1<<ADSC)){}
    return ADC;
}

static void controls_init(void) {
    DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
    PORTC |= (1<<PC2)|(1<<PC3);
    adc_init();
}

static uint8_t start_pressed(void){ return !(PINC&(1<<PC2)); }
static uint8_t shoot_pressed(void){ return !(PINC&(1<<PC3)); }

static void buzzer_init(void){ DDRD|=(1<<PD3); BUZZER_LOW(); }
static void tone_shoot(void){ uint8_t i; for(i=0;i<10;i++){BUZZER_HIGH();_delay_us(180);BUZZER_LOW();_delay_us(180);} }
static void tone_alien_hit(void){ uint8_t i; for(i=0;i<12;i++){BUZZER_HIGH();_delay_us(420);BUZZER_LOW();_delay_us(420);} }
static void tone_player_hit(void){ uint8_t i; for(i=0;i<18;i++){BUZZER_HIGH();_delay_us(700);BUZZER_LOW();_delay_us(700);} }
static void tone_level(void){ uint8_t i; for(i=0;i<12;i++){BUZZER_HIGH();_delay_us(500);BUZZER_LOW();_delay_us(500);} for(i=0;i<16;i++){BUZZER_HIGH();_delay_us(300);BUZZER_LOW();_delay_us(300);} }
static void tone_game_over(void){ uint8_t i; for(i=0;i<16;i++){BUZZER_HIGH();_delay_us(600);BUZZER_LOW();_delay_us(600);} _delay_ms(50); for(i=0;i<16;i++){BUZZER_HIGH();_delay_us(950);BUZZER_LOW();_delay_us(950);} }

static uint16_t random_next(void){ uint16_t bit=((random_state>>0)^(random_state>>2)^(random_state>>3)^(random_state>>5))&1u; random_state=(random_state>>1)|(bit<<15); return random_state; }
static uint8_t random_range(uint8_t a,uint8_t b){ return a+(random_next()%((uint16_t)b-a+1)); }

static uint8_t overlap(int16_t x1,int16_t y1,int16_t w1,int16_t h1,int16_t x2,int16_t y2,int16_t w2,int16_t h2) {
    return !((x1+w1)<=x2||(x2+w2)<=x1||(y1+h1)<=y2||(y2+h2)<=y1);
}

static void draw_starfield(void) {
    static const uint8_t sx[18]={5,17,29,41,54,67,80,93,106,119,11,24,38,72,88,101,114,61};
    static const uint8_t sy[18]={20,31,25,43,19,35,28,48,22,39,66,82,57,73,92,61,109,118};
    uint8_t i; for(i=0;i<18;i++) tft_fill_rect(sx[i],sy[i],1,1,(i&1)?WHITE:SKY);
}

static void draw_player(int16_t x,int16_t y,uint16_t body) {
    tft_fill_rect(x+5,y,1,2,WHITE);
    tft_fill_rect(x+4,y+2,3,2,body);
    tft_fill_rect(x+2,y+4,7,3,body);
    tft_fill_rect(x,y+7,11,2,CYAN);
    tft_fill_rect(x+2,y+8,2,1,ORANGE);
    tft_fill_rect(x+7,y+8,2,1,ORANGE);
}
static void erase_player(int16_t x,int16_t y){ tft_fill_rect(x,y,PLAYER_W,PLAYER_H,BLACK); }

static uint16_t alien_color(uint8_t type,uint8_t health) {
    if(health>=3) return RED;
    if(health==2) return ORANGE;
    if(type==0) return LIME;
    if(type==1) return MAGENTA;
    return YELLOW;
}

static void draw_alien(const Alien *a) {
    uint16_t c=alien_color(a->type,a->health);
    tft_fill_rect(a->x+2,a->y,5,1,c);
    tft_fill_rect(a->x+1,a->y+1,7,1,c);
    tft_fill_rect(a->x,a->y+2,9,3,c);
    tft_fill_rect(a->x+1,a->y+5,2,2,c);
    tft_fill_rect(a->x+6,a->y+5,2,2,c);
    tft_fill_rect(a->x+2,a->y+2,1,1,BLACK);
    tft_fill_rect(a->x+6,a->y+2,1,1,BLACK);
}
static void erase_alien(const Alien *a){ tft_fill_rect(a->old_x,a->old_y,ALIEN_W,ALIEN_H,BLACK); }

static void draw_bullet(const Bullet *b,uint16_t c){ tft_fill_rect(b->x,b->y,2,4,c); }
static void erase_bullet(const Bullet *b){ tft_fill_rect(b->old_x,b->old_y,2,4,BLACK); }

static void explosion(int16_t x,int16_t y) {
    tft_fill_rect(x+3,y+2,4,4,WHITE); _delay_ms(20);
    tft_fill_rect(x+1,y,8,8,YELLOW); _delay_ms(25);
    tft_fill_rect(x,y-1,10,10,ORANGE); _delay_ms(30);
    tft_fill_rect(x-1,y-2,12,12,RED); _delay_ms(30);
    tft_fill_rect(x-2,y-3,14,14,BLACK);
}

static void draw_hud(void) {
    uint16_t v=score; uint8_t h,t,u;
    if (v > 999) v = 999;
    h = v / 100;
    t = (v / 10) % 10;
    u = v % 10;
    tft_fill_rect(0,0,128,HUD_HEIGHT,NAVY);
    draw_text("S",3,2,1,CYAN);
    if(v>=100){draw_digit(h,10,2,1,YELLOW);draw_digit(t,15,2,1,YELLOW);draw_digit(u,20,2,1,YELLOW);}
    else if(v>=10){draw_digit(t,13,2,1,YELLOW);draw_digit(u,18,2,1,YELLOW);} else draw_digit(u,16,2,1,YELLOW);
    draw_text("L",44,2,1,GREEN); draw_digit(level%10,51,2,1,WHITE);
    draw_text("V",75,2,1,RED); draw_digit(lives,82,2,1,WHITE);
    tft_fill_rect(0,13,128,2,CYAN);
}

static void clear_bullets(void) {
    uint8_t i; for(i=0;i<MAX_PLAYER_BULLETS;i++) player_bullets[i].active=0;
    for(i=0;i<MAX_ENEMY_BULLETS;i++) enemy_bullets[i].active=0;
}

static void setup_level(void) {
    uint8_t i,row,col;
    tft_fill_rect(0,PLAY_TOP,128,144,BLACK); draw_starfield();
    for(i=0;i<MAX_ALIENS;i++) aliens[i].active=0;
    for(i=0;i<MAX_ALIENS;i++) {
        row=i/4; col=i%4;
        aliens[i].x=10+col*28;
        aliens[i].y=24+row*18;
        aliens[i].old_x=aliens[i].x; aliens[i].old_y=aliens[i].y;
        aliens[i].type=(uint8_t)((i+level)%3);
        aliens[i].health=(level>=5)?3:((level>=3)?2:1);
        if((i&1)==0 && aliens[i].health>1) aliens[i].health--;
        aliens[i].active=1; draw_alien(&aliens[i]);
    }
    player_x=58; player_y=142; old_player_x=player_x; old_player_y=player_y;
    draw_player(player_x,player_y,CYAN);
    clear_bullets(); alien_direction=1; alien_move_counter=0; enemy_fire_counter=25; shoot_cooldown=0;
    draw_hud();
}

static uint8_t aliens_left(void){ uint8_t i,n=0; for(i=0;i<MAX_ALIENS;i++) if(aliens[i].active)n++; return n; }

static void fire_player(void) {
    uint8_t i; if(shoot_cooldown)return;
    for(i=0;i<MAX_PLAYER_BULLETS;i++) if(!player_bullets[i].active){
        player_bullets[i].x=player_x+5; player_bullets[i].y=player_y-4;
        player_bullets[i].old_x=player_bullets[i].x; player_bullets[i].old_y=player_bullets[i].y;
        player_bullets[i].active=1; draw_bullet(&player_bullets[i],YELLOW); tone_shoot(); shoot_cooldown=6; return;
    }
}

static void fire_enemy(void) {
    uint8_t available[8],count=0,i,pick,b;
    for(i=0;i<MAX_ALIENS;i++) if(aliens[i].active) available[count++]=i;
    if (!count) return;
    pick = available[random_range(0, count - 1)];
    for(b=0;b<MAX_ENEMY_BULLETS;b++) if(!enemy_bullets[b].active){
        enemy_bullets[b].x=aliens[pick].x+4; enemy_bullets[b].y=aliens[pick].y+ALIEN_H;
        enemy_bullets[b].old_x=enemy_bullets[b].x; enemy_bullets[b].old_y=enemy_bullets[b].y;
        enemy_bullets[b].active=1; draw_bullet(&enemy_bullets[b],RED); return;
    }
}

static void update_player(void) {
    uint16_t jx=adc_read(0),jy=adc_read(1);
    old_player_x=player_x; old_player_y=player_y;
    if(jx<JOY_LOW)player_x-=2; else if(jx>JOY_HIGH)player_x+=2;
    if(jy<JOY_LOW)player_y-=2; else if(jy>JOY_HIGH)player_y+=2;
    if (player_x < 2) player_x = 2;
    if (player_x + PLAYER_W > 126) player_x = 126 - PLAYER_W;
    if (player_y < PLAYER_MIN_Y) player_y = PLAYER_MIN_Y;
    if (player_y > PLAYER_MAX_Y) player_y = PLAYER_MAX_Y;
    if(player_x!=old_player_x||player_y!=old_player_y){ erase_player(old_player_x,old_player_y); draw_player(player_x,player_y,CYAN); }
}

static void update_player_bullets(void) {
    uint8_t i,j;
    for(i=0;i<MAX_PLAYER_BULLETS;i++) if(player_bullets[i].active){
        player_bullets[i].old_x=player_bullets[i].x; player_bullets[i].old_y=player_bullets[i].y; erase_bullet(&player_bullets[i]);
        player_bullets[i].y-=4;
        if(player_bullets[i].y<=PLAY_TOP){player_bullets[i].active=0;continue;}
        for(j=0;j<MAX_ALIENS;j++) if(aliens[j].active && overlap(player_bullets[i].x,player_bullets[i].y,2,4,aliens[j].x,aliens[j].y,ALIEN_W,ALIEN_H)){
            player_bullets[i].active=0;
            if(aliens[j].health>0)aliens[j].health--;
            if(aliens[j].health==0){ aliens[j].old_x=aliens[j].x; aliens[j].old_y=aliens[j].y; erase_alien(&aliens[j]); explosion(aliens[j].x,aliens[j].y); aliens[j].active=0; score+=(uint16_t)(5+level); tone_alien_hit(); draw_hud(); }
            else { tft_fill_rect(aliens[j].x,aliens[j].y,ALIEN_W,ALIEN_H,WHITE); _delay_ms(20); draw_alien(&aliens[j]); tone_alien_hit(); }
            break;
        }
        if(player_bullets[i].active)draw_bullet(&player_bullets[i],YELLOW);
    }
}

static void lose_life(void) {
    tone_player_hit(); explosion(player_x,player_y); if(lives>0)lives--; draw_hud();
    if(lives==0){game_over=1;return;}
    player_x=58; player_y=142; old_player_x=player_x; old_player_y=player_y; draw_player(player_x,player_y,CYAN);
    clear_bullets(); _delay_ms(300);
}

static void update_enemy_bullets(void) {
    uint8_t i;
    for(i=0;i<MAX_ENEMY_BULLETS;i++) if(enemy_bullets[i].active){
        enemy_bullets[i].old_x=enemy_bullets[i].x; enemy_bullets[i].old_y=enemy_bullets[i].y; erase_bullet(&enemy_bullets[i]);
        enemy_bullets[i].y += 3 + (level/4);
        if(overlap(enemy_bullets[i].x,enemy_bullets[i].y,2,4,player_x,player_y,PLAYER_W,PLAYER_H)){
            enemy_bullets[i].active=0; lose_life(); if(game_over)return;
        } else if(enemy_bullets[i].y>=156) enemy_bullets[i].active=0;
        else draw_bullet(&enemy_bullets[i],RED);
    }
}

static void update_aliens(void) {
    uint8_t i; int16_t left=127,right=0,bottom=0; uint8_t step_down=0;
    uint8_t interval=(level>=7)?3:(uint8_t)(10-level);
    if(interval<3)interval=3;
    if (++alien_move_counter < interval) return;
    alien_move_counter = 0;
    for(i=0;i<MAX_ALIENS;i++) if(aliens[i].active){ if(aliens[i].x<left)left=aliens[i].x; if(aliens[i].x+ALIEN_W>right)right=aliens[i].x+ALIEN_W; }
    if((alien_direction>0 && right>=124)||(alien_direction<0 && left<=4)){ alien_direction=-alien_direction; step_down=1; }
    for(i=0;i<MAX_ALIENS;i++) if(aliens[i].active){ aliens[i].old_x=aliens[i].x; aliens[i].old_y=aliens[i].y; erase_alien(&aliens[i]); aliens[i].x+=alien_direction*2; if(step_down)aliens[i].y+=5; if(aliens[i].y+ALIEN_H>bottom)bottom=aliens[i].y+ALIEN_H; draw_alien(&aliens[i]); }
    if(bottom>=PLAYER_MIN_Y){ game_over=1; }
}

static void draw_start_screen(void) {
    uint8_t blink=0;
    tft_fill_screen(NAVY);
    tft_fill_rect(0,0,128,4,CYAN); tft_fill_rect(0,156,128,4,MAGENTA);
    draw_starfield();
    draw_text("SPACE",24,18,3,CYAN);
    draw_text("DEFENDER",8,40,3,YELLOW);
    draw_alien(&(Alien){18,76,18,76,1,1,0});
    draw_alien(&(Alien){50,70,50,70,1,2,1});
    draw_alien(&(Alien){92,78,92,78,1,3,2});
    draw_player(58,106,CYAN);
    draw_text("MOVE",8,126,1,WHITE); draw_text("SHOOT",78,126,1,WHITE);
    while(!start_pressed()){
        if(blink){ tft_fill_rect(26,139,76,13,GREEN); draw_text("START",44,143,1,BLACK); }
        else { tft_fill_rect(26,139,76,13,NAVY); tft_fill_rect(26,139,76,2,GREEN); tft_fill_rect(26,150,76,2,GREEN); tft_fill_rect(26,139,2,13,GREEN); tft_fill_rect(100,139,2,13,GREEN); draw_text("START",44,143,1,WHITE); }
        blink^=1; _delay_ms(250);
    }
    _delay_ms(40); while(start_pressed())_delay_ms(10);
}

static void draw_game_over_screen(void) {
    uint16_t v=score; uint8_t h,t,u,blink=0;
    tone_game_over(); if(v>999)v=999; h=v/100;t=(v/10)%10;u=v%10;
    tft_fill_screen(BLACK);
    tft_fill_rect(4,10,120,140,DARK_GRAY);
    tft_fill_rect(7,13,114,134,NAVY);
    draw_text("GAME",28,25,3,RED);
    draw_text("OVER",32,47,3,ORANGE);
    draw_text("SCORE",34,80,2,CYAN);
    if(v>=100){draw_digit(h,36,97,3,YELLOW);draw_digit(t,52,97,3,YELLOW);draw_digit(u,68,97,3,YELLOW);} else if(v>=10){draw_digit(t,45,97,3,YELLOW);draw_digit(u,61,97,3,YELLOW);} else draw_digit(u,54,97,3,YELLOW);
    while(!start_pressed()){
        if(blink){tft_fill_rect(19,128,90,15,MAGENTA);draw_text("RESTART",35,133,1,WHITE);} else {tft_fill_rect(19,128,90,15,NAVY);tft_fill_rect(19,128,90,2,MAGENTA);tft_fill_rect(19,141,90,2,MAGENTA);draw_text("RESTART",35,133,1,YELLOW);} blink^=1; _delay_ms(250);
    }
    _delay_ms(40); while(start_pressed())_delay_ms(10);
}

static void reset_game(void) {
    score=0; lives=3; level=1; game_over=0;
    random_state^=adc_read(0); random_state^=(adc_read(1)<<5);
    tft_fill_screen(BLACK); setup_level(); tone_level();
}

int main(void) {
    tft_init(); controls_init(); buzzer_init();
    draw_start_screen(); reset_game();
    while(1){
        if(game_over){ draw_game_over_screen(); reset_game(); continue; }
        update_player();
        if(shoot_pressed())fire_player();
        if(shoot_cooldown)shoot_cooldown--;
        update_player_bullets();
        update_enemy_bullets();
        if(game_over)continue;
        update_aliens();
        if(game_over)continue;
        if(enemy_fire_counter)enemy_fire_counter--; else { fire_enemy(); enemy_fire_counter=(level>=6)?12:(uint8_t)(30-level*3); }
        if(aliens_left()==0){ level++; if(level>9)level=9; tone_level(); _delay_ms(300); setup_level(); }
        draw_player(player_x,player_y,CYAN);
        _delay_ms(25);
    }
    return 0;
}