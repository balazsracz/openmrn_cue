#!/usr/bin/python3.4

import xml.etree.ElementTree as ET
import sys
import re

FLAGS_purge_all = True

class LocationInfo:
  pass

class Sensor:
  def __init__(self, system_name, user_name):
    self.system_name = system_name
    self.user_name = user_name

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

class Turnout:
  def __init__(self, system_name, user_name, sensor_name):
    self.system_name = system_name
    self.user_name = user_name
    self.sensor_name = sensor_name

  def Render(self, parent_element):
    e = ET.SubElement(parent_element, 'turnout')
    e.tail = '\n'
    e.set('systemName', self.system_name)
    e.set('inverted', 'false')
    e.set('userName', self.user_name)
    e.set('automate', 'Default')
    if self.sensor_name:
      e.set('feedback', 'ONESENSOR')
      e.set('sensor1', self.sensor_name)
    else:
      e.set('feedback', 'DIRECT')
    sn = ET.SubElement(e, 'systemName')
    sn.text = self.system_name
    un = ET.SubElement(e, 'userName')
    un.text = self.user_name

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


class DoubleSignalHead(SignalHead):
  def __init__(self, name, turnout_red, turnout_green):
    self.user_name = name
    self.turnout_red_name = turnout_red
    self.turnout_green_name = turnout_green

#  def key(self):
#    return self.user_name

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
    e.set('class', "jmri.implementation.configurexml.DoubleTurnoutSignalHeadXml")
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
    at = FindOrInsertWithDefines(e, 'turnoutname', 'green')
    at.text = self.turnout_green_name
    at = FindOrInsertWithDefines(e, 'turnoutname', 'red')
    at.text = self.turnout_red_name


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
    m = re.match('perm\.(.*)\.loc\.(.*)', sensor_name)
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
    self.AddJythonAction(e, "PositionLocoMarker(\"2B layout\", \""
                         + self.train_name + "\", "
                         + str(int(coord.center[0]))+ ", "
                         + str(int(coord.center[1])) + ", HORIZONTAL)")

all_sensors = []
sensor_by_user_name = {}
all_turnouts = []
all_locations = []
all_trains = []
all_location_logix_conditionals = []
all_location_coordinates = {}

def ParseAllSensors(sensor_tree):
  """Reads all the sensor objects from the xml tree passed in, and fills out the global list all_sensors."""
  root = sensor_tree
  for entry in root.findall('sensor'):
    system_name = entry.get('systemName')
    user_name = entry.get('userName')
    s = Sensor(system_name, user_name)
    all_sensors.append(s)
    sensor_by_user_name[user_name] = s

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
    m = re.match('turnout_(.*)', sensor.user_name)
    if m:
      systemname = 'MT' + sensor.system_name[2:]
      username = sensor.user_name
      sensorname = sensor.user_name
      all_turnouts.append(Turnout(systemname, username, sensorname))
    m = re.match('sig_(.*)', sensor.user_name)
    if m:
      systemname = 'MT' + sensor.system_name[2:]
      username = sensor.user_name
      sensorname = None
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
  if parent_element is None: return max_id
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
  MergeEntries(all_block_node, desired_block_list, purge = FLAGS_purge_all)
  all_lblock_node = output_tree_root.find('layoutblocks')
  LayoutBlock.SetMaxId(GetMaxSystemId(all_lblock_node))
  MergeEntries(all_lblock_node, desired_lblock_list, purge = FLAGS_purge_all)

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
    m = re.match('logic[.](.*)[.]rsignal.route_set_ab', sensor.user_name)
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
    if not segments:
      print("Cound not find track segment for block '%s'" % loc)
      continue
    segment = segments[0]
    ret[loc] = LocationInfo()
    ret[loc].center = index.FindCenterLocation(segment)
  return ret

def RenderSignalHeads(output_tree_root):
  desired_signalhead_list = []
  for location in all_locations:
    desired_signalhead_list.append(SignalHead('Sig.' + location))
    desired_signalhead_list.append(SignalHead('Sig.R' + location))
  for sensor in all_sensors:
    m = re.match('(sig_.*)_red', sensor.user_name)
    if not m: continue
    signame = m.group(1)
    desired_signalhead_list.append(DoubleSignalHead(signame, signame + "_red", signame + "_green"))
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
  if not all_memory_node:
    print("No memory node - skipping rendering")
    return
  Memory.SetMaxId(GetMaxSystemId(all_memory_node, strip_letters='IM:AUTO:LOC'))
  print("Number of memory entries: ", len(desired_memory_list))
  MergeEntries(all_memory_node, desired_memory_list, purge = FLAGS_purge_all)

def RenderLogixConditionals(output_tree_root):
  desired_conditionals = []
  for sensor in all_sensors:
    m = re.match('perm.*loc.*', sensor.user_name)
    #m = re.match('perm.\([^.]*\).loc.logic.\(.*\)', sensor.user_name)
    if not m: continue
    desired_conditionals.append(TrainLocLogixConditional(sensor.user_name))
  print("Number of train location logix conditionals: ", len(desired_conditionals))
  all_cond_node = output_tree_root.find('conditionals')
  if not all_cond_node:
    print("No conditionals node - skipping rendering")
  else:
    TrainLocLogixConditional.SetMaxId(GetMaxSystemId(all_cond_node, 'IX:GEN:TRAINLOC:BCC'))
    MergeEntries(all_cond_node, desired_conditionals, purge = True)

  # Finds the main logix node
  logixnode = output_tree_root.find('./logixs/logix[@systemName=\'IX:GEN:TRAINLOC:\']')
  if not logixnode:
    print("No logix node - skipping rendering")
    return
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

def CreateSignalLabelOnTrack(x, y, head_name, direction):
  return ET.XML('    <positionablelabel x="' + str(x) + '" y="' + str(y) + '" level="9" forcecontroloff="false" hidden="no" positionable="true" showtooltip="false" editable="true" text="'+ head_name + '" size="8" style="0" red="0" green="0" blue="0" hasBackground="no" justification="centre" orientation="'+ direction +'" class="jmri.jmrit.display.configurexml.PositionableLabelXml"/>\n')

# this many pixels behind the head point should the signals start
g_back = 8
# this many pixels away from the track should the signals start
g_away = 6
# center point of a signal (from top-left)
g_sigcenter = 8
# width of the track which needs to be added to away if we're on the "far" side of the thickness.
g_trackwidth = 4

# this is how much behond the signal the text should be
g_text_from_signal = 10
g_textback = g_back + g_text_from_signal
# length of the text label (for reverse horizontal layout)
g_textlength = 20
g_texth = 10
g_textaway = 2

"""rotation constants:
0 = O-
1 = |
    O
2 = -O
3 = O
    |
"""

# gives the vector offset of where the signal should be from the head point
# given the orientation
g_signal_offset_by_rotation = [
    (g_back - g_sigcenter, g_away - g_sigcenter + g_trackwidth),
    (g_away - g_sigcenter + g_trackwidth, -g_back - g_sigcenter),
    (-g_back - g_sigcenter, -g_away - g_sigcenter),
    (-g_away - g_sigcenter, g_back - g_sigcenter)
]

g_signallabel_offset_by_rotation = [
    (g_textback, g_textaway, 0, 0),
    (g_textaway, -g_textback, 0, -1),
    (-g_textback, - g_textaway - g_texth, -1, 0),
    (- g_textaway - g_texth, g_textback, 0, 0)
]

g_text_direction_by_rotation = [
  "horizontal",
  "vertical_down",
  "horizontal",
  "vertical_up",
]

g_signal_reverse_names = {
    "A240" : "A131",
    "B129" : "A117",
    "A217" : "B229",
    "A317" : "B329",
    "B108" : "A100",
    "A200" : "B208",
    "XX.B1" : "XX.A1",
    "XX.A2" : "XX.B2",
    "XX.A3" : "XX.B3",
    "A406" : "B400",
}

g_missing_signals = {
#  "WW.A11",
#  "YY.A3"
}


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
  layout.append(CreateRGSignalIcon(int(signal_xy[0]), int(signal_xy[1]), "Sig." + block, rotation))
  siglabel_vect = g_signallabel_offset_by_rotation[rotation]
  signame = block
  if signame in g_missing_signals: signame = "NA"
  siglabel_xy = (far_xy[0] + siglabel_vect[0] + siglabel_vect[2] * g_textlength * len(signame) / 4, far_xy[1] + siglabel_vect[1] + siglabel_vect[3] * g_textlength * len(signame) / 4)
  siglabel_rot = g_text_direction_by_rotation[rotation]
  layout.append(CreateSignalLabelOnTrack(int(siglabel_xy[0]), int(siglabel_xy[1]), signame, siglabel_rot))
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
  layout.append(CreateRGSignalIcon(int(signal_xy[0]), int(signal_xy[1]), "Sig.R" + block, rotation))
  siglabel_vect = g_signallabel_offset_by_rotation[rotation]
  signame = "NA"
  if block in g_signal_reverse_names:
    signame = g_signal_reverse_names[block]
  siglabel_xy = (far_xy[0] + siglabel_vect[0] + siglabel_vect[2] * g_textlength * len(signame) / 4, far_xy[1] + siglabel_vect[1] + siglabel_vect[3] * g_textlength * len(signame) / 4)
  siglabel_rot = g_text_direction_by_rotation[rotation]
  layout.append(CreateSignalLabelOnTrack(int(siglabel_xy[0]), int(siglabel_xy[1]), signame, siglabel_rot))

  

def CreateLocoIcon(x, y, text):
  return ET.XML('    <locoicon x="'+str(x)+'" y="' + str(y) + '" level="9" forcecontroloff="false" hidden="no" positionable="true" showtooltip="false" editable="false" text="'+ text + '" size="12" style="0" red="51" green="51" blue="51" hasBackground="no" justification="centre" icon="yes" dockX="967" dockY="153" class="jmri.jmrit.display.configurexml.LocoIconXml">\n      <icon url="program:resources/icons/markers/loco-white.gif" degrees="0" scale="1.0">\n        <rotation>0</rotation>\n      </icon>\n    </locoicon>\n');

def PrintTrainLine(layout, train, x0, y):
  layout.append(CreateTextLabel(x0 - 120, y, train))
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "perm." + train + ".is_reversed"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "perm." + train + ".do_not_move"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic.train." + train + ".is_moving"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic.train." + train + ".req_go"))
  x0 += 40
  layout.append(CreateSensorIcon(x0, y + sensoroffset, "logic.train." + train + ".req_stop"))
  x0 += 40
  layout.append(CreateMemoryLabel(x0, y, "train." + train))
  x0 += 80
  layout.append(CreateLocoIcon(x0, y, train))

def ClearPanelLocationTable(output_tree_root, index):
  layout = output_tree_root.find('./LayoutEditor[@name=\'2B layout\']')
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
  for entry in layout.findall('./memoryicon[@level=\'9\']'):
    layout.remove(entry)
  for block in all_locations:
    PrintLocationControls(layout, block, index)


def RenderPanelLocationTable(output_tree_root, index):
  layout = output_tree_root.find('./LayoutEditor[@name=\'2B layout\']')
  if not layout:
    raise Exception("Cannot find layout editor node.")
  y = 190
  x0 = 250 + 160
  for block in all_locations:
    PrintBlockLine(layout, block, x0, y)
    y += 40
  y = 190
  x0 = 700 + 160
  for train in all_trains:
    PrintTrainLine(layout, train, x0, y);
    y += 40


def main():
  if len(sys.argv) < 2:
    print(("Usage: %s jmri-infile.xml jmri-outfile.xml [skiptable]" % sys.argv[0]), file=sys.stderr)
    sys.exit(1)

  xml_tree = ET.parse(sys.argv[1])
  root = xml_tree.getroot()
  sensor_tree_root = ReadVariableFile()
  ParseAllSensors(sensor_tree_root)
  global all_trains, all_locations, all_location_coordinates
  all_trains = GetAllTrainList()
  all_locations = GetAllLocationList()
  index = LayoutIndex(root, "2B layout")
  all_location_coordinates = GetLocationCoordinates(all_locations, root, index)
  print (len(all_trains), " trains, ", len(all_locations), " locations found (", len(all_location_coordinates), " with coord)")
  RenderSensors(root)
  CreateTurnouts()
  RenderTurnouts(root)
  RenderSignalHeads(root)
  RenderSystemBlocks(root)
  RenderMemoryVariables(root)
  ClearPanelLocationTable(root, index)
  print("len = ", len(sys.argv), " args=", sys.argv)
  if (len(sys.argv) <= 3) or (sys.argv[3] != "skiptable"):
    RenderLogixConditionals(root)
    RenderPanelLocationTable(root, index)
  else:
    print("Skipping panel table.")
  print("number of sensors: %d" % len(all_sensors))
  xml_tree.write(sys.argv[2])



main()
