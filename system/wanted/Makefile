############################################################################
# apps/system/wanted/Makefile
#
# WANTED Engine NuttX built-in application.
#
# Layout (symlinks in this directory):
#   engine -> the wanted-engine checkout root
#   wamr   -> engine/vendor/wamr (the vendored WAMR runtime)
#
# WAMR is built from the vendored copy via product-mini/platforms/nuttx/wamr.mk
# (same mechanism apps/interpreters/wamr uses), driven by the WAMR feature
# flags set below. The engine sources are added on top as ordinary CSRCS.
############################################################################

include $(APPDIR)/Make.defs

# --- Vendored WAMR --------------------------------------------------------
# Match the host/Linux engine build (src/CMakeLists.txt): classic interpreter
# plus libc-builtin; AOT, fast-interp, libc-wasi, multi-module and shared
# memory are off. wamr.mk keys off these CONFIG_* make variables.
CONFIG_INTERPRETERS_WAMR_CLASSIC      = y
CONFIG_INTERPRETERS_WAMR_LIBC_BUILTIN = y
# Bulk memory (memory.copy/fill/init, opcodes fc 08-0b) and reference types are
# on by default in WAMR's CMake build (the host engine build), and modern
# clang/wasi-sdk emits them, so the supervisor/wapp images require them. The
# NuttX wamr.mk defaults both OFF — enable them to match the host feature set.
CONFIG_INTERPRETERS_WAMR_BULK_MEMORY  = y
CONFIG_INTERPRETERS_WAMR_REF_TYPES    = y
# Thread manager: the host CMake build enables it by default, and the
# interpreter's terminate check (WASM_SUSPEND_FLAG_TERMINATE) is compiled in
# only when it is on. Without it wasm_runtime_terminate sets a flag nothing
# ever reads, so a cooperative stop (PlatformWappStop) never ends the wapp and
# the engine's supervisor loop runs forever. Enable it to match the host.
CONFIG_INTERPRETERS_WAMR_THREAD_MGR   = y

WAMR_BUILD_INTERP = 1

-include wamr/product-mini/platforms/nuttx/wamr.mk

VPATH   += wamr/product-mini/platforms/nuttx
DEPPATH += --dep-path wamr/product-mini/platforms/nuttx

# --- Engine include paths -------------------------------------------------
CFLAGS += -Iengine/include
CFLAGS += -Iengine/src/include
CFLAGS += -Iengine/platform/include
CFLAGS += -Iengine/platform/nuttx/include
CFLAGS += -Iengine/vendor/tiny-json
CFLAGS += -Iengine/vendor/c9
CFLAGS += -Iengine/vendor/cwalk/include

CFLAGS += -DPLATFORM_NUTTX=1
CFLAGS += -DSECURE_SOCKETS=0
CFLAGS += -DSUPERVISOR_IMAGE_PATH=\"$(patsubst "%",%,$(strip $(CONFIG_SYSTEM_WANTED_SUPERVISOR_IMAGE)))\"

# --- Engine core ----------------------------------------------------------
CSRCS += wanted_malloc.c wanted.c wanted_wasm_api.c wanted-vfs-api.c log-store.c
VPATH   += engine/src
DEPPATH += --dep-path engine/src

CSRCS += vfs.c vfs-9p.c vfs-config.c vfs-devfs.c vfs-netfs.c vfs-null.c \
         vfs-log.c vfs-pipe.c vfs-procfs.c vfs-stdio.c vfs-tarfs.c vfs-socket.c \
         vfs-wanted.c vfs-wanted-config.c vfs-wanted-wapps.c \
         vfs-wanted-registry.c vfs-virtual.c
VPATH   += engine/src/vfs
DEPPATH += --dep-path engine/src/vfs

CSRCS += wasi-vfs.c
VPATH   += engine/src/wasi
DEPPATH += --dep-path engine/src/wasi

# --- Engine vendored libraries -------------------------------------------
CSRCS += tiny-json.c
VPATH   += engine/vendor/tiny-json
DEPPATH += --dep-path engine/vendor/tiny-json

CSRCS += c9.c
VPATH   += engine/vendor/c9
DEPPATH += --dep-path engine/vendor/c9

CSRCS += cwalk.c
VPATH   += engine/vendor/cwalk/src
DEPPATH += --dep-path engine/vendor/cwalk/src

# --- NuttX platform layer -------------------------------------------------
CSRCS += clock.c random.c mutex.c wapps.c memory.c registry.c fs.c socket.c
VPATH   += engine/platform/nuttx/api
DEPPATH += --dep-path engine/platform/nuttx/api

CSRCS += vfs-nuttx.c
VPATH   += engine/platform/nuttx/vfs
DEPPATH += --dep-path engine/platform/nuttx/vfs

# --- Application entry point ----------------------------------------------
MAINSRC  = wanted_main.c
VPATH   += engine/platform/nuttx
DEPPATH += --dep-path engine/platform/nuttx

# Sim init shim: an alternate entry (CONFIG_INIT_ENTRYPOINT=wanted_sim_main)
# that mounts /data over hostfs before running the engine, for the sim test /
# interactive board config where NSH's rcS does not run. Linked unconditionally
# (a few bytes); only used when selected as the init entrypoint.
CSRCS   += wanted_sim_main.c
VPATH   += engine/platform/nuttx/app
DEPPATH += --dep-path engine/platform/nuttx/app

PROGNAME  = $(patsubst "%",%,$(strip $(CONFIG_SYSTEM_WANTED_PROGNAME)))
PRIORITY  = $(CONFIG_SYSTEM_WANTED_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_WANTED_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_WANTED)

include $(APPDIR)/Application.mk
