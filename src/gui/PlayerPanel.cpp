#include "PlayerPanel.h"

#include "InterfaceModel.h"
#include "IPv4Address.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace gui {

PlayerPanel::PlayerPanel(QWidget* parent) : QWidget(parent) {
    filePathEdit_ = new QLineEdit(this);
    browseButton_ = new QPushButton(tr("Browse..."), this);
    auto* fileRow = new QWidget(this);
    auto* fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->addWidget(filePathEdit_);
    fileLayout->addWidget(browseButton_);

    destAddressEdit_ = new QLineEdit("224.1.1.4", this);
    destPortSpin_ = new QSpinBox(this);
    destPortSpin_->setRange(1, 65535);
    destPortSpin_->setValue(6010);

    interfaceCombo_ = new QComboBox(this);
    populateInterfaceCombo(interfaceCombo_);

    ttlSpin_ = new QSpinBox(this);
    ttlSpin_->setRange(1, 255);
    ttlSpin_->setValue(1);
    ttlSpin_->setToolTip(tr("Multicast TTL; packets won't leave the local subnet at TTL=1"));

    speedSpin_ = new QDoubleSpinBox(this);
    speedSpin_->setRange(0.1, 10.0);
    speedSpin_->setSingleStep(0.1);
    speedSpin_->setValue(1.0);
    speedSpin_->setSuffix("x");

    loopCheck_ = new QCheckBox(tr("Loop playback"), this);

    auto* form = new QFormLayout;
    form->addRow(tr("Input file:"), fileRow);
    form->addRow(tr("Destination address:"), destAddressEdit_);
    form->addRow(tr("Destination port:"), destPortSpin_);
    form->addRow(tr("Interface:"), interfaceCombo_);
    form->addRow(tr("TTL:"), ttlSpin_);
    form->addRow(tr("Speed:"), speedSpin_);
    form->addRow(QString(), loopCheck_);

    playPauseButton_ = new QPushButton(tr("Play"), this);
    stopButton_ = new QPushButton(tr("Stop"), this);
    stopButton_->setEnabled(false);

    auto* buttonRow = new QWidget(this);
    auto* buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(playPauseButton_);
    buttonLayout->addWidget(stopButton_);

    statsLabel_ = new QLabel(tr("0/0 packets sent"), this);
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(2000);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttonRow);
    layout->addWidget(statsLabel_);
    layout->addWidget(progressBar_);
    layout->addWidget(logView_, /*stretch=*/1);

    statsTimer_ = new QTimer(this);
    statsTimer_->setInterval(200);
    connect(statsTimer_, &QTimer::timeout, this, &PlayerPanel::onStatsTimer);
    connect(playPauseButton_, &QPushButton::clicked, this, &PlayerPanel::onPlayPauseClicked);
    connect(stopButton_, &QPushButton::clicked, this, &PlayerPanel::onStopClicked);
    connect(browseButton_, &QPushButton::clicked, this, &PlayerPanel::onBrowseClicked);

    player_.setLogCallback([this](const std::string& msg) {
        const QString line = QString::fromStdString(msg);
        QMetaObject::invokeMethod(this, [this, line] { appendLog(line); }, Qt::QueuedConnection);
    });
}

PlayerPanel::~PlayerPanel() {
    player_.stop();
}

void PlayerPanel::onBrowseClicked() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose pcap file"), filePathEdit_->text(), tr("pcap files (*.pcap)"));
    if (!path.isEmpty()) {
        filePathEdit_->setText(path);
    }
}

void PlayerPanel::onPlayPauseClicked() {
    if (!player_.isRunning()) {
        uint32_t ip = 0;
        if (!net::parseIPv4(destAddressEdit_->text().toStdString(), ip)) {
            QMessageBox::warning(this, tr("Invalid address"), tr("Enter a valid IPv4 destination address"));
            return;
        }
        if (filePathEdit_->text().isEmpty()) {
            QMessageBox::warning(this, tr("Missing input file"), tr("Choose a .pcap file to replay first"));
            return;
        }

        core::PlayerConfig config;
        config.inputPath = filePathEdit_->text().toStdString();
        config.destIp = ip;
        config.destPort = static_cast<uint16_t>(destPortSpin_->value());
        config.localInterfaceIp = selectedInterfaceIp(interfaceCombo_);
        config.multicastTtl = static_cast<uint8_t>(ttlSpin_->value());
        config.speedMultiplier = speedSpin_->value();
        config.loop = loopCheck_->isChecked();

        if (!player_.start(config)) {
            QMessageBox::warning(this, tr("Failed to start"), tr("Could not start playback; see log for details"));
            return;
        }
        setUiState(true, false);
        statsTimer_->start();
        return;
    }

    if (player_.isPaused()) {
        player_.resume();
        setUiState(true, false);
    } else {
        player_.pause();
        setUiState(true, true);
    }
}

void PlayerPanel::onStopClicked() {
    player_.stop();
    statsTimer_->stop();
    setUiState(false, false);
    progressBar_->setValue(0);
}

void PlayerPanel::onStatsTimer() {
    if (!player_.isRunning()) {
        statsTimer_->stop();
        setUiState(false, false);
        return;
    }

    const auto stats = player_.stats();
    progressBar_->setRange(0, static_cast<int>(stats.totalPackets));
    progressBar_->setValue(static_cast<int>(stats.currentIndex));
    statsLabel_->setText(tr("%1/%2 packets sent, %3 bytes")
                              .arg(stats.currentIndex)
                              .arg(stats.totalPackets)
                              .arg(stats.bytesSent));
}

void PlayerPanel::setUiState(bool running, bool paused) {
    filePathEdit_->setEnabled(!running);
    browseButton_->setEnabled(!running);
    destAddressEdit_->setEnabled(!running);
    destPortSpin_->setEnabled(!running);
    interfaceCombo_->setEnabled(!running);
    ttlSpin_->setEnabled(!running);
    speedSpin_->setEnabled(!running);
    loopCheck_->setEnabled(!running);
    stopButton_->setEnabled(running);
    playPauseButton_->setText(!running ? tr("Play") : (paused ? tr("Resume") : tr("Pause")));
}

void PlayerPanel::appendLog(const QString& line) {
    logView_->appendPlainText(line);
}

} // namespace gui
