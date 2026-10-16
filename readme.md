# Tehe-RPG-inko

A fan-made Heaven Burns Red demake for the Nintendo 3DS

> Non-commercial fan project. Not affiliated with Wright Flyer Studios, Key, Yostar, or Nintendo

> AI disclosure: The code for this project was written with the help of AI tools (Claude by Anthropic). Game design, direction, testing, and asset selection were done by the author.

## Installation

1. Copy Tehe-RPG-inko.3dsx to SD:/3ds/
2. Open the Homebrew Launcher and select Tehe-RPG-inko
3. Done. If you're using the cia version, install it with FBI.
4. Open Rosalina Menu (L + Dpad Down + Select), select Miscellaneous options, and select Dump DSP firmware

You can play and install this in 3DS Emulator, but you need to dump dsp firmware from real 3DS and put to SDMC folder

You can add music for dialogue scene and battle (Phase 1 and Phase 2 have different music)

1. Convert the music to ogg file (use ffmpeg for example)
2. Copy to Your 3DS SD Card:/3ds/Tehe-RPG-inko/assets/
3. Name it to "bgm_dialogue.ogg" for dialogue scene, "bgm_battle.ogg" for battle phase 1, and "bgm_awaken.ogg" for battle phase 2

You also can change icon character image with your own image

1. Make a 128x128 transparent PNG for each character you want to change. Source images up to 512x512 and 2 MiB are accepted and resized to 128x128. Missing, invalid, or larger images use the built-in icon.
2. Copy to Your 3DS SD Card:/3ds/Tehe-RPG-inko/assets/
3. Name it to **first name**_icon.png (example ruka_icon.png, yuki_icon.png)

## Custom-Party

Press SELECT in dialogue scene to enter the Custom-Party screen

or

You can edit `SD:/3ds/Tehe-RPG-inko/party.cfg` with a text editor after closing the game.

You can edit character names, role labels, and each skill's name, SP cost, hit count, power, bonus, and supported effects. Role labels do not grant combat bonuses: HP/DP efficiency and devastation bonuses belong to individual skills. Base HP/DP/ATK, skill kinds, EX flags, stun chances, SP recovery chances, and use limits stay fixed to the built-in roster.

The editor uses touch controls; B returns to the previous page. Done saves and returns to dialogue. START saves and quits. If saving fails, the editor stays open with your edits and an error message; check the SD card and retry Done or START.

Saving writes `party.cfg.tmp` before replacing the main file and keeps the previous version in `party.cfg.bak`. If the main file cannot be opened at startup, the game tries the backup. A temporary file is never loaded automatically. Keep a separate backup of edits you want to preserve.

## Skitter encounter

After the dialogue, the party fights Skitter on an asphalt street before reaching Hellspider. Skitter has one phase, 45 DP and 180 HP. Each enemy turn deals one 12-power hit to a random frontline member; it has no party-wide attack or Awakening.

Break uses a damaged pose, Down/Stun lowers the body and spreads the legs, and defeat collapses the model without the Hellspider tree cutscene. Choose **Continue to Hellspider** after victory. HP, DP, SP, formation and remaining skill uses carry forward. Temporary party effects and active Overdrive end at the encounter boundary; the remaining Overdrive gauge carries forward, including partial bars. Play Again starts a fresh run from the dialogue.

The close camera and left-facing Skitter use the new city arena. The top screen retains its native 400×240 aspect ratio.

## Controls

Touchscreen is working for any button

**Dialogue Scene**

A = Advance

B = Hide text box

X = Skip

Y = Open Log

SELECT = Custom Party

**Battle Scene**

A = Select

B = Back

X = Execute

Y = Swap

L/R = Switch front-row character

Press and hold (button / touch) on skill to read the description.

You can drag and drop character icon in Swap Screen.

Overdrive must be click by touchscreen to prevent missclick

## Building

Use a devkitPro environment with devkitARM, libctru, citro2d, citro3d, 3ds-tools, tex3ds, and the 3DS Tremor/Ogg portlibs. `DEVKITARM` must point to devkitARM. The default build uses C++11 and produces `Tehe-RPG-inko.3dsx`, `.elf`, and `.smdh`:

```sh
make -j2
```

On Windows with Docker Desktop (PowerShell, from the repository directory):

```powershell
docker run --rm -v "${PWD}:/project" -w /project devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5 make -j2
```

`make cia` also requires `makerom` and `bannertool` in PATH, or explicit paths via `MAKEROM=/path/to/makerom BANNERTOOL=/path/to/bannertool`. These tools come from [Project_CTR](https://github.com/3DSGuy/Project_CTR) and [3ds-bannertool](https://github.com/carstene1ns/3ds-bannertool); they are separate from devkitPro. Banner inputs and the packaging configuration live in `meta/`.

## Tests

Host tests run on Linux, including Docker or WSL, without devkitARM. They require GNU make, a C++11 compiler, and host Tremor/Ogg development libraries. On Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install g++ make libvorbisidec-dev libogg-dev
make test
```

The suite covers combat/dialogue, party configuration and failed saves, meshes/cameras, framebuffer cache behavior with GPU stubs, PNG limits/conversion, bounded page navigation, and decoding the real bundled SFX. The harness creates a `romfs:` symlink inside `build-tests/runtime/` for the SFX test. `make test` must be invoked separately from 3DS build targets.

GitHub Actions runs the host suite and builds `.3dsx` on pushes and pull requests. Build artifacts are attached to the workflow run. CI does not emulate the GPU, audio threads, or touchscreen: check save-error recovery, log limits, skill details, music transitions, and Old/New 3DS cutscenes on hardware or an emulator before release.

For an extra host check, use `make test HOST_TEST_DIR=build-tests-ubsan HOST_CXXFLAGS='-std=c++11 -O1 -g -Wall -Wextra -fsanitize=undefined -fno-sanitize-recover=all'`.

## Assets

See [ASSETS.md](ASSETS.md) for the conversion pipeline, known provenance, and missing original generation sources. Committed mesh headers are build inputs; no missing generator is required to compile the game.
