#pragma once
// Decides where the simulator's MIDI goes, independent of any operating-system API so it can be tested
// anywhere. By default it sends straight to a hardware destination chosen by name (no virtual port and no
// patchbay needed), finds it again after it is unplugged and re-plugged, and never sends to anything else.
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace midi_route {

constexpr const char* DEFAULT_DESTINATION = "Melodee FM-1";
constexpr const char* VIRTUAL_SOURCE_NAME = "Phase Box Virtual Out";
constexpr uint32_t SCAN_INTERVAL_MS = 1000;    // how often to look for the device while it is missing
constexpr uint32_t VERIFY_INTERVAL_MS = 2000;  // how often to confirm a connected device is still there

/// "Melodee FM-1", "melodee fm 1" and "MelodeeFM-1" all compare equal: only letters and digits count.
inline std::string loose(const std::string& s) {
    std::string out;
    for (unsigned char c : s) if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
    return out;
}

inline bool name_matches(const std::string& name, const std::string& wanted) {
    const std::string w = loose(wanted);
    return !w.empty() && loose(name).find(w) != std::string::npos;
}

struct Endpoint {
    uint64_t handle = 0;              // stable for as long as the endpoint exists (not a list position)
    std::vector<std::string> names;   // display name first, then any other names that identify it
    bool offline = false;
};

/// The operating system's MIDI layer, as seen by the router.
class Backend {
public:
    virtual ~Backend() = default;
    virtual std::vector<Endpoint> destinations() = 0;
    virtual bool send(uint64_t handle, const uint8_t* bytes, size_t n) = 0;   // false: failed (e.g. unplugged)
    virtual bool create_virtual_source(const std::string& name) = 0;
    virtual bool send_virtual(const uint8_t* bytes, size_t n) = 0;
};

struct Config {
    std::string wanted = DEFAULT_DESTINATION;
    bool use_virtual = false;

    /// PHASE_BOX_MIDI_DEST: unset or empty = the default hardware, "virtual" = the old virtual source,
    /// anything else = a name (or part of one) to look for.
    static Config from_env(const char* value) {
        Config c;
        if (value == nullptr) return c;
        std::string v = value;
        const size_t a = v.find_first_not_of(" \t");
        if (a == std::string::npos) return c;
        v = v.substr(a, v.find_last_not_of(" \t") - a + 1);
        if (loose(v) == "virtual") c.use_virtual = true;
        else c.wanted = v;
        return c;
    }
};

class Router {
public:
    Router(Backend& backend, Config config) : backend_(backend), cfg_(std::move(config)) {
        set_status(cfg_.use_virtual ? "starting virtual MIDI source" : "searching for '" + cfg_.wanted + "'");
    }

    /// Call every frame: finds the device, notices when it goes away, finds it again.
    void poll(uint32_t now_ms) {
        if (cfg_.use_virtual) {
            if (!connected_ && (!scanned_ || elapsed(now_ms, last_scan_ms_) >= SCAN_INTERVAL_MS)) {
                scanned_ = true; last_scan_ms_ = now_ms;
                if (backend_.create_virtual_source(VIRTUAL_SOURCE_NAME)) {
                    connected_ = true; ++epoch_;
                    set_status(std::string("virtual source '") + VIRTUAL_SOURCE_NAME + "' (connect it in your DAW or MIDI Studio)");
                } else {
                    set_status("could not create the virtual MIDI source");
                }
            }
            return;
        }
        if (connected_) {
            if (elapsed(now_ms, last_verify_ms_) >= VERIFY_INTERVAL_MS) {
                last_verify_ms_ = now_ms;
                if (!still_present()) lose("'" + name_ + "' went away, searching");
            }
            return;
        }
        if (!scanned_ || elapsed(now_ms, last_scan_ms_) >= SCAN_INTERVAL_MS) {
            scanned_ = true; last_scan_ms_ = now_ms;
            scan(now_ms);
        }
    }

    /// Send one Control Change (channel 1-16). Returns false if nothing was sent.
    bool send_cc(uint8_t channel, uint8_t controller, uint8_t value, uint32_t /*now_ms*/ = 0) {
        if (!connected_) return false;
        const uint8_t msg[3] = {static_cast<uint8_t>(0xB0 | ((channel - 1) & 0x0F)),
                                static_cast<uint8_t>(controller & 0x7F), static_cast<uint8_t>(value & 0x7F)};
        const bool ok = cfg_.use_virtual ? backend_.send_virtual(msg, 3) : backend_.send(handle_, msg, 3);
        if (!ok) lose(cfg_.use_virtual ? "virtual source failed" : "send to '" + name_ + "' failed (unplugged?), searching");
        return ok;
    }

    bool connected() const { return connected_; }
    /// Increases every time a (new) connection is made, so callers can resend their current values.
    uint32_t epoch() const { return epoch_; }
    const std::string& status() const { return status_; }
    /// Messages worth printing (connected, lost, not found...). Only changes are logged, never repeats.
    std::vector<std::string> take_log() { std::vector<std::string> out; out.swap(log_); return out; }

private:
    static uint32_t elapsed(uint32_t now, uint32_t then) { return static_cast<uint32_t>(now - then); }

    void set_status(const std::string& s) {
        if (s == status_) return;
        status_ = s;
        log_.push_back(s);
    }

    void scan(uint32_t now_ms) {
        const std::vector<Endpoint> list = backend_.destinations();
        if (list.empty()) {
            set_status("CoreMIDI reports no MIDI destinations at all (see scripts/reset_coremidi.sh)");
            return;
        }
        std::string seen;
        bool offline_match = false;
        size_t shown = 0;
        for (const Endpoint& e : list) {
            bool match = false;
            for (const std::string& n : e.names) match = match || name_matches(n, cfg_.wanted);
            if (match && !e.offline) {
                handle_ = e.handle;
                name_ = e.names.empty() ? cfg_.wanted : e.names.front();
                connected_ = true; ++epoch_;
                last_verify_ms_ = now_ms;
                set_status("-> " + name_);
                return;
            }
            offline_match = offline_match || match;
            if (shown < 4 && !e.names.empty()) { seen += (shown ? ", " : "") + e.names.front(); ++shown; }
        }
        set_status(std::string(offline_match ? "'" + cfg_.wanted + "' is listed but offline" : "no destination matches '" + cfg_.wanted + "'") +
                   " (seen: " + seen + (list.size() > shown ? ", ..." : "") + ")");
    }

    bool still_present() {
        for (const Endpoint& e : backend_.destinations()) if (e.handle == handle_) return !e.offline;
        return false;
    }

    void lose(const std::string& why) {
        connected_ = false;
        scanned_ = false;   // look again right away
        set_status(why);
    }

    Backend& backend_;
    Config cfg_;
    bool connected_ = false, scanned_ = false;
    uint32_t last_scan_ms_ = 0, last_verify_ms_ = 0, epoch_ = 0;
    uint64_t handle_ = 0;
    std::string name_, status_;
    std::vector<std::string> log_;
};

} // namespace midi_route
