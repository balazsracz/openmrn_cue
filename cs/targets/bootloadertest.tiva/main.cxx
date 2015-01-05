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

#include "os/os.h"
#include "utils/Hub.hxx"
#include "utils/HubDevice.hxx"
#include "utils/HubDeviceNonBlock.hxx"
#include "utils/GridConnectHub.hxx"
#include "executor/Executor.hxx"
#include "can_frame.h"
#include "nmranet_config.h"

//#define STANDALONE

Executor<1> g_executor("g_executor", 2, 1024);
Service g_service(&g_executor);
CanHubFlow can_hub0(&g_service);

OVERRIDE_CONST(gc_generate_newlines, 1);
OVERRIDE_CONST(main_thread_stack_size, 2500);
OVERRIDE_CONST(main_thread_priority, 1);

/** Proxy class for sending canbus traffic to the Bootloader HAL. */
class BootloaderPort : public CanHubPort
{
public:
    BootloaderPort()
        : CanHubPort(&g_service)
    {
        is_waiting_ = false;
    }

    bool is_waiting()
    {
        return is_waiting_;
    }

    virtual Action entry()
    {
        AtomicHolder h(this);
        is_waiting_ = true;
        return wait_and_call(STATE(sent));
    }

    Action sent()
    {
        return release_and_exit();
    }

    bool read_can_frame(struct can_frame *frame)
    {
        {
            AtomicHolder h(this);
            if (is_waiting_)
            {
                *frame = *message()->data();
                is_waiting_ = false;
            }
            else
            {
                return false;
            }
        }
        notify();
        return true;
    }

private:
    /** True if an incoming message is ready for dispatching and the current
     * flow is waiting for a notify. */
    bool is_waiting_ = false;
};
BootloaderPort g_bootloader_port;

HubFlow stdout_hub(&g_service);


extern "C" {

void log_output(char* buf, int size) {
    if (size <= 0) return;
    auto* b = stdout_hub.alloc();
    HASSERT(b);
    b->data()->assign(buf, size);
    if (size > 1) b->data()->push_back('\n');
    stdout_hub.send(b);
}

void bootloader_hw_set_to_safe()
{
}
void bootloader_hw_init()
{
}

bool request_bootloader()
{
    return true;
}

void application_entry()
{
}

void bootloader_reboot()
{
}

bool read_can_frame(struct can_frame *frame)
{
    return g_bootloader_port.read_can_frame(frame);
}

bool try_send_can_frame(const struct can_frame &frame)
{
    auto *b = can_hub0.alloc();
    *b->data()->mutable_frame() = frame;
    b->data()->skipMember_ = &g_bootloader_port;
    can_hub0.send(b);
    return true;
}

#define FLASH_SIZE (256 * 1024)
static const uint8_t* virtual_flash = 0;
#define APP_HEADER_OFFSET 0x270

void get_flash_boundaries(const void **flash_min, const void **flash_max,
    const struct app_header **app_header)
{
    *flash_min = virtual_flash;
    *flash_max = virtual_flash + FLASH_SIZE;
    *app_header = reinterpret_cast<const struct app_header *>(
        &virtual_flash[APP_HEADER_OFFSET]);
}

void get_flash_page_info(
    const void *address, const void **page_start, uint32_t *page_length_bytes)
{
    // Simulates a flat 1KB page structure.
    uint32_t value = reinterpret_cast<uint32_t>(address);
    value -= reinterpret_cast<uint32_t>(&virtual_flash[0]);
    value &= ~1023;
    *page_start = &virtual_flash[value];
    *page_length_bytes = 1024;
}

void erase_flash_page(const void *address)
{
}

void write_flash(const void *address, const void *data, uint32_t size_bytes)
{
}

uint16_t nmranet_alias()
{
    return 0x44F;
}

extern uint64_t nmranet_nodeid()
{
    return 0x05010101144FULL;
}

extern void bootloader_entry();

void checksum_data(const void *data, uint32_t size, uint32_t *checksum)
{
    memset(checksum, 0x5A, 8);
}

} // extern "C"

/** Entry point to application.
 * @param argc number of command line arguments
 * @param argv array of command line arguments
 * @return 0, should never return
 */
int appl_main(int argc, char* argv[])
{
    FdHubPort<HubFlow> stdout_port(&stdout_hub, 0, EmptyNotifiable::DefaultInstance());

    int serial_fd = ::open("/dev/serUSB0", O_RDWR); // or /dev/ser0
    HASSERT(serial_fd >= 0);
    create_gc_port_for_can_hub(&can_hub0, serial_fd);

    can_hub0.register_port(&g_bootloader_port);

    while(1) {
        bootloader_entry();
    }

    return 0;
}
