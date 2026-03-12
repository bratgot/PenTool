#pragma once

#include "PenStroke.h"

#include <QWidget>
#include <QColor>
#include <QCursor>
#include <QSlider>
#include <QToolButton>

class AnnotationNode;   // forward — overlay calls back to node for persistence

// ---------------------------------------------------------------------------
// DagFinder
//   Locates the Nuke Node Graph QWidget by walking Qt's widget tree.
//   Returns nullptr if the panel cannot be found.
// ---------------------------------------------------------------------------
namespace DagFinder
{
    QWidget* findDagViewport();
}

// ---------------------------------------------------------------------------
// PenToolbar
//   Small floating tool palette (Qt::Tool window) that sits above the DAG
//   overlay. Provides colour, width, eraser, undo, clear, and exit controls.
// ---------------------------------------------------------------------------
class PenToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit PenToolbar(QWidget* parent = nullptr);

    QColor currentColor() const { return m_color; }
    float  currentWidth() const;
    bool   isEraser()     const { return m_eraserActive; }

signals:
    void colorChanged(QColor);
    void widthChanged(int);
    void eraserToggled(bool);
    void clearAll();
    void undoRequested();
    void closeRequested();

private slots:
    void pickColor();

private:
    void updateColorSwatch();

    QColor       m_color        = QColor(255, 200, 50);
    QSlider*     m_widthSlider  = nullptr;
    QToolButton* m_colorBtn     = nullptr;
    bool         m_eraserActive = false;
};

// ---------------------------------------------------------------------------
// PenOverlay
//   A transparent QWidget child of the DAG panel. Intercepts mouse events
//   to record strokes, and repaints them using Qt's painter API.
//   The overlay syncs stroke data back to its owning AnnotationNode so that
//   strokes are persisted in the .nk script automatically.
// ---------------------------------------------------------------------------
class PenOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit PenOverlay(QWidget* dagParent, AnnotationNode* node);
    ~PenOverlay() override;

    void activate();
    void deactivate();
    bool isActive() const { return m_active; }

    void             loadStrokes(const StrokeSet& set);
    const StrokeSet& strokeSet()  const { return m_strokeSet; }

protected:
    void paintEvent(QPaintEvent*)        override;
    void mousePressEvent(QMouseEvent*)   override;
    void mouseMoveEvent(QMouseEvent*)    override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*)      override;
    void keyPressEvent(QKeyEvent*)       override;

private:
    void commitCurrentStroke();
    void syncToNode();
    void repositionToolbar();
    void buildPenCursor();

    static void paintStroke(QPainter& painter, const PenStroke& stroke);

    QWidget*        m_dagParent     = nullptr;
    AnnotationNode* m_node          = nullptr;
    PenToolbar*     m_toolbar       = nullptr;

    StrokeSet       m_strokeSet;
    PenStroke       m_currentStroke;

    bool            m_drawing       = false;
    bool            m_active        = false;

    QCursor         m_penCursor;
};
