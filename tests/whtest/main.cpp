// Differential test for the Wowhead dressing-room decoder.
//
// The corpus was not written by hand: every case was encoded by Wowhead's OWN hash
// engine in the live page (WH.calc.encode) and the expected values are what Wowhead's
// own decoder (getCharacterForHash) returned for it. Passing therefore means
// "byte-identical to Wowhead", not "matches our reading of the format".
//
//   whtest <path/to/corpus.json>
//
// Exit code = number of failing cases, so a plain shell/CI check works.
#include <cstdio>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "WowheadDressingRoom.h"

namespace {

QString itemsAsText(const std::vector<WowheadCharacter::Item>& items)
{
  QString out;
  for (const auto& it : items)
    out += QString("[%1,%2,%3]").arg(it.wowheadSlot).arg(it.itemId).arg(it.bonusId);
  return out;
}

QString expectedItemsAsText(const QJsonArray& items)
{
  QString out;
  for (const auto& v : items) {
    const QJsonArray it = v.toArray();
    out += QString("[%1,%2,%3]")
             .arg(it.at(0).toInt()).arg(it.at(1).toInt()).arg(it.at(2).toInt());
  }
  return out;
}

QString choicesAsText(const std::vector<unsigned int>& choices)
{
  QString out;
  for (unsigned int c : choices)
    out += QString("%1,").arg(c);
  return out;
}

QString expectedChoicesAsText(const QJsonArray& choices)
{
  QString out;
  for (const auto& v : choices)
    out += QString("%1,").arg(v.toInt());
  return out;
}

}  // namespace

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);

  if (argc < 2) {
    std::fprintf(stderr, "usage: whtest <corpus.json>\n");
    return 1;
  }

  QFile file(QString::fromLocal8Bit(argv[1]));
  if (!file.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "cannot open %s\n", argv[1]);
    return 1;
  }

  const QJsonArray corpus = QJsonDocument::fromJson(file.readAll()).array();
  if (corpus.isEmpty()) {
    std::fprintf(stderr, "corpus is empty or not a JSON array\n");
    return 1;
  }

  int failures = 0;
  int index = 0;
  for (const auto& value : corpus) {
    const QJsonObject c = value.toObject();
    const QString url =
      "https://www.wowhead.com/dressing-room#" + c.value("h").toString();

    WowheadCharacter got;
    QString error;
    if (!wowhead_parse_dressing_room(url, &got, &error)) {
      std::printf("FAIL case %d: decode error: %s\n", index, qPrintable(error));
      ++failures;
      ++index;
      continue;
    }

    struct { const char* name; int want; int have; } scalars[] = {
      { "race",   c.value("race").toInt(),   got.race },
      { "gender", c.value("gender").toInt(), got.gender },
      { "cls",    c.value("cls").toInt(),    got.classId },
      { "lvl",    c.value("lvl").toInt(),    got.level },
    };

    bool ok = true;
    for (const auto& s : scalars) {
      if (s.want != s.have) {
        std::printf("FAIL case %d: %s want %d got %d\n", index, s.name, s.want, s.have);
        ok = false;
      }
    }

    const QString wantItems = expectedItemsAsText(c.value("items").toArray());
    const QString haveItems = itemsAsText(got.equipment);
    if (wantItems != haveItems) {
      std::printf("FAIL case %d: items\n  want %s\n  got  %s\n",
                  index, qPrintable(wantItems), qPrintable(haveItems));
      ok = false;
    }

    const QString wantChoices = expectedChoicesAsText(c.value("choices").toArray());
    const QString haveChoices = choicesAsText(got.customizationChoices);
    if (wantChoices != haveChoices) {
      std::printf("FAIL case %d: choices\n  want %s\n  got  %s\n",
                  index, qPrintable(wantChoices), qPrintable(haveChoices));
      ok = false;
    }

    // Optional expectations, present only on cases that carry them (added with the
    // slot-14 fix). Absent keys compare against the struct defaults, so old cases
    // implicitly assert "no separate shoulders" -- which is correct for them.
    if (c.value("separateShoulders").toInt(0) != (got.separateShoulders ? 1 : 0)) {
      std::printf("FAIL case %d: separateShoulders want %d got %d\n",
                  index, c.value("separateShoulders").toInt(0),
                  got.separateShoulders ? 1 : 0);
      ok = false;
    }
    if (c.value("shoulder2Item").toInt(0) != got.shoulder2ItemId ||
        c.value("shoulder2Bonus").toInt(0) != got.shoulder2Bonus) {
      std::printf("FAIL case %d: shoulder2 want %d/%d got %d/%d\n",
                  index, c.value("shoulder2Item").toInt(0),
                  c.value("shoulder2Bonus").toInt(0),
                  got.shoulder2ItemId, got.shoulder2Bonus);
      ok = false;
    }

    if (!ok)
      ++failures;
    ++index;
  }

  std::printf("%d/%d cases pass\n", index - failures, index);
  return failures;
}
