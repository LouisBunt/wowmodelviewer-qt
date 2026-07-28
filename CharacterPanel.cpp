#include "CharacterPanel.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "CharDetails.h"
#include "CharDetailsEvent.h"
#include "Game.h"
#include "GameDatabase.h"
#include "WoWModel.h"

namespace {
const char* kText = "#e8eaee";
const char* kSoft = "#b6bdc8";
const char* kDim  = "#5f6874";
const char* kCard = "#14181e";
const char* kBord = "#23282f";

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
      auto nameQuery = GAMEDATABASE.sqlQuery(
        QString("SELECT Name_Lang FROM ChrCustomizationChoice WHERE ID = %1").arg(choices[i]));
      QString choiceName;
      if (nameQuery.valid && !nameQuery.values.empty())
        choiceName = nameQuery.values[0][0];
      if (choiceName.isEmpty())
        choiceName = QString::number(i + 1);   // many choices are unnamed colours

      combo->addItem(choiceName, choices[i]);
      if (choices[i] == current)
        currentIndex = (int)i;
    }
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
