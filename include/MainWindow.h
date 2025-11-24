#ifndef TNP_MAINWINDOW_H
#define TNP_MAINWINDOW_H

#include <QMainWindow>

class QListWidget;
class QDockWidget;
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
    void applyMonochromeStyle();

private:
    CanvasView* m_canvas{nullptr};
    QDockWidget* m_paletteDock{nullptr};
    QDockWidget* m_propsDock{nullptr};
    QListWidget* m_paletteList{nullptr};
};

#endif // TNP_MAINWINDOW_H
