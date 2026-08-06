#include "WowheadDressingRoom.h"

#include <algorithm>

#include <QObject>
#include <QRegularExpression>
#include <QStringList>

#include "wow_enums.h"

namespace {

// Wowhead's base-58 alphabet. Position in this string IS the digit value, so the order
// matters and it cannot be sorted or shortened.
int charValue(QChar c)
{
  static const QString charset =
    QStringLiteral("0zMcmVokRsaqbdrfwihuGINALpTjnyxtgevElBCDFHJKOPQSUWXYZ123456");
  return charset.indexOf(c);
}

// A number in Wowhead's base-58, least significant character LAST.
//
// Mirrors wow.export's decode(): an empty string is 0, and a string containing a
// character outside the alphabet is 0 rather than an error -- the callers treat
// "0" as "nothing in this slot" anyway.
int decodeNumber(const QString& s)
{
  if (s.isEmpty())
    return 0;
  if (s.length() == 1)
    return charValue(s.at(0));

  int result = 0;
  for (int i = 0; i < s.length(); ++i) {
    // Reverse order: the last character is the units digit.
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

// Runs of empty segments are stored as '9' followed by one character giving the count.
// Expanding them back to literal "08" pairs is what makes the segment indices line up.
QString decompressZeros(const QString& s)
{
  QString out;
  out.reserve(s.length());
  for (int i = 0; i < s.length(); ++i) {
    if (s.at(i) == '9' && i + 1 < s.length()) {
      const int count = charValue(s.at(i + 1));
      if (count >= 0) {
        for (int n = 0; n < count; ++n)
          out += "08";
        ++i;                  // the count character is consumed
        continue;
      }
      // Unknown count character: keep the pair verbatim, exactly as the JS regex does
      // when indexOf() returns -1.
      out += s.at(i);
      out += s.at(i + 1);
      ++i;
      continue;
    }
    out += s.at(i);
  }
  return out;
}

// Wowhead numbers the dressing room's visible slots 1..13 in display order. This is
// that order expressed in WMV's CharSlots, derived by composing wow.export's two
// tables (WOWHEAD_SLOT_TO_SLOT_ID and WMV_SLOT_TO_SLOT_ID).
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

QString segment(const QStringList& segments, int index)
{
  return (index >= 0 && index < segments.size()) ? segments.at(index) : QString();
}

int charAtValue(const QString& s, int index)
{
  return (index < s.length()) ? charValue(s.at(index)) : charValue(QChar('0'));
}

// The current layout (version >= 15). Customizations and equipment are no longer at
// fixed segment indices; equipment starts at the first segment marked with a '7'.
void parseV15(const QStringList& segments, int version, WowheadCharacter* out,
              const WowheadItemValidator& isKnownItem)
{
  out->version = version;
  out->race = decodeNumber(segment(segments, 0));

  const QString combined = segment(segments, 1);
  out->gender  = charAtValue(combined, 0);
  out->classId = charAtValue(combined, 1);
  out->spec    = charAtValue(combined, 2);
  out->level   = decodeNumber(combined.mid(3));

  int equipStart = -1;
  for (int i = 6; i < segments.size(); ++i) {
    if (!segments.at(i).isEmpty() && segments.at(i).startsWith('7')) {
      equipStart = i;
      break;
    }
  }

  // Customizations sit between the header and the equipment block, as
  // (unused, choiceID) pairs.
  if (equipStart > 6) {
    for (int i = 6; i < equipStart; i += 2) {
      const int choiceId = decodeNumber(segment(segments, i + 1));
      if (choiceId != 0)
        out->customizationChoices.push_back((unsigned int)choiceId);
    }
  }

  if (equipStart < 0)
    return;

  // With the game database at hand, the equipment block does not have to be walked by
  // Wowhead's own bookkeeping at all -- and it should not be. That bookkeeping relies
  // on marker bytes of unknown width and on knowing how many filler fields follow each
  // slot, and on a real link it both mis-slotted gloves and swallowed the second
  // weapon. Instead: read every segment, and treat as an item exactly those that name
  // a real, equippable item. The filler fields (enchant and illusion ids) are not
  // equippable, so they fall out on their own.
  if (isKnownItem) {
    int whSlot = 1;
    for (int i = equipStart; i < segments.size(); ++i) {
      QString seg = segments.at(i);

      if (seg.startsWith('7') && seg.length() >= 2) {
        const int markerVal = charValue(seg.at(1));
        if (markerVal >= 0 && markerVal <= 12)
          whSlot = markerVal + 1;
        seg = seg.mid(2);
        // The marker is normally one character ("7V<item>") but has been seen taking
        // two ("77i<item>"). Keep whichever reading names a real item.
        if (seg.length() > 1 && !isKnownItem(decodeNumber(seg)) &&
            isKnownItem(decodeNumber(seg.mid(1))))
          seg = seg.mid(1);
      }

      if (seg.isEmpty())
        continue;

      const int itemId = decodeNumber(seg);
      if (itemId > 0 && isKnownItem(itemId)) {
        out->equipment.push_back({charSlotForWowheadSlot(whSlot), itemId});
        ++whSlot;
      }
    }
    return;
  }

  // No database to check against (unit tests, and any caller that has not mounted the
  // game data yet): fall back on Wowhead's own counting, which is right often enough
  // to be useful and is all there is.
  int segIdx = equipStart;
  int whSlot = 1;
  while (segIdx < segments.size() && whSlot <= 13) {
    QString seg = segment(segments, segIdx);

    // A '7' marker restarts the slot numbering, which is how skipped slots stay in
    // sync. How many characters the marker occupies is NOT reliably known: normally
    // one ("7V<item>"), but a real link has been seen carrying two ("77i<item>"), and
    // stripping the usual one then leaves a junk id where the item should be. So when
    // the caller can tell a real item from a junk one, try the longer marker too and
    // keep whichever reading names an item that actually exists.
    if (seg.startsWith('7') && seg.length() >= 2) {
      const int markerVal = charValue(seg.at(1));
      if (markerVal >= 0 && markerVal <= 12)
        whSlot = markerVal + 1;

      QString payload = seg.mid(2);
      if (isKnownItem && payload.length() > 1 && !isKnownItem(decodeNumber(payload)) &&
          isKnownItem(decodeNumber(payload.mid(1))))
        payload = payload.mid(1);
      seg = payload;
    }

    if (seg.isEmpty()) {
      ++segIdx;
      continue;
    }

    const int itemId = decodeNumber(seg);
    if (itemId > 0)
      out->equipment.push_back({charSlotForWowheadSlot(whSlot), itemId});

    // Each item is followed by trailing data (enchant/illusion, and a second field on
    // the weapon slots) that we do not use but have to step over.
    ++segIdx;
    if (segIdx < segments.size() && !segments.at(segIdx).startsWith('7'))
      ++segIdx;
    if (whSlot >= 12 && segIdx < segments.size() && !segments.at(segIdx).startsWith('7'))
      ++segIdx;
    ++whSlot;
  }
}

// Pre-15 hashes: everything is at a fixed segment index.
void parseLegacy(const QStringList& segments, int version, WowheadCharacter* out)
{
  out->version = version;
  out->race = decodeNumber(segment(segments, 0));

  const QString header = segment(segments, 1);
  out->gender  = charAtValue(header, 0);
  out->classId = charAtValue(header, 1);
  out->spec    = charAtValue(header, 2);
  out->level   = decodeNumber(header.mid(3));

  for (int i = 3; i <= 30; ++i) {
    const int value = decodeNumber(segment(segments, i));
    if (value != 0)
      out->customizationChoices.push_back((unsigned int)value);
  }

  // Only the slots the old format stored, at their fixed indices.
  const struct { int segIdx; int whSlot; } kLegacySlots[] = {
    { 31, 1 }, { 33, 2 }, { 35, 3 }, { 37, 4 },
    { 38, 8 }, { 40, 9 }, { 42, 10 }, { 44, 11 }
  };
  for (const auto& entry : kLegacySlots) {
    const QString seg = segment(segments, entry.segIdx);
    // The legacy segments carry a suffix we do not use; the id is the last 4 digits.
    const int itemId = decodeNumber(seg.mid(std::max(0, seg.length() - 4)));
    if (itemId > 0)
      out->equipment.push_back({charSlotForWowheadSlot(entry.whSlot), itemId});
  }
}

}  // namespace

bool wowhead_is_dressing_room_url(const QString& url)
{
  return url.contains("dressing-room", Qt::CaseInsensitive);
}

bool wowhead_parse_dressing_room(const QString& url, WowheadCharacter* out, QString* error,
                                 const WowheadItemValidator& isKnownItem)
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
  if (hash.isEmpty())
    return fail(QObject::tr("In diesem Link steht kein Anprobe-Code hinter dem '#'."));

  const int version = charValue(hash.at(0));
  if (version < 0)
    return fail(QObject::tr("Der Anprobe-Code beginnt mit einem unbekannten Zeichen."));

  const QStringList segments = decompressZeros(hash.mid(1)).split('8');

  WowheadCharacter parsed;
  if (version >= 15)
    parseV15(segments, version, &parsed, isKnownItem);
  else
    parseLegacy(segments, version, &parsed);

  if (parsed.race <= 0)
    return fail(QObject::tr("Aus dem Anprobe-Code ließ sich keine Rasse lesen. "
                            "Stammt der Link wirklich aus der Wowhead-Anprobe?"));

  *out = parsed;
  return true;
}
