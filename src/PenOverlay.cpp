#include "PenOverlay.h"
#include "AnnotationNode.h"

#include <QApplication>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QToolButton>

// ===========================================================================
// DagFinder
// ===========================================================================

QWidget* DagFinder::findDagViewport()
{
    // Strategy 1: objectName starts with "DAG" (Nuke 13–15)
    for (QWidget* w : QApplication::allWidgets()) {
        if (w->objectName().startsWith(QStringLiteral("DAG")))
            return w;
    }

    // Strategy 2: top-level window whose title mentions "Node Graph"
    for (QWidget* top : QApplication::topLevelWidgets()) {
        const QString title = top->windowTitle();
        if (title.contains(QStringLiteral("Node Graph"), Qt::CaseInsensitive) ||
            title.contains(QStringLiteral("DAG"),        Qt::CaseInsensitive))
        {
            return top;
        }
    }

    return nullptr;
}

// ===========================================================================
// PenToolbar
// ===========================================================================

static const QString kBtnStyle =
    "QToolButton {"
    "  background:#2a2a2a; border:1px solid #555; border-radius:4px;"
    "  color:#ddd; padding:4px 8px; font-size:12px;"
    "}"
    "QToolButton:hover   { background:#3c3c3c; }"
    "QToolButton:checked { background:#555555; border-color:#aaa; }";

PenToolbar::PenToolbar(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(46);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(6);

    // ── Colour swatch ──────────────────────────────────────────────────────
    m_colorBtn = new QToolButton(this);
    m_colorBtn->setFixedSize(30, 30);
    m_colorBtn->setToolTip("Pick colour (C)");
    connect(m_colorBtn, &QToolButton::clicked, this, &PenToolbar::pickColor);
    updateColorSwatch();
    layout->addWidget(m_colorBtn);

    layout->addSpacing(4);

    // ── Width label + slider ───────────────────────────────────────────────
    auto* wlabel = new QLabel("Width:", this);
    wlabel->setStyleSheet("color:#bbb; font-size:11px;");
    layout->addWidget(wlabel);

    m_widthSlider = new QSlider(Qt::Horizontal, this);
    m_widthSlider->setRange(1, 20);
    m_widthSlider->setValue(3);
    m_widthSlider->setFixedWidth(90);
    m_widthSlider->setToolTip("Brush width");
    connect(m_widthSlider, &QSlider::valueChanged, this, &PenToolbar::widthChanged);
    layout->addWidget(m_widthSlider);

    layout->addSpacing(4);

    // ── Eraser ─────────────────────────────────────────────────────────────
    auto* eraserBtn = new QToolButton(this);
    eraserBtn->setText("\u232B Eraser");
    eraserBtn->setCheckable(true);
    eraserBtn->setStyleSheet(kBtnStyle);
    eraserBtn->setToolTip("Eraser mode (E)");
    connect(eraserBtn, &QToolButton::toggled, [this](bool on) {
        m_eraserActive = on;
        emit eraserToggled(on);
    });
    layout->addWidget(eraserBtn);

    // ── Undo ───────────────────────────────────────────────────────────────
    auto* undoBtn = new QToolButton(this);
    undoBtn->setText("\u21A9 Undo");
    undoBtn->setStyleSheet(kBtnStyle);
    undoBtn->setToolTip("Undo last stroke (Ctrl+Z)");
    connect(undoBtn, &QToolButton::clicked, this, &PenToolbar::undoRequested);
    layout->addWidget(undoBtn);

    // ── Clear all ──────────────────────────────────────────────────────────
    auto* clearBtn = new QToolButton(this);
    clearBtn->setText("\U0001F5D1 Clear");
    clearBtn->setStyleSheet(kBtnStyle);
    clearBtn->setToolTip("Clear all strokes");
    connect(clearBtn, &QToolButton::clicked, this, &PenToolbar::clearAll);
    layout->addWidget(clearBtn);

    // ── Exit ───────────────────────────────────────────────────────────────
    auto* closeBtn = new QToolButton(this);
    closeBtn->setText("\u2715 Exit");
    closeBtn->setStyleSheet(kBtnStyle);
    closeBtn->setToolTip("Exit pen mode (Escape)");
    connect(closeBtn, &QToolButton::clicked, this, &PenToolbar::closeRequested);
    layout->addWidget(closeBtn);

    setStyleSheet(
        "QWidget { background:#1e1e1e; border:1px solid #555; border-radius:6px; }");
}

float PenToolbar::currentWidth() const
{
    return static_cast<float>(m_widthSlider->value());
}

void PenToolbar::pickColor()
{
    const QColor c = QColorDialog::getColor(m_color, this, "Pen Colour");
    if (c.isValid()) {
        m_color = c;
        updateColorSwatch();
        emit colorChanged(c);
    }
}

void PenToolbar::updateColorSwatch()
{
    m_colorBtn->setStyleSheet(
        QString("QToolButton {"
                "  background:%1;"
                "  border:2px solid #888;"
                "  border-radius:4px;"
                "}").arg(m_color.name()));
}

// ===========================================================================
// PenOverlay
// ===========================================================================

PenOverlay::PenOverlay(QWidget* dagParent, AnnotationNode* node)
    : QWidget(dagParent)
    , m_dagParent(dagParent)
    , m_node(node)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setFocusPolicy(Qt::StrongFocus);

    // ── Toolbar ────────────────────────────────────────────────────────────
    m_toolbar = new PenToolbar(nullptr);    // free-floating, no parent

    connect(m_toolbar, &PenToolbar::undoRequested, [this]() {
        if (!m_strokeSet.strokes.isEmpty()) {
            m_strokeSet.strokes.removeLast();
            syncToNode();
            update();
        }
    });
    connect(m_toolbar, &PenToolbar::clearAll, [this]() {
        m_strokeSet.clear();
        syncToNode();
        update();
    });
    connect(m_toolbar, &PenToolbar::closeRequested, [this]() {
        deactivate();
    });

    buildPenCursor();
    hide();
}

PenOverlay::~PenOverlay()
{
    delete m_toolbar;
}

// ── Public ─────────────────────────────────────────────────────────────────

void PenOverlay::activate()
{
    if (!m_dagParent)
        return;

    m_active = true;
    resize(m_dagParent->size());
    move(0, 0);
    raise();
    show();
    setFocus();
    setCursor(m_penCursor);

    repositionToolbar();
    m_toolbar->show();
    m_toolbar->raise();
}

void PenOverlay::deactivate()
{
    m_active = false;
    hide();
    m_toolbar->hide();
    commitCurrentStroke();
}

void PenOverlay::loadStrokes(const StrokeSet& set)
{
    m_strokeSet = set;
    update();
}

// ── Protected events ────────────────────────────────────────────────────────

void PenOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Subtle active-mode tint
    p.fillRect(rect(), QColor(0, 0, 0, 18));

    for (const PenStroke& stroke : m_strokeSet.strokes)
        paintStroke(p, stroke);

    if (m_drawing && !m_currentStroke.empty())
        paintStroke(p, m_currentStroke);
}

void PenOverlay::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_drawing = true;
        m_currentStroke          = PenStroke();
        m_currentStroke.color    = m_toolbar->currentColor();
        m_currentStroke.width    = m_toolbar->currentWidth();
        m_currentStroke.isEraser = m_toolbar->isEraser();
        m_currentStroke.points.append(e->pos());
        update();
    }
    e->accept();
}

void PenOverlay::mouseMoveEvent(QMouseEvent* e)
{
    if (m_drawing) {
        m_currentStroke.points.append(e->pos());
        update();
    }
    e->accept();
}

void PenOverlay::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        commitCurrentStroke();
    }
    e->accept();
}

void PenOverlay::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    repositionToolbar();
}

void PenOverlay::keyPressEvent(QKeyEvent* e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;

    if (e->key() == Qt::Key_Escape) {
        deactivate();
    } else if (e->key() == Qt::Key_Z && ctrl) {
        if (!m_strokeSet.strokes.isEmpty()) {
            m_strokeSet.strokes.removeLast();
            syncToNode();
            update();
        }
    }
    e->accept();
}

// ── Private helpers ─────────────────────────────────────────────────────────

void PenOverlay::buildPenCursor()
{
    QPixmap px(24, 24);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::white, 1.5));
    p.drawLine(12, 2,  12, 10);
    p.drawLine(12, 14, 12, 22);
    p.drawLine(2,  12, 10, 12);
    p.drawLine(14, 12, 22, 12);
    p.setBrush(QColor(255, 200, 50));
    p.setPen(Qt::NoPen);
    p.drawEllipse(10, 10, 4, 4);
    m_penCursor = QCursor(px, 12, 12);
}

void PenOverlay::repositionToolbar()
{
    if (!m_dagParent)
        return;
    const QPoint globalPos = m_dagParent->mapToGlobal(QPoint(10, 10));
    m_toolbar->move(globalPos);
}

void PenOverlay::commitCurrentStroke()
{
    if (!m_currentStroke.empty()) {
        m_strokeSet.strokes.append(m_currentStroke);
        m_currentStroke = PenStroke();
        syncToNode();
        update();
    }
}

void PenOverlay::syncToNode()
{
    if (m_node)
        m_node->setStrokeData(m_strokeSet.serialise().toStdString());
}

// static
void PenOverlay::paintStroke(QPainter& p, const PenStroke& stroke)
{
    if (stroke.points.isEmpty())
        return;

    // Single-point dot
    if (stroke.points.size() == 1) {
        const float r = stroke.width / 2.0f;
        p.setBrush(stroke.isEraser ? Qt::transparent : stroke.color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(stroke.points[0], r, r);
        return;
    }

    // Build smooth quadratic-bezier path through midpoints
    QPainterPath path;
    path.moveTo(stroke.points[0]);
    for (int i = 1; i < stroke.points.size() - 1; ++i) {
        const QPointF mid = (stroke.points[i] + stroke.points[i + 1]) / 2.0;
        path.quadTo(stroke.points[i], mid);
    }
    path.lineTo(stroke.points.last());

    if (stroke.isEraser) {
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        QPen pen(QColor(0, 0, 0, 255), stroke.width * 2.5f,
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.restore();
    } else {
        QPen pen(stroke.color, stroke.width,
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
}
