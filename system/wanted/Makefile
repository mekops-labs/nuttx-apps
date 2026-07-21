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
CFLAGS += -Iengine/vendor/uzlib/src
CFLAGS += -Iengine/vendor/ed25519/src

CFLAGS += -DPLATFORM_NUTTX=1
# TLS secure sockets: the shared raw-mbedTLS layer over apps/crypto/mbedtls
# (whose Make.defs already puts the mbedtls headers on every app's CFLAGS).
ifeq ($(CONFIG_SYSTEM_WANTED_TLS),y)
CFLAGS += -DSECURE_SOCKETS=1
else
CFLAGS += -DSECURE_SOCKETS=0
endif
CFLAGS += -DSUPERVISOR_IMAGE_PATH=\"$(patsubst "%",%,$(strip $(CONFIG_SYSTEM_WANTED_SUPERVISOR_IMAGE)))\"

# Engine build configuration. The engine owns a Kconfig tree whose generated
# header every engine source includes. It is generated from the engine's own
# .config rather than the host's: the two are separate symbol namespaces.
#
# WANTED_DEFCONFIG names an engine envelope (engine configs/<name>_defconfig);
# unset, the Kconfig defaults apply. The ESP32 target selects one carrying a
# 24 KiB worker stack — its stacks must live in scarce internal DRAM.
#
# Generated while this Makefile is parsed rather than as a rule: Application.mk
# (which defines OBJS and the compile rules) is included at the end, so a rule
# keyed on OBJS here would see it empty and the header would still be missing at
# the first compile. genconfig skips the write when the contents are unchanged,
# so running it every invocation does not churn rebuilds.
WANTED_AUTOCONF_DIR := $(APPDIR)/system/wanted/autoconf
WANTED_AUTOCONF     := $(WANTED_AUTOCONF_DIR)/wanted-autoconf.h
WANTED_DOTCONFIG    := $(WANTED_AUTOCONF_DIR)/.config
WANTED_DEFCONFIG    ?=

CFLAGS += -I$(WANTED_AUTOCONF_DIR)

WANTED_ENGINE_DIR  := $(APPDIR)/system/wanted/engine
WANTED_KCONFIG_ENV := PYTHONPATH=tools/kconfiglib KCONFIG_CONFIG=$(WANTED_DOTCONFIG)
ifeq ($(WANTED_DEFCONFIG),)
WANTED_KCONFIG_SEED := olddefconfig.py Kconfig
else
WANTED_KCONFIG_SEED := defconfig.py --kconfig Kconfig configs/$(WANTED_DEFCONFIG)
endif
# Captured, not bare: a bare $(shell) expands to its stdout, which make would
# then read as makefile syntax. Progress chatter goes to /dev/null; only
# kconfiglib's warnings (stderr) are kept, and they run once per make.
WANTED_KCONFIG_LOG := $(shell mkdir -p $(WANTED_AUTOCONF_DIR) && \
  cd $(WANTED_ENGINE_DIR) && \
  $(WANTED_KCONFIG_ENV) python3 tools/kconfiglib/$(WANTED_KCONFIG_SEED) 2>&1 >/dev/null && \
  $(WANTED_KCONFIG_ENV) python3 tools/kconfiglib/genconfig.py \
    --header-path $(WANTED_AUTOCONF) Kconfig 2>&1 >/dev/null)
ifneq ($(WANTED_KCONFIG_LOG),)
$(info wanted kconfig: $(WANTED_KCONFIG_LOG))
endif

# Override object SUFFIX to be stable regardless of build path (fixes CI failures
# with long absolute paths in CWD mangled by Application.mk).
SUFFIX = .wanted

# Engine version string, surfaced at /proc/wanted. The CMake build derives this
# from git describe (cmake/VersionFromGit.cmake); mirror it here so the NuttX
# build reports the same value instead of the "unknown" fallback. Falls back to
# "unknown" when git or tags are unavailable (e.g. a shallow CI clone).
WANTED_VERSION := $(shell git -C engine describe --tags --dirty 2>/dev/null || echo unknown)
CFLAGS += -DWANTED_VERSION=\"$(WANTED_VERSION)\"

# --- Engine core ----------------------------------------------------------
CSRCS += wanted_malloc.c wanted.c wanted_wasm_api.c wanted-vfs-api.c log-store.c \
         extra-drivers-none.c
VPATH   += engine/src
DEPPATH += --dep-path engine/src

CSRCS += vfs.c vfs-9p.c vfs-config.c vfs-devfs.c vfs-netfs.c vfs-null.c \
         vfs-log.c vfs-logmount.c vfs-pipe.c vfs-procfs.c vfs-procfs-wapps.c \
         vfs-stdio.c vfs-tarfs.c vfs-socket.c \
         vfs-wanted.c vfs-wanted-config.c vfs-wanted-wapps.c \
         vfs-wanted-registry.c vfs-virtual.c \
         vfs-sha256.c vfs-ed25519.c vfs-inflate.c
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

# uzlib: decompressor + checksums only (the compressor stays uncompiled),
# matching the host build (src/CMakeLists.txt). Backs /dev/inflate (vfs-inflate.c).
CSRCS += tinflate.c crc32.c adler32.c
VPATH   += engine/vendor/uzlib/src
DEPPATH += --dep-path engine/vendor/uzlib/src

# ed25519 (orlp/ed25519, verify-only subset): backs PlatformEd25519Verify
# (platform/nuttx/api/crypto.c) - NuttX's vendored mbedTLS has no Ed25519
# support. Not part of the host build: Linux keeps its OpenSSL-backed
# PlatformEd25519Verify (platform/linux/api/crypto.c) unchanged.
CSRCS += verify.c sha512.c ge.c sc.c fe.c
VPATH   += engine/vendor/ed25519/src
DEPPATH += --dep-path engine/vendor/ed25519/src

# --- NuttX platform layer -------------------------------------------------
CSRCS += clock-sleep.c random.c wapps.c memory.c registry.c fs-volume.c extram.c \
         crypto.c
VPATH   += engine/platform/nuttx/api
DEPPATH += --dep-path engine/platform/nuttx/api

# --- Shared POSIX platform layer (also compiled by the Linux engine build) --
# sha256.c backs the PlatformSha256* seam vfs-sha256.c now calls (moved out of
# a NuttX-only inline FIPS 180-4 implementation when the engine gained a
# cross-platform crypto seam); missing this left PlatformSha256New/Update/
# Final/Free undefined at link time.
CSRCS   += mutex.c clock.c fs.c socket.c registry-store.c wapps-image.c sha256.c
ifeq ($(CONFIG_SYSTEM_WANTED_TLS),y)
CSRCS   += ssocket-mbedtls.c
endif
VPATH   += engine/platform/posix
DEPPATH += --dep-path engine/platform/posix

CSRCS += vfs-nuttx.c vfs-gpio.c vfs-wifi.c
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

# Any no-host-fs target's boot ROMFS: bake the supervisor TAR into a ROMFS
# image generated at build time. Both board init shims that mount it at /rom
# are compiled unconditionally here (a few bytes each); only one is ever
# selected as CONFIG_INIT_ENTRYPOINT for a given board config. Gated so the
# sim build is untouched.
ifeq ($(CONFIG_SYSTEM_WANTED_BOOT_ROMFS),y)
CSRCS += wanted_esp32_main.c wanted_rp2350_main.c

BOOT_ROMFS_VARIANT := $(patsubst "%",%,$(strip $(CONFIG_SYSTEM_WANTED_BOOT_ROMFS_SUPERVISOR)))
BOOT_ROMFS_SRC     := engine/wasm/supervisor/$(BOOT_ROMFS_VARIANT)/supervisor.tar
BOOT_ROMFS_HDR     := engine/platform/nuttx/app/boot-romfs.h
# Optional factory-seed registry: any engine/wasm/registry-seed/*.wapp is bundled
# under /rom/registry and copied into the writable registry on first boot.
BOOT_ROMFS_SEED    := engine/wasm/registry-seed

# Generate the header before compiling the shim (apps pre-build hook).
context:: $(BOOT_ROMFS_HDR)

# Depend on the seed *directory* (its mtime tracks add/remove), not the *.wapp
# files: a registry filename contains a colon ("<name>:<ver>.wapp"), which Make
# cannot have in a prerequisite. $(wildcard ) yields nothing when absent.
$(BOOT_ROMFS_HDR): $(BOOT_ROMFS_SRC) $(wildcard $(BOOT_ROMFS_SEED))
	$(Q) rm -rf boot-romfs-stage && mkdir -p boot-romfs-stage/wanted
	$(Q) cp $< boot-romfs-stage/wanted/supervisor.tar
	$(Q) if ls $(BOOT_ROMFS_SEED)/*.wapp >/dev/null 2>&1; then \
	       mkdir -p boot-romfs-stage/registry; \
	       cp $(BOOT_ROMFS_SEED)/*.wapp boot-romfs-stage/registry/; \
	     fi
	$(Q) genromfs -f boot-romfs.img -d boot-romfs-stage -V wanted
	$(Q) { echo '#include <nuttx/compiler.h>'; \
	       xxd -i boot-romfs.img \
	         | sed -e 's/unsigned char boot_romfs_img\[\]/const unsigned char aligned_data(16) boot_romfs_img[]/' \
	               -e 's/unsigned int boot_romfs_img_len/const unsigned int boot_romfs_img_len/'; \
	     } > $@
	$(Q) rm -rf boot-romfs.img boot-romfs-stage

clean::
	$(Q) rm -rf boot-romfs.img boot-romfs-stage $(BOOT_ROMFS_HDR)
endif

PROGNAME  = $(patsubst "%",%,$(strip $(CONFIG_SYSTEM_WANTED_PROGNAME)))
PRIORITY  = $(CONFIG_SYSTEM_WANTED_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_WANTED_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_WANTED)

include $(APPDIR)/Application.mk
