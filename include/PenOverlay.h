#pragma once

#include "PenStroke.h"

#include <QWidget>
#include <QColor>
#include <QCursor>
#include <QHBoxLayout>
#include <QPointF>
#include <QRegion>
#include <QSlider>
#include <QTimer>
#include <QToolButton>

class AnnotationNode;
namespace DagFinder { QWidget* findDagViewport(); }

struct DagTransform {
    float   zoom   = 1.0f;
    QPointF center = {0,0};
};

class PenToolbar : public QWidget {
    Q_OBJECT
public:
    explicit PenToolbar(QWidget* parent = nullptr);
    QColor currentColor() const { return m_color; }
    float  currentWidth() const;
    bool   isEraser()     const { return m_eraserActive; }
    void   setGhostMode(bool on);
    void   fitToWidth(int w);   // resize to match DAG width
signals:
    void colorChanged(QColor);
    void widthChanged(int);
    void eraserToggled(bool);
    void clearAll();
    void undoRequested();
    void ghostToggled(bool);
    void closeRequested();
    void anyButtonPressed();
private slots:
    void pickColor();
private:
    void updateColorSwatch();
    QHBoxLayout* m_layout      = nullptr;
    QColor       m_color       = QColor(255, 200, 50);
    QSlider*     m_widthSlider = nullptr;
    QToolButton* m_colorBtn    = nullptr;
    QToolButton* m_ghostBtn    = nullptr;
    bool         m_eraserActive = false;
};

class PenOverlay : public QWidget {
    Q_OBJECT
public:
    explicit PenOverlay(QWidget* dagViewport, AnnotationNode* node);
    ~PenOverlay() override;

    void activate();
    void deactivate();
    bool isActive()    const { return m_active; }
    bool isGhostMode() const { return m_ghostMode; }

    void             loadStrokes(const StrokeSet& set);
    const StrokeSet& strokeSet() const { return m_strokeSet; }

protected:
    void paintEvent(QPaintEvent*)        override;
    void resizeEvent(QResizeEvent*)      override;
    void mousePressEvent(QMouseEvent*)   override;
    void mouseMoveEvent(QMouseEvent*)    override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*)        override;
    void keyPressEvent(QKeyEvent*)       override;

private slots:
    void onUndo();
    void onClear();
    void onGhostToggled(bool on);
    void syncToNode();
    void onPollTimer();

private:
    QPointF screenToDag(QPointF screen) const;
    QPointF dagToScreen(QPointF dag)    const;
    void    paintStrokeInDagSpace(QPainter& p, const PenStroke& stroke) const;
    DagTransform queryDagTransform() const;
    void repositionOverlay();
    void updateMask();
    void buildPenCursor();
    void forwardMousePress(QMouseEvent* e);

    QWidget*        m_dagViewport = nullptr;
    AnnotationNode* m_node        = nullptr;
    PenToolbar*     m_toolbar     = nullptr;
    QTimer*         m_pollTimer   = nullptr;

    StrokeSet    m_strokeSet;
    PenStroke    m_currentStroke;
    bool         m_drawing   = false;
    bool         m_active    = false;
    bool         m_ghostMode = false;
    DagTransform m_transform;
    QPoint       m_lastDagPos;
    QSize        m_lastDagSize;
    QCursor      m_penCursor;
};
