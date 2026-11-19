#pragma once
// ============================================================
// MoonTileMaker.h — the Moon map's tiles, made off the UI thread (162e)
//
// A captured section's tile is read from its recording, which can be
// gigabytes: never on the thread that draws. Jobs queue here and one
// worker makes them in turn (core/MoonTile.h picks the frame, WicImage
// writes the PNG). Each finished job is reported through `done`, ON THE
// WORKER'S THREAD: the window is told by a posted message, not a call.
//
// The destructor finishes the job in hand and drops the rest; a tile not
// made is made again the next time its section is captured.
// ============================================================

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "WicImage.h"
#include "../core/MoonTile.h"
#include "../core/TextConv.h"

namespace raduk::gui {

class MoonTileMaker {
public:
    static constexpr int kTileSize = 256;

    struct Job { std::string section, recording, tile; };
    // ok, the job, and the reason when not ok. Called on the worker thread.
    using Done = std::function<void(bool ok, const Job& job, const std::string& why)>;

    explicit MoonTileMaker(Done done) : m_done(std::move(done)), m_thread([this] { Run(); }) {}
    ~MoonTileMaker() {
        {
            std::lock_guard<std::mutex> l(m_mx);
            m_stop = true;
            m_jobs.clear();
        }
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
    }
    MoonTileMaker(const MoonTileMaker&) = delete;
    MoonTileMaker& operator=(const MoonTileMaker&) = delete;

    void Add(Job j) {
        {
            std::lock_guard<std::mutex> l(m_mx);
            // A newer recording of the same section replaces a waiting one.
            for (auto& q : m_jobs) if (q.tile == j.tile) { q = j; m_cv.notify_one(); return; }
            m_jobs.push_back(std::move(j));
        }
        m_cv.notify_one();
    }

    // One job, now, on the calling thread. For tests.
    static bool Make(const Job& j, std::string& why) {
        raduk::core::MoonTile t;
        if (!raduk::core::MakeMoonTile(j.recording, kTileSize, t, why)) return false;
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(j.tile).parent_path(), ec);
        return SaveGreyPng(raduk::core::ToWide(j.tile), t.size, t.size, t.grey.data(), why);
    }

private:
    void Run() {
        for (;;) {
            Job j;
            {
                std::unique_lock<std::mutex> l(m_mx);
                m_cv.wait(l, [this] { return m_stop || !m_jobs.empty(); });
                if (m_stop) return;
                j = std::move(m_jobs.front());
                m_jobs.pop_front();
            }
            std::string why;
            const bool ok = Make(j, why);
            if (m_done) m_done(ok, j, why);
        }
    }

    Done m_done;
    std::mutex m_mx;
    std::condition_variable m_cv;
    std::deque<Job> m_jobs;
    bool m_stop = false;
    std::thread m_thread;                 // last: started after the rest exist
};

} // namespace raduk::gui
