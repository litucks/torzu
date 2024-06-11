// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "audio_core/sink/sink_details.h"
#ifdef HAVE_OBOE
#include "audio_core/sink/oboe_sink.h"
#endif
#ifdef HAVE_CUBEB
#include "audio_core/sink/cubeb_sink.h"
#endif
#ifdef HAVE_SDL2
#include "audio_core/sink/sdl2_sink.h"
#endif
#include "audio_core/sink/null_sink.h"
#include "common/logging/log.h"
#include "common/settings_enums.h"

namespace AudioCore::Sink {
namespace {
struct SinkDetails {
    using FactoryFn = std::unique_ptr<Sink> (*)(std::string_view);
    using ListDevicesFn = std::vector<std::string> (*)(bool);
    using LatencyFn = u32 (*)(); // REINTRODUCED FROM 3833 - DIABLO 3 FIX
    // using SuitableFn = bool (*)(); // REVERTED FOR ABOVE - DIABLO 3 FIX

    /// Name for this sink.
    Settings::AudioEngine id;
    /// A method to call to construct an instance of this type of sink.
    FactoryFn factory;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
    /// Method to get the latency of this backend - REINTRODUCED FROM 3833 - DIABLO 3 FIX
    LatencyFn latency;
    /// Check whether this backend is suitable to be used.
    /// SuitableFn is_suitable; // REVERTED FOR LatencyFn latency ABOVE - DIABLO 3 FIX
};

// NOTE TO PROBABLY FIX LATER FOR ANDROID - the return value "0u" for the first HAVE_OBOE 
// section below was just copied from the null section so there's a somewhat valid value
// being returned, since the previous "true" value probably isn't compatible with the 
// previous EA-3833 code. (HAVE_OBOE was introduced in a later release.) Eventually need 
// to change "0u" for something else directly from the oboe_sink.cpp functions.

// sink_details is ordered in terms of desirability, with the best choice at the top.
constexpr SinkDetails sink_details[] = {
#ifdef HAVE_OBOE
    SinkDetails{
        Settings::AudioEngine::Oboe,
        [](std::string_view device_id) -> std::unique_ptr<Sink> {
            return std::make_unique<OboeSink>();
        },
        [](bool capture) { return std::vector<std::string>{"Default"}; },
        []() { return 0u; },
    },
#endif
#ifdef HAVE_CUBEB
    SinkDetails{
        Settings::AudioEngine::Cubeb,
        [](std::string_view device_id) -> std::unique_ptr<Sink> {
            return std::make_unique<CubebSink>(device_id);
        },
        &ListCubebSinkDevices,
        &GetCubebLatency,
    },
#endif
#ifdef HAVE_SDL2
    SinkDetails{
        Settings::AudioEngine::Sdl2,
        [](std::string_view device_id) -> std::unique_ptr<Sink> {
            return std::make_unique<SDLSink>(device_id);
        },
        &ListSDLSinkDevices,
        &GetSDLLatency,
    },
#endif
    SinkDetails{
        Settings::AudioEngine::Null,
        [](std::string_view device_id) -> std::unique_ptr<Sink> {
            return std::make_unique<NullSink>(device_id);
        },
        [](bool capture) { return std::vector<std::string>{"null"}; },
        []() { return 0u; },
    },
};

const SinkDetails& GetOutputSinkDetails(Settings::AudioEngine sink_id) {
    const auto find_backend{[](Settings::AudioEngine id) {
        return std::find_if(std::begin(sink_details), std::end(sink_details),
                            [&id](const auto& sink_detail) { return sink_detail.id == id; });
    }};

    auto iter = find_backend(sink_id);

    if (sink_id == Settings::AudioEngine::Auto) {
        // REVERTED TO 3833 BELOW - DIABLO 3 FIX
        /*
        // Auto-select a backend. Use the sink details ordering, preferring cubeb first, checking
        // that the backend is available and suitable to use.
        for (auto& details : sink_details) {
            if (details.is_suitable()) {
                iter = &details;
                break;
            }
        }
        */ // END REVERTED CODE - DIABLO 3 FIX

        // BEGIN REINTRODUCED FROM 3833 - REPLACED CODE BLOCK ABOVE - DIABLO 3 FIX
        // Auto-select a backend. Prefer CubeB, but it may report a large minimum latency which
        // causes audio issues, in that case go with SDL.
#if defined(HAVE_CUBEB) && defined(HAVE_SDL2)
        iter = find_backend(Settings::AudioEngine::Cubeb);
        if (iter->latency() > TargetSampleCount * 3) {
            iter = find_backend(Settings::AudioEngine::Sdl2);
        }
#else
        iter = std::begin(sink_details);
#endif
        // END REINTRODUCED SECTION FROM 3833 - DIABLO 3 FIX
        LOG_INFO(Service_Audio, "Auto-selecting the {} backend",
                 Settings::CanonicalizeEnum(iter->id));
    /* BEGIN REMOVED - REVERTING BACK TO 3833, this didn't exist at all. - DIABLO 3 FIX
    } else {
        if (iter != std::end(sink_details) && !iter->is_suitable()) {
            LOG_ERROR(Service_Audio, "Selected backend {} is not suitable, falling back to null",
                      Settings::CanonicalizeEnum(iter->id));
            iter = find_backend(Settings::AudioEngine::Null);
        } */ // END REMOVED REVERT - DIABLO 3 FIX
    }

    if (iter == std::end(sink_details)) {
        LOG_ERROR(Audio, "Invalid sink_id {}", Settings::CanonicalizeEnum(sink_id));
        iter = find_backend(Settings::AudioEngine::Null);
    }

    return *iter;
}
} // Anonymous namespace

std::vector<Settings::AudioEngine> GetSinkIDs() {
    std::vector<Settings::AudioEngine> sink_ids(std::size(sink_details));

    std::transform(std::begin(sink_details), std::end(sink_details), std::begin(sink_ids),
                   [](const auto& sink) { return sink.id; });

    return sink_ids;
}

std::vector<std::string> GetDeviceListForSink(Settings::AudioEngine sink_id, bool capture) {
    return GetOutputSinkDetails(sink_id).list_devices(capture);
}

std::unique_ptr<Sink> CreateSinkFromID(Settings::AudioEngine sink_id, std::string_view device_id) {
    return GetOutputSinkDetails(sink_id).factory(device_id);
}

} // namespace AudioCore::Sink
