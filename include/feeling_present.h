#ifndef PW_FEELING_PRESENT_H
#define PW_FEELING_PRESENT_H

#include "feeling.h"

void SocialUpdate(void);
void SocialRender(void);
void SocialOfferCheck(void);

extern const SocialFrame g_socialItemSequence[];
extern const SocialFrame g_socialWatts50Sequence[];
extern const SocialFrame g_socialWatts20Sequence[];
extern const SocialFrame g_socialWatts10Sequence[];
extern const SocialFrame g_socialBoredSequence[];
extern const SocialFrame g_socialNewPokemonSequence[];

#endif
