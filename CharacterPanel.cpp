#include "CharacterPanel.h"

#include <QCheckBox>
#include <QEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include "logger/Logger.h"
#include "CharDetails.h"
#include "CharDetailsEvent.h"
#include "Game.h"
#include "GameDatabase.h"
#include "GameFile.h"
#include "WoWItem.h"
#include "TabardDetails.h"
#include "WoWModel.h"
#include "database.h"
#include "modelheaders.h"

namespace {
const char* kText = "#e8eaee";
const char* kSoft = "#b6bdc8";
const char* kDim  = "#5f6874";
const char* kCard = "#14181e";
const char* kBord = "#23282f";
const char* kAccent = "#c8a15a";

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

// The app configures no log sink (the wx front-end does that via LOGGER.addChild),
// so LOG_INFO goes nowhere. Write diagnostics where they are actually readable.
void note(const QString& s)
{
  // Was a hardcoded path under the development checkout, so on any other machine the
  // trace went nowhere -- exactly where it would be useful. Same location main() uses.
  static bool dirReady = false;
  if (!dirReady) {
    QDir().mkpath("userSettings");
    dirReady = true;
  }
  QFile f("userSettings/qt-frontend-trace.txt");
  if (f.open(QIODevice::Append | QIODevice::Text))
    QTextStream(&f) << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << "  " << s << "\n";
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

  // Demon hunter mode: only Night Elves and Blood Elves have the extra geosets
  // (blindfold, horns, tattoos), so the toggle stays disabled for everyone else.
  dhMode_ = new QCheckBox(QString::fromUtf8("Dämonenjäger"));
  dhMode_->setFont(QFont(uiFamily(), 8));
  dhMode_->setStyleSheet(QString(
    "QCheckBox { color:%1; background:transparent; spacing:7px; }"
    "QCheckBox::indicator { width:13px; height:13px; border-radius:3px;"
    " border:1px solid %2; background:%3; }"
    "QCheckBox::indicator:checked { background:%4; border-color:%4; }"
    "QCheckBox:disabled { color:#414852; }").arg(kSoft).arg(kBord).arg(kCard).arg(kAccent));
  connect(dhMode_, &QCheckBox::toggled, this, [this](bool on) {
    if (updating_ || !model_)
      return;
    model_->cd.setDemonHunterMode(on);
    emit customizationChanged();
  });
  col->addWidget(dhMode_);

  // Guild tabard. Five indices into the tabard tables; the model composes the
  // texture from them.
  tabardHeader_ = new QLabel(QString::fromUtf8("WAPPENROCK"));
  tabardHeader_->setFont(hf);
  tabardHeader_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  col->addWidget(tabardHeader_);

  tabardRows_ = new QVBoxLayout;
  tabardRows_->setSpacing(6);
  col->addLayout(tabardRows_);

  // Geoset visibility. These sat in a "Material" tab of their own, labelled with the
  // raw geoset number -- which told nobody that 1301 is the trouser leg. They belong
  // next to the rest of the character, under the name of the part they hide.
  geosetHeader_ = new QLabel(QString::fromUtf8("SICHTBARE TEILE"));
  geosetHeader_->setFont(hf);
  geosetHeader_->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
  col->addWidget(geosetHeader_);

  geosetRows_ = new QVBoxLayout;
  geosetRows_->setSpacing(4);
  col->addLayout(geosetRows_);

  col->addStretch(1);

  header_->setText(QString::fromUtf8("ANPASSUNG"));
  subHeader_->setText(QString::fromUtf8("Kein Charaktermodell geladen."));
}

namespace {
// Geoset ids are group * 100 + variant: 1301 is variant 1 of group 13, the trousers.
// The group is what a person recognises, so it names the row and the variant is just
// the numbered alternative within it.
QString geosetGroupName(int group)
{
  switch (group) {
    case CG_SKIN_OR_HAIR:    return QString::fromUtf8("Haar");
    case CG_FACE_1:          return QString::fromUtf8("Gesicht (Bart 1)");
    case CG_FACE_2:          return QString::fromUtf8("Gesicht (Bart 2)");
    case CG_FACE_3:          return QString::fromUtf8("Gesicht (Bart 3)");
    case CG_GLOVES:          return QString::fromUtf8("Handschuhe");
    case CG_BOOTS:           return QString::fromUtf8("Stiefel");
    case CG_TAIL:            return QString::fromUtf8("Schweif");
    case CG_EARS:            return QString::fromUtf8("Ohren");
    case CG_SLEEVES:         return QString::fromUtf8("Ärmel");
    case CG_KNEEPADS:        return QString::fromUtf8("Knieschoner");
    case CG_CHEST:           return QString::fromUtf8("Oberteil");
    case CG_PANTS:           return QString::fromUtf8("Hose");
    case CG_TABARD:          return QString::fromUtf8("Wappenrock");
    case CG_TROUSERS:        return QString::fromUtf8("Beinkleid");
    case CG_DH_LOINCLOTH:    return QString::fromUtf8("Lendentuch");
    case CG_CLOAK:           return QString::fromUtf8("Umhang");
    case CG_EYEGLOW:         return QString::fromUtf8("Augenglühen");
    case CG_BELT:            return QString::fromUtf8("Gürtel");
    case CG_BONE:            return QString::fromUtf8("Bart / Knochen");
    case CG_FEET:            return QString::fromUtf8("Füße");
    case CG_TORSO:           return QString::fromUtf8("Torso");
    case CG_HAND_ATTACHMENT: return QString::fromUtf8("Handanbau");
    case CG_HEAD_ATTACHMENT: return QString::fromUtf8("Kopfanbau");
    case CG_DH_BLINDFOLDS:   return QString::fromUtf8("Augenbinde");
    default:                 return QString::fromUtf8("Gruppe %1").arg(group);
  }
}
}

void CharacterPanel::setGeosetModel(WoWModel* model)
{
  geosetModel_ = model;
  buildGeosets();
}

void CharacterPanel::buildGeosets()
{
  while (QLayoutItem* item = geosetRows_->takeAt(0)) {
    if (QWidget* w = item->widget())
      w->deleteLater();
    delete item;
  }

  // A heading with nothing under it reads like something failed to load. There is no
  // geoset list without a model, so the whole section goes away with it.
  if (!geosetModel_ || geosetModel_->geosets.empty()) {
    geosetHeader_->setVisible(false);
    return;
  }
  geosetHeader_->setVisible(true);

  // Saved rather than forced back to false: this also runs from refresh(), which may
  // already be inside an update -- clearing the flag there would drop the reentrancy
  // guard of the whole panel halfway through.
  const bool wasUpdating = updating_;
  updating_ = true;
  int shown = 0, total = 0;
  for (size_t i = 0; i < geosetModel_->geosets.size(); ++i) {
    ModelGeosetHD* g = geosetModel_->geosets[i];
    if (!g)
      continue;
    ++total;

    const int group = g->id / 100;
    const int variant = g->id % 100;
    auto* cb = new QCheckBox(variant > 0
      ? QString::fromUtf8("%1 %2").arg(geosetGroupName(group)).arg(variant)
      : geosetGroupName(group));
    cb->setFont(QFont(uiFamily(), 8));
    cb->setStyleSheet(QString(
      "QCheckBox { color:%1; background:transparent; spacing:7px; }"
      "QCheckBox::indicator { width:13px; height:13px; border-radius:3px;"
      " border:1px solid %2; background:%3; }"
      "QCheckBox::indicator:checked { background:%4; border-color:%4; }")
      .arg(kSoft).arg(kBord).arg(kCard).arg(kAccent));
    cb->setChecked(g->display);
    cb->setToolTip(QString::fromUtf8("Geoset %1 · %2 Dreiecke").arg(g->id).arg(g->icount / 3));
    // Captures the INDEX, not the ModelGeosetHD*. The vector is rebuilt whenever the
    // model merges or unmerges an item's geometry, which would leave a captured pointer
    // dangling; an index is re-checked against the current vector on every click.
    //
    // showGeoset() rather than writing g->display: it is the model's own accessor and
    // keeps the flag consistent with the merged-model bookkeeping. Deliberately NO
    // model refresh here -- refresh() recomputes every display flag from the equipment
    // and would undo the click on the spot.
    const int index = (int)i;
    connect(cb, &QCheckBox::toggled, this, [this, index](bool on) {
      if (updating_ || !geosetModel_ || index >= (int)geosetModel_->geosets.size())
        return;
      geosetModel_->showGeoset((uint)index, on);
    });
    geosetRows_->addWidget(cb);
    if (g->display)
      ++shown;
  }
  updating_ = wasUpdating;

  geosetHeader_->setText(QString::fromUtf8("SICHTBARE TEILE · %1 / %2").arg(shown).arg(total));
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

  // A WoWModel has no item slots of its own -- the front-end creates one WoWItem per
  // slot and adds it as a child (modelviewer.cpp does this right after loading a
  // character). Without them getItem() returns null for every slot and equipping is
  // a silent no-op.
  if (model_->begin() == model_->end()) {
    for (const auto& s : { CS_SHIRT, CS_HEAD, CS_SHOULDER, CS_PANTS, CS_BOOTS,
                           CS_CHEST, CS_TABARD, CS_BELT, CS_BRACERS, CS_GLOVES,
                           CS_HAND_RIGHT, CS_HAND_LEFT, CS_CAPE, CS_QUIVER })
      model_->addChild(new WoWItem(s));
    note("created item slots for the model");
  }

  rebuild();
  buildEquipment();
  buildTabard();

  // Only night elves and blood elves have demon hunter geosets.
  const int r = model_->infos.raceID;
  updating_ = true;
  dhMode_->setEnabled(r == RACE_NIGHTELF || r == RACE_BLOODELF);
  dhMode_->setChecked(model_->cd.isDemonHunter());
  updating_ = false;

  // CharControl::UpdateModel ends the same way. The item slots created above are new
  // and empty, and cd.reset() ran while they did not exist yet, so the model has to be
  // composed once now that it is fully set up.
  model_->refresh();
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
              const uint choiceId = combo->itemData(idx).toUInt();
              model_->cd.set(optionId, choiceId);
              emit customizationChanged();
              checkPostureVariant(choiceId);
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

void CharacterPanel::buildTabard()
{
  tabardSpins_.clear();
  while (QLayoutItem* item = tabardRows_->takeAt(0)) {
    if (QWidget* w = item->widget())
      w->deleteLater();
    delete item;
  }
  if (!model_)
    return;

  TabardDetails& td = model_->td;

  // label, current value, max, setter
  const struct { const char* label; int value; int max; int which; } parts[] = {
    { "Emblem",        td.getIcon(),        td.GetMaxIcon(),                        0 },
    { "Emblemfarbe",   td.getIconColor(),   td.GetMaxIconColor(td.getIcon()),       1 },
    { "Rand",          td.getBorder(),      td.GetMaxBorder(),                      2 },
    { "Randfarbe",     td.getBorderColor(), td.GetMaxBorderColor(td.getBorder()),   3 },
    { "Hintergrund",   td.getBackground(),  td.GetMaxBackground(),                  4 }
  };

  updating_ = true;
  for (const auto& p : parts) {
    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rr = new QHBoxLayout(row);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(8);

    auto* name = new QLabel(QString::fromUtf8(p.label));
    name->setFont(QFont(uiFamily(), 8));
    name->setFixedWidth(88);
    name->setStyleSheet(QString("color:%1; background:transparent;").arg(kSoft));
    rr->addWidget(name);

    auto* spin = new QSpinBox;
    spin->setRange(0, p.max > 0 ? p.max : 0);
    spin->setValue(p.value);
    spin->setFont(QFont(uiFamily(), 8));
    spin->setStyleSheet(QString(
      "QSpinBox { background:%1; border:1px solid %2; border-radius:6px;"
      " padding:2px 6px; color:%3; }").arg(kCard).arg(kBord).arg(kText));
    const int which = p.which;
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, which](int v) {
      if (updating_ || !model_)
        return;
      TabardDetails& t = model_->td;
      switch (which) {
        case 0: t.setIcon(v); break;
        case 1: t.setIconColor(v); break;
        case 2: t.setBorder(v); break;
        case 3: t.setBorderColor(v); break;
        case 4: t.setBackground(v); break;
      }
      model_->refresh();
      buildTabard();      // the colour ranges depend on the chosen emblem/border
      emit customizationChanged();
    });
    rr->addWidget(spin, 1);
    tabardRows_->addWidget(row);
    tabardSpins_.push_back(spin);
  }
  updating_ = false;
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

    // Item view: show only this piece, the figure and the rest switched off. Toggles,
    // so a second click brings the character back.
    auto* focus = new QLabel(QString::fromUtf8("◉"));
    focus->setFont(QFont(uiFamily(), 8));
    focus->setCursor(Qt::PointingHandCursor);
    focus->setToolTip(QString::fromUtf8("Nur dieses Teil zeigen"));
    focus->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
    focus->setProperty("focusSlot", (int)s.slot);
    focus->installEventFilter(this);
    focus->setVisible(false);
    rr->addWidget(focus);

    // The only way to take a single piece OFF used to be equipping something else
    // over it -- clearing always meant everything at once. Same clickable-label
    // pattern the main window uses for its category chips.
    auto* clear = new QLabel(QString::fromUtf8("×"));
    clear->setFont(QFont(uiFamily(), 9));
    clear->setCursor(Qt::PointingHandCursor);
    clear->setToolTip(QString::fromUtf8("Ablegen"));
    clear->setStyleSheet(QString("color:%1; background:transparent;").arg(kDim));
    clear->setProperty("charSlot", (int)s.slot);
    clear->installEventFilter(this);
    clear->setVisible(false);
    rr->addWidget(clear);

    equipRows_->addWidget(row);
    slotLabels_.push_back(itemName);
    clearButtons_.push_back(clear);
    focusButtons_.push_back(focus);
  }

  refreshEquipment();
}

bool CharacterPanel::eventFilter(QObject* obj, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonRelease) {
    const QVariant slot = obj->property("charSlot");
    if (slot.isValid()) {
      unequipSlot(slot.toInt());
      return true;
    }
    const QVariant focus = obj->property("focusSlot");
    if (focus.isValid()) {
      // Toggle: clicking the piece that is already alone brings everyone back.
      const int want = focus.toInt();
      setItemFocus(focusSlot_ == want ? -1 : want);
      return true;
    }
  }
  return QWidget::eventFilter(obj, e);
}

void CharacterPanel::unequipSlot(int slot)
{
  if (!model_ || slot < 0 || slot >= NUM_CHAR_SLOTS)
    return;
  WoWItem* item = model_->getItem((CharSlots)slot);
  if (!item || item->id() == 0)
    return;

  note(QString("unequip slot %1 (was item %2)").arg(slot).arg(item->id()));
  // Same order clearEquipment() uses: change the item, recompose the model once,
  // then re-read the panel from the result.
  item->setId(0);
  model_->refresh();
  refreshEquipment();
  emit customizationChanged();
}

void CharacterPanel::refreshEquipment()
{
  if (!model_)
    return;

  int i = 0;
  for (const auto& s : kSlots) {
    if (i >= (int)slotLabels_.size())
      break;
    QLabel* lbl = slotLabels_[i];
    QLabel* clear = i < (int)clearButtons_.size() ? clearButtons_[i] : nullptr;
    QLabel* focus = i < (int)focusButtons_.size() ? focusButtons_[i] : nullptr;
    ++i;

    WoWItem* item = model_->getItem(s.slot);
    const bool worn = item && item->id() != 0;
    if (clear)
      clear->setVisible(worn);       // an "x" next to an empty slot removes nothing
    if (focus) {
      // Only offered where there IS geometry to look at: texture-only armour would
      // switch the body off and leave an empty viewport.
      const bool showable = worn && item &&
                            (!item->models().empty() || item->mergedModel() != nullptr);
      focus->setVisible(showable);
      const bool active = (focusSlot_ == (int)s.slot);
      focus->setStyleSheet(QString("color:%1; background:transparent;")
                             .arg(active ? kAccent : kDim));
      focus->setToolTip(active ? QString::fromUtf8("Wieder alles zeigen")
                               : QString::fromUtf8("Nur dieses Teil zeigen"));
    }
    if (!worn) {
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
  note(QString("equipById %1 -> type=%2 slot=%3").arg(itemId).arg(rec.type).arg(slot));

  if (slot < 0 || slot >= NUM_CHAR_SLOTS) {
    note(QString("equipById: item %1 resolved to slot %2 -- not equipped").arg(itemId).arg(slot));
    return;
  }

  WoWItem* probe = model_->getItem((CharSlots)slot);
  note(QString("equipById: getItem(%1) -> %2").arg(slot).arg(probe ? "ok" : "NULL"));

  if (WoWItem* item = probe) {
    item->setId(itemId);
    // setId only records the choice; the attachments and the composite skin are
    // rebuilt by refresh(). Without it the item is "worn" but nothing is drawn.
    model_->refresh();
    refreshEquipment();
    emit customizationChanged();
  }
}

void CharacterPanel::setItemFocus(int slot)
{
  if (!model_)
    return;

  // The rule itself lives in WoWModel::applyItemFocus, which refresh() calls last --
  // that is the only place it survives the visibility recomputation every equipment
  // change triggers. Here it is just the switch and the panel's own repaint.
  focusSlot_ = slot;
  model_->setItemFocus(slot);
  note(QString("item focus -> %1").arg(slot < 0 ? QString("aus") : QString::number(slot)));
  refreshEquipment();
  emit customizationChanged();
  emit itemFocusChanged(slot);
}

bool CharacterPanel::slotHasOwnModel(int slot) const
{
  if (!model_)
    return false;
  for (auto it = model_->begin(); it != model_->end(); ++it) {
    WoWItem* item = *it;
    if (item && item->slot() == slot && item->id() != 0 &&
        (!item->models().empty() || item->mergedModel() != nullptr))
      return true;
  }
  return false;
}

void CharacterPanel::refresh()
{
  // Before the character-only early-out: geosets belong to every model, and the list
  // has to be re-read because WoWModel::refreshMerging() throws the whole geoset vector
  // away and builds a new one whenever equipment is merged in. Stale rows would then
  // carry indices that no longer mean what the label says.
  buildGeosets();

  if (!model_)
    return;

  rebuild();
  refreshEquipment();
  buildTabard();

  updating_ = true;
  dhMode_->setChecked(model_->cd.isDemonHunter());
  updating_ = false;
}

void CharacterPanel::clearEquipment()
{
  if (!model_)
    return;

  for (int slot = 0; slot < NUM_CHAR_SLOTS; ++slot)
    if (WoWItem* item = model_->getItem((CharSlots)slot))
      item->setId(0);

  model_->refresh();
  refreshEquipment();
  emit customizationChanged();
}

void CharacterPanel::randomise()
{
  if (!model_)
    return;

  // randomise() batches internally, exactly like reset() -- the pickers are rebuilt
  // once afterwards rather than per option.
  model_->cd.randomise();
  refresh();
  emit customizationChanged();
}

// Conditional-model customizations: a choice that does not change the current model but
// replaces it. There are exactly two in the game data -- ChrCustomizationElement has two
// rows with ChrCustomizationCondModelID != 0, both the male orc's upright posture (Orc and
// Mag'har) -- and wow.dll has never implemented them, which is why switching the posture
// did nothing at all.
//
// ChrCustomizationCondModel itself has no definition in database.xml and no .dbd, so the
// id cannot be resolved to a file through the database. The target is derived from the
// current model's own name instead and then checked against the archive, so a wrong guess
// declines to act rather than loading something arbitrary.
void CharacterPanel::checkPostureVariant(uint choiceId)
{
  if (!model_ || !model_->gamefile)
    return;

  auto r = GAMEDATABASE.sqlQuery(
    QString("SELECT ChrCustomizationCondModelID FROM ChrCustomizationElement "
            "WHERE ChrCustomizationChoiceID = %1").arg(choiceId));
  const bool wantsVariant = r.valid && !r.values.empty() && r.values[0][0].toInt() != 0;

  const QString current = model_->gamefile->fullname();
  const QString kVariant = "upright.m2";
  const QString kBase = "_hd.m2";

  QString wanted;
  if (wantsVariant) {
    if (current.endsWith(kVariant, Qt::CaseInsensitive))
      return;                                   // already on it
    if (!current.endsWith(kBase, Qt::CaseInsensitive))
      return;                                   // not a shape we know how to transform
    wanted = current.left(current.length() - kBase.length()) + kVariant;
  } else {
    if (!current.endsWith(kVariant, Qt::CaseInsensitive))
      return;                                   // already on the base model
    wanted = current.left(current.length() - kVariant.length()) + kBase;
  }

  if (!GAMEDIRECTORY.getFile(wanted)) {
    note(QString("posture: %1 does not exist -- staying on %2").arg(wanted).arg(current));
    return;
  }

  note(QString("posture: %1 -> %2").arg(current).arg(wanted));
  emit postureModelRequested(wanted);
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

  // And the MODEL has to be rebuilt too. CharDetails::set() deliberately does not do
  // that itself -- it fires this event and expects the listener to, which is what
  // CharControl::onEvent does under wx. Without it a customization change updated
  // cd.textures but never re-composed the character's skin texture, so picking a skin
  // colour, face or hairstyle changed nothing on screen -- and an armory import, which
  // sets a dozen options in a row, left the character with a stale composite: armour
  // drawn, body and head untextured.
  //
  // reset()/randomise() apply a choice to EVERY option in one batched pass and refresh
  // exactly once at the end; refreshing per event during a batch would redo every
  // texture, geoset and item per option and defeat that.
  if (!model_->cd.isBatching())
    model_->refresh();
}
