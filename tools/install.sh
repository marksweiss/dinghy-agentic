#!/bin/sh
# Set up this checkout once: a uv venv (Pillow, mido, python-rtmidi, numpy), JieLi's toolchain, three files of the JieLi
# AC79 SDK. Idempotent. Then a build and the host tests. Needs uv (https://docs.astral.sh/uv/), git, and on macOS Docker.
set -e
cd "$(dirname "$0")/.."
[ -d .venv ] || uv venv
uv pip install --python .venv/bin/python Pillow mido python-rtmidi numpy
[ -x "$HOME/.jieli/toolchain/pi32v2/bin/clang" ] || tools/get_toolchain.sh
[ -f "$HOME/fw-AC79_AIoT_SDK/cpu/wl82/tools/uboot.boot" ] || \
    git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$HOME/fw-AC79_AIoT_SDK"
./build.sh
sh tests/run_host.sh
sh tests/run_lib.sh
