---
name: fm1-crash-triage
description: Diagnose a crash or boot failure of an FM-1 app from the red crash screen (vector, PC, return address), a hang, or the FM-1 going into the USB rescue. Use when the user reports one.
---
# Crash triage

1. Get the three hex numbers on the red screen: vector, PC, return address (the app's name is on it too).
2. The same build's disassembly: `build/<app>.dis` (rebuild that exact commit if needed). `grep -n "<pc>:" build/<app>.dis`,
   read around it, find the function from the nearest label; the return address gives the caller.
3. Usual causes: stack overflow (big locals: make them static); an index out of range (a voice, a table, a pattern step from
   flash that was not clamped); the watchdog (8 s without platform_poll: a long loop, a splash or drawing that does not poll);
   flash or the screen touched from the ISR; division by zero giving a wild index; a misaligned 32-bit access through a
   uint8_t pointer.
4. Boot loop: two crashes at boot and the FM-1 starts the USB rescue by itself; OCT- at power-on forces it; install a fixed
   build from there (tools/install_fw.sh or the web tool).
5. Fix, host-test the path, rules-reviewer, then ask for the device test again.
