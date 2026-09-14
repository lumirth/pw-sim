#ifndef PW_DIARY_API_H
#define PW_DIARY_API_H

#include "types.h"
#include "data.h"

void DiaryAppend(Course *course, DiaryEntry *diary, u8 actionId,
                 u8 bonusCourseFlag, u8 encounterSelector, u16 itemNumber);

#endif
