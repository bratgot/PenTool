#include "PenStroke.h"

// ---------------------------------------------------------------------------
// PenStroke
// ---------------------------------------------------------------------------

QString PenStroke::serialise() const
{
    QString out;
    out.reserve(points.size() * 16);

    // Header: "r,g,b,a|width|isEraser|"
    out += QString("%1,%2,%3,%4|%5|%6|")
               .arg(color.red())
               .arg(color.green())
               .arg(color.blue())
               .arg(color.alpha())
               .arg(static_cast<double>(width), 0, 'f', 2)
               .arg(isEraser ? 1 : 0);

    // Points: "x0,y0;x1,y1;..."
    for (int i = 0; i < points.size(); ++i) {
        if (i > 0)
            out += ';';
        out += QString("%1,%2")
                   .arg(points[i].x(), 0, 'f', 3)
                   .arg(points[i].y(), 0, 'f', 3);
    }

    return out;
}

PenStroke PenStroke::deserialise(const QString& s)
{
    PenStroke stroke;
    const QStringList parts = s.split('|');
    if (parts.size() < 4)
        return stroke;

    // Colour
    const QStringList rgba = parts[0].split(',');
    if (rgba.size() == 4) {
        stroke.color = QColor(rgba[0].toInt(), rgba[1].toInt(),
                              rgba[2].toInt(), rgba[3].toInt());
    }

    // Width
    stroke.width = parts[1].toFloat();

    // Eraser flag
    stroke.isEraser = (parts[2].toInt() != 0);

    // Points
    if (!parts[3].isEmpty()) {
        for (const QString& token : parts[3].split(';')) {
            const QStringList xy = token.split(',');
            if (xy.size() == 2)
                stroke.points.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
        }
    }

    return stroke;
}

// ---------------------------------------------------------------------------
// StrokeSet
// ---------------------------------------------------------------------------

QString StrokeSet::serialise() const
{
    QStringList lines;
    lines.reserve(strokes.size());
    for (const PenStroke& s : strokes)
        lines << s.serialise();
    return lines.join('\n');
}

StrokeSet StrokeSet::deserialise(const QString& s)
{
    StrokeSet set;
    if (s.trimmed().isEmpty())
        return set;

    for (const QString& line : s.split('\n')) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            set.strokes << PenStroke::deserialise(trimmed);
    }

    return set;
}
