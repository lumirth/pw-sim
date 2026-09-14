#include "browser.h"
#include "graphics.h"
#include "types.h"
#include "raster_column.h"
#include "iodefine.h"
#include "project.h"
#include "display.h"
#include <machine.h>
#include "eeprom.h"
#include "common.h"
#include "scratch.h"

extern const u8 g_lcdInitScript[];

/* OR shifted two-plane columns into the destination raster. */
void RasterOr(u8 width, u8 height, const u8 *source, u8 x, u8 y,
              u8 *destination, u8 destinationWidth, u8 destinationHeight)
{
  u8 band;
  u8 column;
  u8 first;
  u8 second;
  u8 pixel;
  u8 *row;

  height /= 8;
  row = destination;
  row += ((y / 8) * destinationWidth + x) * 2;
  for (band = 0; band < height; ++band) {
    for (column = 0; column < width; ++column) {
      first = *(source + column * 2);
      second = *(source + column * 2 + 1);
      pixel = *(row + column * 2);
      pixel = pixel | (first << (y & 7));
      *(row + column * 2) = pixel;
      pixel = *(row + column * 2 + 1);
      pixel = pixel | (second << (y & 7));
      *(row + column * 2 + 1) = pixel;
      pixel = *(row + (destinationWidth + column) * 2);
      pixel = pixel | (first >> (8 - (y & 7)));
      *(row + (destinationWidth + column) * 2) = pixel;
      pixel = *(row + (destinationWidth + column) * 2 + 1);
      pixel = pixel | (second >> (8 - (y & 7)));
      *(row + (destinationWidth + column) * 2 + 1) = pixel;
    }
    row += destinationWidth * 2;
    source += width * 2;
  }
}

/* Send one byte over the display SSU. Chip select is active low on PDR1 bit 0.
 * Preserve the command/data phase selected by the caller. */
void DisplaySend(u8 txByte)
{
  IO.PDR1.BIT.B0 = 0;              /* assert display select */
  while (SSU.SSSR.BIT.TDRE == 0) { /* wait transmit-empty */
  }
  BrowserLcdByte(txByte);
  while (SSU.SSSR.BIT.TEND == 0) { /* wait transfer-end */
  }
  IO.PDR1.BIT.B0 = 1; /* release display select */
}

/* Repeat the calibrated low-power delay unit 100 times. */
void DisplayWait(void)
{
  u16 remaining;

  remaining = 100;
  do {
    LowClockDelay();
  } while (--remaining != 0);
}

#define PW_DISPLAY_SSU_ENABLE_VALUE 0x80
#define PW_DISPLAY_COMMAND_E1 0xe1
#define PW_DISPLAY_CMD_EXIT_POWER_SAVE 0xe1
#define PW_DISPLAY_CMD_ENTER_POWER_SAVE 0xa9
#define PW_DISPLAY_STREAM_END_DIRECTIVE 0xfe
#define PW_DISPLAY_STREAM_REPEAT_DIRECTIVE 0xfd
#define PW_DISPLAY_INIT_TRAILER_A6 0xa6
#define PW_DISPLAY_INIT_TRAILER_AF 0xaf
#define PW_DISPLAY_COMMAND_81 0x81
#define PW_DISPLAY_FULL_RASTER_BIT_SPAN 0x40
#define PW_DISPLAY_RASTER_BANK_1 1
#define PW_DISPLAY_RASTER_BANK_0 0
#define PW_LCD_SCRIPT_LENGTH 0x40

/* Initialize from the mirrored EEPROM script, using resident defaults when
 * its first byte is 0x00 or 0xff. The script's 0xfd directive delays execution;
 * 0xfe ends it. */
void DisplayInit(void)
{
  u8 *script;
  u16 i;
  u16 commandCount = 0;

  ScratchReset();
  script = ScratchAlloc(PW_LCD_SCRIPT_LENGTH);
  EepromMirrorRead(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP, script,
                   PW_LCD_SCRIPT_LENGTH);
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(PW_DISPLAY_COMMAND_E1);
  if ((*script == 0) || (*script == 0xff)) {
    script = (u8 *)g_lcdInitScript;
  }
  g_state.baseContrast = *script++;
  for (;;) {
    if (++commandCount > 64) __builtin_trap();
    if (*script == PW_DISPLAY_STREAM_END_DIRECTIVE) {
      break;
    }
    if (*script == PW_DISPLAY_STREAM_REPEAT_DIRECTIVE) {
      script++;
      for (i = 0; i < *script; i++) {
        DisplayWait();
      }
      script++;
    }
    DisplaySend(*script++);
  }
  DisplaySend(PW_DISPLAY_INIT_TRAILER_A6);
  DisplaySetContrast(g_state.save.contrast);
  g_displayBank.bytes.index = PW_DISPLAY_RASTER_BANK_1;
  DisplayClear(PW_DISPLAY_FULL_RASTER_BIT_SPAN);
  g_displayBank.bytes.index = PW_DISPLAY_RASTER_BANK_0;
  DisplayClear(PW_DISPLAY_FULL_RASTER_BIT_SPAN);
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(PW_DISPLAY_INIT_TRAILER_AF);
}

/* Send contrast command 0x81 with the script base plus the requested delta,
 * then wait for transfer completion and release chip select. */
void DisplaySetContrast(u8 contrastDelta)
{
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(PW_DISPLAY_COMMAND_81);
  DisplaySend((g_state.baseContrast + contrastDelta));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

#define PW_DISPLAY_COL_MSB_COMMAND 0x10
#define PW_DISPLAY_PAGE_COMMAND_BASE 0xb0
#define PW_DISPLAY_START_LINE_COMMAND 0x40
#define PW_DISPLAY_PAGE_COUNT 8
#define PW_DISPLAY_COL_MSB_DIVISOR 0x10
#define PW_DISPLAY_COL_LSB_MASK 0x0f
#define PW_DISPLAY_COL_MSB_MASK 0x07
#define PW_DISPLAY_EDGE_INTERIOR_COUNT 0xbc
#define PW_DISPLAY_EDGE_FILL_BIT0 1
#define PW_DISPLAY_EDGE_FILL_BIT7 0x80
#define PW_DISPLAY_PAGE_CMD_4 0xb4
#define PW_DISPLAY_PAGE_CMD_5 0xb5
#define PW_DISPLAY_PAGE_CMD_6 0xb6
#define PW_DISPLAY_PAGE_CMD_7 0xb7
#define PW_DISPLAY_LAST_COL_MSB 0x15
#define PW_DISPLAY_LAST_COL_LSB 0x0f
#define PW_DISPLAY_GLYPH_WIDTH 4
#define PW_DISPLAY_GLYPH_CELL_BYTES 3
#define PW_DISPLAY_RULE_PAGE_OFFSET 6
#define PW_DISPLAY_RULE_BYTE_COUNT 0xc0

/* Command-phase column and page address. Callers already hold chip select.
 * Column is split into SSD1854 0x10|high and 0x00|low bytes; page is 0xB0 plus
 * the active bank's 8-row window plus y. y > 7 parks in sleep rather than
 * emitting a bad page command. */
void DisplayAddress(u8 x, u8 y)
{
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((((x / PW_DISPLAY_COL_MSB_DIVISOR) & PW_DISPLAY_COL_MSB_MASK) +
               PW_DISPLAY_COL_MSB_COMMAND));
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((x & PW_DISPLAY_COL_LSB_MASK));
  if (y > (PW_DISPLAY_PAGE_COUNT - 1)) {
    sleep();
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT + y +
               PW_DISPLAY_PAGE_COMMAND_BASE));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
}

/* Show the current raster bank and switch drawing to the other bank. */
void DisplayToggleBank(void)
{
  u8 *bank;

  bank = &g_displayBank.bytes.index;
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_START_LINE_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((*bank * PW_DISPLAY_START_LINE_COMMAND));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  *bank ^= PW_DISPLAY_RASTER_BANK_1;
}

/* Same start-line transaction as the toggle, but the displayed bank is the
 * argument (0 or 1). Drawing then targets the other bank. */
void DisplaySelectBank(u8 bank)
{
  if (bank > 1) {
    return;
  }
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_START_LINE_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((bank * PW_DISPLAY_START_LINE_COMMAND));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  g_displayBank.bytes.index = bank ^ PW_DISPLAY_RASTER_BANK_1;
}

#define PW_DISPLAY_RASTER_COLUMN_COUNT 0x60
#define PW_DISPLAY_TWO_PLANE_OFF 0
#define PW_DISPLAY_TWO_PLANE_LOWER 1
#define PW_DISPLAY_TWO_PLANE_UPPER 2
#define PW_DISPLAY_TWO_PLANE_BOTH 3

/* Fill every column of the active drawing bank with a two-plane pattern
 * (0/1/2/3). Column address is the left edge; each page reuses the address-set
 * idiom with x = 0. */
void DisplayFill(u8 pixelValue)
{
  u8 band;
  u8 y;
  u8 remaining;

  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  band = 0;
  do {
    y = band;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(0);
    if (y > (PW_DISPLAY_PAGE_COUNT - 1)) {
      sleep();
    }
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT + y +
                 PW_DISPLAY_PAGE_COMMAND_BASE));
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B1 = 1;
    remaining = PW_DISPLAY_RASTER_COLUMN_COUNT;
    while (remaining != 0) {
      switch (pixelValue) {
      case PW_DISPLAY_TWO_PLANE_OFF:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        break;
      case PW_DISPLAY_TWO_PLANE_LOWER:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        break;
      case PW_DISPLAY_TWO_PLANE_UPPER:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        break;
      case PW_DISPLAY_TWO_PLANE_BOTH:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        break;
      }
      remaining--;
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    band++;
  } while (band < PW_DISPLAY_PAGE_COUNT);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Wait for SSU readiness before evaluating each column expression. */
#define PW_LCD_WRITE(value)                                                    \
  do {                                                                         \
    while (SSU.SSSR.BIT.TDRE == 0) {                                           \
    }                                                                          \
    BrowserLcdByte((value));                                                       \
  } while (0)
#define PW_LCD_END()                                                           \
  do {                                                                         \
    while (SSU.SSSR.BIT.TEND == 0) {                                           \
    }                                                                          \
  } while (0)

#pragma inline(DisplaySetPosition)
static void DisplaySetPosition(u8 x, u8 page)
{
  IO.PDR1.BIT.B1 = 0;
  PW_LCD_WRITE(((x / 16) & 7) + 0x10);
  PW_LCD_WRITE(x & 15);
  if (page > 7) {
    sleep();
  }
  PW_LCD_WRITE(g_displayBank.bytes.index * 8 + page + 0xb0);
  PW_LCD_END();
}

/* Fill a rectangle in the active drawing bank. Pages run from y>>3 to
 * (y+height)/8; each page is addressed at (x, page) and then width two-plane
 * columns are written. */
void DisplayFillRect(u8 x, u8 y, u8 width, u8 height, u8 fillPattern)
{
  u8 page;
  u8 col;
  s16 endCol;
  s16 endPage;

  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  page = (y >> 3);
  endCol = x + width;
  endPage = (y + height) / 8;
  while (page < endPage) {
    DisplaySetPosition(x, page);
    IO.PDR1.BIT.B1 = 1;
    col = x;
    while (col < endCol) {
      switch (fillPattern) {
      case PW_DISPLAY_TWO_PLANE_OFF:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        break;
      case PW_DISPLAY_TWO_PLANE_LOWER:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        break;
      case PW_DISPLAY_TWO_PLANE_UPPER:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0);
        break;
      case PW_DISPLAY_TWO_PLANE_BOTH:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(0xff);
        break;
      }
      col++;
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    page++;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Wake the panel with command 0xE1 while both control lines are low, then
 * release chip select after the transfer. */
void DisplayExitPowerSave(void)
{
  SSU.SSER.BYTE = 0x80;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(PW_DISPLAY_CMD_EXIT_POWER_SAVE);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Same transaction with the power-save command. */
void DisplayEnterPowerSave(void)
{
  SSU.SSER.BYTE = 0x80;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(PW_DISPLAY_CMD_ENTER_POWER_SAVE);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Clear floor(span/8) pages of the drawing bank, writing 96 zero pairs per
 * page. Callers pass 0x40 (eight pages). */
void DisplayClear(u8 rasterBitSpan)
{
  u8 band;
  u8 y;
  u8 remaining;
  s16 endBand;

  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  band = 0;
  endBand = rasterBitSpan >> 3;
  while (band < endBand) {
    y = band;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(0);
    if (y > (PW_DISPLAY_PAGE_COUNT - 1)) {
      sleep();
    }
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT + y +
                 PW_DISPLAY_PAGE_COMMAND_BASE));
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B1 = 1;
    remaining = PW_DISPLAY_RASTER_COLUMN_COUNT;
    while (remaining != 0) {
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      BrowserLcdByte(0);
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      BrowserLcdByte(0);
      remaining--;
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    band++;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Blit a two-plane raster into the drawing bank. A y that is not on a page edge
 * left-shifts the current page and right-shifts the previous page so the bits
 * straddle the SSD1854 8-row windows. */
void DisplayBlit(u8 x, u8 y, u8 width, u8 height, const u8 *raster)
{
  u8 yBit;
  u8 startPage;
  u8 endPage;
  s16 lastPage;
  s16 page;
  s16 col;
  const RasterColumn *columns;
  s16 bits;

  yBit = (y & 7);
  startPage = (s8)y / PW_DISPLAY_PAGE_COUNT;
  endPage = (height + (s8)y + 7) / PW_DISPLAY_PAGE_COUNT;
  /* Walk the two plane bytes as one raster column, including prior-row reads.
   */
  columns = (const RasterColumn *)raster;
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  page = startPage;
  lastPage = endPage - 1;
  for (; page < endPage; page++) {
    u8 row;
    u8 left;

    row = page;
    left = x;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte((((left / PW_DISPLAY_COL_MSB_DIVISOR) & PW_DISPLAY_COL_MSB_MASK) +
         PW_DISPLAY_COL_MSB_COMMAND));
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte((left & PW_DISPLAY_COL_LSB_MASK));
    if (row > (PW_DISPLAY_PAGE_COUNT - 1)) {
      sleep();
    }
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT + row +
                 PW_DISPLAY_PAGE_COMMAND_BASE));
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B1 = 1;
    for (col = 0; col < width; col++, columns++) {
      const u8 *secondPlane;

      secondPlane = &columns[0][1];
      if (yBit == 0) {
        bits = columns[0][0] << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
        bits = *secondPlane << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
      } else if (page == lastPage) {
        bits = (columns - width)[0][0] >> (PW_DISPLAY_PAGE_COUNT - yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
        bits = (columns - width)[0][1] >> (PW_DISPLAY_PAGE_COUNT - yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
      } else if (page != startPage) {
        bits = ((columns - width)[0][0] >> (PW_DISPLAY_PAGE_COUNT - yBit)) |
               (columns[0][0] << yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
        bits = ((columns - width)[0][1] >> (PW_DISPLAY_PAGE_COUNT - yBit)) |
               (*secondPlane << yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
      } else {
        bits = columns[0][0] << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
        bits = *secondPlane << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        BrowserLcdByte(bits);
      }
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Same two-plane page walk as blit_clipped, but x and y are signed so a span
 * can start off the left or top edge. Negative pages skip the transfer and
 * advance the raster; negative x addresses column 0 and drops columns until x
 * is on-screen. */
typedef struct {
  s16 left;
  s16 end;
} DisplaySpanInterval;

void DisplayWriteSpan(s8 x, s8 y, uint columnCount, uint rowCount,
                      RasterColumn *raster)
{
  u8 yBit;
  u8 bits;
  s16 startPage;
  uint endRow;
  s16 endPage;
  s16 lastPage;
  s16 page;
  s16 col;
  DisplaySpanInterval bounds[1];
  RasterColumn *columns;

  yBit = (y & 7);
  startPage = y / PW_DISPLAY_PAGE_COUNT;
  /* Preserve the unsigned 16-bit rounded endpoint before deriving its page. */
  endRow = y + rowCount + 7;
  endPage = (endRow >> 3);
  columns = raster;
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  page = startPage;
  bounds[0].left = x;
  bounds[0].end = x + columnCount;
  lastPage = endPage - 1;
  for (; page < endPage; page++) {
    if (page >= PW_DISPLAY_PAGE_COUNT) {
      break;
    }
    if (page < 0) {
      columns += columnCount;
    } else {
      if (x < 0) {
        DisplaySetPosition(0, page);
      } else {
        DisplaySetPosition(x, page);
      }
      IO.PDR1.BIT.B1 = 1;
      for (col = bounds[0].left; col < bounds[0].end; col++, columns++) {
        if (col >= 0) {
          u8 *secondPlane;

          secondPlane = &columns[0][1];

          if (yBit == 0) {
            bits = (columns[0][0] << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
            bits = (*secondPlane << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
          } else if (page == lastPage) {
            bits = ((columns - columnCount)[0][0] >>
                    (PW_DISPLAY_PAGE_COUNT - yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
            bits = ((columns - columnCount)[0][1] >>
                    (PW_DISPLAY_PAGE_COUNT - yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
          } else if (page != startPage) {

            bits = (((columns - columnCount)[0][1] >>
                     (PW_DISPLAY_PAGE_COUNT - yBit)) |
                    (columns[0][0] << yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
            bits = ((*((u8 *)columns - columnCount * 2) >>
                     (PW_DISPLAY_PAGE_COUNT - yBit)) |
                    (*secondPlane << yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
          } else {
            bits = (columns[0][0] << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
            bits = (*secondPlane << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            BrowserLcdByte(bits);
          }
        }
      }
      while (SSU.SSSR.BIT.TEND == 0) {
      }
    }
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Stream the blank, digit and letter cells. */
void DisplayText(u8 x, u8 y, const char *glyphStream)
{
  u8 ch;
  u8 i;
  u8 cell;
  u8 page;

  SSU.SSER.BYTE = 0x80;
  IO.PDR1.BIT.B0 = 0;
  page = y / 8;
  DisplaySetPosition(x, page);
  IO.PDR1.BIT.B1 = 1;
  for (; *glyphStream != 0;) {
    ch = *glyphStream++;
    if (ch == ' ') {
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      PW_LCD_WRITE(0);
      x += 4;
    } else {
      if (ch > '9') {
        cell = ch - '7';
        IO.PDR1.BIT.B0 = 0;
        i = 0;
        do {
          PW_LCD_WRITE(g_alphanumericFont[cell * 3 + i]);
          PW_LCD_WRITE(g_alphanumericFont[cell * 3 + i]);
        } while (++i < 3);
        PW_LCD_WRITE(0);
        PW_LCD_WRITE(0);
        PW_LCD_END();
      } else {
        cell = ch - '0';
        IO.PDR1.BIT.B0 = 0;
        i = 0;
        do {
          PW_LCD_WRITE(g_alphanumericFont[cell * 3 + i]);
          PW_LCD_WRITE(g_alphanumericFont[cell * 3 + i]);
        } while (++i < 3);
        PW_LCD_WRITE(0);
        PW_LCD_WRITE(0);
        PW_LCD_END();
      }
      x += 4;
    }
  }
  PW_LCD_END();
  IO.PDR1.BIT.B0 = 1;
}

#undef PW_LCD_WRITE
#undef PW_LCD_END

/* Draw the 4/5-page frame edge: page 4 is 0xff ends around 188 bytes of 0x01;
 * page 5 is only the first and last column 0xff pairs. */
void DisplayFrame45(void)
{
  u8 *bank;
  u8 remaining;

  bank = &g_displayBank.bytes.index;
  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((*bank * PW_DISPLAY_PAGE_COUNT + PW_DISPLAY_PAGE_CMD_4));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  remaining = PW_DISPLAY_EDGE_INTERIOR_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(PW_DISPLAY_EDGE_FILL_BIT0);
    remaining--;
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((*bank * PW_DISPLAY_PAGE_COUNT + PW_DISPLAY_PAGE_CMD_5));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_LAST_COL_MSB);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_LAST_COL_LSB);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((*bank * PW_DISPLAY_PAGE_COUNT + PW_DISPLAY_PAGE_CMD_5));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Same edge on pages 6 and 7: page 6 interiors are bit 0, page 7 interiors are
 * bit 7. */
void DisplayFrame67(void)
{
  u8 remaining;

  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT +
               PW_DISPLAY_PAGE_CMD_6));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  remaining = PW_DISPLAY_EDGE_INTERIOR_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(PW_DISPLAY_EDGE_FILL_BIT0);
    remaining--;
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT +
               PW_DISPLAY_PAGE_CMD_7));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  remaining = PW_DISPLAY_EDGE_INTERIOR_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(PW_DISPLAY_EDGE_FILL_BIT7);
    remaining--;
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0xff);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Address page 6 of the drawing bank and write 0xC0 bytes of 0x01, a one-pixel
 * horizontal rule across both planes. */
void DisplayRule(void)
{
  u8 remaining;

  SSU.SSER.BYTE = PW_DISPLAY_SSU_ENABLE_VALUE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(PW_DISPLAY_COL_MSB_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte(0);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  BrowserLcdByte((g_displayBank.bytes.index * PW_DISPLAY_PAGE_COUNT +
               PW_DISPLAY_PAGE_COMMAND_BASE + PW_DISPLAY_RULE_PAGE_OFFSET));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  remaining = PW_DISPLAY_RULE_BYTE_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    BrowserLcdByte(1);
    remaining--;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Default script used when the EEPROM base-contrast byte is 0x00 or 0xff. */
const u8 g_lcdInitScript[44] = {
    0x14, 0x48, 0x40, 0xa0, 0xc0, 0x44, 0x20, 0xab, 0x67, 0x25, 0x81,
    0x18, 0x52, 0x95, 0x88, 0x00, 0x89, 0x00, 0x8a, 0x55, 0x8b, 0x55,
    0x8c, 0x77, 0x8d, 0x77, 0x8e, 0x99, 0x8f, 0x99, 0x4c, 0x04, 0xf1,
    0x00, 0xf7, 0x02, 0xf6, 0x0a, 0x2f, 0xfd, 0x01, 0x40, 0x00, 0xfe};
