#include "InterfaceModel.h"

#include "IPv4Address.h"
#include "NetworkInterfaceInfo.h"

#include <QString>

namespace gui {

void populateInterfaceCombo(QComboBox* combo) {
    combo->clear();
    combo->addItem(QObject::tr("Any / Default"), QVariant(static_cast<uint>(0)));

    for (const auto& iface : net::listInterfaces()) {
        if (!iface.isUp || !iface.supportsMulticast || iface.ipv4Address == 0) {
            continue;
        }
        const QString label =
            QString::fromStdString(iface.name) + " (" + QString::fromStdString(net::formatIPv4(iface.ipv4Address)) + ")";
        combo->addItem(label, QVariant(static_cast<uint>(iface.ipv4Address)));
    }
}

uint32_t selectedInterfaceIp(const QComboBox* combo) {
    return static_cast<uint32_t>(combo->currentData().toUInt());
}

} // namespace gui
