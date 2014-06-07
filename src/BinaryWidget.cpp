#include <QListWidget>
#include <QHBoxLayout>
#include <QStackedLayout>

#include "Util.h"
#include "BinaryWidget.h"

#include "panes/Pane.h"
#include "panes/ArchPane.h"
#include "panes/ProgramPane.h"
#include "panes/StringsPane.h"

BinaryWidget::BinaryWidget(FormatPtr fmt) : fmt{fmt} {
  createLayout();
  setup();
}

void BinaryWidget::createLayout() {
  listWidget = new QListWidget;
  listWidget->setFixedWidth(150);
  connect(listWidget, &QListWidget::currentRowChanged,
          this, &BinaryWidget::onModeChanged);

  stackLayout = new QStackedLayout;
  stackLayout->setContentsMargins(0, 0, 0, 0);
  
  auto *layout = new QHBoxLayout;
  layout->setContentsMargins(5, 5, 5, 5);
  layout->addWidget(listWidget);
  layout->addLayout(stackLayout);
  
  setLayout(layout);
}

void BinaryWidget::onModeChanged(int row) {
  stackLayout->setCurrentIndex(row);
}

void BinaryWidget::setup() {
  foreach (const auto obj, fmt->getObjects()) {
    auto *archPane = new ArchPane(obj);
    QString cpuStr = Util::cpuTypeString(obj->getCpuType()),
      cpuSubStr = Util::cpuTypeString(obj->getCpuSubType());
    addPane(tr("%1 (%2)").arg(cpuStr).arg(cpuSubStr), archPane);

    SectionPtr sec = obj->getSection(SectionType::Text);
    if (sec) {
      auto *progPane = new ProgramPane(obj, sec);
      addPane(sec->getName(), progPane, 1);
    }

    sec = obj->getSection(SectionType::String);
    if (sec) {
      auto *strPane = new StringsPane(obj, sec);
      addPane(sec->getName(), strPane, 1);
    }

    foreach (auto sec, obj->getSectionsByType(SectionType::CString)) {
      auto *cstrPane = new StringsPane(obj, sec);
      addPane(sec->getName(), cstrPane, 1);
    }
  }

  if (listWidget->count() > 0) {
    listWidget->setCurrentRow(0);
  }
}

void BinaryWidget::addPane(const QString &title, Pane *pane, int level) {
  panes << pane;
  listWidget->addItem(QString(level * 4, ' ') + title);
  stackLayout->addWidget(pane); 
}
