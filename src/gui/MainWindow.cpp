#include "MainWindow.h"

#include "PlayerPanel.h"
#include "RecorderPanel.h"

#include <QStatusBar>
#include <QTabWidget>

namespace gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("UDP Stream Recorder / Player"));
    resize(720, 560);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(new RecorderPanel(tabs), tr("Recorder"));
    tabs->addTab(new PlayerPanel(tabs), tr("Player"));
    setCentralWidget(tabs);

    statusBar()->showMessage(tr("Ready"));
}

} // namespace gui
