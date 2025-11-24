#include "CanvasView.h"

#include <QGraphicsScene>
#include <QPainter>
#include <cmath>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent) {
    setBackgroundBrush(Qt::white);
    setRenderHint(QPainter::Antialiasing, false); // crisp grid
    setDragMode(QGraphicsView::ScrollHandDrag);
}

void CanvasView::drawBackground(QPainter* painter, const QRectF& rect) {
    // White background
    painter->fillRect(rect, Qt::white);

    // Draw a subtle square grid using semi-transparent black lines
    const int gridSize = 20;
    QPen pen(QColor(0, 0, 0, 26)); // ~10% opacity
    pen.setCosmetic(true);
    painter->setPen(pen);

    const qreal left = std::floor(rect.left());
    const qreal right = std::ceil(rect.right());
    const qreal top = std::floor(rect.top());
    const qreal bottom = std::ceil(rect.bottom());

    // Vertical lines
    for (int x = static_cast<int>(left) - (static_cast<int>(left) % gridSize);
         x < right; x += gridSize) {
        painter->drawLine(QLineF(x, top, x, bottom));
    }

    // Horizontal lines
    for (int y = static_cast<int>(top) - (static_cast<int>(top) % gridSize);
         y < bottom; y += gridSize) {
        painter->drawLine(QLineF(left, y, right, y));
    }
}
