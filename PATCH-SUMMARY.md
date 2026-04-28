# PR #4332 Backport to PJSIP 2.7.2

## Summary
This patch backports PR #4332 to PJSIP 2.7.2, adding a dedicated sending thread method for video streaming rate control.

## Patch File
- **File**: `pr-4332-for-2.7.2.patch`
- **Size**: 570 lines
- **Files Modified**: 2 files (vid_stream.h and vid_stream.c)
- **Lines Changed**: +397, -23

## Key Changes

### 1. New Enum Value (vid_stream.h)
- Added `PJMEDIA_VID_STREAM_RC_SEND_THREAD = 2` to `pjmedia_vid_stream_rc_method` enum
- Updated default from `SIMPLE_BLOCKING` to `SEND_THREAD`

### 2. Sending Thread Implementation (vid_stream.c)
- Added `send_entry`, `send_stream`, and `send_manager` structures
- Implemented `send_worker_thread()` for asynchronous RTP packet sending
- Added `attach_send_manager()` and `detach_send_manager()` functions
- Added `get_send_entry()` for buffer allocation
- Added `send_rtp()` for queuing packets

### 3. Integration with Video Stream
- Modified `put_frame()` to use send thread when configured
- Updated `pjmedia_vid_stream_create()` to initialize send manager
- Updated `pjmedia_vid_stream_destroy()` to cleanup send manager
- Changed default in `pjmedia_vid_stream_rc_config_default()`

## Benefits
- Better video latency compared to simple blocking method
- More accurate bitrate calculation
- Avoids blocking the video capture thread during transmission delays
- Prevents peak bandwidth spikes

## How to Apply

### Method 1: Using git apply
```bash
cd /path/to/pjproject-2.7.2
git apply pr-4332-for-2.7.2.patch
```

### Method 2: Using patch command
```bash
cd /path/to/pjproject-2.7.2
patch -p1 < pr-4332-for-2.7.2.patch
```

## Verification
The patch has been tested and applies cleanly to vanilla PJSIP 2.7.2 (tag: 2.7.2, commit: 614a8ee4).

```bash
git apply --check pr-4332-for-2.7.2.patch
# Should return with no errors
```

## Notes
- This backport adapts the original PR to work with PJSIP 2.7.2's architecture
- Key adaptation: Created separate grp_lock for send_stream (2.7.2 doesn't have grp_lock in channel structure)
- The SEND_THREAD method is now the default rate control method

## Original PR
- PR #4332: "Add dedicated thread method for video sending rate control"
- Author: Nanang Izzuddin <nanang@teluu.com>
- Date: Mon Mar 3 16:10:42 2025 +0700
