/*
 * Solomon Systec Products SSD1306 128x64/32 Dot Matric
 * OLED/PLED Segment/Common Driver Controller
 * http://www.solomon-systech.com/
 *
 * Driver for Raspberry Pi 3 written by
 * Kai Harrekilde-Petersen (C) 2017
 *
 * 0.91" 128x32 module bought off Aliexpress.com
 * Connect VCC to 5V as the module has internal 3.3V LDO (Torex XC6206)
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "i2c.h"
#include "ssd1306.h"
#include "ssd1306_regs.h"

#define VERSION "0.0.2"

uint8_t *gfxbuf = NULL; /* Graphics buffer */

uint8_t img8[8] = {0x7F, 0x09, 0x09, 0x9, 0x09, 0x01, 0x01, 0x00};

uint8_t img16[32] = {
  0xFF, 0xFF, 0xFF, 0xC7, 0xC7, 0xC7, 0xC7, 0xC7, /* Upper 8 pixel rows */
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x00, 0x00,
  0x7F, 0x7F, 0x7F, 0x01, 0x01, 0x01, 0x01, 0x01, /* Lower 8 pixel rows */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void ssd130x_power(int fd, int on)
{
  /* Powering up not controlled in SW */
  return;
}

void ssd130x_reset(int fd)
{
  /* No-op */
  return;
}

void ssd_cmd1(int fd, uint8_t c0)
{
  i2c_wr8(fd, SSD_C, c0);
}

void ssd_cmd2(int fd, uint8_t c0, uint8_t c1)
{
  i2c_wr16(fd, SSD_C, (c1<<8) | c0);
}

void ssd_cmd3(int fd, uint8_t c0, uint8_t c1, uint8_t c2)
{
  uint8_t cmd[3] = {c0, c1, c2};
  i2c_wr_blk(fd, SSD_C, 3, (uint8_t *)&cmd);
}

void ssd_cmd_blk(int fd, int len, uint8_t *cmd)
{
  i2c_wr_blk(fd, SSD_C, len, cmd);
}

void ssd_dat1(int fd, uint8_t d0)
{
  i2c_wr8(fd, SSD_D, d0);
}

void ssd_dat2(int fd, uint8_t d0, uint8_t d1)
{
  i2c_wr16(fd, SSD_D, (d1 << 8) | d0);
}

void ssd_dat_blk(int fd, int len, uint8_t *data)
{
  i2c_wr_blk(fd, SSD_D, len, data);
}

void ssd130x_init(int fd)
{
/*

 * Software initialization must NOT be done according to <SSD1306.pdf,
 * pg64>, as it doesn't work (trust me on this one).
 * Instead use the sequence from
 * <https://github.com/galpavlin/STM32-SSD1306/128x32/src/oled.c>:
 *  1) Set Display OFF (0xAE)
 *  2) Set Display Clock (0xD5, 0x80)
 *  3) Set MUX ratio (0xA8, 0x1F) [x32, x64=0x3F]
 *  4) Set Display offset (0xD3, 0x00)
 *  5) Set Display Start Line (0x40)
 *  6) Set Charge-pump Internal (0x8D, 0x14)
 *  7) Addressing Mode = PAGE (0x20)
 *  8) Set Segment re-map (0xA1)
 *  9) Set COM Output Scan Dir reversed (0xC8)
 * 10) Set COM pin HW config (0xDA, 0x02)
 * 11) Set Contrast Control (0x81, 0x8F)
 * 12) Set precharge period (0xD9, 0xF1)
 * 13) Set VCOMH deselect level (0xDB, 0x40)
 * 14) Set Entire display on (0xA4)
 * 15) Set Display Normal (0xA6)
 * 16) Set Scrolling = OFF (0x2E)
 * 17) Set Display ON (0xAF)
 * 18) /Clear Screen/
 */
  ssd_cmd1(fd, SSD_DISP_SLEEP);        /* 0xAE: DISP_ENTIRE_OFF */
  ssd_cmd2(fd, SSD_DCLK_DIV, 0x80);    /* 0xD5: DCLK_DIV = 0x80 */
  ssd_cmd2(fd, SSD_MUX_RATIO, 0x1F);   /* 0xA8: MUX_RATIO = 0x1F (x32 display) */
  ssd_cmd2(fd, SSD_DISP_OFFSET, 0);    /* 0xD3: DISP_OFFSET = 0 */
  ssd_cmd1(fd, SSD_DISP_ST_LINE | 0);  /* 0x40: DISP_STARTLINE = 0 */
  ssd_cmd2(fd, SSD_CHARGEPUMP, 0x14);  /* 0x8D: CHARGEPUMP = 0x14 */
  ssd_cmd2(fd, SSD_ADDR_MODE, 0);      /* 0x20: ADDR_MODE = 0h */
  ssd_cmd1(fd, SSD_SEG_REMAP127);      /* 0xA1: SEG_REMAP = Y */
  ssd_cmd1(fd, SSD_COM_SCAN_REV);      /* 0xC8: COM_SCAN = Reverse (7..0) */
  ssd_cmd2(fd, SSD_COM_HW_CFG, 0x02);  /* 0xDA: COM_HW PINS = 0x02 */
  ssd_cmd2(fd, SSD_CONTRAST, 0x8F);    /* 0x81: CONTRAST = 0x8F */
  ssd_cmd2(fd, SSD_PRECHARGE, 0xF1);   /* 0xD9: PRECHARGE = 0xF1 */
  ssd_cmd2(fd, SSD_VCOM_LVL, 0x40);    /* 0xDB: VCOMH LEVEL = 0x40 */
  ssd_cmd1(fd, SSD_DISP_ENT_NORM);     /* 0xA4: DISP_ENTIRE_RESUME */
  ssd_cmd1(fd, SSD_DISP_NORM);         /* 0xA6: DISP_NORMAL */
  ssd_cmd1(fd, SSD_SCROLL_OFF);        /* 0x2E: SCROLL = DEACTIVATE */
  ssd_cmd1(fd, SSD_DISP_AWAKE);        /* 0xAF: DISP = ON */

  /* If not allocated, allocate gfx buffer; else: zero contents */
  if(NULL==gfxbuf) {
    gfxbuf = calloc((size_t) SSD_LCD_WIDTH*SSD_LCD_HEIGHT/4, sizeof(uint8_t));
    if(NULL==gfxbuf) {
      puts("Unable to allocate memory for graphics buffer");
      exit(-1);
    }
  } else {
    memset(gfxbuf, 0, (size_t) SSD_LCD_WIDTH*SSD_LCD_HEIGHT/4);
  }
  /* Update GDDRAM from gfxbuf */
  ssd_disp_update(fd);
}

void ssd_disp_awake(int fd, int on)
{
  if(on) {
    i2c_wr8(fd, SSD_C, SSD_DISP_AWAKE);
  } else {
    i2c_wr8(fd, SSD_C, SSD_DISP_SLEEP);
  }
}

void ssd_disp_update(int fd)
{
  int i;
  int len=I2C_BLK_MAX;

  /* Tell which area we want to update */
  ssd_cmd3(fd, SSD_ADDR_COL, 0, SSD_LCD_WIDTH-1);
  ssd_cmd3(fd, SSD_ADDR_PAGE, 0, (SSD_LCD_HEIGHT/8)-1);
  /* Copy from gfxbuf to display */
  for(i=0;i<SSD_LCD_WIDTH*SSD_LCD_HEIGHT/8;i+=len) {
    ssd_dat_blk(fd, len, (uint8_t *) &gfxbuf[i]);
  }
}

void disp_init_triangle()
{
  static uint8_t img[SSD_LCD_HEIGHT*SSD_LCD_WIDTH/8] = {
    /* Row 0-7, col 0-127, bit0 = topmost row, bit7 = lowest row */
    0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
    /* Row 8-15, col 0-127 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 16-23, col 0-127 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 24-31, col 0-127 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  memcpy(gfxbuf,&img,sizeof(img)); /* dst, src, sz */
}

void disp_init_adafruit()
{
  static uint8_t adafruit[SSD_LCD_HEIGHT*SSD_LCD_WIDTH/8] = {
    /* Row 0-7, col 0-127 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x80, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 8-15, col 0-127 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xF8, 0xE0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00,
    0x80, 0xFF, 0xFF, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x8C, 0x8E, 0x84, 0x00, 0x00, 0x80, 0xF8,
    0xF8, 0xF8, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 16-23, col 0-127 */
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xE0, 0xE0, 0xC0, 0x80,
    0x00, 0xE0, 0xFC, 0xFE, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xC7, 0x01, 0x01,
    0x01, 0x01, 0x83, 0xFF, 0xFF, 0x00, 0x00, 0x7C, 0xFE, 0xC7, 0x01, 0x01, 0x01, 0x01, 0x83, 0xFF,
    0xFF, 0xFF, 0x00, 0x38, 0xFE, 0xC7, 0x83, 0x01, 0x01, 0x01, 0x83, 0xC7, 0xFF, 0xFF, 0x00, 0x00,
    0x01, 0xFF, 0xFF, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x07, 0x01, 0x01, 0x01, 0x00, 0x00, 0x7F, 0xFF,
    0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0xFF,
    0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 24-31, col 0-127 */
    0x03, 0x0F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xC7, 0xC7, 0x8F,
    0x8F, 0x9F, 0xBF, 0xFF, 0xFF, 0xC3, 0xC0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0xFC, 0xFC,
    0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xF8, 0xF8, 0xF0, 0xF0, 0xE0, 0xC0, 0x00, 0x01, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x01, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01,
    0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01, 0x03, 0x03, 0x00, 0x00,
    0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x03,
    0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  memcpy(gfxbuf,&adafruit,sizeof(adafruit)); /* dst, src, sz */

}

int width()
{
  return SSD_LCD_WIDTH;
}

int height()
{
  return SSD_LCD_HEIGHT;
}

/*
 * The most basic function, set a single pixel
 */
void ssd_plot(uint8_t x, uint8_t y, int color)
{
  if (x<SSD_LCD_WIDTH && y<SSD_LCD_HEIGHT) {
    int line = y >> 3;
    uint8_t byte = 1 << (y%8);
    if(color==0) {
      gfxbuf[line*SSD_LCD_WIDTH+x] &= ~byte; /* CLR */
    }
    if(color==1) {
      gfxbuf[line*SSD_LCD_WIDTH+x] |= byte; /* SET */
    }
    if(color==2) {
      gfxbuf[line*SSD_LCD_WIDTH+x] ^= byte; /* INV */
    }
  }
}

void ssd_coor(uint8_t row, uint8_t col)
{
#if 0
  const uint8_t cmd[] = {SSD_COL_PAGE_HI+5, SSD_COL_PAGE_LO+4*col,
			 SSD_};
    //Column Address
    sendCommand(0x15);             /* Set Column Address */
    sendCommand(0x08+(Column*4));  /* Start Column: Start from 8 */
    sendCommand(0x37);             /* End Column */
    // Row Address
    sendCommand(0x75);             /* Set Row Address */
    sendCommand(0x00+(Row*8));     /* Start Row*/
    sendCommand(0x07+(Row*8));     /* End Row*/
#endif
}

void ssd_putc(char ch)
{
  /* All characters 0-255 are implemented */
  if(ch<0 || ch>255) {
    ch=' '; //Space
  }
#if 0
  for(char i=0;i<8;i=i+2) {
    for(char j=0;j<8;j++) {
      /* Character is constructed two pixel at a time using vertical mode from the default 8x8 font */
      char c=0x00;
      char bit1= (seedfont[ch][i]   >> j) & 0x01;  
      char bit2= (seedfont[ch][i+1] >> j) & 0x01;
      /* Each bit is changed to a nibble */
      c|=(bit1)?grayH:0x00;
      c|=(bit2)?grayL:0x00;
      sendData(c);
    }
  }
#endif
}

void ssd_puts(const char *str)
{
  unsigned char i=0;
  while(str[i]) {
    ssd_putc(str[i]);     
    i++;
  }
}

void ssd130x_scroll_h(int fd, int dir)
{
  if(dir==0) {
    ssd_cmd1(fd, SSD_SCROLL_OFF);
  } else if (dir==1) {
    uint8_t hscroll[] = {SSD_SCROLL_H_R, 0, 0, 0, 0xF, 0, 0xFF};
    ssd_cmd_blk(fd, sizeof(hscroll), (uint8_t *)&hscroll);
    ssd_cmd1(fd, SSD_SCROLL_ON);
  } else if (dir==-11) {
    uint8_t hscroll[] = {SSD_SCROLL_H_L, 0, 0, 0, 0xF, 0, 0xFF};
    ssd_cmd_blk(fd, sizeof(hscroll), (uint8_t *)&hscroll);
    ssd_cmd1(fd, SSD_SCROLL_ON);
  }
}
 
int main (void)
{
  int disp = i2c_init_dev(RA_SSD1306);
  if(-1==disp) {
    puts("Failed to setup OLED display\n");
    exit(-1);
  }
  ssd130x_init(disp);
  disp_init_adafruit(disp);
  ssd_disp_update(disp);
  usleep(100000);
#if 0
  ssd_plot(0,0,COLOR_WHT);
  ssd_plot(SSD_LCD_WIDTH-1,SSD_LCD_HEIGHT-1,COLOR_INV);
  ssd_plot(127,0,COLOR_INV);
  ssd_plot(0,31,COLOR_INV);
#endif
  
  ssd_disp_update(disp);
  exit(0);
}

/* ************************************************************
 * End of file.
 * ************************************************************/
