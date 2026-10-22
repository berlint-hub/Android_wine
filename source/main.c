#include <switch.h>

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define LOG_DIR  "sdmc:/switch/androidwine"
#define LOG_PATH LOG_DIR "/androidwine.log"

static FILE *g_log;
static bool g_log_sync_ok = true;
static char g_log_status[96] = "not initialized";

static void log_message(const char *level, const char *format, ...)
{
    if (g_log == NULL)
        return;

    time_t now = time(NULL);
    struct tm utc;
    char timestamp[32] = "time-unavailable";
    if (gmtime_r(&now, &utc) != NULL)
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);

    fprintf(g_log, "[%s] %-5s ", timestamp, level);
    va_list args;
    va_start(args, format);
    vfprintf(g_log, format, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);

    // Make each diagnostic event survive a forced app exit or crash as far as
    // the SD filesystem allows. This is intentionally not done in the frame loop.
    if (fsync(fileno(g_log)) != 0)
        g_log_sync_ok = false;
}

static void log_result(const char *operation, Result result)
{
    log_message(R_SUCCEEDED(result) ? "INFO" : "ERROR",
                "%s: result=0x%08" PRIx32,
                operation,
                (uint32_t)result);
}

static void open_log(void)
{
    // libnx mounts sdmc: during normal NRO startup. Mounting it again can
    // fail even though the existing devoptab is usable, so inspect it instead.
    if (fsdevGetDeviceFileSystem("sdmc") == NULL) {
        snprintf(g_log_status, sizeof(g_log_status),
                 "sdmc devoptab is not available");
        return;
    }

    (void)mkdir("sdmc:/switch", 0777);
    if (mkdir(LOG_DIR, 0777) != 0 && errno != EEXIST) {
        snprintf(g_log_status, sizeof(g_log_status),
                 "cannot create log directory: errno=%d", errno);
        return;
    }

    g_log = fopen(LOG_PATH, "a");
    if (g_log == NULL) {
        snprintf(g_log_status, sizeof(g_log_status),
                 "cannot open log: errno=%d", errno);
        return;
    }

    setvbuf(g_log, NULL, _IOLBF, 0);
    snprintf(g_log_status, sizeof(g_log_status), "%s", LOG_PATH);
    log_message("INFO", "========== AndroidWine Probe boot ==========");
    log_message("INFO", "probe_version=0.2.0");
    log_message("INFO", "build=%s %s", __DATE__, __TIME__);
    log_message("INFO", "horizon_version_raw=0x%08" PRIx32, (uint32_t)hosversionGet());
    log_message("INFO", "sdmc_devoptab=available");

    u64 total_memory = 0;
    u64 used_memory = 0;
    Result total_result = svcGetInfo(&total_memory, InfoType_TotalMemorySize,
                                     CUR_PROCESS_HANDLE, 0);
    Result used_result = svcGetInfo(&used_memory, InfoType_UsedMemorySize,
                                    CUR_PROCESS_HANDLE, 0);
    log_result("svcGetInfo(TotalMemorySize)", total_result);
    if (R_SUCCEEDED(total_result))
        log_message("INFO", "total_memory_bytes=%" PRIu64, total_memory);
    log_result("svcGetInfo(UsedMemorySize)", used_result);
    if (R_SUCCEEDED(used_result))
        log_message("INFO", "used_memory_bytes=%" PRIu64, used_memory);
}

static void close_log(void)
{
    if (g_log != NULL) {
        if (!g_log_sync_ok)
            log_message("WARN", "one or more fsync calls failed; recent log lines may not be durable");
        log_message("INFO", "========== AndroidWine Probe exit ==========");
        fclose(g_log);
        g_log = NULL;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    open_log();

    printf("AndroidWine Switch probe v0.2.0\n");
    printf("SD mount fix build\n");
    printf("====================================\n\n");
    printf("This build checks the Horizon homebrew environment.\n");
    printf("It does not run APKs or Android games yet.\n\n");
    printf("Log: %s\n\n", g_log_status);
    printf("A: write a test marker to the log\n");
    printf("Plus: exit\n\n");
    printf("Run as a game override (hold R while launching\n");
    printf("a title) if you want to test with full memory.\n");
    printf("Please copy the log from the SD card after exit.\n");
    consoleUpdate(NULL);

    if (g_log == NULL)
        printf("\nLog file unavailable; note the status above.\n");

    u64 previous_down = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        if (down != 0 && down != previous_down)
            log_message("INPUT", "buttons_down=0x%016" PRIx64, down);

        if (down & HidNpadButton_A) {
            log_message("INFO", "user_marker=A");
            printf("\nMarker written.\n");
        }
        if (down & HidNpadButton_Plus) {
            log_message("INFO", "exit_requested=Plus");
            break;
        }

        previous_down = down;
        consoleUpdate(NULL);
        svcSleepThread(16 * 1000 * 1000LL);
    }

    close_log();
    consoleExit(NULL);
    return 0;
}
