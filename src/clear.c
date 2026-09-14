#include "types.h"
#include <stddef.h>
#include "project.h"
#include "eeprom.h"
#include "clear.h"

/* Clear the Pokemon, item and friend-item slots. */

void ClearReturnInventory(void)
{
  WalkData *data;

  data = (WalkData *)EEPROM_WALK;
  EepromFill((u16)&data->pokemon,
             sizeof(data->pokemon) + sizeof(data->items) +
                 sizeof(data->friendItems),
             0);
}

/* Clear the action ID in every diary slot. */
void ClearDiaryActions(void)
{

  enum { diaryEep = EEPROM_WALK + offsetof(WalkData, diary) };
  DiaryEntry *entry;
  u8 n;

  entry = (DiaryEntry *)diaryEep;
  n = 24;
  do {
    EepromWriteByte((u16)&entry->action, 0);
    entry++;
  } while (--n != 0);
}

void ClearWeeklySteps(void)
{
  WalkData *data;

  data = (WalkData *)EEPROM_WALK;
  EepromFill((u16)&data->dailySteps, sizeof(data->dailySteps), 0);
}
