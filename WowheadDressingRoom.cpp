#include "WowheadDressingRoom.h"

#include <QObject>
#include <QRegularExpression>
#include <QStringList>

#include "wow_enums.h"

namespace {

// Wowhead's encoding alphabet, taken verbatim from their hash engine
// (WH.calc.hash.getEncoding()). '7', '8' and '9' ARE part of the alphabet but never
// appear in values -- they are the delimiters: '8' separates fields, '7' escapes runs
// of empty fields, '9' escapes runs of zero characters. Values are base 58
// (WH.calc.hash.getMaxEncodingIndex()), so digits only ever go up to index 58.
int charValue(QChar c)
{
  static const QString charset =
    QStringLiteral("0zMcmVokRsaqbdrfwihuGINALpTjnyxtgevElBCDFHJKOPQSUWXYZ123456789");
  return charset.indexOf(c);
}

// Base-58 number, least significant character last (WH.calc.decode.longValue).
int decodeNumber(const QString& s)
{
  int result = 0;
  for (int i = 0; i < s.length(); ++i) {
    const int value = charValue(s.at(s.length() - 1 - i));
    if (value < 0)
      return 0;
    int scaled = value;
    for (int j = 0; j < i; ++j)
      scaled *= 58;
    result += scaled;
  }
  return result;
}

// Undo Wowhead's two compression escapes, mirroring WH.calc.decode.zeroDelimiters and
// WH.calc.decode.zeroes:
//
//   '7'...'7' X  ->  N empty fields ("08" repeated),  N = idx(X) + (#7s - 1) * 58
//   '9'...'9' X  ->  N literal '0' characters,        N = idx(X) + (#9s - 1) * 58
//
// The repeated-escape form is how runs longer than 58 are written; a decoder that
// reads only single-escape runs (as wow.export's does) drifts by 58 fields on any
// character with more than 58 consecutive empty values -- which is every character
// that uses fewer than half of the 50 customization pairs.
QString expandEscape(const QString& s, QChar escape, const QString& replacement)
{
  QString out;
  out.reserve(s.length());
  for (int i = 0; i < s.length();) {
    if (s.at(i) != escape) {
      out += s.at(i);
      ++i;
      continue;
    }
    int escapes = 0;
    while (i < s.length() && s.at(i) == escape) {
      ++escapes;
      ++i;
    }
    if (i >= s.length())
      break;                        // trailing escape without a count -- drop it
    const int count = charValue(s.at(i)) + (escapes - 1) * 58;
    ++i;
    for (int n = 0; n < count; ++n)
      out += replacement;
  }
  return out;
}

// Wowhead's dressing-room slots 1..13 in WMV's CharSlots. Slot 14 is deliberately
// absent: on retail it is the SECOND shoulder (see WowheadCharacter::shoulder2ItemId),
// on pre-MoP Classic pages it is Ranged -- either way it must not run through the
// normal placement, which is only the FALLBACK anyway; the importer places items by
// what the item database says they are.
int charSlotForWowheadSlot(int whSlot)
{
  switch (whSlot) {
    case 1:  return CS_HEAD;
    case 2:  return CS_SHOULDER;
    case 3:  return CS_CAPE;
    case 4:  return CS_CHEST;
    case 5:  return CS_SHIRT;
    case 6:  return CS_TABARD;
    case 7:  return CS_BRACERS;
    case 8:  return CS_GLOVES;
    case 9:  return CS_BELT;
    case 10: return CS_PANTS;
    case 11: return CS_BOOTS;
    case 12: return CS_HAND_RIGHT;
    case 13: return CS_HAND_LEFT;
    default: return -1;
  }
}

QString field(const QStringList& fields, int index)
{
  return (index >= 0 && index < fields.size()) ? fields.at(index) : QString();
}

int charAtValue(const QString& s, int index)
{
  return (index >= 0 && index < s.length()) ? charValue(s.at(index)) : 0;
}

// The v15 layout, replicated from Wowhead's own hash template
// (WH.Wow.DressingRoom.getHashTemplate()). The format is strictly POSITIONAL: after
// expanding the two escapes, the fields separated by '8' are, in order:
//
//   [0]      race
//   [1]      gender, class, spec (one char each) + level (rest of the field)
//   [2]      npcOptions, pepe (one char each) + mount (rest of the field)
//   [3..102] 50 customization pairs: optionId, choiceId
//   [103..]  equipment slots 1..14: itemId, itemBonus -- slots 12 and 13 additionally
//            carry an enchant field
//   then     artifact appearances and separateShoulders (unused here)
//
// There are no slot markers and no per-slot layout variation; earlier decoders
// (wow.export's, and ours ported from it) misread the '7'/'9' escapes as markers,
// which happened to work on hashes with short zero runs and scrambled everything else.
void parseV15(const QStringList& fields, int version, WowheadCharacter* out,
              bool classicRanged)
{
  out->version = version;
  out->race = decodeNumber(field(fields, 0));

  const QString header = field(fields, 1);
  out->gender  = charAtValue(header, 0);
  out->classId = charAtValue(header, 1);
  out->spec    = charAtValue(header, 2);
  out->level   = decodeNumber(header.mid(3));

  for (int pair = 0; pair < 50; ++pair) {
    const int choiceId = decodeNumber(field(fields, 4 + pair * 2));
    if (choiceId != 0)
      out->customizationChoices.push_back((unsigned int)choiceId);
  }

  int index = 103;
  for (int whSlot = 1; whSlot <= 13; ++whSlot) {
    const int itemId = decodeNumber(field(fields, index++));
    const int bonusId = decodeNumber(field(fields, index++));
    if (whSlot == 12 || whSlot == 13)
      ++index;                      // enchant/illusion -- nothing to render it with
    if (itemId > 0)
      out->equipment.push_back({whSlot, charSlotForWowheadSlot(whSlot), itemId, bonusId});
  }

  // Slot 14: the second shoulder on retail (see the header). Captured separately, and
  // the separateShoulders flag sits behind the two artifact-appearance fields.
  //
  // On a pre-MoP Classic dressing room the same field is the RANGED weapon instead.
  // That is a property of the PAGE, not the hash -- the caller derives it from the
  // URL -- and there the item goes through normal placement like any weapon.
  const int slot14Item = decodeNumber(field(fields, index++));
  const int slot14Bonus = decodeNumber(field(fields, index++));
  index += 2;                       // artifactAppearanceMainHand, ...OffHand
  out->separateShoulders = decodeNumber(field(fields, index)) != 0;

  if (classicRanged) {
    if (slot14Item > 0)
      out->equipment.push_back({14, CS_HAND_LEFT, slot14Item, slot14Bonus});
  } else {
    out->shoulder2ItemId = slot14Item;
    out->shoulder2Bonus = slot14Bonus;
  }
}

// Pre-15 hashes: everything at a fixed segment index. Kept as ported from wow.export;
// links this old have no bonus data to carry.
void parseLegacy(const QStringList& segments, int version, WowheadCharacter* out)
{
  out->version = version;
  out->race = decodeNumber(field(segments, 0));

  const QString header = field(segments, 1);
  out->gender  = charAtValue(header, 0);
  out->classId = charAtValue(header, 1);
  out->spec    = charAtValue(header, 2);
  out->level   = decodeNumber(header.mid(3));

  for (int i = 3; i <= 30; ++i) {
    const int value = decodeNumber(field(segments, i));
    if (value != 0)
      out->customizationChoices.push_back((unsigned int)value);
  }

  const struct { int segIdx; int whSlot; } kLegacySlots[] = {
    { 31, 1 }, { 33, 2 }, { 35, 3 }, { 37, 4 },
    { 38, 8 }, { 40, 9 }, { 42, 10 }, { 44, 11 }
  };
  for (const auto& entry : kLegacySlots) {
    const QString seg = field(segments, entry.segIdx);
    const int itemId = decodeNumber(seg.right(4));
    if (itemId > 0)
      out->equipment.push_back(
        {entry.whSlot, charSlotForWowheadSlot(entry.whSlot), itemId, 0});
  }
}

}  // namespace

bool wowhead_is_dressing_room_url(const QString& url)
{
  return url.contains("dressing-room", Qt::CaseInsensitive);
}

bool wowhead_parse_dressing_room(const QString& url, WowheadCharacter* out, QString* error)
{
  const auto fail = [error](const QString& msg) {
    if (error)
      *error = msg;
    return false;
  };

  const QRegularExpressionMatch match =
    QRegularExpression("dressing-room#(.+)").match(url.trimmed());
  if (!match.hasMatch())
    return fail(QObject::tr("In diesem Link steht kein Anprobe-Code hinter dem '#'."));

  const QString hash = match.captured(1);
  const int version = charValue(hash.at(0));
  if (version < 0)
    return fail(QObject::tr("Der Anprobe-Code beginnt mit einem unbekannten Zeichen."));

  // Escape expansion order does not matter: the two escapes rewrite disjoint patterns
  // and neither produces the other's escape character.
  const QString expanded =
    expandEscape(expandEscape(hash.mid(1), QChar('7'), QStringLiteral("08")),
                 QChar('9'), QStringLiteral("0"));
  const QStringList fields = expanded.split(QChar('8'));

  // Classic-era dressing rooms repurpose equipment slot 14 as Ranged; recognisable
  // only by the page address (wowhead.com/classic/dressing-room#..., /cata/, /sod/,
  // /tbc/, /wotlk/, /mop-classic/), never by the hash itself.
  const bool classicRanged = url.contains(QRegularExpression(
    "wowhead\\.com/(classic|era|sod|tbc|wotlk|cata|mop)[-a-z]*/",
    QRegularExpression::CaseInsensitiveOption));

  // v15 is strictly positional -- field 0 race, 3..102 the fifty customization pairs,
  // equipment from 103 on. A single inserted field shifts everything behind it, and the
  // only sanity check this decoder has is "race > 0", which a shifted layout passes
  // easily. Accepting anything >= 15 therefore would not FAIL on a future Wowhead format:
  // it would import a plausible-looking but wrong character. Refuse what is untested.
  const int kNewestKnownVersion = 15;
  if (version > kNewestKnownVersion)
    return fail(QObject::tr("Dieser Anprobe-Link nutzt ein neueres Wowhead-Format (v%1), "
                            "als diese Version lesen kann (bis v%2).")
                  .arg(version).arg(kNewestKnownVersion));

  WowheadCharacter parsed;
  if (version == kNewestKnownVersion)
    parseV15(fields, version, &parsed, classicRanged);
  else
    parseLegacy(fields, version, &parsed);

  if (parsed.race <= 0)
    return fail(QObject::tr("Aus dem Anprobe-Code ließ sich keine Rasse lesen. "
                            "Stammt der Link wirklich aus der Wowhead-Anprobe?"));

  *out = parsed;
  return true;
}
