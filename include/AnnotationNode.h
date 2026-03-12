#pragma once

#include "DDImage/NoIop.h"
#include "DDImage/Knobs.h"
#include "DDImage/Op.h"

#include <string>

// ---------------------------------------------------------------------------
// AnnotationNode
//   A lightweight NoIop-derived Nuke node whose only purpose is to own and
//   persist freehand stroke data drawn via PenOverlay.
//
//   Knobs (visible):
//     • activate_pen   (Button)  — opens / closes the PenOverlay on the DAG
//     • clear_strokes  (Button)  — wipes all stroke data
//
//   Knobs (hidden, saved to .nk):
//     • stroke_data    (String)  — serialised StrokeSet; written automatically
//
//   The node is amber-coloured in the graph and appears under Other/ in the
//   node menu.
// ---------------------------------------------------------------------------
class AnnotationNode : public DD::Image::NoIop
{
public:
    explicit AnnotationNode(Node* node);
    ~AnnotationNode() override;

    // Called by PenOverlay whenever strokes are added / removed
    void        setStrokeData(const std::string& data);
    std::string getStrokeData() const;

    // DD::Image::Op interface
    void        knobs(DD::Image::Knob_Callback f) override;
    int         knob_changed(DD::Image::Knob* k)  override;
    const char* Class()     const override { return CLASS; }
    const char* node_help() const override;

    unsigned node_color() const override { return 0xFF9900FF; }  // amber

    static const char* CLASS;
    static const DD::Image::Op::Description desc;
    static DD::Image::Op* build(Node* node);

private:
    void activatePenOverlay();

    // Nuke 17 String_knob binds directly to std::string
    std::string      m_strokeData;
    DD::Image::Knob* m_strokeKnob   = nullptr;
    DD::Image::Knob* m_activateKnob = nullptr;
};
