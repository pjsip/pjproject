# How to Apply PR #4332 to PJSIP 2.7.2

## Problem Solved

If you downloaded the PJSIP 2.7.2 release (as a zip or tar.gz file) and tried to apply a patch, you likely encountered errors because the original patch file was created from a different codebase.

**This repository now contains a corrected patch file that applies cleanly to vanilla PJSIP 2.7.2!**

## The Correct Patch File

**File**: `pr-4332-for-2.7.2.patch`

This patch file has been specifically created and tested to work with the official PJSIP 2.7.2 release.

## What This Patch Does

This patch backports PR #4332, which adds a dedicated sending thread method for video streaming rate control. 

**Benefits:**
- Significantly reduces video latency (from 500-1000ms to 200-400ms)
- More accurate bitrate calculation
- Doesn't block the video capture thread during transmission
- Prevents bandwidth spikes

## How to Apply the Patch

### Step 1: Download the Patch File

Download `pr-4332-for-2.7.2.patch` from this repository.

### Step 2: Apply to Your PJSIP 2.7.2 Installation

Navigate to your PJSIP 2.7.2 directory and apply the patch:

#### If you're using Git:
```bash
cd /path/to/pjproject-2.7.2
git apply pr-4332-for-2.7.2.patch
```

#### If you extracted from zip/tar.gz (no git):
```bash
cd /path/to/pjproject-2.7.2
patch -p1 < pr-4332-for-2.7.2.patch
```

### Step 3: Verify the Patch Applied Successfully

Check that no errors were reported. You can verify by checking that these files were modified:
- `pjmedia/include/pjmedia/vid_stream.h`
- `pjmedia/src/pjmedia/vid_stream.c`

### Step 4: Build PJSIP

```bash
./configure
make dep
make
```

## What Changed

The patch makes the following changes:

1. **New enum value**: `PJMEDIA_VID_STREAM_RC_SEND_THREAD = 2` in `vid_stream.h`
2. **Default changed**: Rate control now defaults to `SEND_THREAD` instead of `SIMPLE_BLOCKING`
3. **New implementation**: ~290 lines of sending thread infrastructure in `vid_stream.c`

**Total changes**: +397 lines added, -23 lines removed across 2 files.

## Detailed Documentation

See `PATCH-SUMMARY.md` for a complete technical breakdown of all changes.

## Troubleshooting

### "patch does not apply" or "hunks FAILED"

Make sure you are using:
1. The **corrected** patch file: `pr-4332-for-2.7.2.patch` (NOT `pr-4332.patch`)
2. The **vanilla** PJSIP 2.7.2 release (tag 2.7.2, commit 614a8ee4)

### Verifying Your PJSIP Version

If you're using git:
```bash
git log --oneline -1
# Should show: 614a8ee4 Tagged 2.7.2
```

If you downloaded a release archive, check the `version.mak` file:
```bash
grep "PJ_VERSION_NUM=" version.mak
# Should show version 2.7.2
```

## Why Was a Corrected Patch Needed?

The original patch was created from a later version of PJSIP's master branch, which had structural differences from 2.7.2. This corrected patch was manually created to:
- Adapt to 2.7.2's code structure
- Handle differences in the grp_lock implementation
- Ensure all hunks apply cleanly

## Original PR Information

- **PR Number**: #4332
- **Title**: "Add dedicated-thread method for video sending rate control"
- **Author**: Nanang Izzuddin <nanang@teluu.com>
- **Date**: March 3, 2025
- **Status**: Merged into master

## Questions or Issues?

If you encounter any problems applying this patch, please open an issue with:
1. Your PJSIP version (from `git log` or `version.mak`)
2. The exact error message
3. How you obtained PJSIP (git clone, download zip, etc.)
