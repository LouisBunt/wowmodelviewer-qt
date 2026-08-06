#ifndef WOWHEADDRESSINGROOM_H
#define WOWHEADDRESSINGROOM_H

#include <functional>
#include <vector>

#include <QString>

// Decoder for a Wowhead dressing-room link.
//
// A dressing-room address carries the whole character behind the '#':
//
//   https://www.wowhead.com/dressing-room#sz80z...
//
// Everything after the '#' is a URL fragment, which a browser never sends to the
// server -- so unlike an outfit or transmog-set page there is nothing to fetch. The
// look has to be decoded locally, and that is what this does.
//
// The encoding is Wowhead's own: a base-58 alphabet, '8' as the segment separator and
// a run-length escape for empty segments. It is not documented anywhere; the layout
// below follows wow.export's implementation (src/js/wowhead.js, MIT), which is the
// only public description of the format.
struct WowheadCharacter
{
  int version = 0;         // hash format version; >= 15 is the current layout
  int race = 0;            // ChrRaces.ID
  int gender = 0;          // 0 male, 1 female
  int classId = 0;         // ChrClasses.ID -- 6 is Death Knight (eye glow)
  int spec = 0;
  int level = 0;

  // ChrCustomizationChoice.IDs. Wowhead stores only the choice, not the option it
  // belongs to -- the option has to be looked up in the game database.
  std::vector<unsigned int> customizationChoices;

  // One entry per equipped item, in the order the hash lists them.
  //
  // `positionalSlot` is where Wowhead's own slot counting says the item belongs, in
  // CharSlots terms. It is only a HINT: the counting relies on marker bytes whose
  // meaning is not fully pinned down, and a real link has been observed placing an
  // item two slots off. The caller should prefer the slot the item itself declares in
  // the game database and fall back on this only for items the client does not know.
  struct Item
  {
    int positionalSlot;   // CharSlots, or -1 if the counting ran off the end
    int itemId;
    // The item-bonus-list id encoded after the item, 0 when none. This is what carries
    // the COLOUR: a tier piece's Raid Finder/Heroic/Mythic tints are all one item id,
    // and the bonus (via its type-7 entry) picks the ItemAppearanceModifier. Ignoring
    // it imports every set in its Normal colours.
    int bonusId = 0;
  };
  std::vector<Item> equipment;
};

// Tells the decoder whether an id is a real, equippable item. Optional, but without it
// the decoder cannot resolve an ambiguous slot marker (see the '7' handling in the
// implementation) and may hand back a nonsense id for the affected slot.
using WowheadItemValidator = std::function<bool(int)>;

// True for anything that looks like a dressing-room address. Deliberately loose: the
// caller wants to route the link here rather than to the outfit fetcher, and a
// malformed hash is reported by the parse, not by this.
bool wowhead_is_dressing_room_url(const QString& url);

// Decodes `url`. Returns false and fills `error` if the link carries no hash or the
// hash does not decode to a usable character. Pass `isKnownItem` wherever the game
// database is reachable -- it is what lets the decoder pick the right reading of an
// ambiguous slot marker.
bool wowhead_parse_dressing_room(const QString& url, WowheadCharacter* out, QString* error,
                                 const WowheadItemValidator& isKnownItem = WowheadItemValidator());

#endif
