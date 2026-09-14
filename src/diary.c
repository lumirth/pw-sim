#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "diary.h"
#include "eeprom.h"

#define PW_DIARY_ACTION_PERIODIC_STEPS 0x1b
#define PW_DIARY_ACTION_WALK_STARTED 0x19
#define PW_DIARY_ACTION_PREFILLED_MAX 0x0a
#define PW_DIARY_ACTION_BATTLE_FLED 0x0f
#define PW_DIARY_ACTION_BATTLE_DEFEATED 0x10
#define PW_DIARY_RING_SLOTS 23

/* Build one device-owned DiaryEntry and append it to the 23-slot ring. */
void DiaryAppend(Course *course, DiaryEntry *diary, u8 actionId,
                 u8 bonusCourseFlag, u8 encounterSelector, u16 itemNumber)
{
  enum { diaryEep = EEPROM_WALK + offsetof(WalkData, diary) };
  DiaryEntry *eepSlot;

  eepSlot = (DiaryEntry *)diaryEep + g_state.save.diaryIndex;
  {
    u8 existing;
    existing = EepromReadByte((u16)&eepSlot->action);
    if ((existing != 0) && (actionId == PW_DIARY_ACTION_PERIODIC_STEPS)) {
      return;
    }
    if (existing == PW_DIARY_ACTION_WALK_STARTED) {
      g_state.save.diaryIndex =
          ((g_state.save.diaryIndex + 1) % PW_DIARY_RING_SLOTS);
      eepSlot = (DiaryEntry *)diaryEep + g_state.save.diaryIndex;
    }
  }
  if (actionId > PW_DIARY_ACTION_PREFILLED_MAX) {
    u8 *p;
    u16 n;
    p = (u8 *)diary;
    n = 0;
    do {
      *p = 0;
      p++;
      n++;
    } while (n < sizeof(DiaryEntry));
  }
  diary->action = actionId;
  diary->itemId = itemNumber;
  diary->rtcSeconds = g_state.save.rtcSeconds;
  diary->ownHourSteps = g_state.hourSteps;
  diary->ownDaySteps = g_state.dailySteps;
  diary->pokemonId = course->pokemon.id;
  {
    u16 n;
    n = 0;
    do {
      diary->nickname[n] = course->nickname[n];
      n++;
    } while ((s16)n < (s16)sizeof(course->nickname));
  }
  diary->friendship = course->friendship;
  diary->ownForm = course->pokemon.form;
  diary->ownSex = course->pokemon.sex;
  diary->ownShiny = course->pokemon.shiny;
  if (bonusCourseFlag == 0) {
    u16 n;
    diary->journalTheme = course->journalTheme;
    n = 0;
    for (;;) {
      diary->courseNameText[n] = course->courseNameText[n];
      n++;
      if ((s16)n >= (s16)sizeof(course->courseNameText)) {
        break;
      }
    }
  } else {
    diary->journalTheme = EepromReadByte(PW_EEPROM_MEMBER_ADDRESS(
        EEPROM_BONUS_COURSE, BonusResources, values.journalTheme));
    EepromRead(
        (u16)((BonusResources *)EEPROM_BONUS_COURSE)->values.courseNameText,
        diary->courseNameText, sizeof(diary->courseNameText));
  }
  switch (encounterSelector) {
  case 1:
  case 2:
  case 3:
    diary->encounterId = course->encounters[encounterSelector - 1].id;
    diary->peerForm = course->encounters[encounterSelector - 1].form;
    diary->peerSex = course->encounters[encounterSelector - 1].sex;
    break;
  case 4: {
    u8 appearance;

    /* Flee/defeat uses the bonus-course species; other actions use the event
     * species. All paths read appearance from bonus-region offset 0x0d. */
    if ((actionId == PW_DIARY_ACTION_BATTLE_FLED) ||
        (actionId == PW_DIARY_ACTION_BATTLE_DEFEATED)) {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                          values.pokemon.id),
                 &diary->encounterId, 2);
    } else {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon,
                                          pokemon.id),
                 &diary->encounterId, 2);
    }
    appearance = EepromReadByte((u16)((u8 *)EEPROM_BONUS_COURSE + 0x0d));
    diary->peerForm = (appearance & 0x1f);
    diary->peerSex = ((appearance / 32) & 3);
    break;
  }
  default:
    break;
  }
  EepromWrite((u16)eepSlot, diary, sizeof(DiaryEntry));
  g_state.save.diaryIndex =
      ((g_state.save.diaryIndex + 1) % PW_DIARY_RING_SLOTS);
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
}
