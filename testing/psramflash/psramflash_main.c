/****************************************************************************
 * apps/testing/psramflash/psramflash_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Reproducer for the ESP32 flash-read-while-PSRAM-dirty corruption.
 *
 * A manual SPI-flash read on the classic ESP32 disables the whole cache for
 * the duration of the read; while another task holds live (dirty) PSRAM, the
 * read returns corrupt bytes. This app isolates that from any application: one
 * thread continuously scribbles a PSRAM buffer (keeping its cache lines dirty)
 * while the main thread reads a known pattern back off a LittleFS file and
 * verifies it. It reports read errors and content mismatches with the PSRAM
 * writer idle, then busy, then idle again (to show the condition is live and
 * reversible, not static corruption).
 *
 * Run from NSH:  psramflash [/mountpoint]   (default /data)
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PSRAM_BUF_SIZE (512 * 1024) /* forces a PSRAM-backed allocation */
#define TEST_FILE_SIZE (256 * 1024) /* large enough to outrun the FS cache */
#define IO_CHUNK (32 * 1024)         /* large reads => long manual-read window */
#define DEFAULT_MOUNT "/data"
#define TEST_FILE_NAME "/psramflash.bin"

#define PSRAM_LO 0x3f800000u
#define PSRAM_HI 0x40000000u

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_writer_run;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Deterministic, position-dependent byte so a mismatch is detectable
 * regardless of where in the file it lands.
 */

static inline uint8_t pattern_byte(uint32_t i)
{
  return (uint8_t)((i * 31u + 7u) & 0xff);
}

/* Dirty a span of PSRAM with read-modify-write so the shared flash/PSRAM cache
 * holds dirty (not-yet-written-back) PSRAM lines at the moment of a flash read.
 */

static FAR volatile uint8_t *g_psram;
static size_t g_psram_off;
static FAR uint8_t *g_iobuf; /* heap I/O buffer (IO_CHUNK too big for stack) */

#define HOT_REGION (64 * 1024) /* mimic a WAMR linear-memory working set */

/* Instruction-cache pressure: a large (> I-cache) straight-line code body so
 * executing it sweeps many flash-XIP cache lines. ESP32 shares one cache
 * between flash (instruction) and PSRAM (data) across even/odd ways, so a big
 * interpreter-like code footprint thrashing flash-XIP — concurrent with dirty
 * PSRAM — is the suspected missing ingredient the minimal data-only harness
 * lacked.
 */

static volatile uint32_t g_sink;

static void burn_icache(void)
{
  uint32_t acc = g_sink;

#define OP(n) acc = acc * 1000003u + (uint32_t)(n); acc ^= acc >> 7;
#define OP4(b) OP(b) OP((b) + 1) OP((b) + 2) OP((b) + 3)
#define OP16(b) OP4(b) OP4((b) + 4) OP4((b) + 8) OP4((b) + 12)
#define OP64(b) OP16(b) OP16((b) + 16) OP16((b) + 32) OP16((b) + 48)
#define OP256(b) OP64(b) OP64((b) + 64) OP64((b) + 128) OP64((b) + 192)
#define OP1024(b) OP256(b) OP256((b) + 256) OP256((b) + 512) OP256((b) + 768)

  OP1024(0)
  OP1024(1024)

#undef OP
#undef OP4
#undef OP16
#undef OP64
#undef OP256
#undef OP1024

  g_sink = acc;
}

static void psram_dirty(size_t bytes)
{
  if (g_psram == NULL)
    {
      return;
    }

  for (size_t n = 0; n < bytes; n++)
    {
      size_t i = g_psram_off % HOT_REGION;
      g_psram[i] = (uint8_t)(g_psram[i] + i + 0x5a);
      g_psram_off += 64; /* stride a cache line within the hot region */
    }
}

/* Background pressure: keep scribbling PSRAM on its own thread too. */

static FAR void *psram_writer(FAR void *arg)
{
  FAR volatile uint8_t *buf = (FAR volatile uint8_t *)arg;
  uint32_t k = 0;

  while (g_writer_run)
    {
      for (size_t i = 0; i < HOT_REGION; i++)
        {
          buf[i] = (uint8_t)(buf[i] + k + i);
        }

      burn_icache();
      k++;
    }

  return NULL;
}

/* Write the known-pattern test file while PSRAM is idle (safe). */

static int write_test_file(FAR const char *path)
{
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    {
      printf("psramflash: open(%s) for write failed: %d\n", path, errno);
      return -1;
    }

  for (uint32_t off = 0; off < TEST_FILE_SIZE; off += IO_CHUNK)
    {
      for (size_t i = 0; i < IO_CHUNK; i++)
        {
          g_iobuf[i] = pattern_byte(off + i);
        }

      if (write(fd, g_iobuf, IO_CHUNK) != IO_CHUNK)
        {
          printf("psramflash: write failed at off %lu: %d\n",
                 (unsigned long)off, errno);
          close(fd);
          return -1;
        }
    }

  close(fd);
  return 0;
}

/* Read the file back (fresh open, to defeat the per-handle FS cache) and
 * verify every byte. Accumulates read errors and content mismatches.
 */

static void verify_read(FAR const char *path, bool dirty, FAR int *read_errors,
                        FAR int *mismatches)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      (*read_errors)++;
      return;
    }

  for (uint32_t off = 0; off < TEST_FILE_SIZE; off += IO_CHUNK)
    {
      /* Dirty PSRAM and thrash flash-XIP I-cache immediately before the flash
       * read, on this same thread, so the shared cache holds both dirty PSRAM
       * lines and freshly-fetched flash code lines during the cache-disabled
       * manual read.
       */

      if (dirty)
        {
          psram_dirty(IO_CHUNK);
          burn_icache();
        }

      ssize_t n = read(fd, g_iobuf, IO_CHUNK);
      if (n != IO_CHUNK)
        {
          /* A LittleFS CRC failure surfaces here as a negative read
           * (LFS_ERR_CORRUPT -> -EFAULT).
           */

          (*read_errors)++;
          break;
        }

      for (size_t i = 0; i < IO_CHUNK; i++)
        {
          if (g_iobuf[i] != pattern_byte(off + i))
            {
              (*mismatches)++;
              break;
            }
        }
    }

  close(fd);
}

static void run_passes(FAR const char *path, FAR const char *label,
                       bool dirty, int passes)
{
  int read_errors = 0;
  int mismatches = 0;

  for (int r = 0; r < passes; r++)
    {
      verify_read(path, dirty, &read_errors, &mismatches);
    }

  printf("psramflash: [%s] %d read passes -> read_errors=%d mismatches=%d\n",
         label, passes, read_errors, mismatches);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const char *mount = (argc > 1) ? argv[1] : DEFAULT_MOUNT;
  char path[64];
  snprintf(path, sizeof(path), "%s%s", mount, TEST_FILE_NAME);

  FAR uint8_t *psram = (FAR uint8_t *)malloc(PSRAM_BUF_SIZE);
  if (psram == NULL)
    {
      printf("psramflash: cannot allocate %d bytes\n", PSRAM_BUF_SIZE);
      return 1;
    }

  uintptr_t a = (uintptr_t)psram;
  bool in_psram = (a >= PSRAM_LO && a < PSRAM_HI);
  printf("psramflash: scribble buffer @ %p (%s)\n", psram,
         in_psram ? "PSRAM" : "INTERNAL - test invalid, need PSRAM");
  g_psram = psram;

  g_iobuf = (FAR uint8_t *)malloc(IO_CHUNK);
  if (g_iobuf == NULL)
    {
      printf("psramflash: cannot allocate I/O buffer\n");
      free(psram);
      return 1;
    }

  printf("psramflash: writing %d KB test file to %s\n",
         TEST_FILE_SIZE / 1024, path);
  if (write_test_file(path) != 0)
    {
      free(psram);
      return 1;
    }

  /* Baseline: PSRAM idle. */

  run_passes(path, "idle PSRAM", false, 5);

  /* Under PSRAM pressure: a background scribbler thread plus interleaved
   * read-modify-write right before each flash read.
   */

  g_writer_run = true;
  pthread_t writer;
  if (pthread_create(&writer, NULL, psram_writer, psram) != 0)
    {
      printf("psramflash: pthread_create failed\n");
      free(psram);
      return 1;
    }

  run_passes(path, "busy PSRAM", true, 20);

  /* Reversibility: stop the writer, read again. */

  g_writer_run = false;
  pthread_join(writer, NULL);
  run_passes(path, "idle again", false, 5);

  free(psram);
  return 0;
}
