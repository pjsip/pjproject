/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <pjsua2.hpp>
#include <pj/lock.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>

/* Fail also in release builds, including stalled worker threads. */
#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "AudioMediaPort: " << #expr << " at line " \
                      << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

namespace pj {

class AudioMediaPortTest : public AudioMediaPort
{
public:
    AudioMediaPortTest()
    : releaseFuture(release.get_future()), callbackFinished(false),
      requested(0), received(0)
    {}

    virtual void onFrameRequested(MediaFrame &frame)
    {
        ++requested;
        holdCallback();
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf.assign(frame.size, 0x5a);
        callbackFinished = true;
    }

    virtual void onFrameReceived(MediaFrame &frame)
    {
        ++received;
        CHECK(frame.type == PJMEDIA_FRAME_TYPE_AUDIO);
        CHECK(frame.buf.size() == 16);
        holdCallback();
        callbackFinished = true;
    }

    static void run(Endpoint &ep, bool receive)
    {
        AudioMediaPortTest media;
        media.detachCallbacks();   /* No port yet. */

        MediaFormatAudio fmt;
        fmt.init(PJMEDIA_FORMAT_L16, 8000, 1, 20000, 16);
        media.createPort("callback-fence", fmt);
        pjmedia_port *port = media.port;

        unsigned char buffer[16];
        pjmedia_frame frame;
        pj_bzero(&frame, sizeof(frame));
        pj_memset(buffer, 0xa5, sizeof(buffer));
        frame.buf = buffer;
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.size = sizeof(buffer);

        std::future<void> entered = media.entered.get_future();
        std::thread callback([&]() {
            ep.libRegisterThread("fence-callback");
            pj_status_t status = receive ?
                pjmedia_port_put_frame(port, &frame) :
                pjmedia_port_get_frame(port, &frame);
            CHECK(status == PJ_SUCCESS);
        });
        CHECK(entered.wait_for(std::chrono::seconds(5)) ==
              std::future_status::ready);

        /* Verify that the held callback really owns the fence's lock. */
        CHECK(pj_grp_lock_tryacquire(port->grp_lock) != PJ_SUCCESS);

        std::promise<void> starting, completed;
        std::future<void> started = starting.get_future();
        std::future<void> done = completed.get_future();
        std::thread detacher([&]() {
            ep.libRegisterThread("fence-detach");
            starting.set_value();
            media.detachCallbacks();
            CHECK(media.callbackFinished.load());
            completed.set_value();
        });
        CHECK(started.wait_for(std::chrono::seconds(5)) ==
              std::future_status::ready);
        CHECK(done.wait_for(std::chrono::milliseconds(100)) ==
              std::future_status::timeout);

        media.release.set_value();
        CHECK(done.wait_for(std::chrono::seconds(5)) ==
              std::future_status::ready);
        detacher.join();
        callback.join();

        if (!receive) {
            CHECK(frame.type == PJMEDIA_FRAME_TYPE_AUDIO);
            CHECK(frame.size == sizeof(buffer));
            CHECK(buffer[0] == 0x5a);
        }

        media.detachCallbacks();
        /* Fencing must not unregister the port or release its resources. */
        CHECK(media.getPortInfo().portId == media.getPortId());
        for (unsigned i = 0; i < 3; ++i) {
            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame.size = sizeof(buffer);
            CHECK(pjmedia_port_get_frame(port, &frame) == PJ_SUCCESS);

            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame.size = sizeof(buffer);
            CHECK(pjmedia_port_put_frame(port, &frame) == PJ_SUCCESS);
        }
        CHECK(media.requested == (receive ? 0u : 1u));
        CHECK(media.received == (receive ? 1u : 0u));
    }

private:
    void holdCallback()
    {
        CHECK(requested + received == 1);
        entered.set_value();
        CHECK(releaseFuture.wait_for(std::chrono::seconds(5)) ==
              std::future_status::ready);
    }

    std::promise<void> entered, release;
    std::future<void> releaseFuture;
    std::atomic<bool> callbackFinished;
    unsigned requested, received;
};

} // namespace pj

void audioMediaPortTest()
{
    pj::Endpoint ep;
    ep.libCreate();
    pj::EpConfig cfg;
    cfg.uaConfig.threadCnt = 0;
    cfg.medConfig.threadCnt = 0;
    cfg.logConfig.level = 2;
    ep.libInit(cfg);
    ep.audDevManager().setNoDev();
    pj::AudioMediaPortTest::run(ep, false);
    pj::AudioMediaPortTest::run(ep, true);
    ep.libDestroy();
    std::cout << "AudioMediaPort callback fence tests passed" << std::endl;
}
