#pragma once

#include <QComboBox>
#include <cstdint>

// Populates a QComboBox with the host's network interfaces (via
// net::listInterfaces(), not QNetworkInterface -- core/net stay Qt-free)
// for use as a "which NIC to join/send multicast on" picker.
namespace gui {

// Adds an "Any / Default" entry (ipv4Address == 0, i.e. let the OS pick)
// followed by one entry per multicast-capable, up interface.
void populateInterfaceCombo(QComboBox* combo);

// Host-order IPv4 address stored as the currently-selected item's data.
uint32_t selectedInterfaceIp(const QComboBox* combo);

} // namespace gui
