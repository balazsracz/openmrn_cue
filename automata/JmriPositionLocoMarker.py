
HORIZONTAL = jmri.jmrit.display.PositionablePopupUtil.HORIZONTAL
VERTICAL_UP = jmri.jmrit.display.PositionablePopupUtil.VERTICAL_UP
VERTICAL_DOWN = jmri.jmrit.display.PositionablePopupUtil.VERTICAL_DOWN

def PositionLocoMarker(panel_name, loco_name, x, y, orientation = HORIZONTAL):
  # First we enumerate all panels known
  i = 0
  elist = []
  while True:
    entry = jmri.InstanceManager.configureManagerInstance().findInstance(
        java.lang.Class.forName("jmri.jmrit.display.layoutEditor.LayoutEditor"),
        i)
    i += 1
    if entry:
      elist.append(entry)
    else:
      break;
  found_panel = None
  for panel in elist:
    if panel.getName() == panel_name:
      found_panel = panel
      break
  if not found_panel: raise Exception("Panel named " + panel_name + " cannot be found.")
  components = found_panel.getContents()
  for widget in components:
    if isinstance(widget, jmri.jmrit.display.LocoIcon) and widget.getUnRotatedText() == loco_name:
      widget.setLocation(x, y)
      widget.getPopupUtility().setOrientation(orientation)
