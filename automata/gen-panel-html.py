#!/usr/bin/python3.4

import xml.etree.ElementTree as ET
import sys
import re
import string
import math


webpage_template = """
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8"/>
<title>OpenLCB panel page</title>
</head>

<body>

<script type="text/javascript" src="js_client.js"></script>

<div id="panel"/>

<script type="text/javascript" id="jshelpers">
//var clickeventname = "DOMActivate"
var clickeventname = "click"
var dblclickeventname = "dlbclick"
var ignorefn = function() {}
function addSensorBinding(sensor_id, event_on, url_on, event_off, url_off) {
  var el = document.getElementById(sensor_id);
  var fn_on = function() {
    el.setAttribute("xlink:href", url_on);
  }
  var fn_off = function() {
    el.setAttribute("xlink:href", url_off);
  }
  var listener = new Module.BitEventPC(event_on, fn_on, event_off, fn_off);
  var fn_click = function() {
    console.log('sensor click ' + event_on);
    listener.toggleState();
  }
  el.addEventListener(clickeventname, fn_click, false);
  el.addEventListener(dblclickeventname, ignorefn, false);
}
function addTrackBinding(track_id, event_on, color_on, event_off, color_off) {
  var el = document.getElementById(track_id);
  if (!el) {
    console.log('could not find element by id: ', sensor_id);
  } else {
    el.style.stroke = color_off;
  }
  var fn_on = function() {
    el.style.stroke = color_on;
  }
  var fn_off = function() {
    el.style.stroke = color_off;
  }
  var listener = new Module.BitEventPC(event_on, fn_on, event_off, fn_off);
}
function addTurnoutBinding(event_closed, event_thrown, activate_id, xcen, ycen, closed_id, x1, y1, thrown_id, x2, y2) {
  var closed = document.getElementById(closed_id);
  var thrown = document.getElementById(thrown_id);
  var fn_closed = function() {
    closed.setAttribute('x2', xcen);
    closed.setAttribute('y2', ycen);
    thrown.setAttribute('x2', (xcen + x2) / 2);
    thrown.setAttribute('y2', (ycen + y2) / 2);
  }
  var fn_thrown = function() {
    thrown.setAttribute('x2', xcen);
    thrown.setAttribute('y2', ycen);
    closed.setAttribute('x2', (xcen + x1) / 2);
    closed.setAttribute('y2', (ycen + y1) / 2);
  }
  var listener = new Module.BitEventPC(event_closed, fn_closed, event_thrown, fn_thrown);
  var fn_click = function() {
    console.log('turnout click ' + event_thrown);
    listener.toggleState();
  }
  var el = document.getElementById(activate_id);
  el.addEventListener(clickeventname, fn_click, false);
  el.addEventListener(dblclickeventname, ignorefn, false);
}
</script>

<script type="text/javascript" id="jsbindings"/>

</body>
</html>
"""


class PositionablePoint:
  pass

class LocationInfo:
  pass

def JmriToEvent(jmri_value):
  """Translates a jmri dotted-hex syntax string to a 0x-hex notation.
  example: input 05.01.01.01.14.FF.00.01
           output 0x0501010114FF0001
  """
  bytematch = '([0-9a-fA-F][0-9a-fA-F])'
  exp = ('%s[.]%s[.]%s[.]%s[.]%s[.]%s[.]%s[.]%s' %
               (bytematch, bytematch, bytematch, bytematch,
                bytematch, bytematch, bytematch, bytematch))
  m = re.match(exp, jmri_value)
  if not m:
    raise Exception("Invalid JMRI value format. expected 11.22.33.44.55.66.AA.BB got '" + jmri_value + "'")
  return '0x' + m.group(1) + m.group(2) + m.group(3) + m.group(4) + m.group(5) + m.group(6) + m.group(7) + m.group(8)

def ResourceToUrl(jmri_resource):
  """Translates a JMRI resource url to a usable serving url.
  Example in: program:resources/icons/smallschematics/tracksegments/circuit-occupied.gif
  Example out: resources/icons/smallschematics/tracksegments/circuit-occupied.gif"""
  if jmri_resource.startswith('program:'):
    return jmri_resource.replace('program:', '')
  if jmri_resource.startswith('scripts:'):
    pos = jmri_resource.find('resources/')
    return jmri_resource[pos:]
  raise Exception('Unknown JMRI url scheme: ' + jmri_resource)

svg_id = 0

def GetSvgId():
  """Returns a new unique identifier for dynamically manipulated entries."""
  global svg_id
  c_id = svg_id
  svg_id += 1
  return 'svg' + str(c_id)

alljsbindings = []

def AddJsBinding(js_text):
  alljsbindings.append(js_text)

def RenderJsBindings(html_root):
  script_element = html_root.find('.//script[@id="jsbindings"]')
  script_element.text = '\n'.join(alljsbindings)

class Sensor:
  def __init__(self, system_name, user_name):
    self.system_name = system_name
    self.user_name = user_name
    self.ParseName()

  def Render(self, parent_element):
    e = ET.SubElement(parent_element, 'sensor')
    e.tail = '\n'
    e.set('systemName', self.system_name)
    e.set('inverted', 'false')
    e.set('userName', self.user_name)
    sn = ET.SubElement(e, 'systemName')
    sn.text = self.system_name
    un = ET.SubElement(e, 'userName')
    un.text = self.user_name

  def ParseName(self):
    m = re.match('MS([^;]*);([^;]*)', self.system_name)
    if not m:
      raise Exception("Cannot parse sensor system name '" + self.system_name + "'")
    self.event_on = JmriToEvent(m.group(1))
    self.event_off = JmriToEvent(m.group(2))

class Turnout:
  def __init__(self, system_name, user_name):
    self.system_name = system_name
    self.user_name = user_name
    self.ParseName()

  def ParseName(self):
    m = re.match('MT([^;]*);([^;]*)', self.system_name)
    if not m:
      raise Exception("Cannot parse turnout system name '" + self.system_name + "'")
    self.event_on = JmriToEvent(m.group(1))
    self.event_off = JmriToEvent(m.group(2))


def FindOrInsert(element, tag):
  """Find the first child element with a given tag, or if not found, creates one. Returns the (old or new) tag."""
  sn = element.find(tag)
  if sn is None:
    sn = ET.SubElement(element, tag)
  return sn

def FindOrInsertWithDefines(element, tag, defines):
  """Find the first child element with a given tag, or if not found, creates one. Returns the (old or new) tag."""
  sn = element.find('./%s[@defines=\'%s\']' % (tag, defines))
  if sn is None:
    sn = ET.SubElement(element, tag)
    sn.set('defines', defines)
  return sn

class SignalHead:
  def __init__(self, name):
    self.user_name = name
    self.turnout_name = name

  def key(self):
    return self.user_name

  @staticmethod
  def get_key(e):
    s = e.get('userName')
    if s: return s
    s = e.find('userName')
    if s is not None: return s.text
    return None

  @staticmethod
  def xml_name():
    return 'signalhead'

  @classmethod
  def GetNextId(cls):
    next_id = cls.max_id
    cls.max_id = cls.max_id + 1
    return next_id

  @classmethod
  def SetMaxId(cls, i):
    cls.max_id = i

  def Update(self, e):
    e.set('class', "jmri.implementation.configurexml.SingleTurnoutSignalHeadXml")
    e.set('userName', self.user_name)
    self.system_name = e.get('systemName')
    if not self.system_name:
      self.system_name = 'LH' + str(self.GetNextId())
    e.set('systemName', self.system_name)
    sn = FindOrInsert(e, 'systemName')
    sn.text = self.system_name
    un = FindOrInsert(e, 'userName')
    if un.text != self.user_name:
      print("Overwriting user name", un.text, "with", self.user_name)
    un.text = self.user_name
    at = FindOrInsertWithDefines(e, 'appearance', 'thrown')
    at.text = 'green'
    ac = FindOrInsertWithDefines(e, 'appearance', 'closed')
    ac.text = 'red'
    ta = FindOrInsertWithDefines(e, 'turnoutname', 'aspect')
    ta.text = self.turnout_name

class SystemBlock:
  def __init__(self, user_name, sensor_name):
    self.user_name = user_name
    self.sensor_name = sensor_name

  def key(self):
    return self.user_name

  @staticmethod
  def get_key(e):
    s = e.get('userName')
    if s: return s
    s = e.find('userName')
    if s is not None: return s.text
    return None

  @staticmethod
  def xml_name():
    return 'block'

  @classmethod
  def GetNextId(cls):
    next_id = cls.max_id
    cls.max_id = cls.max_id + 1
    return next_id

  @classmethod
  def SetMaxId(cls, i):
    cls.max_id = i

  def Update(self, e):
    e.set('userName', self.user_name)
    if not e.get('length'): e.set('length', '0.0')
    if not e.get('curve'): e.set('curve', '0')
    self.system_name = e.get('systemName')
    if not self.system_name:
      self.system_name = 'IB' + str(self.GetNextId())
    e.set('systemName', self.system_name)
    sn = FindOrInsert(e, 'systemName')
    sn.text = self.system_name
    un = FindOrInsert(e, 'userName')
    if un.text != self.user_name:
      print("Overwriting user name", un.text, "with", self.user_name)
    un.text = self.user_name
    pm = FindOrInsert(e, 'permissive')
    pm.text = 'no'
    en = FindOrInsert(e, 'occupancysensor')
    en.text = self.sensor_name


class LayoutBlock:
  def __init__(self, user_name, sensor_name, color = 'red'):
    self.user_name = user_name
    self.sensor_name = sensor_name
    self.color = color

  def key(self):
    return self.user_name

  @staticmethod
  def get_key(e):
    s = e.get('userName')
    if s: return s
    s = e.find('userName')
    if s is not None: return s.text
    return None

  @staticmethod
  def xml_name():
    return 'layoutblock'

  @classmethod
  def GetNextId(cls):
    next_id = cls.max_id
    cls.max_id = cls.max_id + 1
    return next_id

  @classmethod
  def SetMaxId(cls, i):
    cls.max_id = i

  def Update(self, e):
    e.set('userName', self.user_name)
    self.system_name = e.get('systemName')
    if not self.system_name:
      self.system_name = 'ILB' + str(self.GetNextId())
    e.set('systemName', self.system_name)
    if not e.get('occupiedsense'): e.set('occupiedsense', '2')
    if not e.get('trackcolor'): e.set('trackcolor', 'black')
    if not e.get('extracolor'): e.set('extracolor', 'blue')
    e.set('occupiedcolor', self.color)
    e.set('occupancysensor', self.sensor_name)
    sn = FindOrInsert(e, 'systemName')
    sn.text = self.system_name
    sn.tail = '\n'
    un = FindOrInsert(e, 'userName')
    un.text = self.user_name
    un.tail = '\n'


class Memory:
  @staticmethod
  def xml_name():
    return 'memory'

  @classmethod
  def GetNextId(cls):
    next_id = cls.max_id
    cls.max_id = cls.max_id + 1
    return next_id

  @classmethod
  def SetMaxId(cls, i):
    cls.max_id = i

  @staticmethod
  def get_key(e):
    s = e.get('userName')
    if s: return s
    s = e.find('userName')
    if s is not None: return s.text
    return None

  def key(self):
    return self.user_name

  def __init__(self, user_name):
    self.user_name = user_name

  def Update(self, e):
    e.set('userName', self.user_name)
    self.system_name = e.get('systemName')
    if not self.system_name:
      self.system_name = 'IM:LOC:' + str(self.GetNextId())
    e.set('systemName', self.system_name)
    e.set('value', 'unknown')
    sn = FindOrInsert(e, 'systemName')
    sn.text = self.system_name
    un = FindOrInsert(e, 'userName')
    un.text = self.user_name

class TrainLocLogixConditional:
  @staticmethod
  def xml_name():
    return 'conditional'

  @classmethod
  def GetNextId(cls):
    next_id = cls.max_id
    cls.max_id = cls.max_id + 1
    return next_id

  @classmethod
  def SetMaxId(cls, i):
    cls.max_id = i

  @staticmethod
  def get_key(e):
    s = e.get('userName')
    if s: return s
    s = e.find('userName')
    if s is not None: return s.text
    return None

  def key(self):
    return self.user_name

  def __init__(self, sensor_name):
    self.sensor_name = sensor_name
    m = re.match('perm\.(.*)\.loc.logic2?\.(.*)', sensor_name)
 #                'perm.icn.loc.logic.A360'

    if not m:
      raise Exception("Unparseable permaloc sensor name: " + sensor_name)
    self.train_name = m.group(1)
    self.location_name = m.group(2)
    self.user_name = self.train_name + "." + self.location_name

  def AddMemoryAction(self, e, memory_name, value):
    s = ET.SubElement(e, 'conditionalAction')
    s.tail = '\n'
    s.set('data', '-1')
    s.set('option', '1')
    s.set('string', value)
    s.set('systemName', memory_name)
    s.set('type', '12')

  def AddJythonAction(self, e, value):
    s = ET.SubElement(e, 'conditionalAction')
    s.tail = '\n'
    s.set('option', '1')
    s.set('type', '30')
    s.set('systemName', ' ')
    s.set('data', '-1')
    s.set('delay', '0')
    s.set('string', value)

  def Update(self, e):
    e.clear();
    e.set('antecedent', 'R1')
    e.set('logicType', '1')
    self.system_name = e.get('systemName')
    if not self.system_name:
      self.system_name = 'IX:GEN:TRAINLOC:C' + str(self.GetNextId())
    global all_location_logix_conditionals
    all_location_logix_conditionals.append(self.system_name)
    e.set('systemName', self.system_name)
    e.set('triggerOnChange', 'no')
    e.set('userName', self.user_name)
    s = FindOrInsert(e, 'systemName')
    s.text = self.system_name
    s.tail = '\n'
    s = FindOrInsert(e, 'userName')
    s.text = self.user_name
    s.tail = '\n'
    s = FindOrInsert(e, 'conditionalStateVariable')
    s.tail = '\n'
    s.set('debugString', '')
    s.set('negated', 'no')
    s.set('num1', '0')
    s.set('num2', '0')
    s.set('operator', '4')
    s.set('systemName', self.sensor_name)
    s.set('triggersCalc', 'yes')
    s.set('type', '1')
    self.AddMemoryAction(e, 'train.' + self.train_name, self.location_name)
    self.AddMemoryAction(e, 'loc.' + self.location_name, self.train_name)
    coord = all_location_coordinates[self.location_name]
    self.AddJythonAction(e, "PositionLocoMarker(\"3H layout\", \""
                         + self.train_name + "\", "
                         + str(int(coord.center[0]))+ ", "
                         + str(int(coord.center[1])) + ", HORIZONTAL)")

all_sensors = []
sensor_by_user_name = {}

all_turnouts = []
turnout_by_user_name = {}

#all_turnouts = []
#all_locations = []
#all_trains = []
#all_location_logix_conditionals = []
#all_location_coordinates = {}

all_layoutblocks = []
layoutblocks_by_name = {}

def ParseAllSensors(sensor_tree):
  """Reads all the sensor objects from the xml tree passed in, and fills out the global list all_sensors."""
  root = sensor_tree.getroot()
  for entry in root.findall('sensors/sensor'):
    system_name = entry.get('systemName')
    user_name = entry.get('userName')
    s = Sensor(system_name, user_name)
    all_sensors.append(s)
    sensor_by_user_name[user_name] = s
    sensor_by_user_name[system_name] = s
  print(len(all_sensors), "sensors found")

def ParseAllTurnouts(jmri_tree):
  """Reads all the turnout objects from the xml tree passed in, and fills out the global list all_turnouts."""
  root = jmri_tree.getroot()
  for entry in root.findall('turnouts/turnout'):
    system_name = entry.get('systemName')
    user_name = entry.get('userName')
    s = Turnout(system_name, user_name)
    all_turnouts.append(s)
    turnout_by_user_name[user_name] = s
    turnout_by_user_name[system_name] = s
  print(len(all_turnouts), "turnouts found")

def ParseAllBlocks(tree):
  """Reads all the layoutblock objects from the xml tree passed in, and fills out the global list all_layoutblocks."""
  root = tree.getroot()
  for entry in root.findall('layoutblocks/layoutblock'):
    system_name = entry.get('systemName')
    user_name = entry.get('userName')
    sensor = entry.attrib['occupancysensor']
    all_layoutblocks.append(entry)
    layoutblocks_by_name[system_name] = entry
    layoutblocks_by_name[user_name] = entry
  print(len(all_layoutblocks), "layoutblocks found")

def ReadVariableFile():
  """Reads the file jmri-out.xml, parses it as an xml element tree and returns the tree root Element."""
  myfile = open("jmri-out.xml", "r")
  raw_data = "<data>\n" + myfile.read() + "\n</data>"
  tree = ET.fromstring(raw_data)
  return tree

def RenderSensors(output_tree_root):
  print("first sensor: ", all_sensors[0].user_name)
  all_sensors_node = output_tree_root.find('sensors')
  all_sensor_children = all_sensors_node.findall('sensor')
  for sensor in all_sensor_children:
    all_sensors_node.remove(sensor)
  for sensor in all_sensors:
    sensor.Render(all_sensors_node)

def RenderTurnouts(output_tree_root):
  all_turnouts_node = output_tree_root.find('turnouts')
  all_turnout_children = all_turnouts_node.findall('turnout')
  for turnout in all_turnout_children:
    all_turnouts_node.remove(turnout)
  for turnout in all_turnouts:
    turnout.Render(all_turnouts_node)

def CreateTurnouts():
  for sensor in all_sensors:
    m = re.match('logic.(.*).turnout_state', sensor.user_name)
    if m:
      blockname = m.group(1)
      if re.match('fake_turnout', blockname): continue
      magnet_command_name = 'logic.magnets.%s.command' % blockname
      if magnet_command_name in sensor_by_user_name:
        # movable turnout
        systemname = sensor_by_user_name[magnet_command_name].system_name
      else:
        systemname = sensor.system_name
      username = blockname
      sensorname = sensor.user_name
      # rename system_name to have the right prefix
      systemname = 'MT' + systemname[2:]
      all_turnouts.append(Turnout(systemname, username, sensorname))
    m = re.match('logic.(.*)[.]signal.route_set_ab', sensor.user_name)
    if m:
      blockname = m.group(1)
      systemname = 'MT' + sensor.system_name[2:]
      username = 'Sig.' + blockname
      sensorname = sensor.user_name
      all_turnouts.append(Turnout(systemname, username, sensorname))
    m = re.match('logic.(.*)[.]rsignal.route_set_ab', sensor.user_name)
    if m:
      blockname = m.group(1)
      systemname = 'MT' + sensor.system_name[2:]
      username = 'Sig.R' + blockname
      sensorname = sensor.user_name
      all_turnouts.append(Turnout(systemname, username, sensorname))
    continue
    m = re.match('logic.(.*).body_det.simulated_occ', sensor.user_name)
    if m:
      blockname = m.group(1)
      if re.match('^[0-9]', blockname):  #no signal here
        blockname = 'Det' + blockname
      username = blockname
      sensorname = sensor.user_name
      desired_block_list.append(SystemBlock(username, sensorname))
      desired_lblock_list.append(LayoutBlock(username, sensorname))


def GetMaxSystemId(parent_element, strip_letters = 'IBL'):
  """Computes the largest number seen in the numbering of system names.

  input: parent_element is a collection element.

  strip_letters: a superset of all letters appearing at the beginning of the
  system names. Stripping these should yield a number.

  returns: an integer, the maximum ID seen, or 1 if there was no ID seen.
  """
  max_id = 1
  for e in parent_element:
    system_name = e.get('systemName')
    if not system_name:
      print("Missing system name from entry in ", parent_element.tag)
      ET.dump(e)
      continue
    next_num = int(system_name.lstrip(strip_letters))
    if next_num > max_id: max_id = next_num
  return max_id + 1

def MergeEntries(parent_element, desired_list, purge = False):
  """Merges a list of desired objects with an existing XML collection subtree.

  parent_element: an XML Element whose children are the XML representation of our objects.
  desired_list: each entry is a python object that represents a desired entry in the parent element.

  This function will modify parent_element in place. All entries in
  parent_element will be matched up by the key attribute to the entries in the
  desired list. If a list entry has no match, it will be added to the parent
  element. All entries will be updated by calling
  desired_list[i].Update(element).
  """
  if not len(desired_list): return
  rep = desired_list[0]
  #print("desired: %d '%s'" % (len(desired_list), parent_element.tag))
  seen_entries = {}
  num_e = 0
  num_d = 0
  to_delete = []
  for e in parent_element:
    key = rep.__class__.get_key(e)
    if key == None:
      print("Error: key not found in entry of ", parent_element.tag)
      ET.dump(e)
      continue
    if key in seen_entries:
      print("Error: duplicate key ", key, " in entry of ", parent_element.tag)
      ET.dump(seen_entries[key])
      ET.dump(e)
      num_d = num_d + 1
      to_delete.append(seen_entries[key])
    seen_entries[key] = e
    num_e = num_e + 1
    #print("found '%s' at " % key, e);
  #print("seen %d '%s' (add %d del %d total %d): " % (len(seen_entries), parent_element.tag, num_e, num_d, num_e - num_d), seen_entries)
  for e in to_delete:
    parent_element.remove(e)
  desired_map = {}
  for o in desired_list:
    desired_map[o.key()] = o
    if o.key() in seen_entries:
      o.Update(seen_entries[o.key()])
    else:
      print("inserting new element for key '%s' to '%s'" % (o.key(), parent_element.tag))
      e = ET.SubElement(parent_element, rep.xml_name())
      o.Update(e)
      e.tail = '\n'
  for key in seen_entries.keys():
    if key not in desired_map:
      print("Undesired in %s: '%s'" % (parent_element.tag, key))
      if purge:
        parent_element.remove(seen_entries[key])

def RenderSystemBlocks(output_tree_root):
  desired_block_list = []
  desired_lblock_list = []
  for sensor in all_sensors:
    m = re.match('logic.(.*).body.route_set_ab', sensor.user_name)
    if m:
      blockname = m.group(1)
      username = blockname + '.body'
      sensorname = sensor.user_name
      desired_block_list.append(SystemBlock(username, sensorname))
      desired_lblock_list.append(LayoutBlock(username, sensorname, 'blue'))
    m = re.match('logic.(.*).body_det.simulated_occ', sensor.user_name)
    if m:
      blockname = m.group(1)
      if re.match('^[0-9]', blockname):  #no signal here
        blockname = 'Det' + blockname
      username = blockname
      sensorname = sensor.user_name
      desired_block_list.append(SystemBlock(username, sensorname))
      desired_lblock_list.append(LayoutBlock(username, sensorname))
  print("Number of blocks: ", len(desired_block_list))
  all_block_node = output_tree_root.find('blocks')
  SystemBlock.SetMaxId(GetMaxSystemId(all_block_node))
  MergeEntries(all_block_node, desired_block_list)
  all_lblock_node = output_tree_root.find('layoutblocks')
  LayoutBlock.SetMaxId(GetMaxSystemId(all_lblock_node))
  MergeEntries(all_lblock_node, desired_lblock_list)

def GetAllTrainList():
  """Returns a list of strings, with each train name."""
  trains = []
  for sensor in all_sensors:
    m = re.match('perm.(.*).do_not_move', sensor.user_name)
    if m:
      trains.append(m.group(1))
  return trains

def GetAllLocationList():
  """Returns a list of strings, with each location name."""
  locations = []
  for sensor in all_sensors:
    m = re.match('logic.(.*)[.]signal.route_set_ab', sensor.user_name)
    if m:
      locations.append(m.group(1))
  return locations

class LayoutIndex:
  """Class maintaining data structures about a LayoutEditor track plan."""
  def __init__(self, tree_root, name):
    """tree_root is the full XML file, name is the layout editor panel name."""
    self.root = tree_root
    self.panel = self.root.find('./LayoutEditor[@name=\''+name+'\']')
    if self.panel is None: raise Exception("Panel " + name + " not found in file.")
    self.InitIdentMap()
    self.InitBlockMap()

  def InitIdentMap(self):
    self.ident_map = {}
    for entry in self.panel:
      if entry.get('ident'):
        self.ident_map[entry.get('ident')] = entry

  def InitBlockMap(self):
    self.block_map = {}
    for entry in self.panel:
      blockname = entry.get('blockname')
      if entry.get('ident') and blockname:
        if not self.block_map.get(blockname):
          self.block_map[blockname] = []
        self.block_map[blockname].append(entry.get('ident'))
    print("block map: ", self.block_map)

  def GetNeighbors(self, ident):
    """Takes a string referring to a layout element, returns the set of directly connected elements"""
    if ident not in self.ident_map:
      raise Exception("Entry %s not found in ident_map" % ident)
    e = self.ident_map[ident]
    ret = set()
    for (name, value) in e.attrib.items():
      if re.match("connect.name", name):
        ret.add(value)
    return ret

  def GetMarginCoordinate(self, dest, source):
    """returns the coordinates of the point between source and dest"""
    dnode = self.ident_map[dest]
    if dnode.tag == 'positionablepoint':
      return (float(dnode.get('x')), float(dnode.get('y')))
    if dnode.tag == 'layoutturnout':
      if source == dnode.get('connectaname'):
        return (2 * float(dnode.get('xcen')) - float(dnode.get('xb')),
                2 * float(dnode.get('ycen')) - float(dnode.get('yb')))
      elif source == dnode.get('connectbname'):
        return (float(dnode.get('xb')), float(dnode.get('yb')))
      elif source == dnode.get('connectcname'):
        return (float(dnode.get('xc')), float(dnode.get('yc')))
      elif source == dnode.get('connectdname'):
        return (2 * float(dnode.get('xcen')) - float(dnode.get('xc')),
                2 * float(dnode.get('ycen')) - float(dnode.get('yc')))
      else: raise Exception("Target turnout does not seem to connect to source " + source + ": " + dest)
    raise Exception("Don't know how to figure out position for " + dnode);

  def FindCenterLocation(self, segment):
    """Returns a tuple (x,y,orientation) for the center of a given segment. Segment is an XML element for a track segment"""
    if segment.tag != 'tracksegment': raise Exception("FindCenterLocation will only work for tracksegments. Called on: " + segment);
    coord_a = self.GetMarginCoordinate(segment.get('connect1name'), segment.get('ident'))
    coord_b = self.GetMarginCoordinate(segment.get('connect2name'), segment.get('ident'))
    return ((coord_a[0] + coord_b[0]) / 2, (coord_a[1] + coord_b[1]) / 2)



def GetLocationCoordinates(all_location, tree_root, index):
  """Returns a dict from location string to x,y,orientation tuples"""
  ret = {}
  for loc in all_location:
    segments = tree_root.findall('LayoutEditor/tracksegment[@blockname=\'' + loc + '\']')
    if not segments: raise Exception("Cound not find track segment for block '%s'" % loc)
    segment = segments[0]
    ret[loc] = LocationInfo()
    ret[loc].center = index.FindCenterLocation(segment)
  return ret

def RenderSignalHeads(output_tree_root):
  desired_signalhead_list = []
  for location in all_locations:
    desired_signalhead_list.append(SignalHead('Sig.' + location))
    desired_signalhead_list.append(SignalHead('Sig.R' + location))
  all_signalhead_node = output_tree_root.find('signalheads')
  SignalHead.SetMaxId(GetMaxSystemId(all_signalhead_node, strip_letters='LMH'))
  print("Number of signal heads:", len(desired_signalhead_list))
  MergeEntries(all_signalhead_node, desired_signalhead_list, purge = True)

def RenderMemoryVariables(output_tree_root):
  desired_memory_list = []
  for location in all_locations:
    desired_memory_list.append(Memory('loc.' + location))
  for train in all_trains:
    desired_memory_list.append(Memory('train.' + train))
  all_memory_node = output_tree_root.find('memories')
  Memory.SetMaxId(GetMaxSystemId(all_memory_node, strip_letters='IM:AUTO:LOC'))
  print("Number of memory entries: ", len(desired_memory_list))
  MergeEntries(all_memory_node, desired_memory_list)

def RenderLogixConditionals(output_tree_root):
  desired_conditionals = []
  for sensor in all_sensors:
    m = re.match('perm.*loc.logic.*', sensor.user_name)
    #m = re.match('perm.\([^.]*\).loc.logic.\(.*\)', sensor.user_name)
    if not m: continue
    desired_conditionals.append(TrainLocLogixConditional(sensor.user_name))
  print("Number of train location logix conditionals: ", len(desired_conditionals))
  all_cond_node = output_tree_root.find('conditionals')
  TrainLocLogixConditional.SetMaxId(GetMaxSystemId(all_cond_node, 'IX:GEN:TRAINLOC:BCC'))
  MergeEntries(all_cond_node, desired_conditionals)

  # Finds the main logix node
  logixnode = output_tree_root.find('./logixs/logix[@systemName=\'IX:GEN:TRAINLOC:\']')
  if not logixnode:
    raise Exception("cannot find logix node for trainloc")
  conditionals = logixnode.findall('logixConditional')
  for e in conditionals: logixnode.remove(e)
  i = 0
  for systemname in all_location_logix_conditionals:
    e = ET.SubElement(logixnode, 'logixConditional')
    e.tail = '\n'
    e.set('order', str(i))
    e.set('systemName', systemname)
    i += 1

def CreateSensorIcon(x, y, sensorname):
  base = ET.XML("    <sensoricon sensor=\"" + sensorname + "\" x=\"" + str(x) + "\" y=\"" + str(y) + "\" level=\"9\" forcecontroloff=\"false\" hidden=\"no\" positionable=\"true\" showtooltip=\"true\" editable=\"true\" momentary=\"false\" icon=\"yes\" class=\"jmri.jmrit.display.configurexml.SensorIconXml\">      <tooltip>" + sensorname + "</tooltip>      <active url=\"program:resources/icons/smallschematics/tracksegments/circuit-occupied.gif\" degrees=\"0\" scale=\"1.0\"><rotation>0</rotation></active><inactive url=\"program:resources/icons/smallschematics/tracksegments/circuit-empty.gif\" degrees=\"0\" scale=\"1.0\"><rotation>0</rotation></inactive><unknown url=\"program:resources/icons/smallschematics/tracksegments/circuit-error.gif\" degrees=\"0\" scale=\"1.0\"><rotation>0</rotation></unknown><inconsistent url=\"program:resources/icons/smallschematics/tracksegments/circuit-error.gif\" degrees=\"0\" scale=\"1.0\"><rotation>0</rotation></inconsistent><iconmaps /></sensoricon>\n")
  return base

def CreateMemoryLabel(x, y, memoryname):
  return ET.XML('<memoryicon blueBack="255" blueBorder="0" borderSize="1" class="jmri.jmrit.display.configurexml.MemoryIconXml" defaulticon="program:resources/icons/misc/X-red.gif" editable="false" fixedWidth="80" forcecontroloff="false" greenBack="255" greenBorder="0" hidden="no" justification="left" level="9" memory="' + memoryname + '" positionable="true" redBack="255" redBorder="0" selectable="no" showtooltip="false" size="12" style="0" x="' + str(x) + '" y="' + str(y) + '">\n<toolTip>' + memoryname +'</toolTip>\n</memoryicon>\n')

def CreateTextLabel(x, y, text):
  return ET.XML('    <positionablelabel blue="51" class="jmri.jmrit.display.configurexml.PositionableLabelXml" editable="false" forcecontroloff="false" green="51" hidden="no" justification="centre" level="9" positionable="true" red="51" showtooltip="false" size="12" style="1" text="' + text + '" x="' + str(x) + '" y="' + str(y) + '"/>\n')
sensoroffset = 3

def PrintBlockLine(layout, block, x0, y):
  layout.append(CreateTextLabel(x0 - 80, y, block))
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic." + block + ".request_green"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic." + block + ".body_det.simulated_occ"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic." + block + ".body.route_set_ab"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic." + block + ".signal.route_set_ab"))
  x0 += 40
  layout.append(CreateMemoryLabel(x0, y, "loc." + block))

def GetRotationFromVector(v):
  """Returns 0-3 for the best-matching orientation for the given vector, expressed as a tuple of floats.
  rotation 0: train going left
  rotation 1: train going down
  rotation 2: train going right
  rotation 3: train going up
  """
  if abs(v[1]) < abs(v[0]):
    # left or right
    if v[0] < 0: return 0
    return 2
  else:
    if v[1] < 0: return 3
    return 1

def CreateRGSignalIcon(x, y, head_name, rotation):
  return ET.XML('    <signalheadicon signalhead="'+head_name+'" x="'+str(x)+'" y="'+str(y)+'" level="9" forcecontroloff="false" hidden="no" positionable="true" showtooltip="false" editable="false" clickmode="3" litmode="false" class="jmri.jmrit.display.configurexml.SignalHeadIconXml"><tooltip>'+head_name+'</tooltip><icons><held url="program:resources/icons/smallschematics/searchlights/left-held-short.gif" degrees="0" scale="1.0"><rotation>'+str(rotation)+'</rotation></held>        <dark url="program:resources/icons/smallschematics/searchlights/left-dark-short.gif" degrees="0" scale="1.0">          <rotation>'+str(rotation)+'</rotation>        </dark>        <red url="program:resources/icons/smallschematics/searchlights/left-red-short.gif" degrees="0" scale="1.0">          <rotation>'+str(rotation)+'</rotation>        </red>        <green url="program:resources/icons/smallschematics/searchlights/left-green-short.gif" degrees="0" scale="1.0">          <rotation>'+str(rotation)+'</rotation>        </green>      </icons>      <iconmaps />    </signalheadicon>\n')

# this many pixels behind the head point should the signals start
g_back = 5
# this many pixels away from the track should the signals start
g_away = 10

# gives the vector offset of where the signal should be from the head point
# given the orientation
g_signal_offset_by_rotation = [
  (g_back, g_away),
  (g_away, -g_back),
  (-g_back, -g_away),
  (-g_away, g_back)
]

def PrintLocationControls(layout, block, index):
  """Adds signal heads to the layout editor right where the tracks are
  arguments:
    layout is the XML tree root for the panel
    block is a string like "WW.B14"
    index is the LayoutIndex object for this panel
  """
  # First let's find out the XY location of the head of the block to see where
  # to add the signal heads.
  if block not in index.block_map:
    print("Cannot find block", block)
    return
  bodyblock = block + ".body"
  if bodyblock not in index.block_map:
    print("Cannot find block", bodyblock)
    return
  head_neighbors = set()
  body_neighbors = set()
  for h in index.block_map[block]:
    head_neighbors |= index.GetNeighbors(h)
  for b in index.block_map[bodyblock]:
    body_neighbors |= index.GetNeighbors(b)
  connect = head_neighbors & body_neighbors
  if len(connect) != 1:
    print("Cannot find connection point between %s and %s" %(block, bodyblock))
    return
  for h in index.block_map[block]:
    neighbors = index.GetNeighbors(h)
    if connect & neighbors:
      # This is the first segment in head.
      head_segment = h
      connection_point = (connect & neighbors).pop()
      neighbors.remove(connection_point);
      far_point = neighbors.pop()
      break
  if index.ident_map[head_segment].tag != "tracksegment":
    raise Exception("Unexpected head segment  %s of type %s" % (head_segment, index.ident_map[head_segment].tag))
  # Now: we have head_segment, connection_point and far_point.
  far_xy = index.GetMarginCoordinate(far_point, head_segment)
  near_xy = index.GetMarginCoordinate(connection_point, head_segment)
  dir_vect = (far_xy[0] - near_xy[0], far_xy[1] - near_xy[1])
  rotation = GetRotationFromVector(dir_vect)
  signal_vect = g_signal_offset_by_rotation[rotation]
  signal_xy = (far_xy[0] + signal_vect[0], far_xy[1] + signal_vect[1])
  layout.append(CreateRGSignalIcon(int(signal_xy[0] - 8), int(signal_xy[1] - 8), "Sig." + block, rotation))
  # Computes the other end of the body segments.
  body_pt_map = {}
  for b in index.block_map[bodyblock]:
    neighbors = index.GetNeighbors(b)
    for n in neighbors:
      if n not in body_pt_map:
        body_pt_map[n] = []
      body_pt_map[n].append(b)
  multi = [name for (name, listord) in body_pt_map.items() if len(listord) > 1]
  for name in multi: del body_pt_map[name]
  if connection_point not in body_pt_map:
    raise Exception(("Connection point %s not found in " % connection_point) + str(singles))
  del body_pt_map[connection_point]
  print("singles: ", body_pt_map, "connection", connection_point)
  if len(body_pt_map) != 1:
    raise Exception("Cannot find unique body endpoint for block " + block)
  far_tuple = body_pt_map.popitem()
  far_point = far_tuple[0]
  head_segment = far_tuple[1][0]
  head_neighbors = index.GetNeighbors(head_segment)
  head_neighbors.remove(far_point)
  connection_point = head_neighbors.pop()
  far_xy = index.GetMarginCoordinate(far_point, head_segment)
  near_xy = index.GetMarginCoordinate(connection_point, head_segment)
  dir_vect = (far_xy[0] - near_xy[0], far_xy[1] - near_xy[1])
  rotation = GetRotationFromVector(dir_vect)
  signal_vect = g_signal_offset_by_rotation[rotation]
  signal_xy = (far_xy[0] + signal_vect[0], far_xy[1] + signal_vect[1])
  layout.append(CreateRGSignalIcon(int(signal_xy[0] - 8), int(signal_xy[1] - 8), "Sig.R" + block, rotation))
  

def CreateLocoIcon(x, y, text):
  return ET.XML('    <locoicon x="'+str(x)+'" y="' + str(y) + '" level="9" forcecontroloff="false" hidden="no" positionable="true" showtooltip="false" editable="false" text="'+ text + '" size="12" style="0" red="51" green="51" blue="51" redBack="238" greenBack="238" blueBack="238" justification="centre" orientation="horizontal" icon="yes" dockX="967" dockY="153" class="jmri.jmrit.display.configurexml.LocoIconXml">\n      <icon url="program:resources/icons/markers/loco-white.gif" degrees="0" scale="1.0">\n        <rotation>0</rotation>\n      </icon>\n    </locoicon>\n');

def PrintTrainLine(layout, train, x0, y):
  layout.append(CreateTextLabel(x0 - 120, y, train))
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "perm." + train + ".is_reversed"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "perm." + train + ".do_not_move"))
  x0 += 40
  layout.append(CreateMemoryLabel(x0, y, "train." + train))
  x0 += 80
  layout.append(CreateLocoIcon(x0, y, train))

def RenderPanelLocationTable(output_tree_root, index):
  layout = output_tree_root.find('./LayoutEditor[@name=\'3H layout\']')
  if not layout:
    raise Exception("Cannot find layout editor node.")
  for entry in layout.findall('./sensoricon[@level=\'9\']'):
    layout.remove(entry)
  for entry in layout.findall('./positionablelabel[@level=\'9\']'):
    layout.remove(entry)
  for entry in layout.findall('./locoicon[@level=\'9\']'):
    layout.remove(entry)
  for entry in layout.findall('./signalheadicon[@level=\'9\']'):
    layout.remove(entry)
  y = 190;
  x0 = 250;
  for block in all_locations:
    PrintBlockLine(layout, block, x0, y)
    PrintLocationControls(layout, block, index)
    y += 40
  y = 190
  x0 = 700
  for train in all_trains:
    PrintTrainLine(layout, train, x0, y);
    y += 40


sensor_binding_template = string.Template('addSensorBinding("${id}", "${event_on}", "${url_on}", "${event_off}", "${url_off}");');

def AddSensorIcon(svg_root, jmri_icon):
  img = ET.SubElement(svg_root, 'image')
  img.tail = '\n'
  img.set('x', jmri_icon.attrib['x'])
  img.set('y', jmri_icon.attrib['y'])
  img.set('width', '10')  # TODO: figure out proper W and H
  img.set('height', '10')
  unknown_icon = jmri_icon.find('./unknown').attrib['url']
  img.set('xlink:href', ResourceToUrl(unknown_icon))
  ident = GetSvgId()
  img.set('id', ident)
  sensor_name = jmri_icon.attrib['sensor']
  if sensor_name not in sensor_by_user_name:
    raise Exception('Panel is referncing a non-existing sensor "%s"' % sensor_name)
  sensor_obj = sensor_by_user_name[sensor_name]
  AddJsBinding(sensor_binding_template.substitute(
      id=ident,
      event_on=sensor_obj.event_on, event_off=sensor_obj.event_off,
      url_on=ResourceToUrl(jmri_icon.find('./active').attrib['url']),
      url_off=ResourceToUrl(jmri_icon.find('./inactive').attrib['url'])))

track_occupancy_binding_template = string.Template('addTrackBinding("${id}", "${event_on}", "${color_on}", "${event_off}", "${color_off}");');

def AddTrackSegment(svg_root, jmri_segment, points, panelattrs):
  anchor1 = jmri_segment.attrib['connect1name'] + "_" + jmri_segment.attrib['type1']
  anchor2 = jmri_segment.attrib['connect2name'] + "_" + jmri_segment.attrib['type2']
  if anchor1 not in points:
    print("Could not find connection 1 " + anchor1 + " for track segment " + str(jmri_segment))
    return
  if anchor2 not in points:
    print("Could not find connection 2 " + anchor2 + " for track segment " + str(jmri_segment))
    return
  p1 = points[anchor1]
  p2 = points[anchor2]
  if jmri_segment.attrib['arc'] == 'yes':
    line = ET.SubElement(svg_root, 'path')
    d = 'M ' + p1.x + ' ' + p1.y + ' '
    p1x = float(p1.x); p1y = float(p1.y);
    p2x = float(p2.x); p2y = float(p2.y);
    length = math.sqrt((p2y-p1y)*(p2y-p1y) + (p2x-p1x)*(p2x-p1x))
    halfangle = float(jmri_segment.attrib['angle']) / 2
    if halfangle == 0.0:
      raise Exception("Invalid arc angle: " + jmri_segment.attrib['angle'])
    radius = length/2 / math.sin(math.pi * halfangle / 180.0)
    invertflag = 1 if jmri_segment.attrib['flip'] == 'no' else 0
    d += ' A %.1f %.1f, 0, 0, %d, %s %s' % (radius, radius, invertflag, p2.x, p2.y)
    line.set('d', d)
    line.set('fill', 'none')
  else:
    line = ET.SubElement(svg_root, 'line')
    line.set('x1', p1.x)
    line.set('x2', p2.x)
    line.set('y1', p1.y)
    line.set('y2', p2.y)
  trwidth = panelattrs['sidetrackwidth']
  if jmri_segment.attrib['mainline'] == 'yes':
    trwidth = panelattrs['mainlinetrackwidth']
  line.set('style', 'stroke:%s;stroke-width:%s' % (panelattrs['defaulttrackcolor'],trwidth))
  blockname = jmri_segment.attrib.get('blockname')
  if not blockname: return
  block = layoutblocks_by_name.get(blockname)
  if not block:
    raise Exception("Track segment refers to block %s which is not found" % block)
  defcolor = block.attrib['trackcolor']
  occcolor = block.attrib['occupiedcolor']
  ident = GetSvgId()
  line.set('id', ident)
  sensorname = block.attrib['occupancysensor']
  sensor_obj = sensor_by_user_name.get(sensorname)
  if not sensor_obj:
    raise Exception("Track segment %s block %s refers to sensor %s which is not found" % (jmri_segment.attrib['ident'],blockname,sensorname))
  AddJsBinding(track_occupancy_binding_template.substitute(
      id=ident,
      event_on=sensor_obj.event_on, event_off=sensor_obj.event_off,
      color_on=occcolor, color_off=defcolor))

turnout_binding_template = string.Template('addTurnoutBinding("${event_closed}", "${event_thrown}", "${activate_id}", ${xcen}, ${ycen}, "${closed_id}", ${x1}, ${y1}, "${thrown_id}", ${x2}, ${y2});');

def AddTurnout(svg_root, jmri_turnout, points, panelattrs, track_is_mainline):
  ttype = jmri_turnout.attrib['type']
  if not (ttype == "1" or ttype == "2"):
    return
  jid = jmri_turnout.attrib['ident']
  pa = points[jid + "_2"]
  pb = points[jid + "_3"]
  pc = points[jid + "_4"]
  pcen = PositionablePoint()
  pcen.x = jmri_turnout.attrib['xcen']
  pcen.y = jmri_turnout.attrib['ycen']
  coreline = ET.SubElement(svg_root, 'line')
  coreline.set('x1', pa.x)
  coreline.set('x2', pcen.x)
  coreline.set('y1', pa.y)
  coreline.set('y2', pcen.y)
  trwidth = panelattrs['mainlinetrackwidth'] if track_is_mainline[jmri_turnout.attrib['connectaname']] else panelattrs['sidetrackwidth']
  coreline.set('style', 'stroke:%s;stroke-width:%s' % (panelattrs['defaulttrackcolor'],trwidth))

  cloline = ET.SubElement(svg_root, 'line')
  cloline.set('x1', pb.x)
  cloline.set('x2', pcen.x)
  cloline.set('y1', pb.y)
  cloline.set('y2', pcen.y)
  trwidth = panelattrs['mainlinetrackwidth'] if track_is_mainline[jmri_turnout.attrib['connectbname']] else panelattrs['sidetrackwidth']
  cloline.set('style', 'stroke:%s;stroke-width:%s' % (panelattrs['defaulttrackcolor'],trwidth))
  closed_id = GetSvgId()
  cloline.set('id', closed_id)

  divline = ET.SubElement(svg_root, 'line')
  divline.set('x1', pc.x)
  divline.set('x2', pcen.x)
  divline.set('y1', pc.y)
  divline.set('y2', pcen.y)
  trwidth = panelattrs['mainlinetrackwidth'] if track_is_mainline[jmri_turnout.attrib['connectcname']] else panelattrs['sidetrackwidth']
  divline.set('style', 'stroke:%s;stroke-width:%s' % (panelattrs['defaulttrackcolor'],trwidth))
  div_id = GetSvgId()
  divline.set('id', div_id)

  buttons_el = svg_root #.find("./g[@id='turnout_buttons']")
  active_g = ET.SubElement(buttons_el, 'g', opacity="0.0")
  circle = ET.SubElement(active_g, 'circle')
  circle.set('cx', pcen.x)
  circle.set('cy', pcen.y)
  lenb = math.sqrt(math.fabs(float(pcen.x)-float(pb.x)) ** 2 + math.fabs(float(pcen.y)-float(pb.y)) ** 2)
  lenc = math.sqrt(math.fabs(float(pcen.x)-float(pc.x)) ** 2 + math.fabs(float(pcen.y)-float(pc.y)) ** 2)
  circle.set('r', '%.1f' % ((lenb + lenc) / 2))
  circle.set('style', 'fill:white;stroke:black')
  activate_id = GetSvgId()
  active_g.set('id', activate_id)

  turnout_name = jmri_turnout.get('turnoutname')
  if turnout_name:
    event_on = turnout_by_user_name[turnout_name].event_on
    event_off = turnout_by_user_name[turnout_name].event_off
    AddJsBinding(turnout_binding_template.substitute(
        event_closed=event_off,
        event_thrown=event_on,
        activate_id=activate_id,
        xcen=pcen.x,
        ycen=pcen.y,
        closed_id=closed_id,
        x1=pb.x,
        y1=pb.y,
        thrown_id=div_id,
        x2=pc.x,
        y2=pc.y))

def ProcessPanel(panel_root, output_html):
  parent_div = output_html.find('.//div[@id="panel"]')
  panel_div = ET.SubElement(parent_div, 'div')
  parent_div.text = panel_root.attrib['name']
  svg = ET.SubElement(panel_div, 'svg')
  svg.set('width', panel_root.attrib['windowwidth'])
  svg.set('height', panel_root.attrib['windowheight'])
  ET.SubElement(svg, 'g', id="turnout_buttons")
  unknown_count = {}
  points = {}
  track_is_mainline = {}
  for entry in panel_root.findall('./positionablepoint'):
    r = PositionablePoint()
    r.x = entry.attrib['x']
    r.y = entry.attrib['y']
    points[entry.attrib['ident'] + "_1"] = r
  for entry in panel_root.findall('./layoutturnout'):
    ident = entry.attrib['ident']
    ttype = entry.attrib['type']
    if ttype == "1" or ttype == "2" or ttype == "4":
      r = PositionablePoint()
      r.x = entry.attrib['xa']
      r.y = entry.attrib['ya']
      points[ident + "_2"] = r
      r = PositionablePoint()
      r.x = entry.attrib['xb']
      r.y = entry.attrib['yb']
      points[ident + "_3"] = r
      r = PositionablePoint()
      r.x = entry.attrib['xc']
      r.y = entry.attrib['yc']
      points[ident + "_4"] = r
    if ttype == "4":
      r = PositionablePoint()
      r.x = entry.attrib['xd']
      r.y = entry.attrib['yd']
      points[ident + "_5"] = r
  for entry in panel_root.findall('./tracksegment'):
    track_is_mainline[entry.attrib['ident']] = (entry.attrib['mainline'] == 'yes')
  for entry in panel_root:
    if entry.tag == 'sensoricon':
      AddSensorIcon(svg, entry)
    elif entry.tag == 'positionablepoint':
      pass
    elif entry.tag == 'tracksegment':
      AddTrackSegment(svg, entry, points, panel_root.attrib)
    elif entry.tag == 'layoutturnout':
      AddTurnout(svg, entry, points, panel_root.attrib, track_is_mainline)
    else:
      if entry.tag not in unknown_count: unknown_count[entry.tag] = 0
      unknown_count[entry.tag] += 1

  for tag, count in unknown_count.items():
    print('Found %d unknown tags of %s' % (count, tag))


def main():
  if len(sys.argv) < 2:
    print(("Usage: %s jmri-infile.xml panel-outfile.html" % sys.argv[0]), file=sys.stderr)
    sys.exit(1)

  xml_tree = ET.parse(sys.argv[1])
  root = xml_tree.getroot()
  ParseAllSensors(xml_tree)
  ParseAllTurnouts(xml_tree)
  ParseAllBlocks(xml_tree)

  html_root = ET.fromstring(webpage_template)
  html_tree = ET.ElementTree(html_root)

  panel = root.find('./LayoutEditor[@name="3H layout"]')
  if not panel:
    raise Exception("Desired panel not found")
  ProcessPanel(panel, html_tree)

  RenderJsBindings(html_root)
  html_tree.write(sys.argv[2], method="html")


main()
