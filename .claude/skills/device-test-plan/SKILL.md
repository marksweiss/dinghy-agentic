---
name: device-test-plan
description: Write and run the device test plan of an FM-1 app - numbered Do/Expect rows the user works through on their FM-1, reported by number, each failure turned into a fix and a re-test row. Use at the end of every feature and when the user reports test results.
---
# The device test plan (apps/<poc>/DEVICE-TEST-PLAN.md)

A section per round of testing: a title, the setup (which build, how to install), then rows.

```markdown
## 3. Arpeggiator

Install: `APP=<poc> ./build.sh`, `tools/install_fw.sh build/<poc>.fwsc`.

| # | Do | Expect |
|---|----|--------|
| 3.1 | Hold ARP half a second | ARP lights; the ARP page shows HOLD ARP: ON |
| 3.2 | Hold a chord | the notes play one after another, up, at 1/16 |
```

- One acceptance criterion a row; the order a person tries things; ears and eyes only (what they hear and see).
- Include the boring ones: power cycle keeps the settings; the previous features still work (a short regression row).
- When results come: the user writes "3.2 fails: ..." -> reproduce in host mode if possible, fix, add "3.2b (fix)" with what
  to try again, and ask for exactly those rows. Mark passed sections "Passed <date>".
- Tell the user what is new since their last test, and what you could not check on the host.
