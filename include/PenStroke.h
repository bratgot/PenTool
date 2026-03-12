#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// PenStroke
//   A single continuous brush stroke: an ordered list of DAG-space points
//   plus visual properties. Supports compact string serialisation so strokes
//   can be stored in a Nuke String_knob and round-trip through .nk scripts.
//
//   Serialised format (one stroke per line):
//     "r,g,b,a|width|isEraser|x0,y0;x1,y1;..."
// ---------------------------------------------------------------------------
struct PenStroke
{
    QList<QPointF> points;
    QColor         color    = QColor(255, 200, 50);
    float          width    = 2.5f;
    bool           isEraser = false;

    bool empty() const { return points.isEmpty(); }

    QString       serialise()                    const;
    static PenStroke deserialise(const QString& s);
};

// ---------------------------------------------------------------------------
// StrokeSet
//   Owns all strokes for one AnnotationNode. Serialises to a multi-line
//   string (one PenStroke per line) suitable for a String_knob value.
// ---------------------------------------------------------------------------
struct StrokeSet
{
    QList<PenStroke> strokes;

    bool    empty()    const { return strokes.isEmpty(); }
    void    clear()          { strokes.clear(); }

    QString          serialise()                     const;
    static StrokeSet deserialise(const QString& s);
};
