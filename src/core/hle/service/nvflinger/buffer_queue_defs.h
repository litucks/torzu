// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2021 yuzu Emulator Project
// Copyright 2014 The Android Open Source Project
// Parts of this implementation were base on:
// https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueDefs.h

#pragma once

#include <array>

#include "common/common_types.h"
#include "core/hle/service/nvflinger/buffer_slot.h"

namespace android::BufferQueueDefs {

// BufferQueue will keep track of at most this value of buffers.
constexpr s32 NUM_BUFFER_SLOTS = 64;

using SlotsType = std::array<BufferSlot, NUM_BUFFER_SLOTS>;

} // namespace android::BufferQueueDefs
