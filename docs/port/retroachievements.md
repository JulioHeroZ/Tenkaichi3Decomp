# RetroAchievements (experimental)

This branch integrates online accounts through the official rcheevos `rc_client`.
F1 > Achievements offers sign-in/sign-out, game status, unlocked counts and
unsupported counts. Unlocks produce an on-screen notification. This is an initial
**softcore-only** implementation; hardcore and leaderboard tracking are disabled.

## Scope

The memory view exposes stable objects by identity, independently of where the
port allocates them:

| Original PS2 address | Native source | Exposed data |
|---|---|---|
| `0x6B9640..0x6BD63F` | `gSaveData`, `include/sys/save.h` | Pointer-free serialized save, 0x4000 bytes |
| `0x6AF180 + offsets` | `gProgress`, menu/view headers | Scalar fields at 0x18, 0x2C, 0x3C, 0x84, 0x90, 0x620, 0x624, 0x64C, 0x674 |
| `0x31E592`, `0x31E59A` | `gLoadScreen`, `include/sys/loading.h` | Loading minigame score and type (u16) |

All other reads return unsupported. No arbitrary host pointer or zero-filled RAM
image is exposed. Existing definitions are fetched unchanged by rc_client.

The public reference patch in [BT3-Recomp](https://github.com/z3xox/BT3-Recomp/blob/main/docs/retroachievements/bt3-raw-patch.json)
(blob `961b9d1ab48119943407c1dc7ee95f985d37b917`) contains 153 game achievements plus
a zero-point unknown-emulator warning. Of those game achievements, **104 use only
the stable regions above**. The other 49 remain unsupported: 42 depend on fighter
objects, seven on simulation dialogue state. These objects are allocated/freed
across mode changes; reading a fabricated zero or stale snapshot can falsely
satisfy a health or dialogue transition. rc_client eagerly reads shared memory
references before evaluating condition guards, so guards do not solve this.
The live set can change; the UI reports the runtime's actual unsupported count.

## Identification, networking and lifecycle

The game hash is computed from the user's extracted `disc/SLUS_216.78` with the
PS2 algorithm in rcheevos `src/rhash/hash_disc.c`: MD5 of the BOOT2 executable
basename followed by its contents, capped at 64 MiB. The port only accepts the USA
release. Neither the resulting hash nor a game ID is hardcoded.

A worker sends HTTPS requests using WinHTTP on Windows or the system curl library
on Unix, with TLS verification and a product/version User-Agent. No subprocess is
used for credentials. Completion callbacks, rc_client state and memory reads run
on the game thread, once per vertical blank; render/UI code queues commands and
copies status under a mutex. Passwords and tokens are not persisted or logged.
Server retry and session behavior use rc_client rather than custom award requests.

Processing waits for the port's original-data verification. It is suspended for
netplay, replay/scripted input, diagnostic runs, nonstandard speed, clash assistance
and Unlock Everything. Using Unlock Everything taints the process until restart.
No network request is made until the player explicitly signs in. Signing out
unloads the game and clears account status; responses for an aborted account are
drained before another login begins.

The pinned library revision and licenses are in
`port/third_party/rcheevos/UPSTREAM.md`. It is built with the host ABI and excluded
from rollback snapshots. Release packaging includes the library/MD5 notices.

## Verification

```sh
clang -std=c99 -Wall -Wextra -Werror port/src/ra_memory.c port/tests/ra_memory.c -o ra_memory_test
./ra_memory_test
python port/tests/run_ra_client.py
python port/tests/run_ra_game.py
```

The client test compiles the real runtime with a fake transport and no native
network implementation. It checks login, unsupported reads, a local fixture
unlock, retry after connection failure, softcore flags, cancellation, account
isolation and shutdown. It never contacts RetroAchievements or awards a real
account. SDL3 and a C compiler are required; `SDL3_CFLAGS` and `SDL3_LIBS` can select
an installation. `BT3_TEST_TMPDIR` selects a writable test output directory.

The memory test checks relocation, partial pointer reads, non-pointer integers,
null pointers, adjacent regions, holes, malformed descriptors, overflow and
preserving the destination on failure.

The game-adapter test runs production mapping and identification code against
synthetic native objects. It checks actual Save/Progress/loading reads, rejects
unmapped transient objects and pointer fields, compares the executable hash with
Python's independent hashlib result, rejects invalid/empty executables, and
checks data-verification/netplay/speed/clash/rollback/Unlock Everything gates.
It requires an x86-64 Linux or Windows host, but no disc, SDL or account.

`.github/workflows/retroachievements.yml` runs all offline checks and builds both
complete release archives in the project's official Docker toolchain.

A separate copy of an installed Windows build was observed for 30 seconds without
a window. Its relocated Progress/Save object offsets agree with these source
layouts. That run validates object identity in the installed build, not execution
of this modified executable. Changed client/game modules and the settings UI have
been compiled or syntax checked; full game build and live-account acceptance are
still required before treating this branch as release-ready.

References: [rc_client integration](https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration),
[PS2 game page](https://retroachievements.org/game/20590), issues
[#93](https://github.com/z3xox/Tenkaichi3Decomp/issues/93) and
[#111](https://github.com/z3xox/Tenkaichi3Decomp/issues/111).
