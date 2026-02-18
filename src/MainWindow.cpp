#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <functional>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

#include "CanvasView.h"

namespace {
constexpr int kDataDeviceId = 100;
constexpr int kDataLinkId = 101;

class DeviceGraphicsItem : public QGraphicsEllipseItem {
public:
    using Callback = std::function<void(int)>;

    DeviceGraphicsItem(int deviceId, Callback callback, qreal x, qreal y, qreal w, qreal h)
        : QGraphicsEllipseItem(x, y, w, h), m_deviceId(deviceId), m_movedCallback(std::move(callback)) {}

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == QGraphicsItem::ItemPositionHasChanged && m_movedCallback) {
            m_movedCallback(m_deviceId);
        }
        return QGraphicsEllipseItem::itemChange(change, value);
    }

private:
    int m_deviceId;
    Callback m_movedCallback;
};

QColor colorForType(const QString& type) {
    if (type == "Router") return QColor(210, 210, 210);
    if (type == "Switch") return QColor(232, 232, 232);
    if (type == "Server") return QColor(200, 200, 200);
    if (type == "PC") return QColor(242, 242, 242);
    if (type == "Modem") return QColor(222, 222, 222);
    return QColor(235, 235, 235);
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("The Network Project");

    createActions();
    createToolbar();
    createDocks();
    createScene();

    connect(m_paletteList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item != nullptr) {
            addDeviceToScene(item->text());
        }
    });

    statusBar()->showMessage("Ready");
    applyMonochromeStyle();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions() {
    auto* fileMenu = menuBar()->addMenu("File");
    auto* networkMenu = menuBar()->addMenu("Network");

    m_newAction = new QAction("New", this);
    m_openAction = new QAction("Open", this);
    m_saveAction = new QAction("Save", this);
    m_linkAction = new QAction("Link selected devices", this);
    m_simulateAction = new QAction("Run simulation", this);

    connect(m_newAction, &QAction::triggered, this, &MainWindow::clearTopology);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openTopology);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveTopology);
    connect(m_linkAction, &QAction::triggered, this, &MainWindow::createLinkFromSelection);
    connect(m_simulateAction, &QAction::triggered, this, &MainWindow::runSimulation);

    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_saveAction);

    networkMenu->addAction(m_linkAction);
    networkMenu->addAction(m_simulateAction);
}

void MainWindow::createToolbar() {
    auto* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setFloatable(false);

    tb->addAction(m_newAction);
    tb->addAction(m_openAction);
    tb->addAction(m_saveAction);
    tb->addSeparator();
    tb->addAction(m_linkAction);
    tb->addAction(m_simulateAction);
}

void MainWindow::createDocks() {
    m_paletteDock = new QDockWidget("Palette", this);
    m_paletteDock->setObjectName("PaletteDock");
    m_paletteDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_paletteList = new QListWidget(m_paletteDock);
    m_paletteList->addItems({"Router", "Switch", "Server", "PC", "Modem"});
    m_paletteDock->setWidget(m_paletteList);
    addDockWidget(Qt::LeftDockWidgetArea, m_paletteDock);

    m_propsDock = new QDockWidget("Properties", this);
    m_propsDock->setObjectName("PropertiesDock");
    m_propsDock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    auto* propsRoot = new QWidget(m_propsDock);
    auto* layout = new QVBoxLayout(propsRoot);
    auto* form = new QFormLayout();

    m_nameEdit = new QLineEdit(propsRoot);
    m_typeEdit = new QComboBox(propsRoot);
    m_typeEdit->addItems({"Router", "Switch", "Server", "PC", "Modem"});
    m_ipEdit = new QLineEdit(propsRoot);
    m_cpuEdit = new QSpinBox(propsRoot);
    m_cpuEdit->setRange(1, 64);
    m_ramEdit = new QSpinBox(propsRoot);
    m_ramEdit->setRange(1, 512);

    form->addRow("Name", m_nameEdit);
    form->addRow("Type", m_typeEdit);
    form->addRow("IP", m_ipEdit);
    form->addRow("CPU cores", m_cpuEdit);
    form->addRow("RAM (GB)", m_ramEdit);

    m_applyPropsButton = new QPushButton("Apply", propsRoot);

    layout->addLayout(form);
    layout->addWidget(m_applyPropsButton);
    layout->addStretch();

    m_propsDock->setWidget(propsRoot);
    addDockWidget(Qt::RightDockWidgetArea, m_propsDock);

    connect(m_applyPropsButton, &QPushButton::clicked, this, &MainWindow::updateDeviceFromEditors);
}

void MainWindow::createScene() {
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 4000, 3000);

    m_canvas = new CanvasView(this);
    m_canvas->setScene(m_scene);
    setCentralWidget(m_canvas);

    connect(m_scene, &QGraphicsScene::selectionChanged, this, &MainWindow::refreshPropertiesFromSelection);
}

void MainWindow::addDeviceToScene(const QString& deviceType) {
    const QPointF position(160.0 + m_deviceItems.size() * 36.0, 140.0 + m_deviceItems.size() * 24.0);
    const int deviceId = m_model.addDevice(deviceType, position);
    renderModel();

    if (auto* item = m_deviceItems.value(deviceId, nullptr)) {
        item->setSelected(true);
    }

    statusBar()->showMessage(QString("Added %1").arg(deviceType), 1500);
}

void MainWindow::createLinkFromSelection() {
    const auto selected = m_scene->selectedItems();
    if (selected.size() != 2) {
        statusBar()->showMessage("Select exactly 2 devices to create a link", 2500);
        return;
    }

    const int a = selected[0]->data(kDataDeviceId).toInt();
    const int b = selected[1]->data(kDataDeviceId).toInt();
    if (a <= 0 || b <= 0 || a == b) {
        statusBar()->showMessage("Invalid selection for link", 2000);
        return;
    }

    if (m_model.hasLinkBetween(a, b)) {
        statusBar()->showMessage("A link already exists between these devices", 2000);
        return;
    }

    m_model.addLink(a, b);
    renderModel();
    statusBar()->showMessage("Link created", 1500);
}

void MainWindow::refreshPropertiesFromSelection() {
    m_selectedDeviceId = 0;

    const auto selected = m_scene->selectedItems();
    if (selected.size() != 1) {
        m_nameEdit->setText("");
        m_ipEdit->setText("");
        m_cpuEdit->setValue(1);
        m_ramEdit->setValue(1);
        return;
    }

    const int deviceId = selected.first()->data(kDataDeviceId).toInt();
    if (deviceId <= 0) {
        return;
    }

    const Device* device = m_model.findDevice(deviceId);
    if (device == nullptr) {
        return;
    }

    m_selectedDeviceId = deviceId;
    m_nameEdit->setText(device->name);
    m_typeEdit->setCurrentText(device->type);
    m_ipEdit->setText(device->spec.ip);
    m_cpuEdit->setValue(device->spec.cpuCores);
    m_ramEdit->setValue(device->spec.ramGb);
}

void MainWindow::updateDeviceFromEditors() {
    if (m_selectedDeviceId <= 0) {
        statusBar()->showMessage("Select one device first", 2000);
        return;
    }

    Device* device = m_model.findDevice(m_selectedDeviceId);
    if (device == nullptr) {
        return;
    }

    device->name = m_nameEdit->text().trimmed().isEmpty() ? device->name : m_nameEdit->text().trimmed();
    device->type = m_typeEdit->currentText();
    device->spec.ip = m_ipEdit->text().trimmed();
    device->spec.cpuCores = m_cpuEdit->value();
    device->spec.ramGb = m_ramEdit->value();

    renderModel();
    if (auto* item = m_deviceItems.value(m_selectedDeviceId, nullptr)) {
        item->setSelected(true);
    }
    statusBar()->showMessage("Properties updated", 1500);
}

void MainWindow::clearTopology() {
    m_model.clear();
    renderModel();
    statusBar()->showMessage("New empty topology", 1500);
}

void MainWindow::saveTopology() {
    const QString path = QFileDialog::getSaveFileName(this, "Save topology", QString(), "TNP Topology (*.tnp.json)");
    if (path.isEmpty()) {
        return;
    }

    QJsonObject root;
    QJsonArray devices;
    for (const auto& d : m_model.devices()) {
        QJsonObject obj;
        obj["id"] = d.id;
        obj["name"] = d.name;
        obj["type"] = d.type;
        obj["x"] = d.position.x();
        obj["y"] = d.position.y();
        obj["ip"] = d.spec.ip;
        obj["cpuCores"] = d.spec.cpuCores;
        obj["ramGb"] = d.spec.ramGb;
        devices.append(obj);
    }

    QJsonArray links;
    for (const auto& l : m_model.links()) {
        QJsonObject obj;
        obj["id"] = l.id;
        obj["fromDeviceId"] = l.fromDeviceId;
        obj["toDeviceId"] = l.toDeviceId;
        obj["bandwidthMbps"] = l.bandwidthMbps;
        links.append(obj);
    }

    root["devices"] = devices;
    root["links"] = links;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Save failed", "Could not write topology file.");
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    statusBar()->showMessage("Topology saved", 1500);
}

void MainWindow::openTopology() {
    const QString path = QFileDialog::getOpenFileName(this, "Open topology", QString(), "TNP Topology (*.tnp.json)");
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Open failed", "Could not open topology file.");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Open failed", "Invalid topology file format.");
        return;
    }

    m_model.clear();
    const QJsonObject root = doc.object();

    QHash<int, int> oldToNew;
    for (const auto& dv : root.value("devices").toArray()) {
        const QJsonObject d = dv.toObject();
        const QString type = d.value("type").toString("PC");
        const QPointF pos(d.value("x").toDouble(), d.value("y").toDouble());
        const int newId = m_model.addDevice(type, pos);

        if (Device* device = m_model.findDevice(newId)) {
            device->name = d.value("name").toString(device->name);
            device->spec.ip = d.value("ip").toString(device->spec.ip);
            device->spec.cpuCores = d.value("cpuCores").toInt(device->spec.cpuCores);
            device->spec.ramGb = d.value("ramGb").toInt(device->spec.ramGb);
        }
        oldToNew.insert(d.value("id").toInt(newId), newId);
    }

    for (const auto& lv : root.value("links").toArray()) {
        const QJsonObject l = lv.toObject();
        const int from = oldToNew.value(l.value("fromDeviceId").toInt(), 0);
        const int to = oldToNew.value(l.value("toDeviceId").toInt(), 0);
        if (from > 0 && to > 0 && from != to && !m_model.hasLinkBetween(from, to)) {
            m_model.addLink(from, to);
        }
    }

    renderModel();
    statusBar()->showMessage("Topology loaded", 1500);
}

void MainWindow::runSimulation() {
    const auto devices = m_model.devices();
    const auto links = m_model.links();

    if (devices.isEmpty()) {
        QMessageBox::information(this, "Simulation", "No devices to simulate.");
        return;
    }

    int totalCpu = 0;
    int totalRam = 0;
    int unlinkedDevices = 0;

    for (const auto& d : devices) {
        totalCpu += d.spec.cpuCores;
        totalRam += d.spec.ramGb;

        bool linked = false;
        for (const auto& l : links) {
            if (l.fromDeviceId == d.id || l.toDeviceId == d.id) {
                linked = true;
                break;
            }
        }
        if (!linked) {
            ++unlinkedDevices;
        }
    }

    QString verdict = "Healthy";
    if (unlinkedDevices > 0) {
        verdict = "Warning: isolated devices detected";
    }
    if (links.size() < devices.size() - 1) {
        verdict = "Warning: low redundancy / sparse connectivity";
    }

    QMessageBox::information(
        this,
        "Simulation report",
        QString("Devices: %1\nLinks: %2\nTotal CPU cores: %3\nTotal RAM: %4 GB\nIsolated devices: %5\nStatus: %6")
            .arg(devices.size())
            .arg(links.size())
            .arg(totalCpu)
            .arg(totalRam)
            .arg(unlinkedDevices)
            .arg(verdict));
}

void MainWindow::renderModel() {
    m_scene->clear();
    m_deviceItems.clear();
    m_linkItems.clear();

    // First add devices
    for (const auto& device : m_model.devices()) {
        constexpr qreal size = 74.0;
        auto* item = new DeviceGraphicsItem(device.id,
                                            [this](int id) { redrawLinksForDevice(id); },
                                            0,
                                            0,
                                            size,
                                            size);

        item->setPen(QPen(Qt::black, 2));
        item->setBrush(QBrush(colorForType(device.type)));
        item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges);
        item->setPos(device.position);
        item->setData(kDataDeviceId, device.id);
        m_scene->addItem(item);

        auto* label = new QGraphicsSimpleTextItem(device.type, item);
        const QRectF textRect = label->boundingRect();
        label->setPos((size - textRect.width()) / 2.0, (size - textRect.height()) / 2.0);

        m_deviceItems.insert(device.id, item);
    }

    // Then add links behind nodes
    for (const auto& link : m_model.links()) {
        auto* fromItem = m_deviceItems.value(link.fromDeviceId, nullptr);
        auto* toItem = m_deviceItems.value(link.toDeviceId, nullptr);
        if (fromItem == nullptr || toItem == nullptr) {
            continue;
        }

        const QPointF p1 = fromItem->sceneBoundingRect().center();
        const QPointF p2 = toItem->sceneBoundingRect().center();
        auto* line = m_scene->addLine(QLineF(p1, p2), QPen(Qt::black, 2, Qt::DashLine));
        line->setZValue(-1.0);
        line->setData(kDataLinkId, link.id);
        m_linkItems.insert(link.id, line);
    }

    refreshPropertiesFromSelection();
}

void MainWindow::redrawLinksForDevice(int deviceId) {
    auto* movedItem = m_deviceItems.value(deviceId, nullptr);
    if (movedItem == nullptr) {
        return;
    }

    Device* movedDevice = m_model.findDevice(deviceId);
    if (movedDevice != nullptr) {
        movedDevice->position = movedItem->pos();
    }

    for (const auto& link : m_model.links()) {
        if (link.fromDeviceId != deviceId && link.toDeviceId != deviceId) {
            continue;
        }

        auto* line = m_linkItems.value(link.id, nullptr);
        auto* fromItem = m_deviceItems.value(link.fromDeviceId, nullptr);
        auto* toItem = m_deviceItems.value(link.toDeviceId, nullptr);
        if (line == nullptr || fromItem == nullptr || toItem == nullptr) {
            continue;
        }

        line->setLine(QLineF(fromItem->sceneBoundingRect().center(), toItem->sceneBoundingRect().center()));
    }
}

void MainWindow::redrawAllLinks() {
    for (const auto& link : m_model.links()) {
        auto* line = m_linkItems.value(link.id, nullptr);
        auto* fromItem = m_deviceItems.value(link.fromDeviceId, nullptr);
        auto* toItem = m_deviceItems.value(link.toDeviceId, nullptr);
        if (line != nullptr && fromItem != nullptr && toItem != nullptr) {
            line->setLine(QLineF(fromItem->sceneBoundingRect().center(), toItem->sceneBoundingRect().center()));
        }
    }
}

void MainWindow::applyMonochromeStyle() {
    QFile f(":/styles/styles.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString qss = QString::fromUtf8(f.readAll());
        qApp->setStyleSheet(qss);
        f.close();
    }
}
