#include "NetworkModel.h"

void NetworkModel::clear() {
    m_devices.clear();
    m_links.clear();
    m_nextDeviceId = 1;
    m_nextLinkId = 1;
}

int NetworkModel::addDevice(const QString& type, const QPointF& position) {
    Device device;
    device.id = m_nextDeviceId++;
    device.type = type;
    device.name = QString("%1 %2").arg(type).arg(device.id);
    device.position = position;

    if (type == "Router") {
        device.spec.ip = "192.168.1.1";
        device.spec.cpuCores = 4;
        device.spec.ramGb = 8;
    } else if (type == "Server") {
        device.spec.ip = "192.168.1.20";
        device.spec.cpuCores = 8;
        device.spec.ramGb = 16;
    } else if (type == "Switch") {
        device.spec.ip = "192.168.1.2";
        device.spec.cpuCores = 2;
        device.spec.ramGb = 2;
    }

    m_devices.insert(device.id, device);
    return device.id;
}

bool NetworkModel::updateDevice(const Device& device) {
    if (!m_devices.contains(device.id)) {
        return false;
    }
    m_devices[device.id] = device;
    return true;
}

Device* NetworkModel::findDevice(int id) {
    auto it = m_devices.find(id);
    return it == m_devices.end() ? nullptr : &it.value();
}

const Device* NetworkModel::findDevice(int id) const {
    auto it = m_devices.find(id);
    return it == m_devices.end() ? nullptr : &it.value();
}

int NetworkModel::addLink(int fromDeviceId, int toDeviceId) {
    Link link;
    link.id = m_nextLinkId++;
    link.fromDeviceId = fromDeviceId;
    link.toDeviceId = toDeviceId;
    m_links.insert(link.id, link);
    return link.id;
}

bool NetworkModel::removeLink(int id) {
    return m_links.remove(id) > 0;
}

bool NetworkModel::hasLinkBetween(int a, int b) const {
    for (const auto& link : m_links) {
        if ((link.fromDeviceId == a && link.toDeviceId == b) ||
            (link.fromDeviceId == b && link.toDeviceId == a)) {
            return true;
        }
    }
    return false;
}

QList<Device> NetworkModel::devices() const {
    return m_devices.values();
}

QList<Link> NetworkModel::links() const {
    return m_links.values();
}
