#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QGraphicsScene>
#include <QListWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

#include "CanvasView.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("The Network Project");

    createActions();
    createToolbar();
    createDocks();

    // Central canvas
    auto* scene = new QGraphicsScene(this);
    scene->setSceneRect(0, 0, 4000, 3000);

    m_canvas = new CanvasView(this);
    m_canvas->setScene(scene);
    setCentralWidget(m_canvas);

    statusBar()->showMessage("Ready");

    applyMonochromeStyle();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions() {
    // Menu (optional for now)
    auto* fileMenu = menuBar()->addMenu("File");

    auto* actNew = new QAction("New", this);
    auto* actOpen = new QAction("Open", this);
    auto* actSave = new QAction("Save", this);

    connect(actNew, &QAction::triggered, this, [this]() { statusBar()->showMessage("New project", 1500); });
    connect(actOpen, &QAction::triggered, this, [this]() { statusBar()->showMessage("Open project", 1500); });
    connect(actSave, &QAction::triggered, this, [this]() { statusBar()->showMessage("Save project", 1500); });

    fileMenu->addAction(actNew);
    fileMenu->addAction(actOpen);
    fileMenu->addAction(actSave);
}

void MainWindow::createToolbar() {
    auto* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setFloatable(false);

    auto* actNew = new QAction("New", this);
    auto* actOpen = new QAction("Open", this);
    auto* actSave = new QAction("Save", this);

    connect(actNew, &QAction::triggered, this, [this]() { statusBar()->showMessage("New project", 1500); });
    connect(actOpen, &QAction::triggered, this, [this]() { statusBar()->showMessage("Open project", 1500); });
    connect(actSave, &QAction::triggered, this, [this]() { statusBar()->showMessage("Save project", 1500); });

    tb->addAction(actNew);
    tb->addAction(actOpen);
    tb->addAction(actSave);
}

void MainWindow::createDocks() {
    // Palette (left)
    m_paletteDock = new QDockWidget("Palette", this);
    m_paletteDock->setObjectName("PaletteDock");
    m_paletteDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_paletteList = new QListWidget(m_paletteDock);
    m_paletteList->addItems({"Router", "Switch", "Server", "PC", "Modem"});
    m_paletteDock->setWidget(m_paletteList);
    addDockWidget(Qt::LeftDockWidgetArea, m_paletteDock);

    // Properties (right)
    m_propsDock = new QDockWidget("Properties", this);
    m_propsDock->setObjectName("PropertiesDock");
    m_propsDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto* propsList = new QListWidget(m_propsDock);
    propsList->addItems({"No selection"});
    m_propsDock->setWidget(propsList);
    addDockWidget(Qt::RightDockWidgetArea, m_propsDock);
}

void MainWindow::applyMonochromeStyle() {
    QFile f(":/styles/styles.qss");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString qss = QString::fromUtf8(f.readAll());
        qApp->setStyleSheet(qss);
        f.close();
    }
}
