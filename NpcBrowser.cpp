#include "Theme.h"
#include "NpcBrowser.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

#include "Game.h"
#include "GameDatabase.h"

namespace {
// Same cap as ItemBrowser, same reasoning: past a few hundred rows the widgets cost more
// than the query, and nobody scrolls that far. The footer says what was cut.
const int kMaxRows = 500;

// Everything the viewer needs travels on the row itself, so activation is a read of the
// clicked item and never a second query.
enum {
  kRoleCreature = Qt::UserRole,       // Creature.ID
  kRoleDisplay,                       // Creature.DisplayID1
  kRoleFileData                       // CreatureModelData.FileDataID (the .m2)
};

QString uiFamily()
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : {QStringLiteral("IBM Plex Sans"), QStringLiteral("Segoe UI")})
    if (have.contains(f))
      return f;
  return QStringLiteral("sans-serif");
}

QString comboStyle()
{
  return QString(
    "QComboBox { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:3px 7px; color:%3; }"
    "QComboBox::drop-down { border:none; width:16px; }"
    "QComboBox QAbstractItemView { background:%1; border:1px solid %2;"
    " selection-background-color:%4; color:%3; }")
    .arg(tok::kCard).arg(tok::kBorder).arg(tok::kText).arg(tok::kAccentSel);
}

// SQL string literals come from the user's search box. The quote is DOUBLED, not
// stripped like in ItemBrowser: apostrophes are the normal case in NPC names -- O'ros,
// Zul'jin, Kel'Thuzad -- and stripping would turn every such search into a miss.
// '' is SQLite's escape inside a '...' literal, which also keeps the literal closed
// against injection; a double quote is an ordinary character there and needs nothing.
QString sqlEscape(QString s)
{
  return s.replace('\'', QStringLiteral("''"));
}
}

NpcBrowser::NpcBrowser(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background:transparent;");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(12, 0, 12, 8);
  col->setSpacing(7);

  search_ = new QLineEdit;
  search_->setPlaceholderText(QString::fromUtf8("NPC-Name …"));
  search_->setFont(QFont(uiFamily(), 8));
  search_->setFixedHeight(28);
  search_->setStyleSheet(QString(
    "QLineEdit { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:0 8px; color:%3; }"
    "QLineEdit:focus { border-color:%4; }")
    .arg(tok::kCard).arg(tok::kBorder).arg(tok::kText).arg(tok::kAccentBr));
  // No debounce timer here, unlike ItemBrowser: Creature holds 23k named rows against
  // Item's 139k, and the three-way join runs on indexed keys, so a query per keystroke
  // is cheap enough to keep the code simpler.
  connect(search_, &QLineEdit::textChanged, this, [this](const QString&) { refresh(); });
  connect(search_, &QLineEdit::returnPressed, this, [this]() {
    refresh();
    // A single hit means the search NAMED one NPC; Enter opening it directly turns
    // "type name, press Enter" into the whole workflow. Only ever ambiguous-free:
    // with the 500-row cap, a list of one can only come from a total of one.
    if (list_->count() == 1)
      emitRow(list_->item(0));
  });
  col->addWidget(search_);

  type_ = new QComboBox;
  type_->setFont(QFont(uiFamily(), 8));
  type_->setStyleSheet(comboStyle());
  // Filled in initialise() from the CreatureType table -- 15 rows the database already
  // localises, so hardcoding a copy here would only give it a chance to go stale.
  connect(type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { refresh(); });
  col->addWidget(type_);

  count_ = new QLabel;
  QFont cf(uiFamily(), 7);
  cf.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
  count_->setFont(cf);
  count_->setStyleSheet(QString("color:%1; background:transparent;").arg(tok::kDim));
  col->addWidget(count_);

  list_ = new QListWidget;
  list_->setFont(QFont(uiFamily(), 8));
  // Big enough to recognise a silhouette, small enough that rows stay rows.
  list_->setIconSize(QSize(24, 24));
  list_->setStyleSheet(QString(
    "QListWidget { background:transparent; border:none; outline:none; }"
    "QListWidget::item { padding:3px 4px; border-radius:4px; }"
    "QListWidget::item:hover { background:%1; }"
    "QListWidget::item:selected { background:%2; }"
    "QScrollBar:vertical { background:transparent; width:10px; }"
    "QScrollBar::handle:vertical { background:%3; border-radius:5px; min-height:30px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }")
    .arg(tok::kRaised).arg(tok::kAccentSel).arg(tok::kRaised2));
  connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) {
    emitRow(it);
  });
  col->addWidget(list_, 1);
}

void NpcBrowser::initialise()
{
  // Unlike ItemBrowser's initialise this one POPULATES a widget, so a second call must
  // be a no-op or every creature type would appear twice in the dropdown.
  if (ready_)
    return;

  type_->addItem(QString::fromUtf8("Alle Typen"), -1);
  // The game's own type names, in the game's own language -- the same words the user
  // knows from the tooltip of every NPC in the world.
  sqlResult types = GAMEDATABASE.sqlQuery(
    "SELECT ID, Name_Lang FROM CreatureType WHERE Name_Lang <> '' ORDER BY Name_Lang");
  if (types.valid)
    for (const auto& row : types.values)
      type_->addItem(row[1], row[0].toInt());

  ready_ = true;
  refresh();
}

QString NpcBrowser::thumbPath(int displayId)
{
  // Keyed by DISPLAY id, not creature id: creatures sharing a display share the look,
  // so one captured image serves every one of them. Relative on purpose -- the whole
  // application opens userSettings relative to the executable (see main.cpp).
  return QString("userSettings/npc-thumbs/%1.png").arg(displayId);
}

void NpcBrowser::refresh()
{
  if (!ready_)
    return;

  list_->clear();

  QStringList where;
  // DisplayID1 = 0 is data's way of saying "nothing to show" (triggers, controllers,
  // invisible helpers); a row the viewer cannot open would be a dead entry. The inner
  // joins below quietly drop broken display chains for the same reason.
  where << "Creature.DisplayID1 != 0" << "Creature.Name_Lang <> ''";

  const int type = type_->currentData().toInt();
  if (type != -1)
    where << QString("Creature.CreatureType = %1").arg(type);

  const QString needle = sqlEscape(search_->text().trimmed());
  if (!needle.isEmpty())
    where << QString("Creature.Name_Lang LIKE '%%%1%%'").arg(needle);

  const QString from =
    "FROM Creature "
    "JOIN CreatureDisplayInfo ON CreatureDisplayInfo.ID = Creature.DisplayID1 "
    "JOIN CreatureModelData ON CreatureModelData.ID = CreatureDisplayInfo.ModelID "
    "WHERE " + where.join(" AND ");

  // Counted separately so the footer can say how much the cap is hiding rather than
  // silently showing the first 500 as if they were everything.
  sqlResult total = GAMEDATABASE.sqlQuery("SELECT COUNT(*) " + from);
  const int nTotal = (total.valid && !total.values.empty()) ? total.values[0][0].toInt() : 0;

  sqlResult r = GAMEDATABASE.sqlQuery(
    "SELECT Creature.ID, Creature.Name_Lang, Creature.DisplayID1, "
    "CreatureModelData.FileDataID " + from +
    QString(" ORDER BY Creature.Name_Lang LIMIT %1").arg(kMaxRows));

  if (r.valid) {
    for (const auto& row : r.values) {
      auto* item = new QListWidgetItem(row[1]);
      item->setData(kRoleCreature, row[0].toInt());
      item->setData(kRoleDisplay, row[2].toInt());
      item->setData(kRoleFileData, row[3].toInt());
      item->setForeground(QColor(tok::kText));
      decorate(item, row[2].toInt());
      list_->addItem(item);
    }
  }

  const int shown = r.valid ? (int)r.values.size() : 0;
  count_->setText(nTotal > shown
    ? QString::fromUtf8("%1 VON %2 TREFFERN").arg(shown).arg(nTotal)
    : QString::fromUtf8("%1 TREFFER").arg(nTotal));
}

void NpcBrowser::decorate(QListWidgetItem* item, int displayId)
{
  const QString path = thumbPath(displayId);
  if (QFileInfo::exists(path)) {
    item->setIcon(QIcon(path));
    // A tooltip that LOOKS like plain text but starts with a tag is rendered as rich
    // text, which makes it the cheapest large preview there is: no popup widget, no
    // event filter, just the image the viewer already saved. Absolute path, because
    // the tooltip renderer resolves relative ones against whatever the current
    // directory happens to be at hover time.
    // Double quotes around src: a Windows path may legally contain an apostrophe
    // (D:\O'Neill\...), which would end a single-quoted attribute early -- but it can
    // never contain a double quote.
    item->setToolTip(QString("<img src=\"%1\" width=\"192\">")
                       .arg(QFileInfo(path).absoluteFilePath()));
  } else {
    // Honest about why there is no image yet: previews are captured the first time an
    // NPC is shown, never in bulk -- rendering 120k displays up front is not a start-up
    // cost anyone should pay.
    item->setToolTip(QString::fromUtf8("Noch ohne Vorschau — einmal anzeigen"));
  }
}

void NpcBrowser::refreshThumb(int displayId)
{
  if (!QFileInfo::exists(thumbPath(displayId)))
    return;
  // Every matching row, not the first: different creatures share a display id, and all
  // of them just gained the same preview. The list is capped at 500, so the walk is cheap.
  for (int i = 0; i < list_->count(); ++i) {
    QListWidgetItem* it = list_->item(i);
    if (it->data(kRoleDisplay).toInt() == displayId)
      decorate(it, displayId);
  }
}

void NpcBrowser::emitRow(QListWidgetItem* item)
{
  if (!item)
    return;
  emit npcActivated(item->data(kRoleCreature).toInt(),
                    item->data(kRoleDisplay).toInt(),
                    item->data(kRoleFileData).toInt(),
                    item->text());
}
