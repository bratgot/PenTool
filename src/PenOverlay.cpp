#include <cstddef>
typedef ptrdiff_t Py_ssize_t;
struct _PyObject { Py_ssize_t ob_refcnt; void* ob_type; };
typedef struct _PyObject PyObject;
typedef int PyGILState_STATE;
static const int kPyEvalInput = 258;

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
#include <QRegion>
#include <QResizeEvent>
#include <QSlider>
#include <QToolButton>
#include <QWheelEvent>
#include <windows.h>
#include <windowsx.h>

// ==========================================================================
// NukePy — runtime Python API loader
// ==========================================================================
namespace NukePy {
    using tGILEnsure  = PyGILState_STATE (*)();
    using tGILRelease = void (*)(PyGILState_STATE);
    using tAddModule  = PyObject* (*)(const char*);
    using tGetDict    = PyObject* (*)(PyObject*);
    using tRunString  = PyObject* (*)(const char*, int, PyObject*, PyObject*, void*);
    using tFloatVal   = double (*)(PyObject*);
    using tListSize   = long (*)(PyObject*);
    using tListItem   = PyObject* (*)(PyObject*, long);
    using tErrClear   = void (*)();
    using tDecRef     = void (*)(PyObject*);

    static tGILEnsure  gilEnsure  = nullptr;
    static tGILRelease gilRelease = nullptr;
    static tAddModule  addModule  = nullptr;
    static tGetDict    getDict    = nullptr;
    static tRunString  runString  = nullptr;
    static tFloatVal   floatVal   = nullptr;
    static tListSize   listSize   = nullptr;
    static tListItem   listItem   = nullptr;
    static tErrClear   errClear   = nullptr;
    static tDecRef     decRef     = nullptr;
    static bool ready = false, tried = false;

    static bool init() {
        if (tried) return ready;
        tried = true;
        HMODULE hPy = nullptr;
        for (int v = 15; v >= 8 && !hPy; --v) {
            char buf[32]; snprintf(buf, sizeof(buf), "python3%d.dll", v);
            hPy = GetModuleHandleA(buf);
        }
        if (!hPy) return false;
#define BIND(var, sym) var = (decltype(var))GetProcAddress(hPy, sym)
        BIND(gilEnsure,"PyGILState_Ensure"); BIND(gilRelease,"PyGILState_Release");
        BIND(addModule,"PyImport_AddModule"); BIND(getDict,"PyModule_GetDict");
        BIND(runString,"PyRun_StringFlags");  BIND(floatVal,"PyFloat_AsDouble");
        BIND(listSize,"PyList_Size");         BIND(listItem,"PyList_GetItem");
        BIND(errClear,"PyErr_Clear");         BIND(decRef,"Py_DecRef");
#undef BIND
        ready = gilEnsure && gilRelease && addModule && getDict &&
                runString && floatVal && listSize && listItem && errClear && decRef;
        return ready;
    }
}

// ==========================================================================
// DagFinder
// ==========================================================================
QWidget* DagFinder::findDagViewport() {
    for (QWidget* w : QApplication::allWidgets())
        if (w->objectName().startsWith(QStringLiteral("DAG"))) return w;
    for (QWidget* w : QApplication::allWidgets()) {
        const QString cls = w->metaObject()->className();
        if (cls.contains("DAG", Qt::CaseInsensitive) ||
            cls.contains("NodeGraph", Qt::CaseInsensitive)) return w;
    }
    return nullptr;
}

// ==========================================================================
// DagTransform query
// ==========================================================================
DagTransform PenOverlay::queryDagTransform() const {
    DagTransform t;
    if (!NukePy::init()) return t;
    PyGILState_STATE gs = NukePy::gilEnsure();
    auto eval = [&](const char* expr) -> PyObject* {
        PyObject* m = NukePy::addModule("__main__");
        PyObject* g = NukePy::getDict(m);
        PyObject* r = NukePy::runString(expr, kPyEvalInput, g, g, nullptr);
        if (!r) NukePy::errClear();
        return r;
    };
    if (auto* z = eval("__import__('nuke').zoom()")) {
        t.zoom = (float)NukePy::floatVal(z); NukePy::decRef(z);
    }
    if (auto* c = eval("__import__('nuke').center()")) {
        if (NukePy::listSize(c) >= 2) {
            t.center.setX(NukePy::floatVal(NukePy::listItem(c, 0)));
            t.center.setY(NukePy::floatVal(NukePy::listItem(c, 1)));
        }
        NukePy::decRef(c);
    }
    NukePy::gilRelease(gs);
    return t;
}

QPointF PenOverlay::screenToDag(QPointF screen) const {
    return (screen - QPointF(width()/2.0, height()/2.0)) / m_transform.zoom + m_transform.center;
}
QPointF PenOverlay::dagToScreen(QPointF dag) const {
    return (dag - m_transform.center) * m_transform.zoom + QPointF(width()/2.0, height()/2.0);
}

// ==========================================================================
// PenToolbar
// ==========================================================================
static const QString kBtn =
    "QToolButton { background:#2a2a2a; border:1px solid #555; border-radius:4px;"
    "  color:#ddd; padding:4px 8px; font-size:12px; }"
    "QToolButton:hover   { background:#3c3c3c; }"
    "QToolButton:checked { background:#4a6080; border-color:#6af; color:#fff; }";

PenToolbar::PenToolbar(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setFixedHeight(46);
    setStyleSheet("QWidget { background:#1e1e1e; border:1px solid #555; border-radius:6px; }");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 5, 8, 5);
    m_layout->setSpacing(5);

    auto* verLabel = new QLabel("v0.5", this);
    verLabel->setStyleSheet("color:#888; font-size:10px;");
    m_layout->addWidget(verLabel);

    m_colorBtn = new QToolButton(this);
    m_colorBtn->setFixedSize(30, 30);
    m_colorBtn->setToolTip("Pen colour");
    connect(m_colorBtn, &QToolButton::clicked, this, [this]() { emit anyButtonPressed(); pickColor(); });
    updateColorSwatch();
    m_layout->addWidget(m_colorBtn);

    auto* wlabel = new QLabel("W:", this);
    wlabel->setStyleSheet("color:#bbb; font-size:11px;");
    m_layout->addWidget(wlabel);

    m_widthSlider = new QSlider(Qt::Horizontal, this);
    m_widthSlider->setRange(1, 20);
    m_widthSlider->setValue(3);
    m_widthSlider->setFixedWidth(80);
    connect(m_widthSlider, &QSlider::valueChanged, this, [this](int v) {
        emit anyButtonPressed(); emit widthChanged(v);
    });
    m_layout->addWidget(m_widthSlider);

    auto* eraserBtn = new QToolButton(this);
    eraserBtn->setText("Eraser");
    eraserBtn->setCheckable(true);
    eraserBtn->setStyleSheet(kBtn);
    connect(eraserBtn, &QToolButton::toggled, [this](bool on) {
        emit anyButtonPressed(); m_eraserActive = on; emit eraserToggled(on);
    });
    m_layout->addWidget(eraserBtn);

    auto* undoBtn = new QToolButton(this);
    undoBtn->setText("Undo");
    undoBtn->setStyleSheet(kBtn);
    connect(undoBtn, &QToolButton::clicked, this, [this]() { emit anyButtonPressed(); emit undoRequested(); });
    m_layout->addWidget(undoBtn);

    auto* clearBtn = new QToolButton(this);
    clearBtn->setText("Clear");
    clearBtn->setStyleSheet(kBtn);
    connect(clearBtn, &QToolButton::clicked, this, [this]() { emit anyButtonPressed(); emit clearAll(); });
    m_layout->addWidget(clearBtn);

    m_ghostBtn = new QToolButton(this);
    m_ghostBtn->setText("Ghost");
    m_ghostBtn->setToolTip("Ghost mode: annotations track pan/zoom");
    m_ghostBtn->setCheckable(true);
    m_ghostBtn->setStyleSheet(kBtn);
    connect(m_ghostBtn, &QToolButton::toggled, this, &PenToolbar::ghostToggled);
    m_layout->addWidget(m_ghostBtn);

    auto* closeBtn = new QToolButton(this);
    closeBtn->setText("Exit");
    closeBtn->setStyleSheet(kBtn);
    connect(closeBtn, &QToolButton::clicked, this, [this]() { emit anyButtonPressed(); emit closeRequested(); });
    m_layout->addWidget(closeBtn);

    // Don't call adjustSize() — width is managed by fitToWidth() at activate time
    resize(sizeHint());
}

void PenToolbar::fitToWidth(int w) {
    const int natural = sizeHint().width();
    setFixedWidth(qMin(w, natural));
}

float PenToolbar::currentWidth() const { return (float)m_widthSlider->value(); }

void PenToolbar::setGhostMode(bool on) {
    m_ghostBtn->blockSignals(true);
    m_ghostBtn->setChecked(on);
    m_ghostBtn->blockSignals(false);
}

void PenToolbar::pickColor() {
    const QColor c = QColorDialog::getColor(m_color, this, "Pen Colour");
    if (c.isValid()) { m_color = c; updateColorSwatch(); emit colorChanged(c); }
}

void PenToolbar::updateColorSwatch() {
    m_colorBtn->setStyleSheet(
        QString("QToolButton { background:%1; border:2px solid #888; border-radius:4px; }")
            .arg(m_color.name()));
}

// ==========================================================================
// PenOverlay
// ==========================================================================
PenOverlay::PenOverlay(QWidget* dagViewport, AnnotationNode* node)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_dagViewport(dagViewport)
    , m_node(node)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_toolbar = new PenToolbar(nullptr);
    connect(m_toolbar, &PenToolbar::undoRequested,   this, &PenOverlay::onUndo);
    connect(m_toolbar, &PenToolbar::clearAll,         this, &PenOverlay::onClear);
    connect(m_toolbar, &PenToolbar::ghostToggled,     this, &PenOverlay::onGhostToggled);
    connect(m_toolbar, &PenToolbar::closeRequested,   this, [this]() { deactivate(); });
    connect(m_toolbar, &PenToolbar::anyButtonPressed, this, [this]() {
        if (m_ghostMode) { m_toolbar->setGhostMode(false); onGhostToggled(false); }
    });

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(16);
    connect(m_pollTimer, &QTimer::timeout, this, &PenOverlay::onPollTimer);

    buildPenCursor();
    hide();
}

PenOverlay::~PenOverlay() { delete m_toolbar; }

// --------------------------------------------------------------------------
// Get the Win32 HWND of the DAG viewport for PostMessage forwarding
// --------------------------------------------------------------------------
static HWND dagHwnd(QWidget* dag) {
    return dag ? (HWND)dag->winId() : nullptr;
}

// --------------------------------------------------------------------------
// 60fps poll
// --------------------------------------------------------------------------
void PenOverlay::onPollTimer() {
    if (!m_dagViewport || !m_active) return;

    const QPoint dagPos  = m_dagViewport->mapToGlobal(QPoint(0,0));
    const QSize  dagSize = m_dagViewport->size();
    if (dagPos != m_lastDagPos || dagSize != m_lastDagSize) {
        m_lastDagPos  = dagPos;
        m_lastDagSize = dagSize;
        repositionOverlay();
        if (m_toolbar && m_toolbar->isVisible()) {
            m_toolbar->move(dagPos.x(), dagPos.y() - m_toolbar->height() - 2);
            m_toolbar->fitToWidth(dagSize.width());
        }
        updateMask();
    }

    const DagTransform t = queryDagTransform();
    if (t.zoom != m_transform.zoom ||
        t.center.x() != m_transform.center.x() ||
        t.center.y() != m_transform.center.y()) {
        m_transform = t;
        update();
    }
}

// --------------------------------------------------------------------------
// Slots
// --------------------------------------------------------------------------
void PenOverlay::onUndo() {
    if (!m_active || m_strokeSet.strokes.isEmpty()) return;
    m_strokeSet.strokes.removeLast();
    update(); syncToNode();
}

void PenOverlay::onClear() {
    if (!m_active) return;
    m_drawing = false; m_currentStroke = PenStroke();
    m_strokeSet.clear(); update(); syncToNode();
}

void PenOverlay::onGhostToggled(bool on) {
    m_ghostMode = on;
    if (on) {
        clearMask();
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        lower();
        m_toolbar->setWindowOpacity(0.4);
        m_toolbar->raise();
    } else {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        raise(); activateWindow();
        m_toolbar->setWindowOpacity(1.0);
        m_toolbar->show(); m_toolbar->raise(); m_toolbar->activateWindow();
        updateMask();
        setCursor(m_penCursor);
    }
    update();
}

// --------------------------------------------------------------------------
// activate / deactivate
// --------------------------------------------------------------------------
void PenOverlay::activate() {
    if (m_active && m_ghostMode) {
        m_toolbar->setGhostMode(false); onGhostToggled(false); return;
    }

    m_active    = true;
    m_ghostMode = false;
    m_toolbar->setGhostMode(false);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

    m_lastDagPos  = m_dagViewport->mapToGlobal(QPoint(0,0));
    m_lastDagSize = m_dagViewport->size();
    repositionOverlay();
    m_transform = queryDagTransform();
    setCursor(m_penCursor);

    show(); raise(); activateWindow();

    // Toolbar sits just above the DAG, full width
    m_toolbar->move(m_lastDagPos.x(), m_lastDagPos.y() - m_toolbar->height() - 2);
    m_toolbar->fitToWidth(m_lastDagSize.width());
    m_toolbar->show(); m_toolbar->raise(); m_toolbar->activateWindow();
    updateMask();
    m_pollTimer->start();
}

void PenOverlay::deactivate() {
    if (!m_active) return;
    m_active = false; m_ghostMode = false;
    m_pollTimer->stop();
    m_drawing = false;
    if (!m_currentStroke.empty()) {
        m_strokeSet.strokes.append(m_currentStroke);
        m_currentStroke = PenStroke();
    }
    hide(); m_toolbar->hide();
    syncToNode();
}

void PenOverlay::loadStrokes(const StrokeSet& set) { m_strokeSet = set; update(); }

// --------------------------------------------------------------------------
// Mouse events — left=draw, everything else forwarded to DAG via PostMessage
// --------------------------------------------------------------------------
void PenOverlay::mousePressEvent(QMouseEvent* e) {
    // Middle button or any modifier key = navigation, forward to DAG
    if (e->button() != Qt::LeftButton || e->modifiers() != Qt::NoModifier) {
        forwardMousePress(e); e->ignore(); return;
    }
    if (m_ghostMode) { forwardMousePress(e); e->ignore(); return; }

    m_transform = queryDagTransform();
    m_drawing = true;
    m_currentStroke = PenStroke();
    m_currentStroke.color    = m_toolbar->currentColor();
    m_currentStroke.width    = m_toolbar->currentWidth();
    m_currentStroke.isEraser = m_toolbar->isEraser();
    m_currentStroke.points.append(screenToDag(e->pos()));
    update(); e->accept();
}

void PenOverlay::mouseMoveEvent(QMouseEvent* e) {
    if (!m_drawing) { e->ignore(); return; }
    m_currentStroke.points.append(screenToDag(e->pos()));
    update(); e->accept();
}

void PenOverlay::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton || !m_drawing) { e->ignore(); return; }
    m_drawing = false;
    if (!m_currentStroke.empty()) {
        m_strokeSet.strokes.append(m_currentStroke);
        m_currentStroke = PenStroke();
        update(); syncToNode();
    }
    e->accept();
}

void PenOverlay::wheelEvent(QWheelEvent* e) {
    // Forward scroll wheel to DAG's native HWND
    HWND hw = dagHwnd(m_dagViewport);
    if (!hw) { e->ignore(); return; }
    POINT pt;
    GetCursorPos(&pt);
    const int delta = e->angleDelta().y();
    const WPARAM wp = MAKEWPARAM(
        (e->modifiers() & Qt::ControlModifier) ? MK_CONTROL : 0,
        (short)(delta));
    const LPARAM lp = MAKELPARAM(pt.x, pt.y);
    PostMessage(hw, WM_MOUSEWHEEL, wp, lp);
    e->accept();
}

void PenOverlay::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        if (m_drawing) { m_drawing = false; m_currentStroke = PenStroke(); update(); }
        else { deactivate(); }
        e->accept();
    } else if (e->key() == Qt::Key_Z && (e->modifiers() & Qt::ControlModifier)) {
        onUndo(); e->accept();
    } else {
        // Forward other keys (space, F etc.) to DAG
        e->ignore();
    }
}

void PenOverlay::forwardMousePress(QMouseEvent* e) {
    HWND hw = dagHwnd(m_dagViewport);
    if (!hw) return;
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hw, &pt);
    UINT msg = 0;
    WPARAM wp = 0;
    if (e->button() == Qt::MiddleButton) { msg = WM_MBUTTONDOWN; wp = MK_MBUTTON; }
    else if (e->button() == Qt::RightButton) { msg = WM_RBUTTONDOWN; wp = MK_RBUTTON; }
    else if (e->button() == Qt::LeftButton) { msg = WM_LBUTTONDOWN; wp = MK_LBUTTON; }
    if (msg) PostMessage(hw, msg, wp, MAKELPARAM(pt.x, pt.y));
}

// --------------------------------------------------------------------------
// Paint
// --------------------------------------------------------------------------
void PenOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!m_ghostMode) {
        p.fillRect(rect(), QColor(0, 0, 0, 1));
        p.setPen(QPen(QColor(255, 200, 50, 120), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1,1,-1,-1));
        for (const PenStroke& s : m_strokeSet.strokes) paintStrokeInDagSpace(p, s);
        if (m_drawing && !m_currentStroke.empty()) paintStrokeInDagSpace(p, m_currentStroke);
    } else {
        p.setOpacity(0.4);
        for (const PenStroke& s : m_strokeSet.strokes) paintStrokeInDagSpace(p, s);
    }
}

void PenOverlay::resizeEvent(QResizeEvent* e) { QWidget::resizeEvent(e); updateMask(); }

// --------------------------------------------------------------------------
// Stroke rendering
// --------------------------------------------------------------------------
void PenOverlay::paintStrokeInDagSpace(QPainter& p, const PenStroke& stroke) const {
    if (stroke.points.isEmpty()) return;
    const QPointF vc(width()/2.0, height()/2.0);
    QTransform xf;
    xf.translate(vc.x(), vc.y());
    xf.scale(m_transform.zoom, m_transform.zoom);
    xf.translate(-m_transform.center.x(), -m_transform.center.y());

    const auto& pts = stroke.points;
    if (pts.size() == 1) {
        p.save(); p.setTransform(xf, true);
        p.setBrush(stroke.isEraser ? Qt::transparent : stroke.color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(pts[0], stroke.width/2.0f, stroke.width/2.0f);
        p.restore(); return;
    }
    QPainterPath path;
    path.moveTo(pts[0]);
    for (int i = 1; i < pts.size()-1; ++i)
        path.quadTo(pts[i], (pts[i]+pts[i+1])/2.0);
    path.lineTo(pts.last());

    p.save(); p.setTransform(xf, true);
    if (stroke.isEraser) {
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.setPen(QPen(QColor(0,0,0,255), stroke.width*2.5f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    } else {
        p.setPen(QPen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.restore();
}

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------
void PenOverlay::repositionOverlay() {
    if (!m_dagViewport) return;
    setGeometry(QRect(m_dagViewport->mapToGlobal(QPoint(0,0)), m_dagViewport->size()));
}

void PenOverlay::updateMask() {
    if (m_ghostMode) { clearMask(); return; }
    QRegion region(rect());
    if (m_toolbar && m_toolbar->isVisible()) {
        const QPoint tbLocal = mapFromGlobal(m_toolbar->mapToGlobal(QPoint(0,0)));
        region -= QRect(tbLocal, m_toolbar->size()).adjusted(-4,-4,4,4);
    }
    setMask(region);
}

void PenOverlay::buildPenCursor() {
    QPixmap px(24, 24); px.fill(Qt::transparent);
    QPainter p(&px); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::white, 1.5));
    p.drawLine(12,2,12,10); p.drawLine(12,14,12,22);
    p.drawLine(2,12,10,12); p.drawLine(14,12,22,12);
    p.setBrush(QColor(255,200,50)); p.setPen(Qt::NoPen);
    p.drawEllipse(10,10,4,4);
    m_penCursor = QCursor(px, 12, 12);
}

void PenOverlay::syncToNode() {
    if (m_node) m_node->setStrokeData(m_strokeSet.serialise().toStdString());
}
