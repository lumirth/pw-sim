#ifndef PW_EEPROM_H
#define PW_EEPROM_H

#include "types.h"
#include "data.h"

void EepromMirrorWrite(u16 primary, u16 backup, u8 *buffer, u16 length);
void EepromMirrorRead(u16 primary, u16 backup, u8 *buffer, u16 length);
void EepromWrite(u16 address, void *source, u16 length);
void EepromRead(u16 address, void *destination, u16 length);
u8 EepromReadByte(u16 address);
void EepromFillPage(u16 address, u8 byteValue);
void EepromFill(u16 address, u16 count, u8 value);
void EepromWritePage(uint address, u8 *source);
void EepromConfigure(void);
void EepromIdle(void);
u8 EepromReceive(void);
void EepromWriteByte(u16 address, u8 value);
u8 EepromSelfTest(uint value);
void StatusSetReceived(DeviceStatus *status, u8 eventId);
u8 StatusHasReceived(DeviceStatus *status, u8 eventId);

#endif
