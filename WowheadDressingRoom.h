#ifndef WOWHEADDRESSINGROOM_H
#define WOWHEADDRESSINGROOM_H

#include <vector>

#include <QString>

// Decoder for a Wowhead dressing-room link.
//
// A dressing-room address carries the whole character behind the '#':
//
//   https://www.wowhead.com/dressing-room#fz80o...
//
// Everything after the '#' is a URL fragment, which a browser never sends to the
// server -- so unlike an outfit or transmog-set page there is nothing to fetch. The
// look has to be decoded locally, and that is what this does.
//
// The format is replicated from Wowhead's own hash engine (WH.calc.hash /
// WH.Wow.DressingRoom.getHashTemplate(), read out of the live page): a base-58
// alphabet, '8' as the field delimiter, '7'/'9' as run-length escapes, and a strictly
// positional field layout -- see parseV15 in the implementation for the exact map.
struct WowheadCharacter
{
  int version = 0;         // hash format version; >= 15 is the current layout
  int race = 0;            // ChrRaces.ID
  int gender = 0;          // 0 male, 1 female
  int classId = 0;         // ChrClasses.ID -- 6 is Death Knight (eye glow)
  int spec = 0;
  int level = 0;

  // ChrCustomizationChoice.IDs. Wowhead stores option/choice pairs; the option ids are
  // re-derived from the game database by the importer, so only choices travel here.
  std::vector<unsigned int> customizationChoices;

  // One entry per equipped item, in slot order.
  struct Item
  {
    int wowheadSlot;      // Wowhead's own slot number, 1..14 -- kept for diagnostics
    int positionalSlot;   // the same position in CharSlots terms, -1 if unmapped
    int itemId;
    // The item-bonus-list id encoded with the item, 0 when none. This is what carries
    // the COLOUR: a tier piece's Raid Finder/Heroic/Mythic tints are all one item id,
    // and the bonus (via its type-7 entry) picks the ItemAppearanceModifier.
    int bonusId;
  };
  std::vector<Item> equipment;
};

// True for anything that looks like a dressing-room address. Deliberately loose: the
// caller wants to route the link here rather than to the outfit fetcher, and a
// malformed hash is reported by the parse, not by this.
bool wowhead_is_dressing_room_url(const QString& url);

// Decodes `url`. Returns false and fills `error` if the link carries no hash or the
// hash does not decode to a usable character.
bool wowhead_parse_dressing_room(const QString& url, WowheadCharacter* out, QString* error);

#endif
