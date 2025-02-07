/**
 * @file    fps_outlog.c
 * @brief   output log functions for FPS
 */

#include <stdarg.h>
#include <unistd.h> // access()

#include "CommandLineInterface/CLIcore.h"
#include "timeutils.h"

/** @brief Get FPS log filename
 *
 * logfname should be char [STRINGMAXLEN_FULLFILENAME]
 *
 */
errno_t getFPSlogfname(char *logfname)
{
    char shmdname[STRINGMAXLEN_SHMDIRNAME];
    function_parameter_struct_shmdirname(shmdname);

    WRITE_FULLFILENAME(logfname,
                       "%s/fpslog.%ld.%07d.%s",
                       shmdname,
                       data.FPS_TIMESTAMP,
                       getpid(),
                       data.FPS_PROCESS_TYPE);

    return RETURN_SUCCESS;
}


errno_t functionparameter_outlog_file(
    char *keyw,
    char *msgstring,
    FILE *fpout
)
{
    // Get GMT time
    struct timespec tnow;
    time_t          now;

    clock_gettime(CLOCK_REALTIME, &tnow);
    now = tnow.tv_sec;
    struct tm *uttime;
    uttime = gmtime(&now);

    char timestring[TIMESTRINGLEN];
    snprintf(timestring,
             TIMESTRINGLEN,
             "%04d%02d%02dT%02d%02d%02d.%09ld",
             1900 + uttime->tm_year,
             1 + uttime->tm_mon,
             uttime->tm_mday,
             uttime->tm_hour,
             uttime->tm_min,
             uttime->tm_sec,
             tnow.tv_nsec);

    fprintf(fpout, "%s %-12s %s\n", timestring, keyw, msgstring);
    fflush(fpout);

    return RETURN_SUCCESS;
}




/**
 * @brief Add log entry to fps log
 *
 * @param keyw     Entry keyword
 * @param fmt      Format
 * @param ...      Parameters
 * @return errno_t Error code
 */
errno_t functionparameter_outlog(
    char *keyw,
    const char *fmt, ...)
{
    // identify logfile and open file

    static int   LogOutOpen = 0;
    static FILE *fpout;
    static char  logfname[STRINGMAXLEN_FULLFILENAME];

    if(LogOutOpen == 0)  // file not open
    {
        getFPSlogfname(logfname);

        fpout = fopen(logfname, "a");
        if(fpout == NULL)
        {
            printf("ERROR: cannot open file\n");
            exit(EXIT_FAILURE);
        }
        LogOutOpen = 1;
    }

    // Get GMT time and create timestring

    struct timespec tnow;
    time_t          now;

    clock_gettime(CLOCK_REALTIME, &tnow);
    now = tnow.tv_sec;
    struct tm *uttime;
    uttime = gmtime(&now);

    char timestring[TIMESTRINGLEN];
    snprintf(timestring,
             TIMESTRINGLEN,
             "%04d%02d%02dT%02d%02d%02d.%09ld",
             1900 + uttime->tm_year,
             1 + uttime->tm_mon,
             uttime->tm_mday,
             uttime->tm_hour,
             uttime->tm_min,
             uttime->tm_sec,
             tnow.tv_nsec);

    fprintf(fpout, "%s %-12s ", timestring, keyw);

    va_list args;
    va_start(args, fmt);

    vfprintf(fpout, fmt, args);

    fprintf(fpout, "\n");

    fflush(fpout);

    va_end(args);

    if(strcmp(keyw, "LOGFILECLOSE") == 0)
    {
        // Normal exit
        // close log file and remove it from filesystem

        if(LogOutOpen == 1)
        {
            fclose(fpout);
            LogOutOpen = 0;
        }
        remove(logfname);
    }

    return RETURN_SUCCESS;
}




/** @brief Establish sym link for convenience
 *
 * This is a one-time function when running FPS init.\n
 * Creates a human-readable informative sym link to outlog\n
 */
errno_t functionparameter_outlog_namelink()
{
    char shmdname[STRINGMAXLEN_SHMDIRNAME];
    function_parameter_struct_shmdirname(shmdname);

    char logfname[STRINGMAXLEN_FULLFILENAME];
    getFPSlogfname(logfname);

    char linkfname[STRINGMAXLEN_FULLFILENAME];
    WRITE_FULLFILENAME(linkfname,
                       "%s/fpslog.%s",
                       shmdname,
                       data.FPS_PROCESS_TYPE);

    if(access(linkfname, F_OK) == 0)  // link already exists, remove
    {
        printf("outlog file %s exists -> updating symlink\n", linkfname);
        remove(linkfname);
    }

    if(symlink(logfname, linkfname) == -1)
    {
        int errnum = errno;
        fprintf(stderr, "Error symlink: %s\n", strerror(errnum));
        PRINT_ERROR("symlink error %s %s", logfname, linkfname);
    }

    return RETURN_SUCCESS;
}
