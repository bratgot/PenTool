#include "AnnotationNode.h"
#include "PenOverlay.h"
#include "PenStroke.h"

#include "DDImage/Knobs.h"

#include <QMap>
#include <QWidget>

// ---------------------------------------------------------------------------
// Static registration
// ---------------------------------------------------------------------------

const char* AnnotationNode::CLASS = "AnnotationNode";

// Nuke 17: Description(menu, constructor) overload is deprecated.
// Use the single-argument form; menu registration is done in menu.py.
const DD::Image::Op::Description AnnotationNode::desc(
    AnnotationNode::CLASS,
    AnnotationNode::build
);

DD::Image::Op* AnnotationNode::build(Node* node)
{
    return new AnnotationNode(node);
}

// ---------------------------------------------------------------------------
// Per-node overlay registry (one PenOverlay per AnnotationNode instance)
// ---------------------------------------------------------------------------

static QMap<AnnotationNode*, PenOverlay*> s_overlayRegistry;

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

AnnotationNode::AnnotationNode(Node* node)
    : DD::Image::NoIop(node)
{}

AnnotationNode::~AnnotationNode()
{
    if (s_overlayRegistry.contains(this))
        delete s_overlayRegistry.take(this);
}

// ---------------------------------------------------------------------------
// Stroke data accessors
// ---------------------------------------------------------------------------

void AnnotationNode::setStrokeData(const std::string& data)
{
    // The String_knob is bound directly to m_strokeData via pointer.
    // Just update the string — Nuke reads it on script save automatically.
    // Do NOT call set_text() here; it re-enters the knob system and crashes
    // when called from a Qt signal handler (even deferred via QTimer).
    m_strokeData = data;
}

std::string AnnotationNode::getStrokeData() const
{
    return m_strokeData;
}

// ---------------------------------------------------------------------------
// Knobs
// ---------------------------------------------------------------------------

void AnnotationNode::knobs(DD::Image::Knob_Callback f)
{
    // Hidden string knob — persisted to .nk, not shown in Properties panel.
    // Nuke 17 NDK: String_knob takes (callback, &string, name, label).
    // DO_NOT_WRITE_IF_DEFAULT was removed; INVISIBLE alone is sufficient.
    m_strokeKnob = String_knob(f, &m_strokeData, "stroke_data", "Stroke Data");
    SetFlags(f, DD::Image::Knob::INVISIBLE);
    Tooltip(f, "Serialised stroke data — managed automatically, do not edit.");

    Divider(f, "Pen Tool");

    Text_knob(f,
        "Draw freehand annotations on the Node Graph.\n"
        "Strokes are saved with this node in the .nk script.\n"
        "Shortcut: Ctrl+Shift+P");

    m_activateKnob = Button(f, "activate_pen", "Activate Pen Tool");
    Tooltip(f, "Toggle the freehand pen overlay on the Node Graph.");

    Button(f, "clear_strokes", "Clear All Strokes");
    Tooltip(f, "Permanently remove all strokes stored in this node.");
}

// ---------------------------------------------------------------------------
// Knob callbacks
// ---------------------------------------------------------------------------

int AnnotationNode::knob_changed(DD::Image::Knob* k)
{
    if (k->is("activate_pen")) {
        activatePenOverlay();
        return 1;
    }

    if (k->is("clear_strokes")) {
        m_strokeData.clear();
        if (m_strokeKnob)
            m_strokeKnob->set_text("");

        if (s_overlayRegistry.contains(this)) {
            s_overlayRegistry[this]->loadStrokes(StrokeSet{});
        }
        return 1;
    }

    if (k->is("stroke_data")) {
        // Knob was set externally (e.g. script load) — push into live overlay
        m_strokeData = m_strokeKnob ? m_strokeKnob->get_text() : "";

        if (s_overlayRegistry.contains(this)) {
            StrokeSet set = StrokeSet::deserialise(
                QString::fromStdString(m_strokeData));
            s_overlayRegistry[this]->loadStrokes(set);
        }
        return 1;
    }

    return NoIop::knob_changed(k);
}

// ---------------------------------------------------------------------------
// Node help text
// ---------------------------------------------------------------------------

const char* AnnotationNode::node_help() const
{
    return
        "AnnotationNode — freehand pen annotations on the Node Graph.\n\n"
        "Click 'Activate Pen Tool' (or press Ctrl+Shift+P) to enter drawing "
        "mode. Strokes are saved as part of this node in the .nk script and "
        "will be restored when the script is re-opened.\n\n"
        "Controls while drawing:\n"
        "  Left-drag   Draw\n"
        "  Ctrl+Z      Undo last stroke\n"
        "  Escape      Exit drawing mode\n\n"
        "Colour, width, eraser, and clear controls are in the floating toolbar.";
}

// ---------------------------------------------------------------------------
// Overlay activation
// ---------------------------------------------------------------------------

void AnnotationNode::activatePenOverlay()
{
    QWidget* dag = DagFinder::findDagViewport();
    if (!dag) {
        error("PenTool: could not locate the Node Graph panel.\n"
              "Make sure the Node Graph is open and try again.");
        return;
    }

    PenOverlay* overlay = s_overlayRegistry.value(this, nullptr);

    if (!overlay) {
        overlay = new PenOverlay(dag, this);
        s_overlayRegistry.insert(this, overlay);

        // Restore persisted strokes on first open
        if (!m_strokeData.empty()) {
            StrokeSet set = StrokeSet::deserialise(
                QString::fromStdString(m_strokeData));
            overlay->loadStrokes(set);
        }
    }

    // Toggle
    if (overlay->isActive())
        overlay->deactivate();
    else
        overlay->activate();
}
