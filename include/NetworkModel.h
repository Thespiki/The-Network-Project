#ifndef TNP_NETWORKMODEL_H
#define TNP_NETWORKMODEL_H

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>

struct DeviceSpec {
    int cpuCores{2};
    int ramGb{4};
    QString ip{"192.168.1.10"};
};

struct Device {
    int id{0};
    QString name;
    QString type;
    QPointF position;
    DeviceSpec spec;
};

struct Link {
    int id{0};
    int fromDeviceId{0};
    int toDeviceId{0};
    int bandwidthMbps{1000};
};

class NetworkModel {
public:
    void clear();

    int addDevice(const QString& type, const QPointF& position);
    bool updateDevice(const Device& device);
    Device* findDevice(int id);
    const Device* findDevice(int id) const;

    int addLink(int fromDeviceId, int toDeviceId);
    bool removeLink(int id);
    bool hasLinkBetween(int a, int b) const;

    QList<Device> devices() const;
    QList<Link> links() const;

private:
    int m_nextDeviceId{1};
    int m_nextLinkId{1};
    QHash<int, Device> m_devices;
    QHash<int, Link> m_links;
};

#endif
