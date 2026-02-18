#ifndef TNP_MAINWINDOW_H
#define TNP_MAINWINDOW_H

#include <QHash>
#include <QMainWindow>

#include "NetworkModel.h"

class QAction;
class QComboBox;
class QDockWidget;
class QGraphicsItem;
class QGraphicsLineItem;
class QGraphicsScene;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class CanvasView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void createActions();
    void createToolbar();
    void createDocks();
    void createScene();

    void addDeviceToScene(const QString& deviceType);
    void createLinkFromSelection();
    void refreshPropertiesFromSelection();
    void updateDeviceFromEditors();

    void clearTopology();
    void saveTopology();
    void openTopology();
    void runSimulation();

    void renderModel();
    void redrawLinksForDevice(int deviceId);
    void redrawAllLinks();

    void applyMonochromeStyle();

private:
    CanvasView* m_canvas{nullptr};
    QGraphicsScene* m_scene{nullptr};

    QDockWidget* m_paletteDock{nullptr};
    QDockWidget* m_propsDock{nullptr};
    QListWidget* m_paletteList{nullptr};

    QLineEdit* m_nameEdit{nullptr};
    QComboBox* m_typeEdit{nullptr};
    QLineEdit* m_ipEdit{nullptr};
    QSpinBox* m_cpuEdit{nullptr};
    QSpinBox* m_ramEdit{nullptr};
    QPushButton* m_applyPropsButton{nullptr};

    QAction* m_newAction{nullptr};
    QAction* m_openAction{nullptr};
    QAction* m_saveAction{nullptr};
    QAction* m_linkAction{nullptr};
    QAction* m_simulateAction{nullptr};

    NetworkModel m_model;
    QHash<int, QGraphicsItem*> m_deviceItems;
    QHash<int, QGraphicsLineItem*> m_linkItems;

    int m_selectedDeviceId{0};
};

#endif // TNP_MAINWINDOW_H
