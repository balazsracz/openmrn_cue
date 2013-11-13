#ifndef _BRACZ_TRAIN_UPDATER_BASE_HXX_
#define _BRACZ_TRAIN_UPDATER_BASE_HXX_

class Updatable {
public:
  virtual ~Updatable() {}

  virtual void PerformUpdate() = 0;
};

// Used for registering modules that act upon input updaters' actions.
class UpdateListener {
public:
  virtual ~UpdateListener() {}

  // @returns the new value to be written to the backing store.
  virtual uint8_t OnChanged(uint8_t offset, uint8_t previous_value,
                            uint8_t new_value) = 0;
};

#endif // _BRACZ_TRAIN_UPDATER_BASE_HXX_
