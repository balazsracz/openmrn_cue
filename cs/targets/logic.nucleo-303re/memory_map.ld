MEMORY
{
  FLASH      (rx)  : ORIGIN = 0x08000000, LENGTH = 384K
  EEPROMEMU  (r)   : ORIGIN = 0x08060000, LENGTH = 120K
  BOOTLOADER (rx)  : ORIGIN = 0x0807E000, LENGTH = 8K
  RAM        (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

__flash_start = ORIGIN(FLASH);
__flash_end = ORIGIN(FLASH) + LENGTH(FLASH);
__eeprom_start = ORIGIN(EEPROMEMU);
__eeprom_end = ORIGIN(EEPROMEMU) + LENGTH(EEPROMEMU);
__bootloader_start = ORIGIN(BOOTLOADER);
__app_header_offset = 0x270;
__bootloader_magic_ptr = ORIGIN(RAM);

ASSERT(__flash_end <= __eeprom_start, "FLASH overlaps EEPROMEMU.");
ASSERT(__eeprom_end <= __bootloader_start, "EEPROMEMU overlaps BOOTLOADER.");
