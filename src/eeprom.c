#include "project.h"
#include "eeprom.h"
/* Write the payload to primary then backup. Each copy ends with a checksum byte
 * initialized to 1 and incremented by each payload byte. */
void EepromMirrorWrite(u16 primary, u16 backup, u8 *buffer, u16 length)
{
  u8 checksum;
  u8 i;

  checksum = 1;
  EepromWrite(primary, buffer, length);
  i = 0;
  while (i < length) {
    checksum = (checksum + buffer[i]);
    i++;
  }
  EepromWriteByte((primary + length), checksum);
  EepromWrite(backup, buffer, length);
  EepromWriteByte((backup + length), checksum);
}

/* Load and checksum backup then primary, repairing from a valid copy. Fill with
 * 0xFF when both fail; prefer primary when both are valid but their sums
 * differ. */
void EepromMirrorRead(u16 primary, u16 backup, u8 *buffer, u16 length)
{
  u8 checksum[2];
  u8 i;
  u8 erased;
  u16 primaryCsAddr;
  u16 backupCsAddr;
  u8 valid;

  EepromRead(backup, buffer, length);
  checksum[1] = 1;
  i = 0;
  while (i < length) {
    checksum[1] = (checksum[1] + buffer[i]);
    i++;
  }
  EepromRead(primary, buffer, length);
  checksum[0] = 1;
  i = 0;
  while (i < length) {
    checksum[0] = (checksum[0] + buffer[i]);
    i++;
  }
  valid = 0;
  primaryCsAddr = (primary + length);
  if (checksum[0] == EepromReadByte(primaryCsAddr)) {
    valid |= 1;
  }
  backupCsAddr = (backup + length);
  if (checksum[1] == EepromReadByte(backupCsAddr)) {
    valid |= 2;
  }
  switch (valid) {
  case 0:
    i = 0;
    erased = 0xff;
    while (i < length) {
      buffer[i] = erased;
      i++;
    }
    EepromWrite(primary, buffer, length);
    EepromWriteByte(primaryCsAddr, 0xff);
    EepromWrite(backup, buffer, length);
    EepromWriteByte(backupCsAddr, 0xff);
    break;
  case 1:
    EepromRead(primary, buffer, length);
    EepromWrite(backup, buffer, length);
    EepromWriteByte(backupCsAddr, checksum[0]);
    break;
  case 2:
    EepromRead(backup, buffer, length);
    EepromWrite(primary, buffer, length);
    EepromWriteByte(primaryCsAddr, checksum[1]);
    break;
  case 3:
    if (checksum[0] != checksum[1]) {
      EepromWrite(backup, buffer, length);
      EepromWriteByte(backupCsAddr, checksum[0]);
    }
    break;
  }
}

