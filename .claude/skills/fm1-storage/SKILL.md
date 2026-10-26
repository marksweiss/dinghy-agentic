---
name: fm1-storage
description: Saving on the FM-1 - settings, blobs and slots (store.h), the power-cut-safe patch log (patchlog.h), companions, when to write, wear, what survives firmware changes. Use for any feature that saves or loads.
---
# Storage

| What | API | Size | Use for |
| --- | --- | --- | --- |
| settings | settings_load / settings_save | one sector (~4 KB) | the octave, a MIDI channel, preferences |
| blobs | blob_load / blob_save (8) | one sector each | a small table, a user scale |
| slots | slot_load / slot_save (8) | 4 sectors each | a wavetable, a song |
| patch log | patchlog.h in one slot | live records <= 4 KB | patches: 32 ids (+32 companions), <= 256 B a record |

- **When:** an erase stops the audio (~17 ms a sector, interrupts off): write only when quiet and not on every knob step;
  settings 2 s after the last change, when silent (dinghy.c's pattern); a patch on SAVE.
- **Power cuts:** settings / blobs / slots write the header last (a cut leaves the old content invalid -> defaults); the patch
  log appends, so a cut leaves the old record or the new one, never half. Never mix slot_save and the log in one slot.
- **Records:** <= 80 int16 values + 33 words; extra data in a companion id (PL_IDS + id), written after its patch (it counts
  only while newer). The store is full at ~4 KB of live records: STORE FULL, say so on the screen.
- **Trust nothing loaded:** clamp every value; a newer firmware appends parameters at the end (old records get defaults).
- **Firmware switches:** another firmware may erase all of it; give the user a backup (link.h + the web tool).
- Host: `DINGHY_FLASH=file` for the raw flash (the log); settings / blobs / slots are RAM-only on the host.
