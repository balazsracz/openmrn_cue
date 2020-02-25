# Sample script showing how to move a train used in operations
# The train to be moved is identified by a memory
#
# Part of the JMRI distribution
#
# Author: Balazs Racz copyright 2019
#
# To run multiple times, use this in the script console:
# for i in range(4): v = buildSession(); v.start(); v.waitForFinish()


import jmri
import time

class buildSession(jmri.jmrit.automat.AbstractAutomaton) :
  _hasStarted = False
  _isComplete = False

  def init(self):
    # get the train manager
    self.tm = jmri.InstanceManager.getDefault(jmri.jmrit.operations.trains.TrainManager)
    self.lm = jmri.InstanceManager.getDefault(jmri.jmrit.operations.locations.LocationManager)
    self.cm = jmri.InstanceManager.getDefault(jmri.jmrit.operations.rollingstock.cars.CarManager)
    return

  def resetSwitchLists(self):
    """  Location location = locationManager.getLocationByName(locationName);
            if (location.isSwitchListEnabled()) {
                // new switch lists will now be created for the location
                location.setSwitchListState(Location.SW_CREATE);
                location.setStatus(Location.MODIFIED);
            }
        }
        // set trains switch lists unknown, any built trains should remain on the switch lists
        InstanceManager.getDefault(TrainManager.class).setTrainsSwitchListStatus(Train.UNKNOWN);    
    """
    rbl = self.lm.getLocationByName("RBL")
    if rbl == None:
      raise Exception("Yard not found")
    rbl.setSwitchListState(rbl.SW_CREATE);
    rbl.setStatus(rbl.MODIFIED)
    self.tm.setTrainsSwitchListStatus(jmri.jmrit.operations.trains.Train.UNKNOWN)

  def updateSwitchLists(self):
    #buildSwitchList(IS_PREVIEW, !IS_CHANGED, !IS_CSV, IS_UPDATE);
    trainSwitchLists = jmri.jmrit.operations.trains.TrainSwitchLists();
    rbl = self.lm.getLocationByName("RBL")
    if rbl == None:
      raise Exception("Yard not found")
    trainSwitchLists.buildSwitchList(rbl);
    self.tm.setTrainsSwitchListStatus(jmri.jmrit.operations.trains.Train.PRINTED);
    pass


  def findTrainByName(self, name):
    train = self.tm.getTrainByName(name)
    if train == None:
      print "Train '", name , "' does not exist."
      raise Exception("train not found")
    if train.isBuilt():
      print "Train '", name , "' already built."
      raise Exception("train already built")
    return train

  def addStat(self, desc, value):
    self.statsDesc.append(desc)
    self.stats.append(value)

  def buildTrain(self, trainObj, name):
    trainObj.build()
    time.sleep(0.2)
    dest = trainObj.getTrainTerminatesRouteLocation()
    src = trainObj.getRoute().getDepartsRouteLocation()
    tiecar = 0
    startcar = 0
    for car in self.cm.getList(trainObj):
      if car.getRouteDestination() == dest:
        tiecar += 1
      if car.getRouteLocation() == src:
        startcar += 1
    stv = str(startcar) + '_' + str(trainObj.getNumberCarsWorked()) + '_' + str(tiecar)
    self.addStat(name, stv)

  def addLocationStat(self, locationObj, name):
    if locationObj is None:
      print name, ' location not found'
      return
    stv = str(locationObj.getNumberRS()) + '/'
    stv = stv + str(locationObj.getUsedLength())
    percent = locationObj.getUsedLength() * 100 / locationObj.getLength()
    stv = stv + '/' + str(percent) + '%'
    self.addStat(name, stv)

  def waitForFinish(self):
    while not self._isComplete:
      time.sleep(0.2)

  def handle(self):
    self._hasStarted = True
    self.stats = []
    self.statsDesc = []
    morn_east_th = self.findTrainByName("Morning east through")
    morn_west_th = self.findTrainByName("Morning west through")
    morn_dott_east = self.findTrainByName("Morning Dottikon east")
    timesaver = self.findTrainByName("Timesaver")
    west_lo = self.findTrainByName("West local")
    east_lo = self.findTrainByName("East local")
    eve_east_th = self.findTrainByName("Evening east through")
    eve_west_th = self.findTrainByName("Evening west through")
    eve_dott_west = self.findTrainByName("Evening Dottikon west")
    self.resetSwitchLists()

    self.buildTrain(morn_east_th, 'morn_east_th')
    self.updateSwitchLists()
    morn_east_th.move()
    self.buildTrain(morn_west_th, 'morn_west_th')
    self.buildTrain(morn_dott_east, 'morn_dott_east')
    self.updateSwitchLists()
    morn_dott_east.move()
    self.buildTrain(west_lo, 'west_lo')
    self.buildTrain(timesaver, 'timesaver')
    self.updateSwitchLists()
    west_lo.move()
    timesaver.move()
    morn_east_th.terminate()
    morn_west_th.terminate()
    self.buildTrain(east_lo, 'east_lo')
    self.updateSwitchLists()
    east_lo.move()
    west_lo.terminate()
    self.buildTrain(eve_east_th, 'eve_east_th')
    self.updateSwitchLists()
    eve_east_th.move()
    morn_dott_east.move()
    morn_dott_east.move()
    morn_dott_east.move()
    morn_dott_east.move()
    self.buildTrain(eve_dott_west, 'eve_dott_west')
    self.buildTrain(eve_west_th, 'eve_west_th')
    self.updateSwitchLists()

    #
    rbl = self.lm.getLocationByName("RBL")
    bsl = self.lm.getLocationByName("Basel Staging")
    rblst = self.lm.getLocationByName("RBL Staging")
    lenz = self.lm.getLocationByName("Lenzburg")
    dott = self.lm.getLocationByName("Dottikon")
    schaf = self.lm.getLocationByName("Schafisheim")
    daen = self.lm.getLocationByName("Daeniken")
    olten = self.lm.getLocationByName("Olten")

    # terminate everything
    eve_dott_west.move()
    eve_west_th.move()
    self.addLocationStat(rbl, 'RBL an')
    morn_dott_east.terminate()
    timesaver.terminate()
    east_lo.terminate()
    eve_east_th.terminate()
    eve_dott_west.terminate()
    eve_west_th.terminate()
    self.addLocationStat(rbl, 'RBL')
    self.addLocationStat(bsl, 'Bsl st')
    self.addLocationStat(rblst, 'RBL st')
    self.addLocationStat(lenz, 'Lenz')
    self.addLocationStat(dott, 'Dott')
    self.addLocationStat(schaf, 'Schaf')
    self.addLocationStat(daen, 'Daen')
    self.addLocationStat(olten, 'Olt')

    print '\t'.join(self.statsDesc)
    print '\t'.join([str(x) for x in self.stats])
    self._isComplete = True

    return False
    # define which memory the train id is located in
    memSysName = "12"    # Memory system name IM12
    # get the memory
    memTrainId = memories.provideMemory(memSysName)
    if (memTrainId.getValue() == None):
      print "No train id in memory", trainId
      return False              # all done, don't repeat again
    else:
      print "get train by id:", memTrainId.getValue()

    # get train by id
    train = self.tm.getTrainById(memTrainId.getValue())
    if (train != None):
      if (train.isBuilt() == True):
        print "Move train", train.getName(), train.getDescription()
        train.move()
        print "Status:", train.getStatus();
        print "Current location:", train.getCurrentLocationName()
      else:
        print "Train", train.getName(), train.getDescription(), "hasn't been built"
    else :
      print "Train id", memTrainId.getValue(), "does not exist"

    return False              # all done, don't repeat again

buildSession().start()          # create one of these, and start it running
