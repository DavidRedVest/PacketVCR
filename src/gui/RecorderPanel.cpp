#include "RecorderPanel.h"

#include "InterfaceModel.h"
#include "IPv4Address.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace gui {

namespace {
QString formatBytes(uint64_t bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}
} // namespace

RecorderPanel::RecorderPanel(QWidget* parent) : QWidget(parent) {
    addressEdit_ = new QLineEdit("224.1.1.4", this);
    addressEdit_->setToolTip(tr("Multicast group (e.g. 224.1.1.4) or a unicast bind address (e.g. 0.0.0.0)"));

    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(6010);

    interfaceCombo_ = new QComboBox(this);
    populateInterfaceCombo(interfaceCombo_);

    outputPathEdit_ = new QLineEdit(this);
    browseButton_ = new QPushButton(tr("Browse..."), this);

    auto* outputRow = new QWidget(this);
    auto* outputLayout = new QHBoxLayout(outputRow);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->addWidget(outputPathEdit_);
    outputLayout->addWidget(browseButton_);

    auto* form = new QFormLayout;
    form->addRow(tr("Address:"), addressEdit_);
    form->addRow(tr("Port:"), portSpin_);
    form->addRow(tr("Interface:"), interfaceCombo_);
    form->addRow(tr("Output file:"), outputRow);

    startStopButton_ = new QPushButton(tr("Start Recording"), this);
    statsLabel_ = new QLabel(tr("packets=0  bytes=0  bitrate=0 Mbps"), this);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(2000);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(startStopButton_);
    layout->addWidget(statsLabel_);
    layout->addWidget(logView_, /*stretch=*/1);

    statsTimer_ = new QTimer(this);
    statsTimer_->setInterval(300);
    connect(statsTimer_, &QTimer::timeout, this, &RecorderPanel::onStatsTimer);
    connect(startStopButton_, &QPushButton::clicked, this, &RecorderPanel::onStartStopClicked);
    connect(browseButton_, &QPushButton::clicked, this, &RecorderPanel::onBrowseClicked);

    // Fires on the Recorder's worker thread; marshal to the GUI thread
    // before touching any widget.
    recorder_.setLogCallback([this](const std::string& msg) {
        const QString line = QString::fromStdString(msg);
        QMetaObject::invokeMethod(this, [this, line] { appendLog(line); }, Qt::QueuedConnection);
    });
}

RecorderPanel::~RecorderPanel() {
    recorder_.stop();
}

void RecorderPanel::onBrowseClicked() {
    QString path = QFileDialog::getSaveFileName(this, tr("Choose output file"), outputPathEdit_->text(), tr("pcap files (*.pcap)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(".pcap", Qt::CaseInsensitive)) {
        path += ".pcap";
    }
    outputPathEdit_->setText(path);
}

void RecorderPanel::onStartStopClicked() {
    if (recorder_.isRunning()) {
        recorder_.stop();
        statsTimer_->stop();
        setUiRunning(false);
        return;
    }

    uint32_t ip = 0;
    if (!net::parseIPv4(addressEdit_->text().toStdString(), ip)) {
        QMessageBox::warning(this, tr("Invalid address"), tr("Enter a valid IPv4 address, e.g. 224.1.1.4"));
        return;
    }
    if (outputPathEdit_->text().isEmpty()) {
        QMessageBox::warning(this, tr("Missing output file"), tr("Choose an output .pcap file first"));
        return;
    }

    core::RecorderConfig config;
    config.bindOrGroupIp = ip;
    config.port = static_cast<uint16_t>(portSpin_->value());
    config.localInterfaceIp = selectedInterfaceIp(interfaceCombo_);
    config.outputPath = outputPathEdit_->text().toStdString();

    lastByteCount_ = 0;
    lastStatsTime_ = std::chrono::steady_clock::now();

    if (!recorder_.start(config)) {
        QMessageBox::warning(this, tr("Failed to start"), tr("Could not start recording; see log for details"));
        return;
    }

    setUiRunning(true);
    statsTimer_->start();
}

void RecorderPanel::onStatsTimer() {
    if (!recorder_.isRunning()) {
        statsTimer_->stop();
        setUiRunning(false);
        return;
    }

    const auto stats = recorder_.stats();
    const auto now = std::chrono::steady_clock::now();
    const double elapsedS = std::chrono::duration<double>(now - lastStatsTime_).count();
    const double bitrateMbps = elapsedS > 0.0 ? (static_cast<double>(stats.byteCount - lastByteCount_) * 8.0) / elapsedS / 1e6 : 0.0;
    lastByteCount_ = stats.byteCount;
    lastStatsTime_ = now;

    statsLabel_->setText(tr("packets=%1  bytes=%2  bitrate=%3 Mbps")
                              .arg(stats.packetCount)
                              .arg(formatBytes(stats.byteCount))
                              .arg(bitrateMbps, 0, 'f', 2));
}

void RecorderPanel::setUiRunning(bool running) {
    startStopButton_->setText(running ? tr("Stop Recording") : tr("Start Recording"));
    addressEdit_->setEnabled(!running);
    portSpin_->setEnabled(!running);
    interfaceCombo_->setEnabled(!running);
    outputPathEdit_->setEnabled(!running);
    browseButton_->setEnabled(!running);
}

void RecorderPanel::appendLog(const QString& line) {
    logView_->appendPlainText(line);
}

} // namespace gui
