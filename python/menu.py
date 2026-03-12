# python/menu.py — PenTool menu registration
#
# Place this alongside PenTool.so in your plugin path, or source it from your
# studio's menu.py.  The CMakeLists install target copies it automatically.

import nuke


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get_or_create_annotation_node():
    """Return the first AnnotationNode in the script, creating one if needed."""
    for node in nuke.allNodes('AnnotationNode'):
        return node

    n = nuke.createNode('AnnotationNode', inpanel=False)
    n['label'].setValue('Pen Annotations')
    n.setXYpos(-200, -200)
    return n


def activate_pen_tool():
    """Toggle freehand pen mode on the Node Graph."""
    n = _get_or_create_annotation_node()
    n['activate_pen'].execute()


# ---------------------------------------------------------------------------
# Menu entries
# ---------------------------------------------------------------------------

# Edit menu
nuke.menu('Nuke').addCommand(
    'Edit/Pen Tool',
    activate_pen_tool,
    'ctrl+shift+p',
    icon='pencil.png'   # optional 16x16 PNG in your plugin directory
)

# Node Graph right-click context menu
_dag = nuke.menu('DAG')
if _dag:
    _dag.addCommand('Pen Tool', activate_pen_tool, 'ctrl+shift+p')

print("[PenTool] menu.py loaded — Ctrl+Shift+P to draw on the graph.")
