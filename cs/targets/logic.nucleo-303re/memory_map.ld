MEMORY
{
  FLASH      (rx)  : ORIGIN = 0x08000000, LENGTH = 384K
  SPIFFS     (r)   : ORIGIN = 0x08060000, LENGTH = 120K
  BOOTLOADER (rx)  : ORIGIN = 0x0807E000, LENGTH = 8K
  RAM        (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

__flash_start = ORIGIN(FLASH);
__flash_end = ORIGIN(FLASH) + LENGTH(FLASH);
__flash_fs_start = ORIGIN(SPIFFS);
__flash_fs_end = ORIGIN(SPIFFS) + LENGTH(SPIFFS);
__bootloader_start = ORIGIN(BOOTLOADER);
__app_header_offset = 0x270;
__bootloader_magic_ptr = ORIGIN(RAM);

ASSERT(__flash_end <= __flash_fs_start, "FLASH overlaps SPIFFS.");
ASSERT(__flash_fs_end <= __bootloader_start, "SPIFFS overlaps BOOTLOADER.");
