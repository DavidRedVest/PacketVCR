#pragma once

#include "Player.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QTimer;

namespace gui {

class PlayerPanel : public QWidget {
    Q_OBJECT

public:
    explicit PlayerPanel(QWidget* parent = nullptr);
    ~PlayerPanel() override;

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onBrowseClicked();
    void onStatsTimer();

private:
    void appendLog(const QString& line);
    void setUiState(bool running, bool paused);

    QLineEdit* filePathEdit_;
    QPushButton* browseButton_;
    QLineEdit* destAddressEdit_;
    QSpinBox* destPortSpin_;
    QComboBox* interfaceCombo_;
    QSpinBox* ttlSpin_;
    QDoubleSpinBox* speedSpin_;
    QCheckBox* loopCheck_;
    QPushButton* playPauseButton_;
    QPushButton* stopButton_;
    QLabel* statsLabel_;
    QProgressBar* progressBar_;
    QPlainTextEdit* logView_;
    QTimer* statsTimer_;

    core::Player player_;
};

} // namespace gui
