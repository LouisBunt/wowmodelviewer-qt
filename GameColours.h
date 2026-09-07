#ifndef GAMECOLOURS_H
#define GAMECOLOURS_H

// Colour that belongs to World of Warcraft, not to this interface.
//
// Item quality is the game's own vocabulary: purple means epic, and it means that whatever the
// application looks like around it. These values are therefore deliberately NOT in Theme.h --
// a redesign may change every other colour in the window and must not touch these.
//
// They lived twice, byte for byte, in CharacterPanel.cpp and ItemBrowser.cpp. Two copies of a
// constant is one copy too many even when both are right.

#include <QString>

namespace game {

// The eight quality tiers, in the game's own order.
inline const char* qualityColour(int quality)
{
  switch (quality) {
    case 0:  return "#9d9d9d";   // poor
    case 1:  return "#ffffff";   // common
    case 2:  return "#1eff00";   // uncommon
    case 3:  return "#0070dd";   // rare
    case 4:  return "#a335ee";   // epic
    case 5:  return "#ff8000";   // legendary
    case 6:  return "#e6cc80";   // artifact
    case 7:  return "#00ccff";   // heirloom
    default: return "#ffffff";
  }
}

// The same tier, lightened for small text on a near-black ground.
//
// Uncommon green and rare blue are tuned for the game's own parchment-and-stone panels; at 13 px
// over #121215 the blue in particular drops close to the floor of what is readable. The true
// value stays for anything with mass -- a swatch, a 3px edge -- and this one carries the text.
inline const char* qualityTextColour(int quality)
{
  switch (quality) {
    case 2:  return "#4ade4a";   // uncommon, lifted
    case 3:  return "#4a9eff";   // rare, lifted
    default: return qualityColour(quality);
  }
}

}  // namespace game

#endif  // GAMECOLOURS_H
