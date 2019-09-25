#include "video.h"
#include "src/interrupts.h"

enum LCDC {
  BGDisplay = 1,
  OBJDisplayEnable = 2,
  OBJSize = 4,
  BGTileMapDisplaySelect = 8,
  BGWindowTileDataSelect = 16,
  WindowDisplayEnable = 32,
  WindowTileMapDisplaySelect = 64,
  LCDDisplayEnable = 128
};

enum LCDCStatus {
  LCDMode = 3,
  CoincidenceFlag = 4,
  HBlankInterruptEnabled = 8,
  VBlankInterruptEnabled = 16,
  OAMInterruptEnabled = 32,
  CoincidenceInterruptEnabled = 64
};

bool get_bg_data_select(lcd_regs_t *regs) {
  return (regs->lcd_control & BGWindowTileDataSelect) != 0;
}

gb_address_t get_bg_data_start_offset(lcd_regs_t *regs) {
  return (uint16_t) (
      get_bg_data_select(regs) ? 0 : 0x800);
}

bool window_enabled(lcd_regs_t *regs) {
  return regs->lcd_control & WindowDisplayEnable;
}

bool get_bg_display_select(lcd_regs_t *regs) {
  return (regs->lcd_control & BGTileMapDisplaySelect) != 0;
}

gb_address_t get_bg_display_start_offset(lcd_regs_t *regs) {
  return (uint16_t) (get_bg_display_select(regs) ? 0x1C00 : 0x1800);
}

gb_address_t get_window_display_start_offset(lcd_regs_t *regs) {
  return (uint16_t) (
      regs->lcd_control & WindowTileMapDisplaySelect ? 0x1C00 : 0x1800);
}

gb_address_t get_sprite_data_start_offset(lcd_regs_t *regs) {
  return (uint16_t) 0;
}

bool obj_height_is_16_bit(lcd_regs_t *regs) {
  return regs->lcd_control & OBJSize;
}

bool lcd_enabled(lcd_regs_t *regs) {
  return regs->lcd_control & LCDDisplayEnable;
}

void set_mode(cpu_t *cpu, lcd_regs_t *regs, uint8_t mode) {
  regs->lcd_status &= ~LCDMode;
  regs->lcd_status |= mode;

  switch (mode) {
    case 0:
      if (regs->lcd_status & HBlankInterruptEnabled) {
        raise_interrupt(cpu, INT_LCD_STAT);
      }
      break;
    case 1:
      if (regs->lcd_status & VBlankInterruptEnabled) {
        raise_interrupt(cpu, INT_LCD_STAT);
      }
      break;
    case 2:
      if (regs->lcd_status & OAMInterruptEnabled) {
        raise_interrupt(cpu, INT_LCD_STAT);
      }
      break;
    default:
      break;
  }
}

int get_mode(lcd_regs_t *regs) {
  return regs->lcd_status & LCDMode;
}

bool obj_enabled(lcd_regs_t *regs) {
  return regs->lcd_control & OBJDisplayEnable;
}

bool check_coincidence(lcd_regs_t *regs) {
  if (regs->lcdc_y == regs->ly_compare)
    regs->lcd_status |= CoincidenceFlag;
  else
    regs->lcd_status &= ~CoincidenceFlag;

  if (!(regs->lcd_status & CoincidenceInterruptEnabled))
    return false;

  return regs->lcd_status & CoincidenceFlag;
}
