/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file main.cxx
 *
 * A simple application to demonstrate asynchronous interfaces.
 *
 * @author Balazs Racz
 * @date 7 Dec 2013
 */

#define LOGLEVEL INFO

#include <stdio.h>
#include <unistd.h>


#include "openlcb/SimpleStack.hxx"

#include "commandstation/UpdateProcessor.hxx"
#include "custom/AutomataControl.hxx"
#include "custom/HostPacketCanPort.hxx"
#include "custom/HostProtocol.hxx"
#include "custom/TivaShortDetection.hxx"
#include "custom/LoggingBit.hxx"
#include "custom/BlinkerFlow.hxx"
#include "dcc/LocalTrackIf.hxx"
#include "dcc/RailCom.hxx"
#include "dcc/RailcomHub.hxx"
#include "dcc/RailcomPortDebug.hxx"
#include "executor/PoolToQueueFlow.hxx"
#include "commandstation/AllTrainNodes.hxx"
#include "mobilestation/MobileStationTraction.hxx"
#include "commandstation/TrainDb.hxx"
#include "openlcb/EventHandlerTemplates.hxx"
#include "openlcb/PolledProducer.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/TractionTrain.hxx"
#include "openlcb/TractionCvSpace.hxx"
#include "os/watchdog.h"
#include "utils/Debouncer.hxx"
#include "utils/HubDeviceSelect.hxx"
#include "commandstation/TrackPowerBit.hxx"

#include "hardware.hxx"
#include "config.hxx"

// for logging implementation
#include "src/host_packet.h"
#include "src/usb_proto.h"


#define STANDALONE
//#define ENABLE_HOST
#define LOGTOSTDOUT

#if !defined(ENABLE_HOST) && !defined(LOGTOSTDOUT)
#define LOGTOSTDOUT
#endif

// Used to talk to the booster.
//OVERRIDE_CONST(can2_bitrate, 250000);

// Forces all trains to be our responsibility.
OVERRIDE_CONST(mobile_station_train_count, 0);

OVERRIDE_CONST(automata_init_backoff, 20000);
//OVERRIDE_CONST(node_init_identify, 1);

OVERRIDE_CONST(dcc_packet_min_refresh_delay_ms, 1);
OVERRIDE_CONST(num_datagram_registry_entries, 3);
OVERRIDE_CONST(num_memory_spaces, 10);


namespace commandstation {

/*
extern const struct const_traindb_entry_t const_lokdb[];
const struct const_traindb_entry_t const_lokdb[] = {
  // 0
  //  { 51, { LIGHT, TELEX, FN_NONEXISTANT, SHUNT, ABV },
  //  "BR 260417", DCC_28 },  // ESU LokPilot 3.0
  // 1
  //{ 66, { LIGHT },
  //"Re 6/6 11665", DCC_128 },
  { 0, {0,}, "", 0},
  { 0, {0,}, "", 0},
  { 0, {0,}, "", 0},
};
extern const size_t const_lokdb_size;
const size_t const_lokdb_size = sizeof(const_lokdb) / sizeof(const_lokdb[0]);
*/

}  // namespace commandstation

static const openlcb::NodeID NODE_ID = 0x050101011432ULL;

openlcb::SimpleCanStack stack(NODE_ID);
CanHubFlow can_hub1(stack.service());  // this CANbus will have no hardware.

openlcb::ConfigDef cfg(0);

extern const char *const openlcb::CONFIG_FILENAME = "/dev/eeprom";
extern const size_t openlcb::CONFIG_FILE_SIZE =
    cfg.trains().size() + cfg.trains().offset();
static_assert(openlcb::CONFIG_FILE_SIZE <=  4*1024, "eeprom too small for train database");
extern const char *const openlcb::SNIP_DYNAMIC_FILENAME =
    openlcb::CONFIG_FILENAME;

extern char __automata_start[];
extern char __automata_end[];

openlcb::FileMemorySpace automata_space("/etc/automata", __automata_end - __automata_start);

//auto* g_gc_adapter = GCAdapterBase::CreateGridConnectAdapter(&stdout_hub, &can_hub0, false);

#ifdef ENABLE_HOST
bracz_custom::HostClient host_client(stack.dg_service(), stack.node(), &can_hub1);
#endif

extern "C" {
#ifdef LOGTOSTDOUT

Executor<1> stdout_exec("logger", 1, 1000);
Service stdout_service(&stdout_exec);
HubFlow stdout_hub(&stdout_service);

void log_output(char* buf, int size) {
    if (size <= 0) return;
    auto* b = stdout_hub.alloc();
    HASSERT(b);
    b->data()->assign(buf, size);
    if (size > 1) b->data()->push_back('\n');
    stdout_hub.send(b);
}

#else

void log_output(char* buf, int size) {
    if (size <= 0) return;
    host_client.log_output(buf, size);
}

#endif
} // extern c

namespace bracz_custom {
#ifdef ENABLE_HOST
void send_host_log_event(HostLogEvent e) {
  host_client.send_host_log_event(e);
}
#endif
}


OVERRIDE_CONST(local_nodes_count, 30);
OVERRIDE_CONST(local_alias_cache_size, 30);

class TivaGPIOProducerBit : public openlcb::BitEventInterface {
 public:
  TivaGPIOProducerBit(uint64_t event_on, uint64_t event_off, uint32_t port_base,
                      uint8_t port_bit)
      : BitEventInterface(event_on, event_off),
        ptr_(reinterpret_cast<const uint8_t*>(port_base +
                                              (((unsigned)port_bit) << 2))) {}


  openlcb::EventState get_current_state() OVERRIDE {
    return *ptr_ ? openlcb::EventState::VALID : openlcb::EventState::INVALID;
  }

  void set_state(bool new_value) OVERRIDE {
    DIE("cannot set state of input producer");
  }

  openlcb::Node* node() OVERRIDE { return stack.node(); }

 private:
  const uint8_t* ptr_;
};

class TivaGPIOConsumer : public openlcb::BitEventInterface,
                         public openlcb::BitEventConsumer {
 public:
  TivaGPIOConsumer(uint64_t event_on, uint64_t event_off, uint32_t port,
                   uint8_t pin)
      : BitEventInterface(event_on, event_off),
        BitEventConsumer(this),
        memory_(reinterpret_cast<uint8_t*>(port + (pin << 2))) {}

  openlcb::EventState get_current_state() OVERRIDE {
    return (*memory_) ? openlcb::EventState::VALID
                      : openlcb::EventState::INVALID;
  }
  void set_state(bool new_value) OVERRIDE {
    if (new_value) {
      *memory_ = 0xff;
    } else {
      *memory_ = 0;
    }
  }

  openlcb::Node* node() OVERRIDE { return stack.node(); }

 private:
  volatile uint8_t* memory_;
};

dcc::RailcomHubFlow railcom_hub(stack.service());
HubDeviceNonBlock<dcc::RailcomHubFlow>* railcom_reader_flow;
dcc::LocalTrackIf track_if(stack.service(), 2);
commandstation::UpdateProcessor cs_loop(stack.service(), &track_if);
PoolToQueueFlow<Buffer<dcc::Packet>> pool_translator(stack.service(), track_if.pool(), &cs_loop);
openlcb::TrainService traction_service(stack.iface());

commandstation::TrackPowerState trackPowerState(stack.node(),
    openlcb::Defs::CLEAR_EMERGENCY_OFF_EVENT,
    openlcb::Defs::EMERGENCY_OFF_EVENT,
    openlcb::Defs::CLEAR_EMERGENCY_STOP_EVENT,
    openlcb::Defs::EMERGENCY_STOP_EVENT);
class TrackConsumer : public openlcb::BitEventConsumer
{
public:
    TrackConsumer(openlcb::BitEventInterface* bit)
        : openlcb::BitEventConsumer(bit)
    {
    }

    /// Ignores "identified" events. The base class behavior is that these turn
    /// the state on/off. We only want the event reports to turn the state
    /// on/off.
    void handle_producer_identified(const openlcb::EventRegistryEntry &entry,
        openlcb::EventReport *event, BarrierNotifiable *done) override
    {
        done->notify();
    }
};
TrackConsumer trackPowerConsumer{trackPowerState.get_power_bit()};
TrackConsumer estopConsumer{trackPowerState.get_estop_bit()};

TivaAccPowerOnOffBit<AccHwDefs> acc_on_off(stack.node(), BRACZ_LAYOUT | 0x0004, BRACZ_LAYOUT | 0x0005);
openlcb::BitEventConsumer accpowerbit(&acc_on_off);

typedef openlcb::PolledProducer<ToggleDebouncer<QuiesceDebouncer>,
                                TivaGPIOProducerBit> TivaSwitchProducer;
QuiesceDebouncer::Options opts(3);

TivaSwitchProducer sw1(opts, openlcb::Defs::CLEAR_EMERGENCY_OFF_EVENT,
                       openlcb::Defs::EMERGENCY_OFF_EVENT,
                       USR_SW1_Pin::GPIO_BASE, USR_SW1_Pin::GPIO_PIN);

TivaSwitchProducer sw2(opts, BRACZ_LAYOUT | 0x0000,
                       BRACZ_LAYOUT | 0x0001,
                       USR_SW2_Pin::GPIO_BASE, USR_SW2_Pin::GPIO_PIN);

TivaGPIOConsumer led_acc(BRACZ_LAYOUT | 4, BRACZ_LAYOUT | 5, io::AccPwrLed::GPIO_BASE, io::AccPwrLed::GPIO_PIN);
TivaGPIOConsumer led_go(BRACZ_LAYOUT | 1, BRACZ_LAYOUT | 0,  io::GoPausedLed::GPIO_BASE, io::GoPausedLed::GPIO_PIN);

openlcb::RefreshLoop loop(stack.node(), {&sw1, &sw2});

bracz_custom::AutomataControl automatas(stack.node(), stack.dg_service(), (const insn_t*) __automata_start);

/*TivaSwitchProducer sw2(opts, openlcb::Defs::CLEAR_EMERGENCY_OFF_EVENT,
                       openlcb::Defs::EMERGENCY_OFF_EVENT,
                       USR_SW1::GPIO_BASE, USR_SW1::GPIO_PIN);
*/

//dcc::Dcc28Train train_Am843(dcc::DccShortAddress(43));
//openlcb::TrainNode train_Am843_node(&traction_service, &train_Am843);
//dcc::Dcc28Train train_Re460(dcc::DccShortAddress(22));
//dcc::MMNewTrain train_Re460(dcc::MMAddress(22));
//openlcb::TrainNode train_Re460_node(&traction_service, &train_Re460);


//mobilestation::MobileStationSlave mosta_slave(&g_executor, &can1_interface);
commandstation::TrainDb train_db(cfg.trains().all_trains());
CanIf can1_interface(stack.service(), &can_hub1);
mobilestation::MobileStationTraction mosta_traction(&can1_interface, stack.iface(), &train_db, stack.node());

commandstation::AllTrainNodes all_trains(&train_db, &traction_service, stack.info_flow(), stack.memory_config_handler());

openlcb::TractionCvSpace traction_cv(stack.memory_config_handler(), &track_if, &railcom_hub, openlcb::MemoryConfigDefs::SPACE_DCC_CV);

typedef openlcb::PolledProducer<QuiesceDebouncer, TivaGPIOProducerBit>
    TivaGPIOProducer;

void mydisable()
{
  IntDisable(INT_TIMER5A);
  IntDisable(INT_TIMER0A);
  IntDisable(DccHwDefs::ADC_INTERRUPT);
  IntDisable(DccHwDefs::OS_INTERRUPT);
  IntDisable(DccHwDefs::INTERVAL_INTERRUPT);
  IntDisable(INT_USB0);
  IntDisable(INT_CAN0);
  IntDisable(INT_UART2);
  asm("BKPT 0");
}

TivaShortDetectionModule<DccHwDefs> g_short_detector(stack.node());

AccessoryOvercurrentMeasurement<AccHwDefs> g_acc_short_detector(stack.service(), stack.node());

extern "C" {
void adc0_seq3_interrupt_handler(void) {
  g_short_detector.interrupt_handler();
}
void adc0_seq2_interrupt_handler(void) {
  g_acc_short_detector.interrupt_handler();
}
}


commandstation::TrainDbFactoryResetHelper g_reset_helper(cfg.trains().all_trains());
HubDeviceSelect<HubFlow>* usb_port;

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    DccHwDefs::Output::set_disable_reason(
        DccOutput::DisableReason::INITIALIZATION_PENDING);
    DccHwDefs::Output::set_disable_reason(
        DccOutput::DisableReason::GLOBAL_EOFF);

    stack.check_version_and_factory_reset(cfg.seg().internal_config(), openlcb::CANONICAL_VERSION, false);

  //  mydisable();
  // TODO(balazs.racz): add a workign implementation of watchdog.
  //start_watchdog(5000);
  // add_watchdog_reset_timer(500);
    stack.add_can_port_select("/dev/can0");
    //stack.add_gridconnect_port("/dev/serUSB0");
    //usb_port = new HubDeviceSelect<HubFlow>(stack.gridconnect_hub(), "/dev/serUSB0");
#ifdef LOGTOSTDOUT
    FdHubPort<HubFlow> stdout_port(&stdout_hub, 0, EmptyNotifiable::DefaultInstance());
#endif
    int mainline = open("/dev/mainline", O_RDWR);
    HASSERT(mainline > 0);
    track_if.set_fd(mainline);
    
    railcom_reader_flow =
        new HubDeviceNonBlock<dcc::RailcomHubFlow>(&railcom_hub, "/dev/railcom");
    //int railcom_fd = open("/dev/railcom", O_RDWR);
    //HASSERT(railcom_fd > 0);
    dcc::RailcomPrintfFlow railcom_debug(&railcom_hub);


    openlcb::Velocity v;
    v.set_mph(29);
    //XXtrain_Re460.set_speed(v);
    /*train_Am843.set_speed(v);
    train_Am843.set_fn(0, 1);
    train_Re460.set_fn(0, 1);*/

    /*int fd = open("/dev/can0", O_RDWR);
    ASSERT(fd >= 0);
    dcc_can_init(fd);*/

    static const uint64_t BLINKER_EVENT_ID = 0x0501010114FF2B08ULL;
    LoggingBit logger(BLINKER_EVENT_ID, BLINKER_EVENT_ID + 1, "blinker");
    openlcb::BitEventConsumer consumer(&logger);

    stack.memory_config_handler()->registry()->insert(stack.node(), 0xA0, &automata_space);


#ifdef STANDALONE
    // Start dcc output
    DccHwDefs::Output::clear_disable_reason(
        DccOutput::DisableReason::GLOBAL_EOFF);
#else
    // Do not start dcc output.
    DccHwDefs::Output::set_disable_reason(
        DccOutput::DisableReason::GLOBAL_EOFF);
#endif

    stack.loop_executor();
    return 0;
}
