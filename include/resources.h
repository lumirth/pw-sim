#ifndef PW_RESOURCES_H
#define PW_RESOURCES_H

#include "records.h"

/* Each 8-row page stores two bit planes per column. These declarations
 * describe serial EEPROM offsets; they do not map EEPROM onto the CPU bus. */
#define RASTER_BYTES(width, height) ((width) * (height) / 4)
#define TEXT_RASTER_BYTES 320
#define MESSAGE_RASTER_BYTES 384
#define MESSAGE_COUNT 69
#define POKEMON_FRAME_BYTES RASTER_BYTES(32, 24)
#define POKEMON_ANIMATION_BYTES (2 * POKEMON_FRAME_BYTES)
#define POKEMON_LARGE_FRAME_BYTES RASTER_BYTES(64, 48)
#define POKEMON_LARGE_ANIMATION_BYTES (2 * POKEMON_LARGE_FRAME_BYTES)

typedef struct {
  u8 digits[13 * RASTER_BYTES(8, 16)];
  u8 watts[RASTER_BYTES(16, 16)];
  u8 ball[RASTER_BYTES(8, 8)];
  u8 eventBall[RASTER_BYTES(8, 8)];
  u8 ballMask[8];
  u8 treasure[RASTER_BYTES(8, 8)];
  u8 eventTreasure[RASTER_BYTES(8, 8)];
  u8 eventMap[RASTER_BYTES(8, 8)];
  u8 stamps[4 * RASTER_BYTES(8, 8)];
  u8 arrows[3 * 4 * RASTER_BYTES(8, 8)];
  u8 largeArrows[2 * 2 * RASTER_BYTES(8, 16)];
  u8 next[RASTER_BYTES(8, 8)];
  u8 nextMask[8];
  u8 present[RASTER_BYTES(8, 8)];
  u8 battery[RASTER_BYTES(8, 8)];
  u8 feelings[7 * RASTER_BYTES(24, 16)];
  u8 menuLabels[6 * TEXT_RASTER_BYTES];
  u8 menuIcons[6 * RASTER_BYTES(16, 16)];
  u8 nameIcon[RASTER_BYTES(16, 16)];
  u8 name[TEXT_RASTER_BYTES];
  u8 course[RASTER_BYTES(16, 16)];
  u8 steps[RASTER_BYTES(40, 16)];
  u8 amPm[RASTER_BYTES(32, 16)];
  u8 elapsedDays[RASTER_BYTES(40, 16)];
  u8 totalDays[RASTER_BYTES(64, 16)];
  u8 volume[RASTER_BYTES(40, 16)];
  u8 contrast[RASTER_BYTES(40, 16)];
  u8 volumeOff[RASTER_BYTES(24, 16)];
  u8 volumeLow[RASTER_BYTES(24, 16)];
  u8 volumeHigh[RASTER_BYTES(24, 16)];
  u8 meter[RASTER_BYTES(8, 16)];
  u8 itemTreasure[RASTER_BYTES(32, 24)];
  u8 itemMap[RASTER_BYTES(32, 24)];
  u8 itemPresent[RASTER_BYTES(32, 24)];
  u8 grass[RASTER_BYTES(16, 16)];
  u8 litGrass[RASTER_BYTES(16, 16)];
  u8 attemptsLabel[RASTER_BYTES(32, 16)];
  u8 timesLabel[RASTER_BYTES(24, 16)];
  u8 radarGrass[RASTER_BYTES(32, 24)];
  u8 radarIcons[4 * RASTER_BYTES(16, 16)];
  u8 attack[RASTER_BYTES(32, 16)];
  u8 weakPoint[RASTER_BYTES(32, 16)];
  u8 gas[RASTER_BYTES(32, 24)];
  u8 battleMeter[RASTER_BYTES(8, 8)];
  u8 star[RASTER_BYTES(8, 8)];
  u8 battleMenu[2 * MESSAGE_RASTER_BYTES];
  u8 walker[RASTER_BYTES(32, 32)];
  u8 radio[RASTER_BYTES(16, 8)];
  u8 note[RASTER_BYTES(8, 8)];
  u8 pokemonShadow[RASTER_BYTES(8, 8)];
  u8 ending[RASTER_BYTES(40, 16)];
  u8 messages[MESSAGE_COUNT * MESSAGE_RASTER_BYTES];
} UiResources;

typedef struct {
  Course values;
  u8 course[RASTER_BYTES(32, 24)];
  u8 courseName[TEXT_RASTER_BYTES];
  u8 pokemonImage[POKEMON_ANIMATION_BYTES];
  u8 pokemonImageLarge[POKEMON_LARGE_ANIMATION_BYTES];
  u8 pokemonName[TEXT_RASTER_BYTES];
  u8 encounterImages[COURSE_ENCOUNTERS * POKEMON_ANIMATION_BYTES];
  u8 encounterImageLarge[POKEMON_LARGE_ANIMATION_BYTES];
  u8 encounterNames[COURSE_ENCOUNTERS * TEXT_RASTER_BYTES];
  u8 itemImages[COURSE_ITEMS * MESSAGE_RASTER_BYTES];
} CourseResources;

typedef char CourseEncounterImages
    [offsetof(CourseResources, encounterImages) == 0x0B7E ? 1 : -1];
typedef char CourseJoiningImages
    [offsetof(CourseResources, encounterImageLarge) == 0x0FFE ? 1 : -1];

#pragma bit_order right

typedef struct {
  Pokemon pokemon;
  PokemonMetadata metadata;
  u8 pokemonImage[POKEMON_ANIMATION_BYTES];
  u8 pokemonName[TEXT_RASTER_BYTES];
} EventPokemon;

typedef struct {
  ItemPrefix prefix;
  u16 itemIdLe;
} EventItemHeader;

typedef struct {
  EventItemHeader header;
  u8 itemName[MESSAGE_RASTER_BYTES];
} EventItem;

typedef char EventItemHeaderSize[sizeof(EventItemHeader) == 8 ? 1 : -1];
typedef char EventItemNumber[offsetof(EventItemHeader, itemIdLe) == 6 ? 1 : -1];
typedef char EventItemName[offsetof(EventItem, itemName) == 8 ? 1 : -1];
typedef char EventItemSize[sizeof(EventItem) == 392 ? 1 : -1];

typedef struct {
  BonusCourse values;
  u8 pokemonImage[POKEMON_ANIMATION_BYTES];
  u8 preservedAfterPokemonImage[0x600];
  u8 pokemonName[TEXT_RASTER_BYTES];
  u8 course[RASTER_BYTES(32, 24)];
  u8 courseName[TEXT_RASTER_BYTES];
  u8 itemName[MESSAGE_RASTER_BYTES];
} BonusResources;

typedef char
    BonusPokemonImage[offsetof(BonusResources, pokemonImage) == 0x7C ? 1 : -1];
typedef char
    BonusPokemonName[offsetof(BonusResources, pokemonName) == 0x7FC ? 1 : -1];
typedef char BonusResourcesSize[sizeof(BonusResources) == 0xCBC ? 1 : -1];

#pragma bit_order left

#endif
