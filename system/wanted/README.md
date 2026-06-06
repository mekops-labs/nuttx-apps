# NuttX built-in app (apps/system/wanted)

This directory is the WANTED Engine NuttX application package. It is consumed
by a NuttX `apps/` tree as `apps/system/wanted` and builds the engine plus its
vendored WAMR into the `wanted` NSH built-in command (entry `wanted_main`).

## Contents

| File         | Role                                                          |
|--------------|---------------------------------------------------------------|
| `Kconfig`    | `config SYSTEM_WANTED` (+ progname/priority/stack/image path) |
| `Make.defs`  | Registers the app (`CONFIGURED_APPS += .../system/wanted`)    |
| `Makefile`   | Builds vendored WAMR via `wamr.mk` + the engine `CSRCS`       |

WAMR is vendored, not pulled from `apps/interpreters/wamr`: the `Makefile`
includes `wamr/product-mini/platforms/nuttx/wamr.mk` with the same feature set
as the host build (classic interpreter + libc-builtin; AOT / fast-interp /
libc-wasi / multi-module / shared-memory off).

## Building the sim

The whole flow is automated by `test/nuttx-sim.sh` (wrapped by the top-level
`Makefile`); `NUTTX_DIR`/`APPS_DIR` are the shallow `third_party/nuttx{,-apps}`
submodules (the mekops forks):

```sh
make nuttx-build     # init submodules, link this app in, configure + build sim:nsh
make nuttx-smoke     # run the smoke suite against the sim
make nuttx-shell     # boot the sim into the interactive wsh prompt
```

`deps` links this package into the apps submodule as a **real** `system/wanted`
directory (NuttX globs real subdirectories of `apps/<category>/` — a
whole-directory symlink is not seen by `mkkconfig`): the three build files plus
`engine`/`wamr` are relative symlinks back into the engine. Those symlinks are
kept out of the apps submodule's status via its `info/exclude`, so the engine
stays the single source of truth for the package. The build enables
`CONFIG_SYSTEM_WANTED` + `CONFIG_FS_HOSTFS` and runs wsh as the **init task**
(`CONFIG_INIT_ENTRYPOINT=wanted_sim_main`) so it owns `/dev/console`.

## Sim console + smoke notes

- An NSH-spawned built-in's stdout does **not** reach the captured console in
  this sim config, so the engine runs as the init task via the `wanted_sim_main`
  shim, which mounts `/data` over hostfs (the board `rcS` does that, but `rcS`
  does not run when wsh is init) and then calls `wanted_main`.
- `/data` is the hostfs mount of the sim's launch dir; `nuttx-sim.sh` stages the
  supervisor image at `$SIMROOT/wanted/supervisor.tar` and the config at
  `$SIMROOT/smoke.json` (`CONFIG_INIT_ARGS`).
- WAMR is built with **bulk memory + reference types on** (the NuttX `wamr.mk`
  defaults them off) to match the host build — modern clang emits bulk-memory
  ops, so the supervisor/wapp images need them.
- The init task's C `stdout`/`stderr` FILE streams are not bound to the fds —
  only raw `write(1/2)` reaches the console, so `fprintf`-based `DEBUG_TRACE` is
  invisible there.
