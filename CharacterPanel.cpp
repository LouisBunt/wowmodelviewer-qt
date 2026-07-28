#include "CharacterPanel.h"

#include <QComboBox>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "CharDetails.h"
#include "CharDetailsEvent.h"
#include "Game.h"
#include "GameDatabase.h"
#include "WoWItem.h"
#include "WoWModel.h"
#include "database.h"

namespace {
const char* kText = "#e8eaee";
const char* kSoft = "#b6bdc8";
const char* kDim  = "#5f6874";
const char* kCard = "#14181e";
const char* kBord = "#23282f";

// ChrCustomizationChoice.SwatchColor is packed 0xAARRGGBB. Build the same little
// swatch the wx panel drew: a solid fill, a left/right split for dual-colour
// choices, or a crossed-out box for "no colour".
QIcon makeSwatch(unsigned int c0, unsigned int c1)
{
  const int w = 34, h = 14;
  QPixmap pm(w, h);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, false);
  auto toCol = [](unsigned int c) {
    return QColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
  };

  if (c0 == 0 && c1 == 0) {                 // "none"
    p.fillRect(0, 0, w, h, QColor("#1c222a"));
    p.setPen(QColor("#5f6874"));
    p.drawLine(0, h - 1, w - 1, 0);
  } else if (c1 != 0) {                     // dual colour
    p.fillRect(0, 0, w / 2, h, toCol(c0));
    p.fillRect(w / 2, 0, w - w / 2, h, toCol(c1));
  } else {                                  // single colour
    p.fillRect(0, 0, w, h, toCol(c0));
  }
  p.setPen(QColor("#23282f"));
  p.drawRect(0, 0, w - 1, h - 1);
  p.end();

  return QIcon(pm);
}

QString uiFamily()
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : {QStringLiteral("IBM Plex Sans"), QStringLiteral("Segoe UI")})
    if (have.contains(f))
      return f;
  return QStringLiteral("sans-serif");
}
}

CharacterPanel::CharacterPanel(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background:transparent;");

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(14);

  header_ = new QLabel;
  QFont hf(uiFamily(), 7);
  hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
  header_->setFont(hf);
  header_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  col->addWidget(header_);

  subHeader_ = new QLabel;
  subHeader_->setFont(QFont(uiFamily(), 8));
  subHeader_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  subHeader_->setWordWrap(true);
  col->addWidget(subHeader_);

  rows_ = new QVBoxLayout;
  rows_->setSpacing(10);
  col->addLayout(rows_);

  equipHeader_ = new QLabel;
  equipHeader_->setFont(hf);
  equipHeader_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  equipHeader_->setText(QString::fromUtf8("AUSRÜSTUNG"));
  col->addWidget(equipHeader_);

  // Equipping by item id is the lowest common denominator: the armory importer,
  // a wowhead item link and manual entry all end up handing over an id.
  itemInput_ = new QLineEdit;
  itemInput_->setPlaceholderText(QString::fromUtf8("Item-ID anlegen …"));
  itemInput_->setFont(QFont(uiFamily(), 8));
  itemInput_->setStyleSheet(QString(
    "QLineEdit { background:%1; border:1px solid %2; border-radius:6px;"
    " padding:4px 8px; color:%3; }").arg(kCard).arg(kBord).arg(kText));
  connect(itemInput_, &QLineEdit::returnPressed, this, [this]() {
    bool ok = false;
    const int id = itemInput_->text().trimmed().toInt(&ok);
    if (ok && id > 0) {
      equipById(id);
      itemInput_->clear();
    }
  });
  col->addWidget(itemInput_);

  equipRows_ = new QVBoxLayout;
  equipRows_->setSpacing(6);
  col->addLayout(equipRows_);

  col->addStretch(1);

  header_->setText(QString::fromUtf8("ANPASSUNG"));
  subHeader_->setText(QString::fromUtf8("Kein Charaktermodell geladen."));
}

void CharacterPanel::clearRows()
{
  combos_.clear();
  while (QLayoutItem* item = rows_->takeAt(0)) {
    if (QWidget* w = item->widget())
      w->deleteLater();
    delete item;
  }
}

void CharacterPanel::setModel(WoWModel* model)
{
  model_ = model;
  if (!model_) {
    clearRows();
    subHeader_->setText(QString::fromUtf8("Kein Charaktermodell geladen."));
    return;
  }

  // A raw M2 is not a character: without this setup only the always-visible geosets
  // draw and nothing is textured (which is exactly what Phase 1 produced).
  model_->cd.showEars = true;
  model_->cd.showHair = true;
  model_->cd.showFacialHair = true;
  model_->cd.showUnderwear = true;
  model_->cd.attach(this);

  // reset() fills the customization map and applies a default to every option. It
  // batches internally -- set() would otherwise trigger a full model refresh per
  // option, which on models with dozens of options is a multi-second freeze.
  model_->cd.reset(model_);

  rebuild();
  buildEquipment();
}

void CharacterPanel::rebuild()
{
  clearRows();

  if (!model_ || model_->infos.ChrModelID.empty()) {
    subHeader_->setText(QString::fromUtf8("Dieses Modell hat keine Charakterdaten."));
    return;
  }

  const int chrModelId = model_->infos.ChrModelID[0];
  subHeader_->setText(QString::fromUtf8("Rasse %1 · ChrModel %2")
                        .arg(model_->infos.raceID).arg(chrModelId));

  // Every option of this ChrModel, in the game's own order. Deliberately unfiltered:
  // filtering on ChrCustomizationID drops legitimate options on mixed models (the wx
  // panel lost Face/Hair/Eye Colour on the Dracthyr visage that way).
  auto options = GAMEDATABASE.sqlQuery(
    QString("SELECT ID, Name_Lang FROM ChrCustomizationOption WHERE ChrModelID = %1 ORDER BY OrderIndex")
      .arg(chrModelId));

  if (!options.valid || options.values.empty()) {
    subHeader_->setText(QString::fromUtf8("Keine Anpassungsoptionen gefunden."));
    return;
  }

  updating_ = true;
  for (auto& opt : options.values) {
    const uint optionId = opt[0].toUInt();
    const QString label = opt.size() > 1 && !opt[1].isEmpty() ? opt[1]
                                                              : QString("Option %1").arg(optionId);

    const std::vector<uint> choices = model_->cd.getCustomizationChoices(optionId);
    if (choices.empty())
      continue;

    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rc = new QVBoxLayout(row);
    rc->setContentsMargins(0, 0, 0, 0);
    rc->setSpacing(5);

    auto* head = new QHBoxLayout;
    auto* name = new QLabel(label);
    name->setFont(QFont(uiFamily(), 9));
    name->setStyleSheet(QString("color:%1; background:transparent;").arg(kSoft));
    auto* count = new QLabel(QString::number(choices.size()));
    count->setFont(QFont(uiFamily(), 8));
    count->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
    head->addWidget(name);
    head->addStretch(1);
    head->addWidget(count);
    rc->addLayout(head);

    auto* combo = new QComboBox;
    combo->setFont(QFont(uiFamily(), 8));
    combo->setStyleSheet(QString(
      "QComboBox { background:%1; border:1px solid %2; border-radius:6px;"
      " padding:4px 8px; color:%3; }"
      "QComboBox::drop-down { border:none; width:18px; }"
      "QComboBox QAbstractItemView { background:%1; border:1px solid %2;"
      " selection-background-color:#181510; color:%3; }")
      .arg(kCard).arg(kBord).arg(kText));

    const uint current = model_->cd.get(optionId);
    int currentIndex = 0;
    for (size_t i = 0; i < choices.size(); ++i) {
      auto q = GAMEDATABASE.sqlQuery(
        QString("SELECT Name_Lang, SwatchColor1, SwatchColor2 FROM ChrCustomizationChoice WHERE ID = %1")
          .arg(choices[i]));

      QString choiceName;
      unsigned int c0 = 0, c1 = 0;
      if (q.valid && !q.values.empty()) {
        choiceName = q.values[0][0];
        // Stored signed; reinterpret the bits rather than clamping at zero.
        if (q.values[0].size() > 2) {
          c0 = (unsigned int)q.values[0][1].toInt();
          c1 = (unsigned int)q.values[0][2].toInt();
        }
      }

      const bool isColour = (c0 != 0 || c1 != 0);
      if (choiceName.isEmpty())
        choiceName = isColour ? QString() : QString::number(i + 1);

      if (isColour)
        combo->addItem(makeSwatch(c0, c1), choiceName, choices[i]);
      else
        combo->addItem(choiceName, choices[i]);

      if (choices[i] == current)
        currentIndex = (int)i;
    }
    combo->setIconSize(QSize(34, 14));
    combo->setCurrentIndex(currentIndex);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, combo, optionId](int idx) {
              if (updating_ || !model_ || idx < 0)
                return;
              model_->cd.set(optionId, combo->itemData(idx).toUInt());
              emit customizationChanged();
            });

    rc->addWidget(combo);
    rows_->addWidget(row);
    combos_.push_back(combo);
  }
  updating_ = false;

  header_->setText(QString::fromUtf8("ANPASSUNG · %1").arg(combos_.size()));
}

namespace {
// The slots the design's equipment grid shows, in its order.
const struct { CharSlots slot; const char* label; } kSlots[] = {
  { CS_HEAD,       "Kopf" },     { CS_SHOULDER, "Schulter" },
  { CS_CHEST,      "Brust" },    { CS_GLOVES,   "Hände" },
  { CS_BELT,       "Gürtel" },   { CS_PANTS,    "Beine" },
  { CS_BOOTS,      "Füße" },     { CS_CAPE,     "Umhang" },
  { CS_HAND_RIGHT, "Waffe" },    { CS_HAND_LEFT, "Schildhand" },
  { CS_SHIRT,      "Hemd" },     { CS_TABARD,   "Wappenrock" },
  { CS_BRACERS,    "Armschienen" }
};

// Item quality -> the colours WoW itself uses, matching the mock-up's grid.
const char* qualityColour(int quality)
{
  switch (quality) {
    case 0:  return "#9d9d9d";   // poor
    case 1:  return "#e8eaee";   // common
    case 2:  return "#1eff00";   // uncommon
    case 3:  return "#0070dd";   // rare
    case 4:  return "#a335ee";   // epic
    case 5:  return "#ff8000";   // legendary
    case 6:  return "#e6cc80";   // artifact
    case 7:  return "#00ccff";   // heirloom
    default: return "#7d8693";
  }
}
}

void CharacterPanel::buildEquipment()
{
  slotLabels_.clear();
  while (QLayoutItem* item = equipRows_->takeAt(0)) {
    if (QWidget* w = item->widget())
      w->deleteLater();
    delete item;
  }

  if (!model_)
    return;

  for (const auto& s : kSlots) {
    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rr = new QHBoxLayout(row);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(8);

    auto* slotName = new QLabel(QString::fromUtf8(s.label).toUpper());
    QFont sf(uiFamily(), 7);
    sf.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
    slotName->setFont(sf);
    slotName->setFixedWidth(88);
    slotName->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
    rr->addWidget(slotName);

    auto* itemName = new QLabel(QString::fromUtf8("—"));
    itemName->setFont(QFont(uiFamily(), 8));
    itemName->setMinimumWidth(1);
    itemName->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
    rr->addWidget(itemName, 1);

    equipRows_->addWidget(row);
    slotLabels_.push_back(itemName);
  }

  refreshEquipment();
}

void CharacterPanel::refreshEquipment()
{
  if (!model_)
    return;

  int i = 0;
  for (const auto& s : kSlots) {
    if (i >= (int)slotLabels_.size())
      break;
    QLabel* lbl = slotLabels_[i++];

    WoWItem* item = model_->getItem(s.slot);
    if (!item || item->id() == 0) {
      lbl->setText(QString::fromUtf8("—"));
      lbl->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
      continue;
    }

    const QString name = item->name();
    lbl->setText(name.isEmpty() ? QString::number(item->id()) : name);
    lbl->setStyleSheet(QString("color:%1; background:transparent;")
                         .arg(qualityColour(item->quality())));
  }

  int worn = 0;
  for (const auto& s : kSlots) {
    WoWItem* it = model_->getItem(s.slot);
    if (it && it->id() != 0)
      ++worn;
  }
  equipHeader_->setText(QString::fromUtf8("AUSRÜSTUNG · %1").arg(worn));
}

void CharacterPanel::equipById(int itemId)
{
  if (!model_ || itemId <= 0)
    return;

  // Same route the wx control took: look the record up, let it name its own slot.
  ItemRecord rec = items.getById(itemId);
  const int slot = rec.slot();
  if (slot < 0 || slot >= NUM_CHAR_SLOTS)
    return;

  if (WoWItem* item = model_->getItem((CharSlots)slot)) {
    item->setId(itemId);
    // setId only records the choice; the attachments and the composite skin are
    // rebuilt by refresh(). Without it the item is "worn" but nothing is drawn.
    model_->refresh();
    refreshEquipment();
    emit customizationChanged();
  }
}

void CharacterPanel::onEvent(Event* e)
{
  if (!e || !model_)
    return;

  // CharDetails reshuffles its choice lists when a parent option changes (and when
  // demon-hunter mode toggles), so the pickers have to be rebuilt rather than just
  // re-read.
  if (e->type() == CharDetailsEvent::CHOICE_LIST_CHANGED ||
      e->type() == CharDetailsEvent::DH_MODE_CHANGED)
    rebuild();
}
