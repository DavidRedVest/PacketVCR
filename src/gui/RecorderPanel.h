#pragma once

#include "Recorder.h"

#include <QWidget>
#include <chrono>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QTimer;

namespace gui {

class RecorderPanel : public QWidget {
    Q_OBJECT

public:
    explicit RecorderPanel(QWidget* parent = nullptr);
    ~RecorderPanel() override;

private slots:
    void onStartStopClicked();
    void onBrowseClicked();
    void onStatsTimer();

private:
    void appendLog(const QString& line);
    void setUiRunning(bool running);

    QLineEdit* addressEdit_;
    QSpinBox* portSpin_;
    QComboBox* interfaceCombo_;
    QLineEdit* outputPathEdit_;
    QPushButton* browseButton_;
    QPushButton* startStopButton_;
    QLabel* statsLabel_;
    QPlainTextEdit* logView_;
    QTimer* statsTimer_;

    core::Recorder recorder_;
    uint64_t lastByteCount_ = 0;
    std::chrono::steady_clock::time_point lastStatsTime_;
};

} // namespace gui
