#include "ItemBrowser.h"

#include <map>

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

#include "Game.h"
#include "GameDatabase.h"

namespace {
const char* kText   = "#e8eaee";
const char* kMuted  = "#8a93a0";
const char* kDim    = "#5f6874";
const char* kCard   = "#14181e";
const char* kBord   = "#23282f";
const char* kAccent = "#c8a15a";

// A list this long has to be capped: 139k rows would take longer to build as widgets than
// the query takes to run, and nobody scrolls that far. The footer says what was cut.
const int kMaxRows = 500;

QString uiFamily()
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : {QStringLiteral("IBM Plex Sans"), QStringLiteral("Segoe UI")})
    if (have.contains(f))
      return f;
  return QStringLiteral("sans-serif");
}

// Item.InventoryType -> label. Only the slots that carry an appearance are offered;
// trinkets, rings and bags have nothing to show.
const struct { const char* label; const char* types; } kSlots[] = {
  { "Alle Slots",   ""            },
  { "Kopf",         "1"           },
  { "Schulter",     "3"           },
  { "Hemd",         "4"           },
  { "Brust",        "5,20"        },   // 20 = robe, same slot
  { "Gürtel",       "6"           },
  { "Beine",        "7"           },
  { "Füße",         "8"           },
  { "Armschienen",  "9"           },
  { "Hände",        "10"          },
  { "Umhang",       "16"          },
  { "Wappenrock",   "19"          },
  { "Einhandwaffe", "13,21,22"    },
  { "Zweihandwaffe","17"          },
  { "Schild",       "14"          },
  { "Distanzwaffe", "15,25,26"    },
  { "Nebenhand",    "23"          }
};

// ItemSparse.ExpansionID. -3 is the "not tied to an expansion" sentinel the data uses.
const struct { const char* label; int id; } kExpansions[] = {
  { "Alle Erweiterungen", -99 },
  { "Ohne Zuordnung",      -3 },
  { "Classic",              0 },
  { "Burning Crusade",      1 },
  { "Wrath of the Lich King", 2 },
  { "Cataclysm",            3 },
  { "Mists of Pandaria",    4 },
  { "Warlords of Draenor",  5 },
  { "Legion",               6 },
  { "Battle for Azeroth",   7 },
  { "Shadowlands",          8 },
  { "Dragonflight",         9 },
  { "The War Within",      10 },
  { "Midnight",            11 }
};

// Item.ClassID 4 is armour; SubclassID then says which material. Both columns are declared
// in database.xml without store="no", so they exist in the cache already -- no schema bump,
// no database rebuild. Weapons and everything else fall under "Alle Arten".
//
// -99 follows kExpansions' sentinel convention. Miscellaneous armour (ClassID 4,
// SubclassID 0: rings, trinkets, cloaks) is deliberately not offered as its own entry --
// it is not an armour class a player thinks in.
const struct { const char* label; int subclass; } kArmorClasses[] = {
  { "Alle Arten", -99 },
  { "Stoff",        1 },
  { "Leder",        2 },
  { "Kette",        3 },
  { "Platte",       4 }
};

const struct { const char* label; int id; } kQualities[] = {
  { "Alle Qualitäten", -1 },
  { "Schlecht",         0 },
  { "Gewöhnlich",       1 },
  { "Ungewöhnlich",     2 },
  { "Selten",           3 },
  { "Episch",           4 },
  { "Legendär",         5 },
  { "Artefakt",         6 },
  { "Erbstück",         7 }
};

// The colours WoW itself uses, same table as the character panel's equipment list.
const char* qualityColour(int q)
{
  switch (q) {
    case 0:  return "#9d9d9d";
    case 1:  return "#e8eaee";
    case 2:  return "#1eff00";
    case 3:  return "#0070dd";
    case 4:  return "#a335ee";
    case 5:  return "#ff8000";
    case 6:  return "#e6cc80";
    case 7:  return "#00ccff";
    default: return "#7d8693";
  }
}

QString comboStyle()
{
  return QString(
    "QComboBox { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:3px 7px; color:%3; }"
    "QComboBox::drop-down { border:none; width:16px; }"
    "QComboBox QAbstractItemView { background:%1; border:1px solid %2;"
    " selection-background-color:#181510; color:%3; }")
    .arg(kCard).arg(kBord).arg(kText);
}

QLabel* chip(const QString& text, bool active)
{
  auto* l = new QLabel(text);
  l->setFont(QFont(uiFamily(), 8));
  l->setAlignment(Qt::AlignCenter);
  l->setCursor(Qt::PointingHandCursor);
  l->setStyleSheet(active
    ? QString("color:%1; background:#191509; border:1px solid #3a3222;"
              " border-radius:9px; padding:3px 10px;").arg(kAccent)
    : QString("color:%1; background:#12161b; border:1px solid %2;"
              " border-radius:9px; padding:3px 10px;").arg(kMuted).arg(kBord));
  return l;
}

// SQL string literals come from the user's search box, so the quote has to go.
QString sqlEscape(QString s)
{
  return s.replace('\'', ' ').replace('"', ' ');
}

// Item.InventoryType -> the heading the item is filed under. Built from kSlots so the
// grouping and the filter dropdown can never drift apart. Types kSlots does not list
// (rings, trinkets, bags) carry no appearance and are already excluded by the query.
const QHash<int, QString>& slotHeadings()
{
  static const QHash<int, QString> map = [] {
    QHash<int, QString> m;
    for (const auto& s : kSlots) {
      const QString types = QString::fromLatin1(s.types);
      if (types.isEmpty())
        continue;                       // the "Alle Slots" entry has no type of its own
      for (const QString& t : types.split(',', QString::SkipEmptyParts))
        m.insert(t.toInt(), QString::fromUtf8(s.label));
    }
    return m;
  }();
  return map;
}

// The order headings appear in: the same order as the filter dropdown, which runs
// head to foot and then weapons -- how someone dresses a character.
int slotRank(const QString& heading)
{
  for (int i = 0; i < (int)(sizeof(kSlots) / sizeof(kSlots[0])); ++i)
    if (heading == QString::fromUtf8(kSlots[i].label))
      return i;
  return 999;
}
}

ItemBrowser::ItemBrowser(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background:transparent;");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(12, 0, 12, 8);
  col->setSpacing(7);

  // Items / Sets
  auto* modes = new QWidget;
  modes->setStyleSheet("background:transparent;");
  auto* mr = new QHBoxLayout(modes);
  mr->setContentsMargins(0, 0, 0, 0);
  mr->setSpacing(5);
  itemsChip_ = chip(QString::fromUtf8("Einzelteile"), true);
  setsChip_  = chip(QString::fromUtf8("Sets"), false);
  itemsChip_->setProperty("browserMode", 0);
  setsChip_->setProperty("browserMode", 1);
  itemsChip_->installEventFilter(this);
  setsChip_->installEventFilter(this);
  mr->addWidget(itemsChip_);
  mr->addWidget(setsChip_);
  mr->addStretch(1);
  col->addWidget(modes);

  search_ = new QLineEdit;
  search_->setPlaceholderText(QString::fromUtf8("Name oder Item-ID suchen …"));
  search_->setFont(QFont(uiFamily(), 8));
  search_->setFixedHeight(28);
  search_->setStyleSheet(QString(
    "QLineEdit { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:0 8px; color:%3; }"
    "QLineEdit:focus { border-color:#3a434f; }").arg(kCard).arg(kBord).arg(kText));
  // Typing filters straight away. Every keystroke would mean a query against a table
  // of 110k rows, so the query waits until the typing pauses; Enter skips the wait.
  searchDelay_ = new QTimer(this);
  searchDelay_->setSingleShot(true);
  searchDelay_->setInterval(250);
  connect(searchDelay_, &QTimer::timeout, this, [this]() { refresh(); });
  connect(search_, &QLineEdit::textChanged, this, [this](const QString&) {
    searchDelay_->start();
  });
  connect(search_, &QLineEdit::returnPressed, this, [this]() {
    searchDelay_->stop();
    refresh();
  });
  col->addWidget(search_);

  slot_ = new QComboBox;
  slot_->setFont(QFont(uiFamily(), 8));
  slot_->setStyleSheet(comboStyle());
  for (const auto& s : kSlots)
    slot_->addItem(QString::fromUtf8(s.label), QString::fromLatin1(s.types));
  connect(slot_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { refresh(); });
  col->addWidget(slot_);

  expansion_ = new QComboBox;
  expansion_->setFont(QFont(uiFamily(), 8));
  expansion_->setStyleSheet(comboStyle());
  for (const auto& e : kExpansions)
    expansion_->addItem(QString::fromUtf8(e.label), e.id);
  connect(expansion_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { refresh(); });
  col->addWidget(expansion_);

  armor_ = new QComboBox;
  armor_->setFont(QFont(uiFamily(), 8));
  armor_->setStyleSheet(comboStyle());
  for (const auto& a : kArmorClasses)
    armor_->addItem(QString::fromUtf8(a.label), a.subclass);
  connect(armor_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { refresh(); });
  col->addWidget(armor_);

  quality_ = new QComboBox;
  quality_->setFont(QFont(uiFamily(), 8));
  quality_->setStyleSheet(comboStyle());
  for (const auto& q : kQualities)
    quality_->addItem(QString::fromUtf8(q.label), q.id);
  connect(quality_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { refresh(); });
  col->addWidget(quality_);

  standalone_ = new QCheckBox(QString::fromUtf8("Nur Item, ohne Figur"));
  standalone_->setFont(QFont(uiFamily(), 8));
  standalone_->setStyleSheet(QString(
    "QCheckBox { color:%1; background:transparent; spacing:7px; }"
    "QCheckBox::indicator { width:13px; height:13px; border-radius:3px;"
    " border:1px solid %2; background:%3; }"
    "QCheckBox::indicator:checked { background:%4; border-color:%4; }")
    .arg(kMuted).arg(kBord).arg(kCard).arg(kAccent));
  standalone_->setToolTip(QString::fromUtf8(
    "Zeigt das eigene Modell des Items. Nur Kopf, Schulter, Umhang und Waffen haben "
    "eines -- Brust, Beine, Hände und so weiter sind Texturen auf dem Körper und "
    "brauchen die Figur."));
  col->addWidget(standalone_);

  // Sets mode only: without this a set click always WIPES the current outfit first,
  // which makes mixing two sets impossible.
  keepEquip_ = new QCheckBox(QString::fromUtf8("Vorhandene Ausrüstung behalten"));
  keepEquip_->setFont(QFont(uiFamily(), 8));
  keepEquip_->setStyleSheet(standalone_->styleSheet());
  keepEquip_->setVisible(false);
  col->addWidget(keepEquip_);

  count_ = new QLabel;
  QFont cf(uiFamily(), 7);
  cf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
  count_->setFont(cf);
  count_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  col->addWidget(count_);

  list_ = new QListWidget;
  list_->setFont(QFont(uiFamily(), 8));
  list_->setStyleSheet(QString(
    "QListWidget { background:transparent; border:none; outline:none; }"
    "QListWidget::item { padding:3px 4px; border-radius:4px; }"
    "QListWidget::item:hover { background:#181d23; }"
    "QListWidget::item:selected { background:#181510; }"
    "QScrollBar:vertical { background:transparent; width:10px; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:5px; min-height:30px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }"));
  connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) {
    if (!it)
      return;
    const int id = it->data(Qt::UserRole).toInt();
    if (setMode_)
      emit setActivated(id, keepEquip_->isChecked());
    else
      emit itemActivated(id, standalone_->isChecked());
  });
  col->addWidget(list_, 1);
}

void ItemBrowser::initialise()
{
  ready_ = true;
  refresh();
}

void ItemBrowser::setMode(bool sets)
{
  setMode_ = sets;
  itemsChip_->setStyleSheet(chip("", !sets)->styleSheet());
  setsChip_->setStyleSheet(chip("", sets)->styleSheet());
  // Sets are named collections; slot, armour class and quality do not apply to them.
  slot_->setEnabled(!sets);
  armor_->setEnabled(!sets);
  expansion_->setEnabled(!sets);
  quality_->setEnabled(!sets);
  standalone_->setVisible(!sets);
  keepEquip_->setVisible(sets);
  refresh();
}

bool ItemBrowser::eventFilter(QObject* obj, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonRelease) {
    const QVariant m = obj->property("browserMode");
    if (m.isValid()) {
      setMode(m.toInt() == 1);
      return true;
    }
  }
  return QWidget::eventFilter(obj, e);
}

void ItemBrowser::refresh()
{
  if (!ready_)
    return;
  if (setMode_)
    refreshSets();
  else
    refreshItems();
}

void ItemBrowser::refreshItems()
{
  list_->clear();

  QStringList where;
  where << "Item.InventoryType != 0" << "ItemSparse.Display_Lang <> ''";

  const QString types = slot_->currentData().toString();
  if (!types.isEmpty())
    where << QString("Item.InventoryType IN (%1)").arg(types);

  const int exp = expansion_->currentData().toInt();
  if (exp != -99)
    where << QString("ItemSparse.ExpansionID = %1").arg(exp);

  const int qual = quality_->currentData().toInt();
  if (qual != -1)
    where << QString("ItemSparse.OverallQualityID = %1").arg(qual);

  const int armor = armor_->currentData().toInt();
  if (armor != -99)
    where << QString("Item.ClassID = 4 AND Item.SubclassID = %1").arg(armor);

  // A search that is only digits is meant as an item id -- that is how someone pastes
  // an id out of a link or a log. The name match stays in the OR so a numeric NAME
  // (there are items called "1000 Years of Polish") is still reachable.
  const QString needle = sqlEscape(search_->text().trimmed());
  if (!needle.isEmpty()) {
    const bool numeric = QRegularExpression("^\\d+$").match(needle).hasMatch();
    where << (numeric
      ? QString("(Item.ID = %1 OR ItemSparse.Display_Lang LIKE '%%%1%%')").arg(needle)
      : QString("ItemSparse.Display_Lang LIKE '%%%1%%'").arg(needle));
  }

  const QString from = "FROM Item LEFT JOIN ItemSparse ON Item.ID = ItemSparse.ID WHERE "
                       + where.join(" AND ");

  // Counted separately so the footer can say how much the cap is hiding rather than
  // silently showing the first 500 as if they were everything.
  sqlResult total = GAMEDATABASE.sqlQuery("SELECT COUNT(*) " + from);
  const int nTotal = (total.valid && !total.values.empty()) ? total.values[0][0].toInt() : 0;

  sqlResult r = GAMEDATABASE.sqlQuery(
    "SELECT Item.ID, ItemSparse.Display_Lang, ItemSparse.OverallQualityID, "
    "ItemSparse.ItemLevel, Item.InventoryType " + from +
    QString(" ORDER BY ItemSparse.Display_Lang LIMIT %1").arg(kMaxRows));

  // Without a slot filter the list used to be one alphabetical run in which a helmet
  // sat between two pairs of boots. Group it under slot headings instead; with a slot
  // already chosen the heading would say the same thing on every row, so it is left off.
  const bool group = types.isEmpty();
  std::map<int, std::vector<const std::vector<QString>*>> bySlot;   // slotRank -> rows

  if (r.valid) {
    for (const auto& row : r.values) {
      if (group) {
        const QString heading = slotHeadings().value(row[4].toInt());
        bySlot[slotRank(heading)].push_back(&row);
      } else {
        addItemRow(row);
      }
    }
  }

  for (const auto& entry : bySlot) {
    const QString heading = slotHeadings().value(entry.second.front()->at(4).toInt());
    auto* head = new QListWidgetItem(heading.toUpper());
    QFont hf(uiFamily(), 7);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    head->setFont(hf);
    head->setForeground(QColor(kDim));
    head->setFlags(Qt::NoItemFlags);      // a heading is not a thing one can equip
    list_->addItem(head);
    for (const auto* row : entry.second)
      addItemRow(*row);
  }

  // Counted from the query, not from the widget: the widget also holds the headings.
  const int shown = r.valid ? (int)r.values.size() : 0;
  count_->setText(nTotal > shown
    ? QString::fromUtf8("%1 VON %2 TREFFERN").arg(shown).arg(nTotal)
    : QString::fromUtf8("%1 TREFFER").arg(nTotal));
}

void ItemBrowser::addItemRow(const std::vector<QString>& row)
{
  auto* item = new QListWidgetItem(row[1]);
  item->setData(Qt::UserRole, row[0].toInt());
  item->setForeground(QColor(qualityColour(row[2].toInt())));
  const int ilvl = row[3].toInt();
  item->setToolTip(QString::fromUtf8("Item %1%2")
                     .arg(row[0])
                     .arg(ilvl > 1 ? QString::fromUtf8(" · Stufe %1").arg(ilvl) : QString()));
  list_->addItem(item);
}

void ItemBrowser::refreshSets()
{
  list_->clear();

  QString where = "Name_Lang <> ''";
  const QString needle = sqlEscape(search_->text().trimmed());
  if (!needle.isEmpty())
    where += QString(" AND Name_Lang LIKE '%%%1%%'").arg(needle);

  // ItemSet has no quality to colour by -- and "quality of the first item" would be
  // one extra lookup per row and a lie for mixed sets. The piece count is honest and
  // computable from the columns this query already returns.
  QString itemCols;
  for (int i = 1; i <= 17; ++i)
    itemCols += QString(", ItemID%1").arg(i);
  sqlResult r = GAMEDATABASE.sqlQuery(
    "SELECT ID, Name_Lang" + itemCols + " FROM ItemSet WHERE " + where +
    " ORDER BY Name_Lang");

  if (r.valid) {
    for (const auto& row : r.values) {
      int pieces = 0;
      for (int i = 2; i < (int)row.size(); ++i)
        if (row[i].toInt() > 0)
          ++pieces;
      auto* item = new QListWidgetItem(
        QString::fromUtf8("%1  · %2 Teile").arg(row[1]).arg(pieces));
      item->setData(Qt::UserRole, row[0].toInt());
      item->setForeground(QColor(kText));
      item->setToolTip(QString::fromUtf8("Set %1").arg(row[0]));
      list_->addItem(item);
    }
  }

  count_->setText(QString::fromUtf8("%1 SETS").arg(list_->count()));
}
