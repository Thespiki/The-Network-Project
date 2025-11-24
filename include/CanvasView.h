#ifndef TNP_CANVASVIEW_H
#define TNP_CANVASVIEW_H

#include <QGraphicsView>

class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
};

#endif // TNP_CANVASVIEW_H
