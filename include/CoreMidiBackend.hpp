#pragma once
// macOS CoreMIDI adapter for midi_route::Router. Only compiled on Apple platforms.
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <cstdio>
#include <string>
#include <vector>
#include "MidiRouter.hpp"

class CoreMidiBackend : public midi_route::Backend {
public:
    CoreMidiBackend() {
        error_ = MIDIClientCreate(CFSTR("Phase Box"), nullptr, nullptr, &client_);
        if (error_ == noErr) error_ = MIDIOutputPortCreate(client_, CFSTR("Phase Box Out"), &out_port_);
    }
    ~CoreMidiBackend() override {
        if (virtual_source_) MIDIEndpointDispose(virtual_source_);
        if (out_port_) MIDIPortDispose(out_port_);
        if (client_) MIDIClientDispose(client_);
    }
    CoreMidiBackend(const CoreMidiBackend&) = delete;
    CoreMidiBackend& operator=(const CoreMidiBackend&) = delete;

    bool ok() const { return error_ == noErr; }
    int error() const { return static_cast<int>(error_); }

    std::vector<midi_route::Endpoint> destinations() override {
        std::vector<midi_route::Endpoint> out;
        const ItemCount n = MIDIGetNumberOfDestinations();
        for (ItemCount i = 0; i < n; ++i) {
            const MIDIEndpointRef ep = MIDIGetDestination(i);
            if (ep == 0) continue;
            out.push_back(describe(ep));
        }
        return out;
    }

    bool send(uint64_t handle, const uint8_t* bytes, size_t n) override {
        if (out_port_ == 0) return false;
        alignas(8) Byte buffer[1024];
        MIDIPacketList* list = reinterpret_cast<MIDIPacketList*>(buffer);
        MIDIPacket* packet = MIDIPacketListInit(list);
        packet = MIDIPacketListAdd(list, sizeof(buffer), packet, 0, n, bytes);   // time 0 = now
        if (packet == nullptr) return false;
        return MIDISend(out_port_, static_cast<MIDIEndpointRef>(handle), list) == noErr;
    }

    bool create_virtual_source(const std::string& name) override {
        if (client_ == 0) return false;
        CFStringRef cf = CFStringCreateWithCString(kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8);
        const OSStatus st = MIDISourceCreate(client_, cf, &virtual_source_);
        CFRelease(cf);
        return st == noErr;
    }

    bool send_virtual(const uint8_t* bytes, size_t n) override {
        if (virtual_source_ == 0) return false;
        alignas(8) Byte buffer[1024];
        MIDIPacketList* list = reinterpret_cast<MIDIPacketList*>(buffer);
        MIDIPacket* packet = MIDIPacketListInit(list);
        packet = MIDIPacketListAdd(list, sizeof(buffer), packet, 0, n, bytes);
        if (packet == nullptr) return false;
        return MIDIReceived(virtual_source_, list) == noErr;
    }

    /// For `--list-midi`: what CoreMIDI itself reports, independent of the Audio MIDI Setup window.
    void print_all(FILE* out) {
        std::fprintf(out, "CoreMIDI client: %s (status %d)\n", ok() ? "ok" : "FAILED", error());
        const ItemCount nd = MIDIGetNumberOfDestinations();
        std::fprintf(out, "Destinations (%lu):\n", static_cast<unsigned long>(nd));
        for (ItemCount i = 0; i < nd; ++i) print_endpoint(out, MIDIGetDestination(i));
        const ItemCount ns = MIDIGetNumberOfSources();
        std::fprintf(out, "Sources (%lu):\n", static_cast<unsigned long>(ns));
        for (ItemCount i = 0; i < ns; ++i) print_endpoint(out, MIDIGetSource(i));
    }

private:
    static std::string string_property(MIDIObjectRef obj, CFStringRef property) {
        std::string result;
        CFStringRef value = nullptr;
        if (obj != 0 && MIDIObjectGetStringProperty(obj, property, &value) == noErr && value != nullptr) {
            char buf[256];
            if (CFStringGetCString(value, buf, sizeof(buf), kCFStringEncodingUTF8)) result = buf;
            CFRelease(value);
        }
        return result;
    }

    static midi_route::Endpoint describe(MIDIEndpointRef ep) {
        midi_route::Endpoint e;
        e.handle = static_cast<uint64_t>(ep);
        auto add = [&](const std::string& s) { if (!s.empty()) e.names.push_back(s); };
        add(string_property(ep, kMIDIPropertyDisplayName));   // "Melodee FM-1" style: device + port
        add(string_property(ep, kMIDIPropertyName));
        MIDIEntityRef entity = 0;
        if (MIDIEndpointGetEntity(ep, &entity) == noErr && entity != 0) {
            add(string_property(entity, kMIDIPropertyName));
            MIDIDeviceRef device = 0;
            if (MIDIEntityGetDevice(entity, &device) == noErr && device != 0) {
                add(string_property(device, kMIDIPropertyName));
                add(string_property(device, kMIDIPropertyModel));
            }
        }
        SInt32 offline = 0;
        if (MIDIObjectGetIntegerProperty(ep, kMIDIPropertyOffline, &offline) == noErr) e.offline = offline != 0;
        return e;
    }

    static void print_endpoint(FILE* out, MIDIEndpointRef ep) {
        const midi_route::Endpoint e = describe(ep);
        std::fprintf(out, "  [%llu]%s", static_cast<unsigned long long>(e.handle), e.offline ? " (offline)" : "");
        for (size_t i = 0; i < e.names.size(); ++i) std::fprintf(out, "%s \"%s\"", i ? "," : "", e.names[i].c_str());
        std::fprintf(out, "\n");
    }

    MIDIClientRef client_ = 0;
    MIDIPortRef out_port_ = 0;
    MIDIEndpointRef virtual_source_ = 0;
    OSStatus error_ = noErr;
};
#endif // __APPLE__
