/**
 * @file function_parameters.c
 * @brief Tools to help expose and control function parameters
 *
 * @see @ref page_FunctionParameterStructure
 *
 * @defgroup FPSconf Configuration function for Function Parameter Structure (FPS)
 * @defgroup FPSrun  Run function using Function Parameter Structure (FPS)
 *
 */


#define _GNU_SOURCE

/* =============================================================================================== */
/* =============================================================================================== */
/*                                        HEADER FILES                                             */
/* =============================================================================================== */
/* =============================================================================================== */


#ifndef __STDC_LIB_EXT1__
typedef int errno_t;
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <limits.h>
#include <sys/syscall.h> // needed for tid = syscall(SYS_gettid);
#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <time.h>

#include <sys/types.h>

#include <ncurses.h>
#include <dirent.h>

#include <pthread.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <sys/mman.h> // mmap
#include <sys/stat.h> // fstat
#include <signal.h>
#include <unistd.h> // usleep

#include <sys/ioctl.h> // for terminal size


#ifndef STANDALONE
#include <CommandLineInterface/CLIcore.h>
#include "COREMOD_iofits/COREMOD_iofits.h"
#include "COREMOD_tools/COREMOD_tools.h"
#include "COREMOD_memory/COREMOD_memory.h"
#define SHAREDSHMDIR data.shmdir
#else
#include "standalone_dependencies.h"
#endif

#include "function_parameters.h"


/* =============================================================================================== */
/* =============================================================================================== */
/*                                      DEFINES, MACROS                                            */
/* =============================================================================================== */
/* =============================================================================================== */



#define NB_FPS_MAX 100
#define NB_KEYWNODE_MAX 10000

#define MAXNBLEVELS 20


#define SCREENPRINT_STDIO   0
#define SCREENPRINT_NCURSES 1
#define SCREENPRINT_NONE    2


// ANSI ESCAPE CODES

static int printAEC = 0;

#define AEC_NORMAL    0
#define AEC_BOLD      1
#define AEC_FAINT     2
#define AEC_ITALIC    3
#define AEC_UNDERLINE 4
#define AEC_SLOWBLINK 5
#define AEC_FASTBLINK 6
#define AEC_REVERSE   7

#define AEC_BOLDOFF      22
#define AEC_FAINTOFF     22
#define AEC_ITALICOFF     3
#define AEC_UNDERLINEOFF 24
#define AEC_BLINKOFF     25
#define AEC_REVERSEOFF   27

// Foreground color

static int printAECfgcolor = 37;

#define AEC_FGCOLOR_BLACK   30
#define AEC_FGCOLOR_RED     31
#define AEC_FGCOLOR_GREEN   32
#define AEC_FGCOLOR_YELLOW  33
#define AEC_FGCOLOR_BLUE    34
#define AEC_FGCOLOR_MAGENTA 35
#define AEC_FGCOLOR_CYAN    36
#define AEC_FGCOLOR_WHITE   37
// note : +60 for high intensity


// Background color

static int printAECbgcolor = 40;

#define AEC_BGCOLOR_BLACK   40
#define AEC_BGCOLOR_RED     41
#define AEC_BGCOLOR_GREEN   42
#define AEC_BGCOLOR_YELLOW  43
#define AEC_BGCOLOR_BLUE    44
#define AEC_BGCOLOR_MAGENTA 45
#define AEC_BGCOLOR_CYAN    46
#define AEC_BGCOLOR_WHITE   47
// note : +60 for high intensity


#define AECBOLDHIGREEN "\033[1;92;40m"
#define AECBOLDHIRED "\033[1;91;40m"
#define AECNORMAL "\033[37;40;0m"


/* =============================================================================================== */
/* =============================================================================================== */
/*                                  GLOBAL DATA DECLARATION                                        */
/* =============================================================================================== */
/* =============================================================================================== */


static int wrow, wcol;

/*
 * Defines printfw output
 * 
 * SCREENPRINT_STDIO     printf to stdout
 * SCREENPRINT_NCURSES   printw
 * SCREENPRINT_NONE      don't print (silent)
 */

static int screenprintmode = SCREENPRINT_STDIO;

struct termios orig_termios;
struct termios new_termios;


#define MAX_NB_CHILD 500

typedef struct
{
    char keywordfull[FUNCTION_PARAMETER_KEYWORD_STRMAXLEN *
                                                          FUNCTION_PARAMETER_KEYWORD_MAXLEVEL];
    char keyword[FUNCTION_PARAMETER_KEYWORD_MAXLEVEL][FUNCTION_PARAMETER_KEYWORD_STRMAXLEN];
    int  keywordlevel;

    int parent_index;

    int NBchild;
    int child[MAX_NB_CHILD];

    int leaf; // 1 if this is a leaf (no child)
    int fpsindex;
    int pindex;


} KEYWORD_TREE_NODE;




/* =============================================================================================== */
/* =============================================================================================== */
/*                                    FUNCTIONS SOURCE CODE                                        */
/* =============================================================================================== */
/* =============================================================================================== */

/** @brief print to screen, or not
 *
 * mode=0 : printf (stdout)
 * mode=1 : printw
 * mode=3 : don't print (silent)
 */

static void printfw(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);


    if(screenprintmode == SCREENPRINT_STDIO)
    {
        vfprintf(stdout, fmt, args);
    }

    if(screenprintmode == SCREENPRINT_NCURSES)
    {
        vw_printw(stdscr, fmt, args);
    }

    va_end(args);
}




static void screenprint_setcolor( int colorcode )
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attron(COLOR_PAIR(colorcode));
	}
	else
	{
		switch (colorcode) 
		{
			case 1:
			printAECfgcolor = AEC_FGCOLOR_WHITE;
			printAECbgcolor = AEC_BGCOLOR_BLACK;			
			break;

			case 2:
			printAECfgcolor = AEC_FGCOLOR_BLACK;
			printAECbgcolor = AEC_BGCOLOR_GREEN;			
			break;

			case 3:
			printAECfgcolor = AEC_FGCOLOR_BLACK;
			printAECbgcolor = AEC_BGCOLOR_YELLOW;			
			break;

			case 4:
			printAECfgcolor = AEC_FGCOLOR_WHITE;
			printAECbgcolor = AEC_BGCOLOR_RED;			
			break;

			case 5:
			printAECfgcolor = AEC_FGCOLOR_WHITE;
			printAECbgcolor = AEC_BGCOLOR_BLUE;			
			break;

			case 6:
			printAECfgcolor = AEC_FGCOLOR_BLACK;
			printAECbgcolor = AEC_BGCOLOR_GREEN;			
			break;

			case 7:
			printAECfgcolor = AEC_FGCOLOR_WHITE;
			printAECbgcolor = AEC_BGCOLOR_YELLOW;			
			break;

			case 8:
			printAECfgcolor = AEC_FGCOLOR_BLACK;
			printAECbgcolor = AEC_BGCOLOR_RED;			
			break;

			case 9:
			printAECfgcolor = AEC_FGCOLOR_RED;
			printAECbgcolor = AEC_BGCOLOR_BLACK;			
			break;
			
			case 10:
			printAECfgcolor = AEC_FGCOLOR_BLACK;
			printAECbgcolor = AEC_BGCOLOR_BLUE + 60;			
			break;									
		}
		
		printf( "\033[%d;%dm",  printAECfgcolor, printAECbgcolor);
	}
}


static void screenprint_unsetcolor( int colorcode )
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attroff(COLOR_PAIR(colorcode));
	}
	else
	{
		printAEC = AEC_NORMAL;
		printAECfgcolor = AEC_FGCOLOR_WHITE;
		printAECbgcolor = AEC_BGCOLOR_BLACK;	
		printf( "\033[%dm", printAEC);//, printAECbgcolor);
	}
}



static void screenprint_setbold()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attron(A_BOLD);
	}
	else
	{
		printAEC = AEC_BOLD;
		printf( "\033[%dm",  printAEC);
	}
}

static void screenprint_unsetbold()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attroff(A_BOLD);
	}
	else
	{
		printAEC = AEC_NORMAL; //AEC_BOLDOFF;
		printf( "\033[%dm",  printAEC);
	}
}





static void screenprint_setblink()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attron(A_BLINK);
	}
	else
	{
		printAEC = AEC_FASTBLINK;
		printf( "\033[%dm",  printAEC);
	}
}

static void screenprint_unsetblink()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attroff(A_BLINK);
	}
	else
	{
		printAEC = AEC_NORMAL; //AEC_BLINKOFF;
		printf( "\033[%dm", AEC_NORMAL);
	}
}



static void screenprint_setdim()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attron(A_DIM);
	}
	else
	{
		printAEC = AEC_FAINT;
		printf( "\033[%dm",  printAEC);
	}
}


static void screenprint_unsetdim()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attroff(A_DIM);
	}
	else
	{
		printAEC = AEC_NORMAL; //AEC_FAINTOFF;
		printf( "\033[%dm",  printAEC);
	}
}




static void screenprint_setreverse()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attron(A_REVERSE);
	}
	else
	{
		printAEC = AEC_REVERSE;
		printf( "\033[%dm",  printAEC);
	}
}

static void screenprint_unsetreverse()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		attroff(A_REVERSE);
	}
	else
	{
		printAEC = AEC_NORMAL; //AEC_REVERSEOFF;
		printf( "\033[%dm",  printAEC);
	}
}




static void screenprint_setnormal()
{
	if(screenprintmode == SCREENPRINT_NCURSES)
	{
		//attron(A_REVERSE);
	}
	else
	{
		printAEC = AEC_NORMAL;
		printAECfgcolor = AEC_FGCOLOR_WHITE;
		printAECbgcolor = AEC_BGCOLOR_BLACK;
		printf( "\033[%d;%d;%dm", printAEC, printAECfgcolor, printAECbgcolor );
	}
}












static errno_t function_parameter__print_header(const char *str, char c)
{
    long n;
    long i;

    screenprint_setbold();
    n = strlen(str);
    for(i = 0; i < (wcol - n) / 2; i++)
    {
        printfw("%c", c);
    }
    printfw("%s", str);
    for(i = 0; i < (wcol - n) / 2 - 1; i++)
    {
        printfw("%c", c);
    }
    printfw("\n");
    screenprint_unsetbold();

    return RETURN_SUCCESS;
}




errno_t function_parameter_struct_shmdirname(char *shmdname)
{
    int shmdirOK = 0;
    DIR *tmpdir;
    static unsigned long functioncnt = 0;
    static char shmdname_static[STRINGMAXLEN_SHMDIRNAME];

    if(functioncnt == 0)
    {
        functioncnt++; // ensure we only run this once, and then retrieve stored result from shmdname_static

        // first, we try the env variable if it exists
        char *MILK_SHM_DIR = getenv("MILK_SHM_DIR");
        if(MILK_SHM_DIR != NULL)
        {
            printf(" [ MILK_SHM_DIR ] is '%s'\n", MILK_SHM_DIR);

            {
                int slen = snprintf(shmdname, STRINGMAXLEN_SHMDIRNAME, "%s", MILK_SHM_DIR);
                if(slen < 1)
                {
                    PRINT_ERROR("snprintf wrote <1 char");
                    abort(); // can't handle this error any other way
                }
                if(slen >= STRINGMAXLEN_SHMDIRNAME)
                {
                    PRINT_ERROR("snprintf string truncation");
                    abort(); // can't handle this error any other way
                }
            }

            // does this direcory exist ?
            tmpdir = opendir(shmdname);
            if(tmpdir) // directory exits
            {
                shmdirOK = 1;
                closedir(tmpdir);
            }
            else
            {
                abort();
            }
        }

        // second, we try SHAREDSHMDIR default
        if(shmdirOK == 0)
        {
            tmpdir = opendir(SHAREDSHMDIR);
            if(tmpdir) // directory exits
            {
                {
                    int slen = snprintf(shmdname, STRINGMAXLEN_SHMDIRNAME, "%s", SHAREDSHMDIR);
                    if(slen < 1)
                    {
                        PRINT_ERROR("snprintf wrote <1 char");
                        abort(); // can't handle this error any other way
                    }
                    if(slen >= STRINGMAXLEN_SHMDIRNAME)
                    {
                        PRINT_ERROR("snprintf string truncation");
                        abort(); // can't handle this error any other way
                    }
                }


                shmdirOK = 1;
                closedir(tmpdir);
            }
        }

        // if all above fails, set to /tmp
        if(shmdirOK == 0)
        {
            tmpdir = opendir("/tmp");
            if(!tmpdir)
            {
                exit(EXIT_FAILURE);
            }
            else
            {
                sprintf(shmdname, "/tmp");
                shmdirOK = 1;
                closedir(tmpdir);
            }
        }


        {
            int slen = snprintf(shmdname_static, STRINGMAXLEN_SHMDIRNAME, "%s",
                                shmdname); // keep it memory
            if(slen < 1)
            {
                PRINT_ERROR("snprintf wrote <1 char");
                abort(); // can't handle this error any other way
            }
            if(slen >= STRINGMAXLEN_SHMDIRNAME)
            {
                PRINT_ERROR("snprintf string truncation");
                abort(); // can't handle this error any other way
            }
        }
    }
    else
    {
        {
            int slen = snprintf(shmdname, STRINGMAXLEN_SHMDIRNAME, "%s", shmdname_static);
            if(slen < 1)
            {
                PRINT_ERROR("snprintf wrote <1 char");
                abort(); // can't handle this error any other way
            }
            if(slen >= STRINGMAXLEN_SHMDIRNAME)
            {
                PRINT_ERROR("snprintf string truncation");
                abort(); // can't handle this error any other way
            }
        }

    }

    return RETURN_SUCCESS;
}






/** @brief Get FPS log filename
 * 
 * logfname should be char [STRINGMAXLEN_FULLFILENAME]
 * 
 */
static errno_t getFPSlogfname(char *logfname)
{
	char shmdname[STRINGMAXLEN_SHMDIRNAME];
    function_parameter_struct_shmdirname(shmdname);   
    
    WRITE_FULLFILENAME(logfname, "%s/fpslog.%ld.%07d.%s", shmdname, data.FPS_TIMESTAMP, getpid(), data.FPS_PROCESS_TYPE);
	
	return RETURN_SUCCESS;
}

















/**
 *
 * ## Purpose
 *
 * Construct FPS name and set FPSCMDCODE from command line function call
 *
 *
 */

errno_t function_parameter_getFPSname_from_CLIfunc(
    char     *fpsname_default
)
{

#ifndef STANDALONE
    // Check if function will be executed through FPS interface
    // set to 0 as default (no FPS)
    data.FPS_CMDCODE = 0;

    // if using FPS implementation, FPSCMDCODE will be set to != 0
    if(CLI_checkarg(1, CLIARG_STR) == 0)
    {
        // check that first arg is a string
        // if it isn't, the non-FPS implementation should be called

        // check if recognized FPSCMDCODE
        if(strcmp(data.cmdargtoken[1].val.string,
                  "_FPSINIT_") == 0)    // Initialize FPS
        {
            data.FPS_CMDCODE = FPSCMDCODE_FPSINIT;
        }
        else if(strcmp(data.cmdargtoken[1].val.string,
                       "_CONFSTART_") == 0)     // Start conf process
        {
            data.FPS_CMDCODE = FPSCMDCODE_CONFSTART;
        }
        else if(strcmp(data.cmdargtoken[1].val.string,
                       "_CONFSTOP_") == 0)   // Stop conf process
        {
            data.FPS_CMDCODE = FPSCMDCODE_CONFSTOP;
        }
        else if(strcmp(data.cmdargtoken[1].val.string,
                       "_RUNSTART_") == 0)   // Run process
        {
            data.FPS_CMDCODE = FPSCMDCODE_RUNSTART;
        }
        else if(strcmp(data.cmdargtoken[1].val.string,
                       "_RUNSTOP_") == 0)   // Stop process
        {
            data.FPS_CMDCODE = FPSCMDCODE_RUNSTOP;
        }
    }


    // if recognized FPSCMDCODE, use FPS implementation
    if(data.FPS_CMDCODE != 0)
    {
        // ===============================
        //     SET FPS INTERFACE NAME
        // ===============================

        // if main CLI process has been named with -n option, then use the process name to construct fpsname
        if(data.processnameflag == 1)
        {
            // Automatically set fps name to be process name up to first instance of character '.'
            strcpy(data.FPS_name, data.processname0);
        }
        else   // otherwise, construct name as follows
        {
            // Adopt default name for fpsname
            int slen = snprintf(data.FPS_name, FUNCTION_PARAMETER_STRMAXLEN, "%s",
                                fpsname_default);
            if(slen < 1)
            {
                PRINT_ERROR("snprintf wrote <1 char");
                abort(); // can't handle this error any other way
            }
            if(slen >= FUNCTION_PARAMETER_STRMAXLEN)
            {
                PRINT_ERROR("snprintf string truncation.\n"
                            "Full string  : %s\n"
                            "Truncated to : %s",
                            fpsname_default,
                            data.FPS_name);
                abort(); // can't handle this error any other way
            }


            // By convention, if there are optional arguments,
            // they should be appended to the default fps name
            //
            int argindex = 2; // start at arg #2
            while(strlen(data.cmdargtoken[argindex].val.string) > 0)
            {
                char fpsname1[FUNCTION_PARAMETER_STRMAXLEN];

                int slen = snprintf(fpsname1, FUNCTION_PARAMETER_STRMAXLEN,
                                    "%s-%s", data.FPS_name, data.cmdargtoken[argindex].val.string);
                if(slen < 1)
                {
                    PRINT_ERROR("snprintf wrote <1 char");
                    abort(); // can't handle this error any other way
                }
                if(slen >= FUNCTION_PARAMETER_STRMAXLEN)
                {
                    PRINT_ERROR("snprintf string truncation.\n"
                                "Full string  : %s-%s\n"
                                "Truncated to : %s",
                                data.FPS_name,
                                data.cmdargtoken[argindex].val.string,
                                fpsname1);
                    abort(); // can't handle this error any other way
                }

                strncpy(data.FPS_name, fpsname1, FUNCTION_PARAMETER_STRMAXLEN);
                argindex ++;
            }
        }
    }

#endif
    return RETURN_SUCCESS;
}




errno_t function_parameter_execFPScmd()
{
#ifndef STANDALONE
    if(data.FPS_CMDCODE == FPSCMDCODE_FPSINIT)   // Initialize FPS
    {
        data.FPS_CONFfunc(); // call conf function
        return RETURN_SUCCESS;
    }

    if(data.FPS_CMDCODE == FPSCMDCODE_CONFSTART)    // Start CONF process
    {
        data.FPS_CONFfunc(); // call conf function
        return RETURN_SUCCESS;
    }

    if(data.FPS_CMDCODE == FPSCMDCODE_CONFSTOP)   // Stop CONF process
    {
        data.FPS_CONFfunc(); // call conf function
        return RETURN_SUCCESS;
    }

    if(data.FPS_CMDCODE == FPSCMDCODE_RUNSTART)   // Start RUN process
    {
        data.FPS_RUNfunc(); // call run function
        return RETURN_SUCCESS;
    }

    if(data.FPS_CMDCODE == FPSCMDCODE_RUNSTOP)   // Stop RUN process
    {
        data.FPS_CONFfunc(); // call conf function
        return RETURN_SUCCESS;
    }
#endif

    return RETURN_SUCCESS;
}











errno_t function_parameter_struct_create(
    int NBparamMAX,
    const char *name
)
{
    int index;
    char *mapv;
    FUNCTION_PARAMETER_STRUCT fps;

    //  FUNCTION_PARAMETER_STRUCT_MD *funcparammd;
    //  FUNCTION_PARAMETER *funcparamarray;

    char SM_fname[200];
    size_t sharedsize = 0; // shared memory size in bytes
    int SM_fd; // shared memory file descriptor

    char shmdname[200];
    function_parameter_struct_shmdirname(shmdname);

    if(snprintf(SM_fname, sizeof(SM_fname), "%s/%s.fps.shm", shmdname, name) < 0)
    {
        PRINT_ERROR("snprintf error");
    }
    remove(SM_fname);

    printf("Creating file %s, holding NBparamMAX = %d\n", SM_fname, NBparamMAX);
    fflush(stdout);

    sharedsize = sizeof(FUNCTION_PARAMETER_STRUCT_MD);
    sharedsize += sizeof(FUNCTION_PARAMETER) * NBparamMAX;

    SM_fd = open(SM_fname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    if(SM_fd == -1)
    {
        perror("Error opening file for writing");
        printf("STEP %s %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(0);
    }

    fps.SMfd = SM_fd;

    int result;
    result = lseek(SM_fd, sharedsize - 1, SEEK_SET);
    if(result == -1)
    {
        close(SM_fd);
        printf("ERROR [%s %s %d]: Error calling lseek() to 'stretch' the file\n",
               __FILE__, __func__, __LINE__);
        printf("STEP %s %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(0);
    }

    result = write(SM_fd, "", 1);
    if(result != 1)
    {
        close(SM_fd);
        perror("Error writing last byte of the file");
        printf("STEP %s %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(0);
    }

    fps.md = (FUNCTION_PARAMETER_STRUCT_MD *) mmap(0, sharedsize,
             PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
    if(fps.md == MAP_FAILED)
    {
        close(SM_fd);
        perror("Error mmapping the file");
        printf("STEP %s %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(0);
    }
    //funcparamstruct->md = funcparammd;

    mapv = (char *) fps.md;
    mapv += sizeof(FUNCTION_PARAMETER_STRUCT_MD);
    fps.parray = (FUNCTION_PARAMETER *) mapv;



    printf("shared memory space = %ld bytes\n", sharedsize); //TEST


    fps.md->NBparamMAX = NBparamMAX;

    for(index = 0; index < NBparamMAX; index++)
    {
        fps.parray[index].fpflag = 0; // not active
        fps.parray[index].cnt0 = 0;   // update counter
    }


    strcpy(fps.md->name, name);



    char cwd[FPS_CWD_STRLENMAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL)
    {
        strncpy(fps.md->fpsdirectory, cwd, FPS_CWD_STRLENMAX);
    }
    else
    {
        perror("getcwd() error");
        return 1;
    }

    strncpy(fps.md->sourcefname, "NULL", FPS_SRCDIR_STRLENMAX);
    fps.md->sourceline = 0;



    fps.md->signal     = (uint64_t) FUNCTION_PARAMETER_STRUCT_SIGNAL_CONFRUN;
    fps.md->confwaitus = (uint64_t) 1000; // 1 kHz default
    fps.md->msgcnt = 0;

    munmap(fps.md, sharedsize);


    return EXIT_SUCCESS;
}





/**
 *
 * ## Purpose
 *
 * Connect to function parameter structure
 *
 *
 * ## Arguments
 *
 * fpsconnectmode can take following value
 *
 * FPSCONNECT_SIMPLE : simple connect, don't try load streams
 * FPSCONNECT_CONF   : connect as CONF process
 * FPSCONNECT_RUN    : connect as RUN process
 *
 */


long function_parameter_struct_connect(
    const char *name,
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsconnectmode
)
{
    int stringmaxlen = 500;
    char SM_fname[stringmaxlen];
    int SM_fd; // shared memory file descriptor
    long NBparamMAX;
    //    long NBparamActive;
    char *mapv;

    char shmdname[stringmaxlen];

    if(fps->SMfd > -1)
    {
        printf("[%s %s %d] ERROR: file descriptor already allocated : %d\n", __FILE__,
               __func__, __LINE__, fps->SMfd);
        //	exit(0);
    }


    function_parameter_struct_shmdirname(shmdname);

    if(snprintf(SM_fname, sizeof(SM_fname), "%s/%s.fps.shm", shmdname, name) < 0)
    {
        PRINT_ERROR("snprintf error");
    }
    printf("File : %s\n", SM_fname);
    SM_fd = open(SM_fname, O_RDWR);
    if(SM_fd == -1)
    {
        printf("ERROR [%s %s %d]: cannot connect to %s\n", __FILE__, __func__, __LINE__,
               SM_fname);
        return(-1);
    }
    else
    {
        fps->SMfd = SM_fd;
    }


    struct stat file_stat;
    fstat(SM_fd, &file_stat);


    fps->md = (FUNCTION_PARAMETER_STRUCT_MD *) mmap(0, file_stat.st_size,
              PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
    if(fps->md == MAP_FAILED)
    {
        close(SM_fd);
        perror("Error mmapping the file");
        printf("STEP %s %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    if(fpsconnectmode == FPSCONNECT_CONF)
    {
        fps->md->confpid = getpid();    // write process PID into FPS
    }

    if(fpsconnectmode == FPSCONNECT_RUN)
    {
        fps->md->runpid = getpid();    // write process PID into FPS
    }

    mapv = (char *) fps->md;
    mapv += sizeof(FUNCTION_PARAMETER_STRUCT_MD);
    fps->parray = (FUNCTION_PARAMETER *) mapv;

    //	NBparam = (int) (file_stat.st_size / sizeof(FUNCTION_PARAMETER));
    NBparamMAX = fps->md->NBparamMAX;
    printf("[%s %5d] Connected to %s, %ld entries\n", __FILE__, __LINE__, SM_fname,
           NBparamMAX);
    fflush(stdout);


    // decompose full name into pname and indices
    int NBi = 0;
    char tmpstring[stringmaxlen];
    char tmpstring1[stringmaxlen];
    char *pch;


    strncpy(tmpstring, name, stringmaxlen);
    NBi = -1;
    pch = strtok(tmpstring, "-");
    while(pch != NULL)
    {
        strncpy(tmpstring1, pch, stringmaxlen);

        if(NBi == -1)
        {
            //            strncpy(fps->md->pname, tmpstring1, stringmaxlen);
            if(snprintf(fps->md->pname, FPS_PNAME_STRMAXLEN, "%s", tmpstring1) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
        }

        if((NBi >= 0) && (NBi < 10))
        {
            if(snprintf(fps->md->nameindexW[NBi], 16, "%s", tmpstring1) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            //strncpy(fps->md->nameindexW[NBi], tmpstring1, 16);
        }

        NBi++;
        pch = strtok(NULL, "-");
    }


    fps->md->NBnameindex = NBi;
    function_parameter_printlist(fps->parray, NBparamMAX);


    if((fpsconnectmode == FPSCONNECT_CONF) || (fpsconnectmode == FPSCONNECT_RUN))
    {
        // load streams
        int pindex;
        for(pindex = 0; pindex < NBparamMAX; pindex++)
        {
            if((fps->parray[pindex].fpflag & FPFLAG_ACTIVE)
                    && (fps->parray[pindex].fpflag & FPFLAG_USED)
                    && (fps->parray[pindex].type & FPTYPE_STREAMNAME))
            {
                functionparameter_LoadStream(fps, pindex, fpsconnectmode);
            }
        }
    }

    return(NBparamMAX);
}





int function_parameter_struct_disconnect(FUNCTION_PARAMETER_STRUCT
        *funcparamstruct)
{
    int NBparamMAX;

    NBparamMAX = funcparamstruct->md->NBparamMAX;
    //funcparamstruct->md->NBparam = 0;
    funcparamstruct->parray = NULL;
    munmap(funcparamstruct->md,
           sizeof(FUNCTION_PARAMETER_STRUCT_MD) + sizeof(FUNCTION_PARAMETER)*NBparamMAX);
    close(funcparamstruct->SMfd);
    funcparamstruct->SMfd = -1;

    return(0);
}






//
// stand-alone function to set parameter value
//
int function_parameter_SetValue_int64(char *keywordfull, long val)
{
    FUNCTION_PARAMETER_STRUCT fps;
    char tmpstring[FUNCTION_PARAMETER_KEYWORD_STRMAXLEN *
                                                        FUNCTION_PARAMETER_KEYWORD_MAXLEVEL];
    char keyword[FUNCTION_PARAMETER_KEYWORD_MAXLEVEL][FUNCTION_PARAMETER_KEYWORD_STRMAXLEN];
    int keywordlevel = 0;
    char *pch;


    // break full keyword into keywords
    strncpy(tmpstring, keywordfull,
            FUNCTION_PARAMETER_KEYWORD_STRMAXLEN * FUNCTION_PARAMETER_KEYWORD_MAXLEVEL);
    keywordlevel = 0;
    pch = strtok(tmpstring, ".");
    while(pch != NULL)
    {
        strncpy(keyword[keywordlevel], pch, FUNCTION_PARAMETER_KEYWORD_STRMAXLEN);
        keywordlevel++;
        pch = strtok(NULL, ".");
    }

    function_parameter_struct_connect(keyword[9], &fps, FPSCONNECT_SIMPLE);

    int pindex = functionparameter_GetParamIndex(&fps, keywordfull);


    fps.parray[pindex].val.l[0] = val;

    function_parameter_struct_disconnect(&fps);

    return EXIT_SUCCESS;
}









int function_parameter_printlist(
    FUNCTION_PARAMETER  *funcparamarray,
    long NBparamMAX
)
{
    long pindex = 0;
    long pcnt = 0;

    printf("\n");
    for(pindex = 0; pindex < NBparamMAX; pindex++)
    {
        if(funcparamarray[pindex].fpflag & FPFLAG_ACTIVE)
        {
            printf("Parameter %4ld : %s\n", pindex, funcparamarray[pindex].keywordfull);
            /*for(int kl=0; kl< funcparamarray[pindex].keywordlevel; kl++)
            	printf("  %s", funcparamarray[pindex].keyword[kl]);
            printf("\n");*/
            printf("    %s\n", funcparamarray[pindex].description);

            // STATUS FLAGS
            printf("    STATUS FLAGS (0x%02hhx) :", (int) funcparamarray[pindex].fpflag);
            if(funcparamarray[pindex].fpflag & FPFLAG_ACTIVE)
            {
                printf(" ACTIVE");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_USED)
            {
                printf(" USED");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_VISIBLE)
            {
                printf(" VISIBLE");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_WRITE)
            {
                printf(" WRITE");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_WRITECONF)
            {
                printf(" WRITECONF");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_WRITERUN)
            {
                printf(" WRITERUN");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_LOG)
            {
                printf(" LOG");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_SAVEONCHANGE)
            {
                printf(" SAVEONCHANGE");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_SAVEONCLOSE)
            {
                printf(" SAVEONCLOSE");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_MINLIMIT)
            {
                printf(" MINLIMIT");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_MAXLIMIT)
            {
                printf(" MAXLIMIT");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_CHECKSTREAM)
            {
                printf(" CHECKSTREAM");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_IMPORTED)
            {
                printf(" IMPORTED");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_FEEDBACK)
            {
                printf(" FEEDBACK");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_ERROR)
            {
                printf(" ERROR");
            }
            if(funcparamarray[pindex].fpflag & FPFLAG_ONOFF)
            {
                printf(" ONOFF");
            }
            printf("\n");

            // DATA TYPE
            //			printf("    TYPE : 0x%02hhx\n", (int) funcparamarray[pindex].type);
            if(funcparamarray[pindex].type & FPTYPE_UNDEF)
            {
                printf("    TYPE = UNDEF\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_INT64)
            {
                printf("    TYPE  = INT64\n");
                printf("    VALUE = %ld\n", (long) funcparamarray[pindex].val.l[0]);
            }
            if(funcparamarray[pindex].type & FPTYPE_FLOAT64)
            {
                printf("    TYPE = FLOAT64\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_PID)
            {
                printf("    TYPE = PID\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_TIMESPEC)
            {
                printf("    TYPE = TIMESPEC\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_FILENAME)
            {
                printf("    TYPE = FILENAME\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_DIRNAME)
            {
                printf("    TYPE = DIRNAME\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_STREAMNAME)
            {
                printf("    TYPE = STREAMNAME\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_STRING)
            {
                printf("    TYPE = STRING\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_ONOFF)
            {
                printf("    TYPE = ONOFF\n");
            }
            if(funcparamarray[pindex].type & FPTYPE_FPSNAME)
            {
                printf("    TYPE = FPSNAME\n");
            }

            pcnt ++;
        }
    }
    printf("\n");
    printf("%ld/%ld active parameters\n", pcnt, NBparamMAX);
    printf("\n");

    return 0;
}






int functionparameter_GetFileName(
    FUNCTION_PARAMETER_STRUCT *fps,
    FUNCTION_PARAMETER *fparam,
    char *outfname,
    char *tagname
)
{
    int stringmaxlen = STRINGMAXLEN_DIRNAME/2;
    char ffname[STRINGMAXLEN_FULLFILENAME]; // full filename
    char fname1[stringmaxlen];
    int l;
	char fpsconfdirname[STRINGMAXLEN_DIRNAME];
	
    if(snprintf(fpsconfdirname, stringmaxlen, "%s/fpsconf", fps->md->fpsdirectory) < 0)
    {
        PRINT_ERROR("snprintf error");
    }
    
    EXECUTE_SYSTEM_COMMAND("mkdir -p %s", fpsconfdirname);
    

	// build up directory name
    for(l = 0; l < fparam->keywordlevel - 1; l++)
    {
        if(snprintf(fname1, stringmaxlen, "/%s", fparam->keyword[l]) < 0)
        {
            PRINT_ERROR("snprintf error");
        }
        strncat(fpsconfdirname, fname1, STRINGMAXLEN_DIRNAME-1);
        
        EXECUTE_SYSTEM_COMMAND("mkdir -p %s", fpsconfdirname);
    }

    if(snprintf(fname1, stringmaxlen, "/%s.%s.txt", fparam->keyword[l],
                tagname) < 0)
    {
        PRINT_ERROR("snprintf error");
    }
    
    snprintf(ffname, STRINGMAXLEN_FULLFILENAME, "%s%s", fpsconfdirname, fname1);

    strcpy(outfname, ffname);

    return 0;
}





int functionparameter_GetParamIndex(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char                *paramname
)
{
    long index = -1;
    long pindex = 0;

    long NBparamMAX = fps->md->NBparamMAX;

    int found = 0;
    for(pindex = 0; pindex < NBparamMAX; pindex++)
    {
        if(found == 0)
        {
            if(fps->parray[pindex].fpflag & FPFLAG_ACTIVE)
            {
                if(strstr(fps->parray[pindex].keywordfull, paramname) != NULL)
                {
                    index = pindex;
                    found = 1;
                }
            }
        }
    }

    if(index == -1)
    {
        printf("ERROR: cannot find parameter \"%s\" in structure\n", paramname);
        printf("STEP %s %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(0);
    }

    return index;
}




long functionparameter_GetParamValue_INT64(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    long value;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    value = fps->parray[fpsi].val.l[0];
    fps->parray[fpsi].val.l[3] = value;

    return value;
}


int functionparameter_SetParamValue_INT64(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname,
    long value
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    fps->parray[fpsi].val.l[0] = value;
    fps->parray[fpsi].cnt0++;

    return EXIT_SUCCESS;
}


long *functionparameter_GetParamPtr_INT64(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    long *ptr;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    ptr = &fps->parray[fpsi].val.l[0];

    return ptr;
}





double functionparameter_GetParamValue_FLOAT64(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    double value;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    value = fps->parray[fpsi].val.f[0];
    fps->parray[fpsi].val.f[3] = value;

    return value;
}

int functionparameter_SetParamValue_FLOAT64(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname,
    double value
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    fps->parray[fpsi].val.f[0] = value;
    fps->parray[fpsi].cnt0++;

    return EXIT_SUCCESS;
}

double *functionparameter_GetParamPtr_FLOAT64(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    double *ptr;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    ptr = &fps->parray[fpsi].val.f[0];

    return ptr;
}


float functionparameter_GetParamValue_FLOAT32(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    float value;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    value = fps->parray[fpsi].val.s[0];
    fps->parray[fpsi].val.s[3] = value;

    return value;
}

int functionparameter_SetParamValue_FLOAT32(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname,
    float value
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    fps->parray[fpsi].val.s[0] = value;
    fps->parray[fpsi].cnt0++;

    return EXIT_SUCCESS;
}

float *functionparameter_GetParamPtr_FLOAT32(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    float *ptr;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    ptr = &fps->parray[fpsi].val.s[0];

    return ptr;
}



char *functionparameter_GetParamPtr_STRING(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    return fps->parray[fpsi].val.string[0];
}

int functionparameter_SetParamValue_STRING(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname,
    const char *stringvalue
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);

    strncpy(fps->parray[fpsi].val.string[0], stringvalue,
            FUNCTION_PARAMETER_STRMAXLEN);
    fps->parray[fpsi].cnt0++;

    return EXIT_SUCCESS;
}




int functionparameter_GetParamValue_ONOFF(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);

    if(fps->parray[fpsi].fpflag & FPFLAG_ONOFF)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}



int functionparameter_SetParamValue_ONOFF(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname,
    int ONOFFvalue
)
{
    int fpsi = functionparameter_GetParamIndex(fps, paramname);

    if(ONOFFvalue == 1)
    {
        fps->parray[fpsi].fpflag |= FPFLAG_ONOFF;
        fps->parray[fpsi].val.l[0] = 1;
    }
    else
    {
        fps->parray[fpsi].fpflag &= ~FPFLAG_ONOFF;
        fps->parray[fpsi].val.l[0] = 0;
    }

    fps->parray[fpsi].cnt0++;

    return EXIT_SUCCESS;
}



uint64_t *functionparameter_GetParamPtr_fpflag(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char *paramname
)
{
    uint64_t *ptr;

    int fpsi = functionparameter_GetParamIndex(fps, paramname);
    ptr = &fps->parray[fpsi].fpflag;

    return ptr;
}








imageID functionparameter_LoadStream(
    FUNCTION_PARAMETER_STRUCT *fps,
    int                        pindex,
    int                        fpsconnectmode
)
{
    imageID ID = -1;
    uint32_t     imLOC;


#ifdef STANDALONE
    printf("====================== Not working in standalone mode \n");
#else
    printf("====================== Loading stream \"%s\" = %s\n",
           fps->parray[pindex].keywordfull, fps->parray[pindex].val.string[0]);
    ID = COREMOD_IOFITS_LoadMemStream(fps->parray[pindex].val.string[0],
                                      &(fps->parray[pindex].fpflag), &imLOC);


    if(fpsconnectmode == FPSCONNECT_CONF)
    {
        if(fps->parray[pindex].fpflag & FPFLAG_STREAM_CONF_REQUIRED)
        {
            printf("    FPFLAG_STREAM_CONF_REQUIRED\n");
            if(ID == -1)
            {
                printf("FAILURE: Required stream %s could not be loaded\n",
                       fps->parray[pindex].val.string[0]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if(fpsconnectmode == FPSCONNECT_RUN)
    {
        if(fps->parray[pindex].fpflag & FPFLAG_STREAM_RUN_REQUIRED)
        {
            printf("    FPFLAG_STREAM_RUN_REQUIRED\n");
            if(ID == -1)
            {
                printf("FAILURE: Required stream %s could not be loaded\n",
                       fps->parray[pindex].val.string[0]);
                exit(EXIT_FAILURE);
            }
        }
    }
#endif


    // TODO: Add testing for fps



    return ID;
}





/**
 * ## Purpose
 *
 * Add parameter to database with default settings
 *
 * If entry already exists, do not modify it
 *
 */

int function_parameter_add_entry(
    FUNCTION_PARAMETER_STRUCT *fps,
    const char                *keywordstring,
    const char                *descriptionstring,
    uint64_t             type,
    uint64_t             fpflag,
    void                *valueptr
)
{
    long pindex = 0;
    char *pch;
    char tmpstring[FUNCTION_PARAMETER_KEYWORD_STRMAXLEN *
                                                        FUNCTION_PARAMETER_KEYWORD_MAXLEVEL];
    FUNCTION_PARAMETER *funcparamarray;

    funcparamarray = fps->parray;

    long NBparamMAX = -1;

    NBparamMAX = fps->md->NBparamMAX;





    // process keywordstring
    // if string starts with ".", insert fps name
    char keywordstringC[FUNCTION_PARAMETER_KEYWORD_STRMAXLEN *
                                                             FUNCTION_PARAMETER_KEYWORD_MAXLEVEL];
    if(keywordstring[0] == '.')
    {
        //printf("--------------- keywstring \"%s\" starts with dot -> adding \"%s\"\n", keywordstring, fps->md->name);
        sprintf(keywordstringC, "%s%s", fps->md->name, keywordstring);
    }
    else
    {
        //printf("--------------- keywstring \"%s\" unchanged\n", keywordstring);
        strcpy(keywordstringC, keywordstring);
    }



    // scan for existing keyword
    int scanOK = 0;
    long pindexscan;
    for(pindexscan = 0; pindexscan < NBparamMAX; pindexscan++)
    {
        if(strcmp(keywordstringC, funcparamarray[pindexscan].keywordfull) == 0)
        {
            pindex = pindexscan;
            scanOK = 1;
        }
    }

    if(scanOK == 0) // not found
    {
        // scan for first available entry
        pindex = 0;
        while((funcparamarray[pindex].fpflag & FPFLAG_ACTIVE) && (pindex < NBparamMAX))
        {
            pindex++;
        }

        if(pindex == NBparamMAX)
        {
            printf("ERROR [%s line %d]: NBparamMAX %ld limit reached\n", __FILE__, __LINE__,
                   NBparamMAX);
            fflush(stdout);
            printf("STEP %s %d\n", __FILE__, __LINE__);
            fflush(stdout);
            exit(0);
        }







        funcparamarray[pindex].fpflag = fpflag;



        // break full keyword into keywords
        strncpy(funcparamarray[pindex].keywordfull, keywordstringC,
                FUNCTION_PARAMETER_KEYWORD_STRMAXLEN * FUNCTION_PARAMETER_KEYWORD_MAXLEVEL);
        strncpy(tmpstring, keywordstringC,
                FUNCTION_PARAMETER_KEYWORD_STRMAXLEN * FUNCTION_PARAMETER_KEYWORD_MAXLEVEL);
        funcparamarray[pindex].keywordlevel = 0;
        pch = strtok(tmpstring, ".");
        while(pch != NULL)
        {
            strncpy(funcparamarray[pindex].keyword[funcparamarray[pindex].keywordlevel],
                    pch, FUNCTION_PARAMETER_KEYWORD_STRMAXLEN);
            funcparamarray[pindex].keywordlevel++;
            pch = strtok(NULL, ".");
        }


        // Write description
        strncpy(funcparamarray[pindex].description, descriptionstring,
                FUNCTION_PARAMETER_DESCR_STRMAXLEN);

        // type
        funcparamarray[pindex].type = type;



        // Allocate value
        funcparamarray[pindex].cnt0 = 0; // not allocated

        // Default values
        switch(funcparamarray[pindex].type)
        {
        case FPTYPE_INT64 :
            funcparamarray[pindex].val.l[0] = 0;
            funcparamarray[pindex].val.l[1] = 0;
            funcparamarray[pindex].val.l[2] = 0;
            funcparamarray[pindex].val.l[3] = 0;
            break;

        case FPTYPE_FLOAT64 :
            funcparamarray[pindex].val.f[0] = 0.0;
            funcparamarray[pindex].val.f[1] = 0.0;
            funcparamarray[pindex].val.f[2] = 0.0;
            funcparamarray[pindex].val.f[3] = 0.0;
            break;

        case FPTYPE_FLOAT32 :
            funcparamarray[pindex].val.s[0] = 0.0;
            funcparamarray[pindex].val.s[1] = 0.0;
            funcparamarray[pindex].val.s[2] = 0.0;
            funcparamarray[pindex].val.s[3] = 0.0;
            break;

        case FPTYPE_PID :
            funcparamarray[pindex].val.pid[0] = 0;
            funcparamarray[pindex].val.pid[1] = 0;
            break;

        case FPTYPE_TIMESPEC :
            funcparamarray[pindex].val.ts[0].tv_sec = 0;
            funcparamarray[pindex].val.ts[0].tv_nsec = 0;
            funcparamarray[pindex].val.ts[1].tv_sec = 0;
            funcparamarray[pindex].val.ts[1].tv_nsec = 0;
            break;

        case FPTYPE_FILENAME :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_FITSFILENAME :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_EXECFILENAME :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_DIRNAME :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_STREAMNAME :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_STRING :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_ONOFF :
            funcparamarray[pindex].fpflag &= ~FPFLAG_ONOFF; // initialize state to OFF
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "OFF state") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        " ON state") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;

        case FPTYPE_FPSNAME :
            if(snprintf(funcparamarray[pindex].val.string[0], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            if(snprintf(funcparamarray[pindex].val.string[1], FUNCTION_PARAMETER_STRMAXLEN,
                        "NULL") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            break;
        }



        if(valueptr != NULL)  // allocate value requested by function call
        {
            int64_t *valueptr_INT64;
            double *valueptr_FLOAT64;
            float *valueptr_FLOAT32;
            struct timespec *valueptr_ts;

            switch(funcparamarray[pindex].type)
            {

            case FPTYPE_INT64 :
                valueptr_INT64 = (int64_t *) valueptr;
                funcparamarray[pindex].val.l[0] = valueptr_INT64[0];
                funcparamarray[pindex].val.l[1] = valueptr_INT64[1];
                funcparamarray[pindex].val.l[2] = valueptr_INT64[2];
                funcparamarray[pindex].val.l[3] = valueptr_INT64[3];
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_FLOAT64 :
                valueptr_FLOAT64 = (double *) valueptr;
                funcparamarray[pindex].val.f[0] = valueptr_FLOAT64[0];
                funcparamarray[pindex].val.f[1] = valueptr_FLOAT64[1];
                funcparamarray[pindex].val.f[2] = valueptr_FLOAT64[2];
                funcparamarray[pindex].val.f[3] = valueptr_FLOAT64[3];
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_FLOAT32 :
                valueptr_FLOAT32 = (float *) valueptr;
                funcparamarray[pindex].val.s[0] = valueptr_FLOAT32[0];
                funcparamarray[pindex].val.s[1] = valueptr_FLOAT32[1];
                funcparamarray[pindex].val.s[2] = valueptr_FLOAT32[2];
                funcparamarray[pindex].val.s[3] = valueptr_FLOAT32[3];
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_PID :
                valueptr_INT64 = (int64_t *) valueptr;
                funcparamarray[pindex].val.pid[0] = (pid_t)(*valueptr_INT64);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_TIMESPEC:
                valueptr_ts = (struct timespec *) valueptr;
                funcparamarray[pindex].val.ts[0] = *valueptr_ts;
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_FILENAME :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_FITSFILENAME :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_EXECFILENAME :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_DIRNAME :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_STREAMNAME :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_STRING :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;

            case FPTYPE_ONOFF : // already allocated through the status flag
                break;

            case FPTYPE_FPSNAME :
                strncpy(funcparamarray[pindex].val.string[0], (char *) valueptr,
                        FUNCTION_PARAMETER_STRMAXLEN);
                funcparamarray[pindex].cnt0++;
                break;
            }

            // RVAL = 2;  // default value entered
        }

    }





    /*

    	// READING PARAMETER FROM DISK


        // attempt to read value for filesystem
        char fname[200];
        FILE *fp;
        long tmpl;

        int RVAL = 0;
        // 0: parameter initialized to default value
        // 1: initialized using file value (read from disk)
        // 2: initialized to function argument value


        int index;
        // index = 0  : setval
        // index = 1  : minval
        // index = 2  : maxval


        for(index = 0; index < 3; index++)
        {
            switch(index)
            {
                case 0 :
                    functionparameter_GetFileName(fps, &funcparamarray[pindex], fname, "setval");
                    break;

                case 1 :
                    functionparameter_GetFileName(fps, &funcparamarray[pindex], fname, "minval");
                    break;

                case 2 :
                    functionparameter_GetFileName(fps, &funcparamarray[pindex], fname, "maxval");
                    break;

            }


            if((fp = fopen(fname, "r")) != NULL)
            {
                switch(funcparamarray[pindex].type)
                {

                    case FPTYPE_INT64 :
                        if(fscanf(fp, "%ld", &funcparamarray[pindex].val.l[index]) == 1)
                            if(index ==
                                    0)     // return value is set by setval, cnt0 tracks updates to setval, not to minval or maxval
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        break;

                    case FPTYPE_FLOAT64 :
                        if(fscanf(fp, "%lf", &funcparamarray[pindex].val.f[index]) == 1)
                            if(index == 0)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        break;

                    case FPTYPE_FLOAT32 :
                        if(fscanf(fp, "%f", &funcparamarray[pindex].val.s[index]) == 1)
                            if(index == 0)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        break;

                    case FPTYPE_PID :
                        if(index == 0) // PID does not have min / max
                        {
                            if(fscanf(fp, "%d", &funcparamarray[pindex].val.pid[index]) == 1)
                            {
                                RVAL = 1;
                            }
                            funcparamarray[pindex].cnt0++;
                        }
                        break;

                    case FPTYPE_TIMESPEC :
                        if(fscanf(fp, "%ld %ld", &funcparamarray[pindex].val.ts[index].tv_sec,
                                  &funcparamarray[pindex].val.ts[index].tv_nsec) == 2)
                            if(index == 0)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        break;

                    case FPTYPE_FILENAME :
                        if(index == 0)    // FILENAME does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;

                    case FPTYPE_FITSFILENAME :
                        if(index == 0)    // FITSFILENAME does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;

                    case FPTYPE_EXECFILENAME :
                        if(index == 0)    // EXECFILENAME does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;


                    case FPTYPE_DIRNAME :
                        if(index == 0)    // DIRNAME does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;

                    case FPTYPE_STREAMNAME :
                        if(index == 0)    // STREAMNAME does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;

                    case FPTYPE_STRING :
                        if(index == 0)    // STRING does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;

                    case FPTYPE_ONOFF :
                        if(index == 0)
                        {
                            if(fscanf(fp, "%ld", &tmpl) == 1)
                            {
                                if(tmpl == 1)
                                {
                                    funcparamarray[pindex].fpflag |= FPFLAG_ONOFF;
                                }
                                else
                                {
                                    funcparamarray[pindex].fpflag &= ~FPFLAG_ONOFF;
                                }

                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;


                    case FPTYPE_FPSNAME :
                        if(index == 0)    // FPSNAME does not have min / max
                        {
                            if(fscanf(fp, "%s", funcparamarray[pindex].val.string[0]) == 1)
                            {
                                RVAL = 1;
                                funcparamarray[pindex].cnt0++;
                            }
                        }
                        break;

                }
                fclose(fp);


            }

        }




    	// WRITING PARAMETER TO DISK
    	//

        if(RVAL == 0)
        {
            functionparameter_WriteParameterToDisk(fps, pindex, "setval",
                                                   "AddEntry created");
            if(funcparamarray[pindex].fpflag |= FPFLAG_MINLIMIT)
            {
                functionparameter_WriteParameterToDisk(fps, pindex, "minval",
                                                       "AddEntry created");
            }
            if(funcparamarray[pindex].fpflag |= FPFLAG_MAXLIMIT)
            {
                functionparameter_WriteParameterToDisk(fps, pindex, "maxval",
                                                       "AddEntry created");
            }
        }

        if(RVAL == 2)
        {
            functionparameter_WriteParameterToDisk(fps, pindex, "setval",
                                                   "AddEntry argument");
            if(funcparamarray[pindex].fpflag |= FPFLAG_MINLIMIT)
            {
                functionparameter_WriteParameterToDisk(fps, pindex, "minval",
                                                       "AddEntry argument");
            }
            if(funcparamarray[pindex].fpflag |= FPFLAG_MAXLIMIT)
            {
                functionparameter_WriteParameterToDisk(fps, pindex, "maxval",
                                                       "AddEntry argument");
            }
        }

        if(RVAL != 0)
        {
            functionparameter_WriteParameterToDisk(fps, pindex, "fpsname", "AddEntry");
            functionparameter_WriteParameterToDisk(fps, pindex, "fpsdir", "AddEntry");
            functionparameter_WriteParameterToDisk(fps, pindex, "status", "AddEntry");
        }
    */

    return pindex;
}









// ======================================== LOOP MANAGEMENT FUNCTIONS =======================================

/** @brief FPS config setup
 *
 */
FUNCTION_PARAMETER_STRUCT function_parameter_FPCONFsetup(
    const char *fpsname,
    uint32_t CMDmode
)
{
    long NBparamMAX = FUNCTION_PARAMETER_NBPARAM_DEFAULT;
    uint32_t FPSCONNECTFLAG;

    FUNCTION_PARAMETER_STRUCT fps;

    fps.CMDmode = CMDmode;
    fps.SMfd = -1;


    // record timestamp
    struct timespec tnow;
    clock_gettime(CLOCK_REALTIME, &tnow);
    data.FPS_TIMESTAMP = tnow.tv_sec;

    strcpy(data.FPS_PROCESS_TYPE, "UNDEF");
//	char ptstring[STRINGMAXLEN_FPSPROCESSTYPE];
	
    switch(CMDmode)
    {
        case FPSCMDCODE_CONFSTART:
			snprintf(data.FPS_PROCESS_TYPE, STRINGMAXLEN_FPSPROCESSTYPE, "confstart-%s", fpsname);            
            break;

        case FPSCMDCODE_CONFSTOP:
			snprintf(data.FPS_PROCESS_TYPE, STRINGMAXLEN_FPSPROCESSTYPE, "confstop-%s", fpsname);
            break;

        case FPSCMDCODE_FPSINIT:
            snprintf(data.FPS_PROCESS_TYPE, STRINGMAXLEN_FPSPROCESSTYPE, "fpsinit-%s", fpsname);
            break;

        case FPSCMDCODE_FPSINITCREATE:
            snprintf(data.FPS_PROCESS_TYPE, STRINGMAXLEN_FPSPROCESSTYPE, "fpsinitcreate-%s", fpsname);
            break;

        case FPSCMDCODE_RUNSTART:
			snprintf(data.FPS_PROCESS_TYPE, STRINGMAXLEN_FPSPROCESSTYPE, "runstart-%s", fpsname);
            break;

        case FPSCMDCODE_RUNSTOP:
            snprintf(data.FPS_PROCESS_TYPE, STRINGMAXLEN_FPSPROCESSTYPE, "runstop-%s", fpsname);
            break;
    }




    if(CMDmode & FPSCMDCODE_FPSINITCREATE)   // (re-)create fps even if it exists
    {
        printf("=== FPSINITCREATE NBparamMAX = %ld\n", NBparamMAX);
        function_parameter_struct_create(NBparamMAX, fpsname);
        function_parameter_struct_connect(fpsname, &fps, FPSCONNECT_SIMPLE);
    }
    else     // load existing fps if exists
    {
        printf("=== CHECK IF FPS EXISTS\n");


        FPSCONNECTFLAG = FPSCONNECT_SIMPLE;
        if(CMDmode & FPSCMDCODE_CONFSTART)
        {
            FPSCONNECTFLAG = FPSCONNECT_CONF;
        }

        if(function_parameter_struct_connect(fpsname, &fps, FPSCONNECTFLAG) == -1)
        {
            printf("=== FPS DOES NOT EXISTS -> CREATE\n");
            function_parameter_struct_create(NBparamMAX, fpsname);
            function_parameter_struct_connect(fpsname, &fps, FPSCONNECTFLAG);
        }
        else
        {
            printf("=== FPS EXISTS\n");
        }
    }

    if(CMDmode & FPSCMDCODE_CONFSTOP)   // stop conf
    {
        fps.md->signal &= ~FUNCTION_PARAMETER_STRUCT_SIGNAL_CONFRUN;
        function_parameter_struct_disconnect(&fps);
        fps.localstatus &= ~FPS_LOCALSTATUS_CONFLOOP; // stop loop
    }
    else
    {
        fps.localstatus |= FPS_LOCALSTATUS_CONFLOOP;
    }



    if((CMDmode & FPSCMDCODE_FPSINITCREATE) || (CMDmode & FPSCMDCODE_FPSINIT)
            || (CMDmode & FPSCMDCODE_CONFSTOP))
    {
        fps.localstatus &= ~FPS_LOCALSTATUS_CONFLOOP; // do not start conf
    }

    if(CMDmode & FPSCMDCODE_CONFSTART)
    {
        fps.localstatus |= FPS_LOCALSTATUS_CONFLOOP;
    }


    return fps;
}





uint16_t function_parameter_FPCONFloopstep(
    FUNCTION_PARAMETER_STRUCT *fps
)
{
    static int loopINIT = 0;
    uint16_t updateFLAG = 0;

    static uint32_t prev_status;
    //static uint32_t statuschanged = 0;


    if(loopINIT == 0)
    {
        loopINIT = 1; // update on first loop iteration
        fps->md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE;

        if(fps->CMDmode & FPSCMDCODE_CONFSTART)    // parameter configuration loop
        {
            fps->md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_CONFRUN;
            fps->md->confpid = getpid();
            fps->localstatus |= FPS_LOCALSTATUS_CONFLOOP;
        }
        else
        {
            fps->localstatus &= ~FPS_LOCALSTATUS_CONFLOOP;
        }
    }


    if(fps->md->signal & FUNCTION_PARAMETER_STRUCT_SIGNAL_CONFRUN)
    {
        // Test if CONF process is running
        if((getpgid(fps->md->confpid) >= 0) && (fps->md->confpid > 0))
        {
            fps->md->status |= FUNCTION_PARAMETER_STRUCT_STATUS_CONF;    // running
        }
        else
        {
            fps->md->status &= ~FUNCTION_PARAMETER_STRUCT_STATUS_CONF;    // not running
        }

        // Test if RUN process is running
        if((getpgid(fps->md->runpid) >= 0) && (fps->md->runpid > 0))
        {
            fps->md->status |= FUNCTION_PARAMETER_STRUCT_STATUS_RUN;    // running
        }
        else
        {
            fps->md->status &= ~FUNCTION_PARAMETER_STRUCT_STATUS_RUN;    // not running
        }


        if(prev_status != fps->md->status)
        {
            fps->md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // request an update
        }



        if(fps->md->signal &
                FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE)   // update is required
        {
            updateFLAG = 1;
            fps->md->signal &=
                ~FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // disable update (should be moved to conf process)
        }
        usleep(fps->md->confwaitus);
    }
    else
    {
        fps->localstatus &= ~FPS_LOCALSTATUS_CONFLOOP;
    }



    prev_status = fps->md->status;


    return updateFLAG;
}





uint16_t function_parameter_FPCONFexit(FUNCTION_PARAMETER_STRUCT *fps)
{
    //fps->md->confpid = 0;


    fps->md->status &= ~FUNCTION_PARAMETER_STRUCT_STATUS_CMDCONF;
    function_parameter_struct_disconnect(fps);

    return 0;
}



uint16_t function_parameter_RUNexit(FUNCTION_PARAMETER_STRUCT *fps)
{
    //fps->md->confpid = 0;


    fps->md->status &= ~FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN;
    function_parameter_struct_disconnect(fps);

    return 0;
}






// ======================================== GUI FUNCTIONS =======================================

/** @brief restore terminal settings
 */
static void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}


static errno_t inittermios()
{
	tcgetattr(0, &orig_termios);

    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    //cfmakeraw(&new_termios);
    new_termios.c_lflag &= ~ICANON;
    new_termios.c_lflag &= ~ECHO;
    new_termios.c_lflag &= ~ISIG;
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;

    tcsetattr(0, TCSANOW, &new_termios);

    atexit(reset_terminal_mode);

    return RETURN_SUCCESS;
}


/**
 * @brief INITIALIZE ncurses
 *
 */
static errno_t initncurses()
{
    if(initscr() == NULL)
    {
        fprintf(stderr, "Error initialising ncurses.\n");
        exit(EXIT_FAILURE);
    }
    getmaxyx(stdscr, wrow, wcol);		/* get the number of rows and columns */

    cbreak();
    // disables line buffering and erase/kill character-processing (interrupt and flow control characters are unaffected),
    // making characters typed by the user immediately available to the program

    keypad(stdscr, TRUE);
    // enable F1, F2 etc..

    nodelay(stdscr, TRUE);
    curs_set(0);

    noecho();
    // Don't echo() while we do getch



    //nonl();
    // Do not translates newline into return and line-feed on output


    init_color(COLOR_GREEN, 400, 1000, 400);
    start_color();

    //  colored background
    init_pair(  1, COLOR_BLACK,  COLOR_WHITE  );
    init_pair(  2, COLOR_BLACK,  COLOR_GREEN  );  // all good
    init_pair(  3, COLOR_BLACK,  COLOR_YELLOW ); // parameter out of sync
    init_pair(  4, COLOR_WHITE,  COLOR_RED    );
    init_pair(  5, COLOR_WHITE,  COLOR_BLUE   ); // DIRECTORY
    init_pair(  6, COLOR_GREEN,  COLOR_BLACK  );
    init_pair(  7, COLOR_YELLOW, COLOR_BLACK  );
    init_pair(  8, COLOR_RED,    COLOR_BLACK  );
    init_pair(  9, COLOR_BLACK,  COLOR_RED    );
    init_pair( 10, COLOR_BLACK,  COLOR_CYAN   );


    return RETURN_SUCCESS;
}




static int get_singlechar_nonblock()
{
    int ch = -1;

    if(screenprintmode == SCREENPRINT_NCURSES)
    {
        ch = getch();  // ncurses function, non-blocking
    }
    else
    {
        char buff[3];

        int l = read(STDIN_FILENO, buff, 3);

        if(l>0) {
            ch = buff[0];

            if (buff[0] == 13) // enter
            {
                ch = 10; // new line
            }


            if (buff[0] == 27) { // if the first value is esc


                if(buff[1] == 91) {
                    switch (buff[2])
                    {   // the real value
                    case 'A':
                        ch = KEY_UP; // code for arrow up
                        break;
                    case 'B':
                        ch = KEY_DOWN; // code for arrow down
                        break;
                    case 'C':
                        ch = KEY_RIGHT; // code for arrow right
                        break;
                    case 'D':
                        ch = KEY_LEFT; // code for arrow left
                        break;
                    }
                }


                if(buff[1] == 79)
                {
                    switch (buff[2])
                    {
                    case 80:
                        ch = KEY_F(1);
                        break;
                    case 81:
                        ch = KEY_F(2);
                        break;
                    case 82:
                        ch = KEY_F(3);
                        break;
                    }
                }
            }

        }
    }

    return ch;
}


static int get_singlechar_block()
{
	int ch;
	
    if(screenprintmode == SCREENPRINT_NCURSES)
    {
        ch = getchar();
    }
    else
    {
        int getchardt_us = 100000;

        ch = -1;
        while(ch == -1)
        {
            usleep(getchardt_us); // kHz
            ch = get_singlechar_nonblock();
        }
    }
    return ch;
}








/**
 * ## Purpose
 *
 * Write parameter to disk
 *
 * ## TAG names
 *
 * One of the following:
 * - "setval"  Set value
 * - "fpsname" Name of FPS to which parameter belongs
 * - "fpsdir"  FPS directory
 * - "minval"  Minimum value (if applicable)
 * - "maxval"  Maximum value (if applicable)
 * - "currval" Current value (if applicable)
 *
 *
 */
int functionparameter_WriteParameterToDisk(
    FUNCTION_PARAMETER_STRUCT *fpsentry,
    int pindex,
    char *tagname,
    char *commentstr
)
{
    char fname[200];
    FILE *fp;


    // create time change tag
    pid_t tid;
    tid = syscall(SYS_gettid);

    // Get GMT time
    char timestring[200];
    struct timespec tnow;
    time_t now;

    clock_gettime(CLOCK_REALTIME, &tnow);
    now = tnow.tv_sec;
    struct tm *uttime;
    uttime = gmtime(&now);

    sprintf(timestring, "%04d%02d%02d%02d%02d%02d.%09ld %8ld [%6d %6d] %s",
            1900 + uttime->tm_year, 1 + uttime->tm_mon, uttime->tm_mday, uttime->tm_hour,
            uttime->tm_min,  uttime->tm_sec, tnow.tv_nsec,
            fpsentry->parray[pindex].cnt0, getpid(), (int) tid, commentstr);



    if(strcmp(tagname, "setval") == 0)   // VALUE
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);
        fp = fopen(fname, "w");
        switch(fpsentry->parray[pindex].type)
        {

            case FPTYPE_INT64:
                fprintf(fp, "%10ld  # %s\n", fpsentry->parray[pindex].val.l[0], timestring);
                break;

            case FPTYPE_FLOAT64:
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.f[0], timestring);
                break;

            case FPTYPE_FLOAT32:
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.s[0], timestring);
                break;

            case FPTYPE_PID:
                fprintf(fp, "%18ld  # %s\n", (long) fpsentry->parray[pindex].val.pid[0],
                        timestring);
                break;

            case FPTYPE_TIMESPEC:
                fprintf(fp, "%15ld %09ld  # %s\n",
                        (long) fpsentry->parray[pindex].val.ts[0].tv_sec,
                        (long) fpsentry->parray[pindex].val.ts[0].tv_nsec, timestring);
                break;

            case FPTYPE_FILENAME:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

            case FPTYPE_FITSFILENAME:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

            case FPTYPE_EXECFILENAME:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

            case FPTYPE_DIRNAME:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

            case FPTYPE_STREAMNAME:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

            case FPTYPE_STRING:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

            case FPTYPE_ONOFF:
                if(fpsentry->parray[pindex].fpflag & FPFLAG_ONOFF)
                {
                    fprintf(fp, "1  %10s # %s\n", fpsentry->parray[pindex].val.string[1],
                            timestring);
                }
                else
                {
                    fprintf(fp, "0  %10s # %s\n", fpsentry->parray[pindex].val.string[0],
                            timestring);
                }
                break;

            case FPTYPE_FPSNAME:
                fprintf(fp, "%s  # %s\n", fpsentry->parray[pindex].val.string[0], timestring);
                break;

        }
        fclose(fp);
    }



    if(strcmp(tagname, "minval") == 0)   // MIN VALUE
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);

        switch(fpsentry->parray[pindex].type)
        {

            case FPTYPE_INT64:
                fp = fopen(fname, "w");
                fprintf(fp, "%10ld  # %s\n", fpsentry->parray[pindex].val.l[1], timestring);
                fclose(fp);
                break;

            case FPTYPE_FLOAT64:
                fp = fopen(fname, "w");
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.f[1], timestring);
                fclose(fp);
                break;

            case FPTYPE_FLOAT32:
                fp = fopen(fname, "w");
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.s[1], timestring);
                fclose(fp);
                break;
        }
    }


    if(strcmp(tagname, "maxval") == 0)   // MAX VALUE
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);

        switch(fpsentry->parray[pindex].type)
        {

            case FPTYPE_INT64:
                fp = fopen(fname, "w");
                fprintf(fp, "%10ld  # %s\n", fpsentry->parray[pindex].val.l[2], timestring);
                fclose(fp);
                break;

            case FPTYPE_FLOAT64:
                fp = fopen(fname, "w");
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.f[2], timestring);
                fclose(fp);
                break;

            case FPTYPE_FLOAT32:
                fp = fopen(fname, "w");
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.s[2], timestring);
                fclose(fp);
                break;
        }
    }


    if(strcmp(tagname, "currval") == 0)   // CURRENT VALUE
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);

        switch(fpsentry->parray[pindex].type)
        {

            case FPTYPE_INT64:
                fp = fopen(fname, "w");
                fprintf(fp, "%10ld  # %s\n", fpsentry->parray[pindex].val.l[3], timestring);
                fclose(fp);
                break;

            case FPTYPE_FLOAT64:
                fp = fopen(fname, "w");
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.f[3], timestring);
                fclose(fp);
                break;

            case FPTYPE_FLOAT32:
                fp = fopen(fname, "w");
                fprintf(fp, "%18f  # %s\n", fpsentry->parray[pindex].val.s[3], timestring);
                fclose(fp);
                break;
        }
    }




    if(strcmp(tagname, "fpsname") == 0)   // FPS name
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);
        fp = fopen(fname, "w");
        fprintf(fp, "%10s    # %s\n", fpsentry->md->name, timestring);
        fclose(fp);
    }

    if(strcmp(tagname, "fpsdir") == 0)   // FPS name
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);
        fp = fopen(fname, "w");
        fprintf(fp, "%10s    # %s\n", fpsentry->md->fpsdirectory, timestring);
        fclose(fp);
    }

    if(strcmp(tagname, "status") == 0)   // FPS name
    {
        functionparameter_GetFileName(fpsentry, &(fpsentry->parray[pindex]), fname,
                                      tagname);
        fp = fopen(fname, "w");
        fprintf(fp, "%10ld    # %s\n", fpsentry->parray[pindex].fpflag, timestring);
        fclose(fp);
    }






    return 0;
}










int functionparameter_CheckParameter(
    FUNCTION_PARAMETER_STRUCT *fpsentry,
    int pindex
)
{
    int err = 0;

    // if entry is not active or not used, no error reported
    if((!(fpsentry->parray[pindex].fpflag & FPFLAG_ACTIVE)))
    {
        return 0;
    }
    else
    {
        char msg[STRINGMAXLEN_FPS_LOGMSG];
        SNPRINTF_CHECK(msg, STRINGMAXLEN_FPS_LOGMSG, "%s",
                       fpsentry->parray[pindex].keywordfull);
        functionparameter_outlog("CHECKPARAM", "%s", msg);
    }

    // if entry is not used, no error reported
    if(!(fpsentry->parray[pindex].fpflag & FPFLAG_USED))
    {
        return 0;
    }


    if(fpsentry->parray[pindex].fpflag & FPFLAG_CHECKINIT)
        if(fpsentry->parray[pindex].cnt0 == 0)
        {
            fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
            fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_NOTINITIALIZED |
                    FPS_MSG_FLAG_ERROR;
            if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                        FUNCTION_PARAMETER_STRUCT_MSG_SIZE, "Not initialized") < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            fpsentry->md->msgcnt++;
            fpsentry->md->conferrcnt++;
            err = 1;
        }

    if(err == 0)
    {
        // Check min value
        if(fpsentry->parray[pindex].type == FPTYPE_INT64)
            if(fpsentry->parray[pindex].fpflag & FPFLAG_MINLIMIT)
                if(fpsentry->parray[pindex].val.l[0] < fpsentry->parray[pindex].val.l[1])
                {
                    fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                    fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_BELOWMIN |
                            FPS_MSG_FLAG_ERROR;
                    if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                                FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                                "int64 value %ld below min %ld",
                                fpsentry->parray[pindex].val.l[0],
                                fpsentry->parray[pindex].val.l[1]) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    fpsentry->md->msgcnt++;
                    fpsentry->md->conferrcnt++;
                    err = 1;
                }

        if(fpsentry->parray[pindex].type == FPTYPE_FLOAT64)
            if(fpsentry->parray[pindex].fpflag & FPFLAG_MINLIMIT)
                if(fpsentry->parray[pindex].val.f[0] < fpsentry->parray[pindex].val.f[1])
                {
                    fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                    fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_BELOWMIN |
                            FPS_MSG_FLAG_ERROR;
                    if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                                FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                                "float64 value %lf below min %lf",
                                fpsentry->parray[pindex].val.f[0],
                                fpsentry->parray[pindex].val.f[1]) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    fpsentry->md->msgcnt++;
                    fpsentry->md->conferrcnt++;
                    err = 1;
                }

        if(fpsentry->parray[pindex].type == FPTYPE_FLOAT32)
            if(fpsentry->parray[pindex].fpflag & FPFLAG_MINLIMIT)
                if(fpsentry->parray[pindex].val.s[0] < fpsentry->parray[pindex].val.s[1])
                {
                    fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                    fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_BELOWMIN |
                            FPS_MSG_FLAG_ERROR;
                    if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                                FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                                "float32 value %f below min %f",
                                fpsentry->parray[pindex].val.s[0],
                                fpsentry->parray[pindex].val.s[1]) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    fpsentry->md->msgcnt++;
                    fpsentry->md->conferrcnt++;
                    err = 1;
                }
    }

    if(err == 0)
    {
        // Check max value
        if(fpsentry->parray[pindex].type == FPTYPE_INT64)
            if(fpsentry->parray[pindex].fpflag & FPFLAG_MAXLIMIT)
                if(fpsentry->parray[pindex].val.l[0] > fpsentry->parray[pindex].val.l[2])
                {
                    fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                    fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ABOVEMAX |
                            FPS_MSG_FLAG_ERROR;
                    if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                                FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                                "int64 value %ld above max %ld",
                                fpsentry->parray[pindex].val.l[0],
                                fpsentry->parray[pindex].val.l[2]) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    fpsentry->md->msgcnt++;
                    fpsentry->md->conferrcnt++;
                    err = 1;
                }


        if(fpsentry->parray[pindex].type == FPTYPE_FLOAT64)
            if(fpsentry->parray[pindex].fpflag & FPFLAG_MAXLIMIT)
                if(fpsentry->parray[pindex].val.f[0] > fpsentry->parray[pindex].val.f[2])
                {
                    fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                    fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ABOVEMAX |
                            FPS_MSG_FLAG_ERROR;
                    if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                                FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                                "float64 value %lf above max %lf",
                                fpsentry->parray[pindex].val.f[0],
                                fpsentry->parray[pindex].val.f[2]) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    fpsentry->md->msgcnt++;
                    fpsentry->md->conferrcnt++;
                    err = 1;
                }



        if(fpsentry->parray[pindex].type == FPTYPE_FLOAT32)
            if(fpsentry->parray[pindex].fpflag & FPFLAG_MAXLIMIT)
                if(fpsentry->parray[pindex].val.s[0] > fpsentry->parray[pindex].val.s[2])
                {
                    fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                    fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ABOVEMAX |
                            FPS_MSG_FLAG_ERROR;
                    if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                                FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                                "float32 value %f above max %f",
                                fpsentry->parray[pindex].val.s[0],
                                fpsentry->parray[pindex].val.s[2]) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    fpsentry->md->msgcnt++;
                    fpsentry->md->conferrcnt++;
                    err = 1;
                }
    }


#ifndef STANDALONE
    if(fpsentry->parray[pindex].type == FPTYPE_FILENAME)
    {
        if(fpsentry->parray[pindex].fpflag & FPFLAG_FILE_RUN_REQUIRED)
        {
            if(file_exists(fpsentry->parray[pindex].val.string[0]) == 0)
            {
                fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ERROR;
                if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                            FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                            "File %s does not exist",
                            fpsentry->parray[pindex].val.string[0]) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                fpsentry->md->msgcnt++;
                fpsentry->md->conferrcnt++;
                err = 1;
            }
        }
    }



    if(fpsentry->parray[pindex].type == FPTYPE_FITSFILENAME)
    {
        if(fpsentry->parray[pindex].fpflag & FPFLAG_FILE_RUN_REQUIRED)
        {
            if(is_fits_file(fpsentry->parray[pindex].val.string[0]) == 0)
            {
                fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ERROR;
                if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                            FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                            "FITS file %s does not exist",
                            fpsentry->parray[pindex].val.string[0]) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                fpsentry->md->msgcnt++;
                fpsentry->md->conferrcnt++;
                err = 1;
            }
        }
    }
#endif


    if(fpsentry->parray[pindex].type == FPTYPE_EXECFILENAME)
    {
        if(fpsentry->parray[pindex].fpflag & FPFLAG_FILE_RUN_REQUIRED)
        {
            struct stat sb;
            if(!(stat(fpsentry->parray[pindex].val.string[0], &sb) == 0
                    && sb.st_mode & S_IXUSR))
            {
                fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ERROR;
                if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                            FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                            "File %s cannot be executed",
                            fpsentry->parray[pindex].val.string[0]) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                fpsentry->md->msgcnt++;
                fpsentry->md->conferrcnt++;
                err = 1;
            }
        }
    }



    FUNCTION_PARAMETER_STRUCT fpstest;
    if(fpsentry->parray[pindex].type == FPTYPE_FPSNAME)
    {
        if(fpsentry->parray[pindex].fpflag & FPFLAG_FPS_RUN_REQUIRED)
        {
            long NBparamMAX = function_parameter_struct_connect(
                                  fpsentry->parray[pindex].val.string[0], &fpstest, FPSCONNECT_SIMPLE);
            if(NBparamMAX < 1)
            {
                fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ERROR;
                if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                            FUNCTION_PARAMETER_STRUCT_MSG_SIZE,
                            "FPS %s: no connection",
                            fpsentry->parray[pindex].val.string[0]) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                fpsentry->md->msgcnt++;
                fpsentry->md->conferrcnt++;
                err = 1;
            }
            else
            {
                function_parameter_struct_disconnect(&fpstest);
            }
        }
    }





#ifdef STANDALONE
    printf("====================== Not working in standalone mode \n");
#else
    // STREAM CHECK

    if((fpsentry->parray[pindex].type & FPTYPE_STREAMNAME))
    {

        uint32_t imLOC;
        long ID = COREMOD_IOFITS_LoadMemStream(fpsentry->parray[pindex].val.string[0],
                                               &(fpsentry->parray[pindex].fpflag), &imLOC);
        fpsentry->parray[pindex].info.stream.streamID = ID;

        if(ID > -1)
        {
            fpsentry->parray[pindex].info.stream.stream_sourceLocation = imLOC;
            fpsentry->parray[pindex].info.stream.stream_atype =
                data.image[ID].md[0].datatype;

            fpsentry->parray[pindex].info.stream.stream_naxis[0] =
                data.image[ID].md[0].naxis;
            fpsentry->parray[pindex].info.stream.stream_xsize[0] =
                data.image[ID].md[0].size[0];

            if(fpsentry->parray[pindex].info.stream.stream_naxis[0] > 1)
            {
                fpsentry->parray[pindex].info.stream.stream_ysize[0] =
                    data.image[ID].md[0].size[1];
            }
            else
            {
                fpsentry->parray[pindex].info.stream.stream_ysize[0] = 1;
            }

            if(fpsentry->parray[pindex].info.stream.stream_naxis[0] > 2)
            {
                fpsentry->parray[pindex].info.stream.stream_zsize[0] =
                    data.image[ID].md[0].size[2];
            }
            else
            {
                fpsentry->parray[pindex].info.stream.stream_zsize[0] = 1;
            }
        }



        if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_RUN_REQUIRED)
        {
            char msg[200];
            sprintf(msg, "Loading stream %s", fpsentry->parray[pindex].val.string[0]);
            functionparameter_outlog("LOADMEMSTREAM", "%s", msg);

            if(imLOC == STREAM_LOAD_SOURCE_NOTFOUND)
            {
                fpsentry->md->msgpindex[fpsentry->md->msgcnt] = pindex;
                fpsentry->md->msgcode[fpsentry->md->msgcnt] =  FPS_MSG_FLAG_ERROR;
                if(snprintf(fpsentry->md->message[fpsentry->md->msgcnt],
                            FUNCTION_PARAMETER_STRUCT_MSG_SIZE, "cannot load stream %s",
                            fpsentry->parray[pindex].val.string[0]) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                fpsentry->md->msgcnt++;
                fpsentry->md->conferrcnt++;
                err = 1;
            }
        }
    }
#endif


    if(err == 1)
    {
        fpsentry->parray[pindex].fpflag |= FPFLAG_ERROR;
    }
    else
    {
        fpsentry->parray[pindex].fpflag &= ~FPFLAG_ERROR;
    }



    return err;
}








int functionparameter_CheckParametersAll(
    FUNCTION_PARAMETER_STRUCT *fpsentry
)
{
    long NBparamMAX;
    long pindex;
    int errcnt = 0;

    char msg[200];
    sprintf(msg, "%s", fpsentry->md->name);
    functionparameter_outlog("CHECKPARAMALL", "%s", msg);



    strcpy(fpsentry->md->message[0], "\0");
    NBparamMAX = fpsentry->md->NBparamMAX;

    // Check if Value is OK
    fpsentry->md->msgcnt = 0;
    fpsentry->md->conferrcnt = 0;
    //    printf("Checking %d parameter entries\n", NBparam);
    for(pindex = 0; pindex < NBparamMAX; pindex++)
    {
        errcnt += functionparameter_CheckParameter(fpsentry, pindex);
    }


    // number of configuration errors - should be zero for run process to start
    fpsentry->md->conferrcnt = errcnt;


    if(errcnt == 0)
    {
        fpsentry->md->status |= FUNCTION_PARAMETER_STRUCT_STATUS_CHECKOK;
    }
    else
    {
        fpsentry->md->status &= ~FUNCTION_PARAMETER_STRUCT_STATUS_CHECKOK;
    }


    // compute write status

    for(pindex = 0; pindex < NBparamMAX; pindex++)
    {
        int writeOK; // do we have write permission ?

        // by default, adopt FPFLAG_WRITE flag
        if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITE)
        {
            writeOK = 1;
        }
        else
        {
            writeOK = 0;
        }

        // if CONF running
        if(fpsentry->md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CONF)
        {
            if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITECONF)
            {
                writeOK = 1;
            }
            else
            {
                writeOK = 0;
            }
        }

        // if RUN running
        if(fpsentry->md->status & FUNCTION_PARAMETER_STRUCT_STATUS_RUN)
        {
            if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITERUN)
            {
                writeOK = 1;
            }
            else
            {
                writeOK = 0;
            }
        }

        if(writeOK == 0)
        {
            fpsentry->parray[pindex].fpflag &= ~FPFLAG_WRITESTATUS;
        }
        else
        {
            fpsentry->parray[pindex].fpflag |= FPFLAG_WRITESTATUS;
        }
    }

    fpsentry->md->signal &= ~FUNCTION_PARAMETER_STRUCT_SIGNAL_CHECKED;


    return 0;
}








int functionparameter_ConnectExternalFPS(
    FUNCTION_PARAMETER_STRUCT *FPS,
    int pindex,
    FUNCTION_PARAMETER_STRUCT *FPSext
)
{
    FPS->parray[pindex].info.fps.FPSNBparamMAX = function_parameter_struct_connect(
                FPS->parray[pindex].val.string[0], FPSext, FPSCONNECT_SIMPLE);

    FPS->parray[pindex].info.fps.FPSNBparamActive = 0;
    FPS->parray[pindex].info.fps.FPSNBparamUsed = 0;
    int pindexext;
    for(pindexext = 0; pindexext < FPS->parray[pindex].info.fps.FPSNBparamMAX;
            pindexext++)
    {
        if(FPSext->parray[pindexext].fpflag & FPFLAG_ACTIVE)
        {
            FPS->parray[pindex].info.fps.FPSNBparamActive++;
        }
        if(FPSext->parray[pindexext].fpflag & FPFLAG_USED)
        {
            FPS->parray[pindex].info.fps.FPSNBparamUsed++;
        }
    }

    return 0;
}




errno_t functionparameter_GetTypeString(
    uint32_t type,
    char *typestring
)
{

    sprintf(typestring, " ");

    // using if statements (not switch) to allow for multiple types
    if(type & FPTYPE_UNDEF)
    {
        strcat(typestring, "UNDEF ");
    }
    if(type & FPTYPE_INT64)
    {
        strcat(typestring, "INT64 ");
    }
    if(type & FPTYPE_FLOAT64)
    {
        strcat(typestring, "FLOAT64 ");
    }
    if(type & FPTYPE_FLOAT32)
    {
        strcat(typestring, "FLOAT32 ");
    }
    if(type & FPTYPE_PID)
    {
        strcat(typestring, "PID ");
    }
    if(type & FPTYPE_TIMESPEC)
    {
        strcat(typestring, "TIMESPEC ");
    }
    if(type & FPTYPE_FILENAME)
    {
        strcat(typestring, "FILENAME ");
    }
    if(type & FPTYPE_FITSFILENAME)
    {
        strcat(typestring, "FITSFILENAME ");
    }
    if(type & FPTYPE_EXECFILENAME)
    {
        strcat(typestring, "EXECFILENAME");
    }
    if(type & FPTYPE_DIRNAME)
    {
        strcat(typestring, "DIRNAME");
    }
    if(type & FPTYPE_STREAMNAME)
    {
        strcat(typestring, "STREAMNAME");
    }
    if(type & FPTYPE_STRING)
    {
        strcat(typestring, "STRING ");
    }
    if(type & FPTYPE_ONOFF)
    {
        strcat(typestring, "ONOFF ");
    }
    if(type & FPTYPE_FPSNAME)
    {
        strcat(typestring, "FPSNAME ");
    }

    return RETURN_SUCCESS;
}







errno_t functionparameter_PrintParameterInfo(
    FUNCTION_PARAMETER_STRUCT *fpsentry,
    int pindex
)
{
    printf("%s\n", fpsentry->parray[pindex].description);
    printf("\n");


    printf("------------- FUNCTION PARAMETER STRUCTURE\n");
    printf("FPS name       : %s\n", fpsentry->md->name);
    printf("   %s ", fpsentry->md->pname);
    int i;
    for(i = 0; i < fpsentry->md->NBnameindex; i++)
    {
        printf(" [%s]", fpsentry->md->nameindexW[i]);
    }
    printf("\n\n");

    if(fpsentry->md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CHECKOK)
    {
        printf("[%ld] Scan OK\n", fpsentry->md->msgcnt);
    }
    else
    {
        int msgi;

        printf("%s [%ld] %d ERROR(s)\n", fpsentry->md->name, fpsentry->md->msgcnt,
               fpsentry->md->conferrcnt);
        for(msgi = 0; msgi < fpsentry->md->msgcnt; msgi++)
        {
            printf("%s [%3d] %s\n", fpsentry->md->name, fpsentry->md->msgpindex[msgi],
                   fpsentry->md->message[msgi]);
        }
    }


    //snprintf(fpsentry->md->message[fpsentry->md->msgcnt], FUNCTION_PARAMETER_STRUCT_MSG_SIZE, "cannot load stream");
    //			fpsentry->md->msgcnt++;

    printf("\n");



    printf("------------- FUNCTION PARAMETER \n");
    printf("[%d] Parameter name : %s\n", pindex,
           fpsentry->parray[pindex].keywordfull);

    char typestring[100];
    functionparameter_GetTypeString(fpsentry->parray[pindex].type, typestring);
    printf("type: %s\n", typestring);


    printf("\n");
    printf("-- FLAG: ");



    // print binary flag
    printfw("FLAG : ");
    uint64_t mask = (uint64_t) 1 << (sizeof(uint64_t) * CHAR_BIT - 1);
    while(mask)
    {
        int digit = fpsentry->parray[pindex].fpflag & mask ? 1 : 0;
        if(digit == 1)
        {
            printf( AECBOLDHIGREEN );
            printf("%d", digit);
            printf( AECNORMAL );
        }
        else
        {
            printf("%d", digit);
        }
        mask >>= 1;
    }
    printf("\n");


    int flagstringlen = 32;

    if(fpsentry->parray[pindex].fpflag & FPFLAG_ACTIVE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "ACTIVE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "ACTIVE");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_USED)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "USED");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "USED");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_VISIBLE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "VISIBLE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "VISIBLE");
    }

    printf("%*s", flagstringlen, "---");

    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "WRITE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "WRITE");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITECONF)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "WRITECONF");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "WRITECONF");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITERUN)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "WRITERUN");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "WRITERUN");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITESTATUS)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "WRITESTATUS");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "WRITESTATUS");
    }

    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_LOG)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "LOG");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "LOG");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_SAVEONCHANGE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "SAVEONCHANGE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "SAVEONCHANGE");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_SAVEONCLOSE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "SAVEONCLOSE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "SAVEONCLOSE");
    }


    printf("%*s", flagstringlen, "---");


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_IMPORTED)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "IMPORTED");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "IMPORTED");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_FEEDBACK)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "FEEDBACK");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "FEEDBACK");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_ONOFF)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "ONOFF");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "ONOFF");
    }


    printf("%*s", flagstringlen, "---");


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_CHECKINIT)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "CHECKINIT");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "CHECKINIT");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_MINLIMIT)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "MINLIMIT");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "MINLIMIT");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_MAXLIMIT)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "MAXLIMIT");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "MAXLIMIT");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_ERROR)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "ERROR");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "ERROR");
    }


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_FORCE_LOCALMEM)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_LOCALMEM");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_LOCALMEM");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_FORCE_SHAREMEM)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_SHAREMEM");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_SHAREMEM");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_FORCE_CONFFITS)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_CONFFITS");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_CONFFITS");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_FORCE_CONFNAME)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_CONFNAME");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_FORCE_CONFNAME");
    }


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_SKIPSEARCH_LOCALMEM)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_LOCALMEM");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_LOCALMEM");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_SKIPSEARCH_SHAREMEM)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_SHAREMEM");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_SHAREMEM");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_SKIPSEARCH_CONFFITS)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_CONFFITS");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_CONFFITS");
    }


    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_SKIPSEARCH_CONFNAME)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_CONFNAME");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_SKIPSEARCH_CONFNAME");
    }


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_UPDATE_SHAREMEM)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_UPDATE_SHAREMEM");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_UPDATE_SHAREMEM");
    }


    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_LOAD_UPDATE_CONFFITS)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_LOAD_UPDATE_CONFFITS");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_LOAD_UPDATE_CONFFITS");
    }


    if(fpsentry->parray[pindex].fpflag & FPFLAG_FILE_CONF_REQUIRED)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "FILE/FPS/STREAM_CONF_REQUIRED");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "FILE/FPS/STREAM_CONF_REQUIRED");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_FILE_RUN_REQUIRED)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "FILE/FPS/STREAM_RUN_REQUIRED");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "FILE/FPS/STREAM_RUN_REQUIRED");
    }


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_DATATYPE)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_DATATYPE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_DATATYPE");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_UINT8)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT8");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT8");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_INT8)
    {
		printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT8");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT8");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_UINT16)
    {
       printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT16");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT16");
    }

    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_INT16)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT16");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT16");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_UINT32)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT32");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT32");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_INT32)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT32");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT32");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_UINT64)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT64");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_UINT64");
    }

    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_INT64)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT64");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_INT64");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_HALF)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_HALF");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_HALF");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_FLOAT)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_FLOAT");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_FLOAT");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_TEST_DATATYPE_DOUBLE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_DOUBLE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_TEST_DATATYPE_DOUBLE");
    }


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_1D)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_1D");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_1D");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_2D)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_2D");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_2D");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_3D)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_3D");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_3D");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_XSIZE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_XSIZE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_XSIZE");
    }


    printf("\n");

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_YSIZE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_YSIZE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_YSIZE");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_ENFORCE_ZSIZE)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_ENFORCE_ZSIZE");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_ENFORCE_ZSIZE");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_CHECKSTREAM)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "CHECKSTREAM");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "CHECKSTREAM");
    }

    if(fpsentry->parray[pindex].fpflag & FPFLAG_STREAM_MEMLOADREPORT)
    {
        printf( AECBOLDHIGREEN );
        printf("%*s", flagstringlen, "STREAM_MEMLOADREPORT");
        printf( AECNORMAL );
    }
    else
    {
        printf("%*s", flagstringlen, "STREAM_MEMLOADREPORT");
    }









    printf("\n");
    printf("\n");
    printf("cnt0 = %ld\n", fpsentry->parray[pindex].cnt0);

    printf("\n");

    printf("Current value : ");

    if(fpsentry->parray[pindex].type == FPTYPE_UNDEF)
    {
        printf("  %s", "-undef-");
    }

    if(fpsentry->parray[pindex].type == FPTYPE_INT64)
    {
        printf("  %10d", (int) fpsentry->parray[pindex].val.l[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_FLOAT64)
    {
        printf("  %10f", (float) fpsentry->parray[pindex].val.f[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_FLOAT32)
    {
        printf("  %10f", (float) fpsentry->parray[pindex].val.s[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_PID)
    {
        printf("  %10d", (int) fpsentry->parray[pindex].val.pid[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_TIMESPEC)
    {
        printf("  %10s", "-timespec-");
    }

    if(fpsentry->parray[pindex].type == FPTYPE_FILENAME)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_FITSFILENAME)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_EXECFILENAME)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_DIRNAME)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_STREAMNAME)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_STRING)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    if(fpsentry->parray[pindex].type == FPTYPE_ONOFF)
    {
        if(fpsentry->parray[pindex].fpflag & FPFLAG_ONOFF)
        {
            printf("    ON  [ %s ]\n", fpsentry->parray[pindex].val.string[1]);
        }
        else
        {
            printf("   OFF  [ %s ]\n", fpsentry->parray[pindex].val.string[0]);
        }
    }

    if(fpsentry->parray[pindex].type == FPTYPE_FPSNAME)
    {
        printf("  %10s", fpsentry->parray[pindex].val.string[0]);
    }

    printf("\n");
    printf("\n");

    return RETURN_SUCCESS;

}





static errno_t functionparameter_PrintParameter_ValueString(
    FUNCTION_PARAMETER *fpsentry,
    char *outstring,
    int stringmaxlen
)
{
	int cmdOK = 0;
	
	
    switch(fpsentry->type)
    {
    case FPTYPE_INT64:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s INT64      %ld %ld %ld %ld",
            fpsentry->keywordfull,
            fpsentry->val.l[0],
            fpsentry->val.l[1],
            fpsentry->val.l[2],
            fpsentry->val.l[3]);
        cmdOK = 1;
        break;

    case FPTYPE_FLOAT64:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s FLOAT64    %f %f %f %f",
            fpsentry->keywordfull,
            fpsentry->val.f[0],
            fpsentry->val.f[1],
            fpsentry->val.f[2],
            fpsentry->val.f[3]);
        cmdOK = 1;
        break;

    case FPTYPE_FLOAT32:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s FLOAT32    %f %f %f %f",
            fpsentry->keywordfull,
            fpsentry->val.s[0],
            fpsentry->val.s[1],
            fpsentry->val.s[2],
            fpsentry->val.s[3]);
        cmdOK = 1;
        break;

    case FPTYPE_PID:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s PID        %ld",
            fpsentry->keywordfull,
            fpsentry->val.l[0]);
        cmdOK = 1;
        break;

    case FPTYPE_TIMESPEC:
        //
        break;

    case FPTYPE_FILENAME:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s FILENAME   %s",
            fpsentry->keywordfull,
            fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    case FPTYPE_FITSFILENAME:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s FITSFILENAME   %s",
            fpsentry->keywordfull,
            fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    case FPTYPE_EXECFILENAME:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s EXECFILENAME   %s",
            fpsentry->keywordfull,
            fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    case FPTYPE_DIRNAME:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s DIRNAME    %s",
            fpsentry->keywordfull,
            fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    case FPTYPE_STREAMNAME:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s STREAMNAME %s",
            fpsentry->keywordfull,
            fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    case FPTYPE_STRING:
        SNPRINTF_CHECK(
            outstring,
            stringmaxlen,
            "%-40s STRING     %s",
            fpsentry->keywordfull,
            fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    case FPTYPE_ONOFF:
        if(fpsentry->fpflag & FPFLAG_ONOFF)
        {
            SNPRINTF_CHECK(outstring, stringmaxlen, "%-40s ONOFF      ON",
                           fpsentry->keywordfull);
        }
        else
        {
            SNPRINTF_CHECK(outstring, stringmaxlen, "%-40s ONOFF      OFF",
                           fpsentry->keywordfull);
        }
        cmdOK = 1;
        break;


    case FPTYPE_FPSNAME:
        SNPRINTF_CHECK(outstring, stringmaxlen, "%-40s FPSNAME   %s",
                       fpsentry->keywordfull, fpsentry->val.string[0]);
        cmdOK = 1;
        break;

    }


	if(cmdOK==1)
	{	
		return RETURN_SUCCESS;
	}
	else
	{
		return RETURN_FAILURE;
	}
		
}
















int functionparameter_SaveParam2disk(
    FUNCTION_PARAMETER_STRUCT *fpsentry,
    const char *paramname
)
{
    int pindex;

    pindex = functionparameter_GetParamIndex(fpsentry, paramname);
    functionparameter_WriteParameterToDisk(fpsentry, pindex, "setval",
                                           "SaveParam2disk");

    return RETURN_SUCCESS;
}








/**
 *
 * ## PURPOSE
 *
 * Enter new value for parameter
 *
 *
 */


int functionparameter_UserInputSetParamValue(
    FUNCTION_PARAMETER_STRUCT *fpsentry,
    int pindex
)
{
    int inputOK;
    int strlenmax = 20;
    char buff[100];
    char c = -1;

    functionparameter_PrintParameterInfo(fpsentry, pindex);


    if(fpsentry->parray[pindex].fpflag & FPFLAG_WRITESTATUS)
    {
        inputOK = 0;
        fflush(stdout);

        while(inputOK == 0)
        {
            printf("\nESC or update value : ");
            fflush(stdout);

            int stringindex = 0;

			c = get_singlechar_block();

            while((c != 27) && (c != 10) && (c != 13) && (stringindex < strlenmax - 1))
            {
                buff[stringindex] = c;
                if(c == 127)   // delete key
                {
                    putchar(0x8);
                    putchar(' ');
                    putchar(0x8);
                    stringindex --;
                }
                else
                {					
                    putchar(c);  // echo on screen
                    fflush(stdout);
                    stringindex++;
                }
                if(stringindex < 0)
                {
                    stringindex = 0;
                }
                
				c = get_singlechar_block();
            }
            buff[stringindex] = '\0';
            inputOK = 1;
        }

        if(c != 27)   // do not update value if escape key
        {

            long lval = 0;
            double fval = 0.0;
            char *endptr;
            int vOK = 1;

            switch(fpsentry->parray[pindex].type)
            {

                case FPTYPE_INT64:
                    errno = 0;    /* To distinguish success/failure after call */
                    lval = strtol(buff, &endptr, 10);

                    /* Check for various possible errors */
                    if((errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN))
                            || (errno != 0 && lval == 0))
                    {
                        perror("strtol");
                        vOK = 0;
                        sleep(1);
                    }

                    if(endptr == buff)
                    {
                        fprintf(stderr, "\nERROR: No digits were found\n");
                        vOK = 0;
                        sleep(1);
                    }

                    if(vOK == 1)
                    {
                        fpsentry->parray[pindex].val.l[0] = lval;
                    }
                    break;

                case FPTYPE_FLOAT64:
                    errno = 0;    /* To distinguish success/failure after call */
                    fval = strtod(buff, &endptr);

                    /* Check for various possible errors */
                    if((errno == ERANGE)
                            || (errno != 0 && fval == 0.0))
                    {
                        perror("strtod");
                        vOK = 0;
                        sleep(1);
                    }

                    if(endptr == buff)
                    {
                        fprintf(stderr, "\nERROR: No digits were found\n");
                        vOK = 0;
                        sleep(1);
                    }

                    if(vOK == 1)
                    {
                        fpsentry->parray[pindex].val.f[0] = fval;
                    }
                    break;


                case FPTYPE_FLOAT32:
                    errno = 0;    /* To distinguish success/failure after call */
                    fval = strtod(buff, &endptr);

                    /* Check for various possible errors */
                    if((errno == ERANGE)
                            || (errno != 0 && fval == 0.0))
                    {
                        perror("strtod");
                        vOK = 0;
                        sleep(1);
                    }

                    if(endptr == buff)
                    {
                        fprintf(stderr, "\nERROR: No digits were found\n");
                        vOK = 0;
                        sleep(1);
                    }

                    if(vOK == 1)
                    {
                        fpsentry->parray[pindex].val.s[0] = fval;
                    }
                    break;


                case FPTYPE_PID :
                    errno = 0;    /* To distinguish success/failure after call */
                    lval = strtol(buff, &endptr, 10);

                    /* Check for various possible errors */
                    if((errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN))
                            || (errno != 0 && lval == 0))
                    {
                        perror("strtol");
                        vOK = 0;
                        sleep(1);
                    }

                    if(endptr == buff)
                    {
                        fprintf(stderr, "\nERROR: No digits were found\n");
                        vOK = 0;
                        sleep(1);
                    }

                    if(vOK == 1)
                    {
                        fpsentry->parray[pindex].val.pid[0] = (pid_t) lval;
                    }
                    break;


                case FPTYPE_FILENAME :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

                case FPTYPE_FITSFILENAME :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

                case FPTYPE_EXECFILENAME :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

                case FPTYPE_DIRNAME :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

                case FPTYPE_STREAMNAME :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

                case FPTYPE_STRING :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

                case FPTYPE_FPSNAME :
                    if(snprintf(fpsentry->parray[pindex].val.string[0],
                                FUNCTION_PARAMETER_STRMAXLEN, "%s", buff) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }
                    break;

            }

            fpsentry->parray[pindex].cnt0++;

            // notify GUI
            fpsentry->md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE;


            // Save to disk
            if(fpsentry->parray[pindex].fpflag & FPFLAG_SAVEONCHANGE)
            {
                functionparameter_WriteParameterToDisk(fpsentry, pindex, "setval",
                                                       "UserInputSetParamValue");
            }
        }
    }
    else
    {
        printf("%s Value cannot be modified %s\n", AECBOLDHIRED, AECNORMAL );
        c = getchar();
    }



    return 0;
}







/**
 * ## Purpose
 *
 * Process command line.
 *
 * ## Commands
 *
 * - logsymlink  : create log sym link
 * - setval      : set parameter value
 * - getval      : get value, write to output log
 * - fwrval      : get value, write to file or fifo
 * - confupdate  : update configuration
 * - confwupdate : update configuration, wait for completion to proceed
 * - runstart    : start RUN process associated with parameter
 * - runstop     : stop RUN process associated with parameter
 * - fpsrm       : remove fps
 * - cntinc      : counter test to check fifo connection
 * - exit        : exit fpsCTRL tool
 *
 * - queueprio   : change queue priority
 *
 *
 */


int functionparameter_FPSprocess_cmdline(
    char *FPScmdline,
    FPSCTRL_TASK_QUEUE *fpsctrlqueuelist,
    KEYWORD_TREE_NODE *keywnode,
    FPSCTRL_PROCESS_VARS *fpsCTRLvar,
    FUNCTION_PARAMETER_STRUCT *fps,
    uint64_t *taskstatus
)
{
    int  fpsindex;
    long pindex;

    // break FPScmdline in words
    // [FPScommand] [FPSentryname]
    //
    char *pch;
    int   nbword = 0;
    char  FPScommand[100];

    int   cmdOK = 2;    // 0 : failed, 1: OK
    int   cmdFOUND = 0; // toggles to 1 when command has been found

	// first arg is always an FPS entry name
    char  FPSentryname[FUNCTION_PARAMETER_KEYWORD_STRMAXLEN * FUNCTION_PARAMETER_KEYWORD_MAXLEVEL];
    char  FPScmdarg1[FUNCTION_PARAMETER_STRMAXLEN];

    
    
    char  FPSarg0[FUNCTION_PARAMETER_KEYWORD_STRMAXLEN * FUNCTION_PARAMETER_KEYWORD_MAXLEVEL];
    char  FPSarg1[FUNCTION_PARAMETER_STRMAXLEN];
    char  FPSarg2[FUNCTION_PARAMETER_STRMAXLEN];
    char  FPSarg3[FUNCTION_PARAMETER_STRMAXLEN];




    char msgstring[STRINGMAXLEN_FPS_LOGMSG];
    char inputcmd[STRINGMAXLEN_FPS_CMDLINE];


    int inputcmdOK = 0; // 1 if command should be processed


	static int testcnt; // test counter to be incremented by cntinc command


    if(strlen(FPScmdline) > 0)   // only send command if non-empty
    {
        SNPRINTF_CHECK(inputcmd, STRINGMAXLEN_FPS_CMDLINE, "%s", FPScmdline);
        inputcmdOK = 1;
    }

    // don't process lines starting with # (comment)
    if(inputcmdOK == 1)
    {
        if(inputcmd[0] == '#')
        {
            inputcmdOK = 0;
        }
    }

    if(inputcmdOK == 0)
    {
        return (-1);
    }



    functionparameter_outlog("CMDRCV", "[%s]", inputcmd);
	*taskstatus |= FPSTASK_STATUS_RECEIVED;

    DEBUG_TRACEPOINT(" ");

    if(strlen(inputcmd) > 1)
    {
        pch = strtok(inputcmd, " \t");
        sprintf(FPScommand, "%s", pch);
    }
    else
    {
        pch = NULL;
    }


    DEBUG_TRACEPOINT(" ");



	// Break command line into words
	//
	// output words are:
	//
	// FPScommand
	// FPSarg0
	// FPSarg1
	// FPSarg2
	// FPSarg3
	
    while(pch != NULL)
    {

        nbword++;
        pch = strtok(NULL, " \t");

        if(nbword == 1)   // first arg (0)
        {
            char *pos;
            sprintf(FPSarg0, "%s", pch);
            if((pos = strchr(FPSarg0, '\n')) != NULL)
            {
                *pos = '\0';
            }

        }

        if(nbword == 2)
        {
            char *pos;
            if(snprintf(FPSarg1, FUNCTION_PARAMETER_STRMAXLEN, "%s",
                        pch) >= FUNCTION_PARAMETER_STRMAXLEN)
            {
                printf("WARNING: string truncated\n");
                printf("STRING: %s\n", pch);
            }
            if((pos = strchr(FPSarg1, '\n')) != NULL)
            {
                *pos = '\0';
            }
        }

        if(nbword == 3)
        {
            char *pos;
            if(snprintf(FPSarg2, FUNCTION_PARAMETER_STRMAXLEN, "%s",
                        pch) >= FUNCTION_PARAMETER_STRMAXLEN)
            {
                printf("WARNING: string truncated\n");
                printf("STRING: %s\n", pch);
            }
            if((pos = strchr(FPSarg2, '\n')) != NULL)
            {
                *pos = '\0';
            }
        }

        if(nbword == 4)
        {
            char *pos;
            if(snprintf(FPSarg3, FUNCTION_PARAMETER_STRMAXLEN, "%s",
                        pch) >= FUNCTION_PARAMETER_STRMAXLEN)
            {
                printf("WARNING: string truncated\n");
                printf("STRING: %s\n", pch);
            }
            if((pos = strchr(FPSarg3, '\n')) != NULL)
            {
                *pos = '\0';
            }
        }

    }



    DEBUG_TRACEPOINT(" ");


    if(nbword == 0)
    {
        cmdFOUND = 1;   // do nothing, proceed
        cmdOK = 2;
    }






    // Handle commands for which FPSarg0 is NOT an FPS entry


    // cntinc
    if((cmdFOUND == 0)
            && (strcmp(FPScommand, "exit") == 0))
    {
        cmdFOUND = 1;
        if(nbword != 1)
        {
            functionparameter_outlog("ERROR", "COMMAND cntinc takes NBARGS = 1");
            cmdOK = 0;
        }
        else
        {
			fpsCTRLvar->exitloop = 1;
            functionparameter_outlog("INFO", "EXIT");
        }
    }




    // cntinc
    if((cmdFOUND == 0)
            && (strcmp(FPScommand, "cntinc") == 0))
    {
        cmdFOUND = 1;
        if(nbword != 2)
        {
            functionparameter_outlog("ERROR", "COMMAND cntinc takes NBARGS = 2");
            cmdOK = 0;
        }
        else
        {
			testcnt ++;
            functionparameter_outlog("INFO", "TEST [%d] counter = %d", atoi(FPSarg0), testcnt);
        }
    }







    // logsymlink
    if((cmdFOUND == 0)
            && (strcmp(FPScommand, "logsymlink") == 0))
    {
        cmdFOUND = 1;
        if(nbword != 2)
        {

            functionparameter_outlog("ERROR", "COMMAND logsymlink takes NBARGS = 1");
            cmdOK = 0;
        }
        else
        {
            char logfname[STRINGMAXLEN_FULLFILENAME];                       
			getFPSlogfname(logfname);           

            functionparameter_outlog("INFO", "CREATE SYM LINK %s <- %s", FPSarg0, logfname);

            if(symlink(logfname, FPSarg0) != 0)
            {
                PRINT_ERROR("symlink error");
            }

        }
    }




    // queueprio
    if((cmdFOUND == 0)
            && (strcmp(FPScommand, "queueprio") == 0))
    {
        cmdFOUND = 1;
        if(nbword != 3)
        {
            functionparameter_outlog("ERROR", "COMMAND queueprio takes NBARGS = 2");
            cmdOK = 0;
        }
        else
        {
            int queue = atoi(FPSarg0);
            int prio = atoi(FPSarg1);

            if((queue >= 0) && (queue < NB_FPSCTRL_TASKQUEUE_MAX))
            {
                fpsctrlqueuelist[queue].priority = prio;
                functionparameter_outlog("INFO", "%s", "QUEUE %d PRIO = %d", queue, prio);
            }
        }
    }









    // From this point on, FPSarg0 is expected to be a FPS entry
    // so we resolve it and look for fps
    int kwnindex = -1;
    if(cmdFOUND == 0)
    {
        strcpy(FPSentryname, FPSarg0);
        strcpy(FPScmdarg1, FPSarg1);


        // look for entry, if found, kwnindex points to it
        if(nbword > 1)
        {
            //                printf("Looking for entry for %s\n", FPSentryname);

            int kwnindexscan = 0;
            while((kwnindex == -1) && (kwnindexscan < fpsCTRLvar->NBkwn))
            {
                if(strcmp(keywnode[kwnindexscan].keywordfull, FPSentryname) == 0)
                {
                    kwnindex = kwnindexscan;
                }
                kwnindexscan ++;
            }
        }

        //            sprintf(msgstring, "nbword = %d  cmdOK = %d   kwnindex = %d",  nbword, cmdOK, kwnindex);
        //            functionparameter_outlog("INFO", "%s", msgstring);
    

    if(kwnindex != -1)
    {
        fpsindex = keywnode[kwnindex].fpsindex;
        pindex = keywnode[kwnindex].pindex;
        functionparameter_outlog("INFO", "FPS ENTRY FOUND : %-40s  %d %ld", FPSentryname, fpsindex, pindex);
    }
    else
    {
        functionparameter_outlog("ERROR", "FPS ENTRY NOT FOUND : %-40s", FPSentryname);
        cmdOK = 0;
    }
	}



    if(kwnindex != -1)   // if FPS has been found
    {

        // confstart
        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "confstart") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "%s", "COMMAND confstart takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");
                functionparameter_CONFstart(fps, fpsindex);

                functionparameter_outlog("CONFSTART", "start CONF process %d %s",
                               fpsindex, fps[fpsindex].md->name);
                cmdOK = 1;
            }
        }


        // confstop
        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "confstop") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND confstop takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");
                functionparameter_CONFstop(fps, fpsindex);
                functionparameter_outlog("CONFSTOP", "stop CONF process %d %s",
                               fpsindex, fps[fpsindex].md->name);
                cmdOK = 1;
            }
        }










        // confupdate

        DEBUG_TRACEPOINT(" ");
        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "confupdate") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND confupdate takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");
                fps[fpsindex].md->signal |=
                    FUNCTION_PARAMETER_STRUCT_SIGNAL_CHECKED; // update status: check waiting to be done
                fps[fpsindex].md->signal |=
                    FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // request an update

                functionparameter_outlog("CONFUPDATE", "update CONF process %d %s",
                               fpsindex, fps[fpsindex].md->name);
                cmdOK = 1;
            }
        }





        // confwupdate
        // Wait until update is cleared
        // if not successful, retry until time lapsed

        DEBUG_TRACEPOINT(" ");
        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "confwupdate") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND confwupdate takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                int looptry = 1;
                int looptrycnt = 0;
                unsigned int timercnt = 0;
                useconds_t dt = 100;
                unsigned int timercntmax = 10000; // 1 sec max

                while(looptry == 1)
                {

                    DEBUG_TRACEPOINT(" ");
                    fps[fpsindex].md->signal |=
                        FUNCTION_PARAMETER_STRUCT_SIGNAL_CHECKED; // update status: check waiting to be done
                    fps[fpsindex].md->signal |=
                        FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // request an update

                    while(((fps[fpsindex].md->signal & FUNCTION_PARAMETER_STRUCT_SIGNAL_CHECKED))
                            && (timercnt < timercntmax))
                    {
                        usleep(dt);
                        timercnt++;
                    }
                    usleep(dt);
                    timercnt++;

                    functionparameter_outlog("CONFWUPDATE", 
                    "[%d] waited %d us on FPS %d %s. conferrcnt = %d",
                        looptrycnt,
                        dt * timercnt,
                        fpsindex,
                        fps[fpsindex].md->name,
                        fps[fpsindex].md->conferrcnt);
                        
                    looptrycnt++;

                    if(fps[fpsindex].md->conferrcnt == 0)   // no error ! we can proceed
                    {
                        looptry = 0;
                    }

                    if(timercnt > timercntmax)    // ran out of time ... giving up
                    {
                        looptry = 0;
                    }


                }

                cmdOK = 1;
            }
        }




        // runstart
        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "runstart") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND runstart takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");
                functionparameter_RUNstart(fps, fpsindex);

                functionparameter_outlog("RUNSTART", "start RUN process %d %s",
                               fpsindex, fps[fpsindex].md->name);
                cmdOK = 1;

            }
        }



        // runwait
        // wait until run process is completed

        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "runwait") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND runwait takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");

                unsigned int timercnt = 0;
                useconds_t dt = 10000;
                unsigned int timercntmax = 100000; // 10000 sec max

                while(((fps[fpsindex].md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN))
                        && (timercnt < timercntmax))
                {
                    usleep(dt);
                    timercnt++;
                }
                functionparameter_outlog("RUNWAIT", "waited %d us on FPS %d %s",
                               dt * timercnt, fpsindex, fps[fpsindex].md->name);
                cmdOK = 1;
            }
        }



        // runstop

        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "runstop") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND runstop takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");
                functionparameter_RUNstop(fps, fpsindex);
                functionparameter_outlog("RUNSTOP", "stop RUN process %d %s",
                               fpsindex, fps[fpsindex].md->name);
                cmdOK = 1;
            }
        }




        // fpsrm

        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "fpsrm") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 2)
            {
                functionparameter_outlog("ERROR", "COMMAND fpsrm takes NBARGS = 1");
                cmdOK = 0;
            }
            else
            {
                DEBUG_TRACEPOINT(" ");
                functionparameter_FPSremove(fps, fpsindex);

                functionparameter_outlog("FPSRM", "FPS remove %d %s", fpsindex,
                               fps[fpsindex].md->name);
                cmdOK = 1;
            }
        }







        DEBUG_TRACEPOINT(" ");





        // setval
        if((cmdFOUND == 0)
                && (strcmp(FPScommand, "setval") == 0))
        {
            cmdFOUND = 1;
            if(nbword != 3)
            {
                SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "COMMAND setval takes NBARGS = 2");
                functionparameter_outlog("ERROR", "%s", msgstring);
            }
            else
            {
                int updated = 0;

                switch(fps[fpsindex].parray[pindex].type)
                {

                    case FPTYPE_INT64:
                        if(functionparameter_SetParamValue_INT64(&fps[fpsindex], FPSentryname,
                                atol(FPScmdarg1)) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s INT64      %ld",
                                       FPSentryname, atol(FPScmdarg1));
                        break;

                    case FPTYPE_FLOAT64:
                        if(functionparameter_SetParamValue_FLOAT64(&fps[fpsindex], FPSentryname,
                                atof(FPScmdarg1)) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s FLOAT64    %f",
                                       FPSentryname, atof(FPScmdarg1));
                        break;

                    case FPTYPE_FLOAT32:
                        if(functionparameter_SetParamValue_FLOAT32(&fps[fpsindex], FPSentryname,
                                atof(FPScmdarg1)) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s FLOAT32    %f",
                                       FPSentryname, atof(FPScmdarg1));
                        break;

                    case FPTYPE_PID:
                        if(functionparameter_SetParamValue_INT64(&fps[fpsindex], FPSentryname,
                                atol(FPScmdarg1)) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s PID        %ld",
                                       FPSentryname, atol(FPScmdarg1));
                        break;

                    case FPTYPE_TIMESPEC:
                        //
                        break;

                    case FPTYPE_FILENAME:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s FILENAME   %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                    case FPTYPE_FITSFILENAME:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s FITSFILENAME   %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                    case FPTYPE_EXECFILENAME:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s EXECFILENAME   %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                    case FPTYPE_DIRNAME:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s DIRNAME    %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                    case FPTYPE_STREAMNAME:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s STREAMNAME %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                    case FPTYPE_STRING:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s STRING     %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                    case FPTYPE_ONOFF:
                        if(strncmp(FPScmdarg1, "ON", 2) == 0)
                        {
                            if(functionparameter_SetParamValue_ONOFF(&fps[fpsindex], FPSentryname,
                                    1) == EXIT_SUCCESS)
                            {
                                updated = 1;
                            }
                            functionparameter_outlog("SETVAL", "%-40s ONOFF      ON",
                                           FPSentryname);
                        }
                        if(strncmp(FPScmdarg1, "OFF", 3) == 0)
                        {
                            if(functionparameter_SetParamValue_ONOFF(&fps[fpsindex], FPSentryname,
                                    0) == EXIT_SUCCESS)
                            {
                                updated = 1;
                            }
                            functionparameter_outlog("SETVAL", "%-40s ONOFF      OFF",
                                           FPSentryname);
                        }
                        break;


                    case FPTYPE_FPSNAME:
                        if(functionparameter_SetParamValue_STRING(&fps[fpsindex], FPSentryname,
                                FPScmdarg1) == EXIT_SUCCESS)
                        {
                            updated = 1;
                        }
                        functionparameter_outlog("SETVAL", "%-40s FPSNAME   %s",
                                       FPSentryname, FPScmdarg1);
                        break;

                }

                // notify fpsCTRL that parameter has been updated
                if(updated == 1)
                {
                    cmdOK = 1;
                    functionparameter_WriteParameterToDisk(&fps[fpsindex], pindex, "setval",
                                                           "input command file");
                    fps[fpsindex].md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE;
                }
                else
                {
                    cmdOK = 0;
                }

            }
        }





        // getval or fwrval
        if((cmdFOUND == 0)
                && ((strcmp(FPScommand, "getval") == 0) || (strcmp(FPScommand, "fwrval") == 0)))
        {
            cmdFOUND = 1;
            cmdOK = 0;

            if((strcmp(FPScommand, "getval") == 0) && (nbword != 2))
            {
                functionparameter_outlog("ERROR", "COMMAND getval NBARGS = 1");
            }
            else if((strcmp(FPScommand, "fwrval") == 0) && (nbword != 3))
            {
                functionparameter_outlog("ERROR", "COMMAND fwrval NBARGS = 2");
            }
            else
            {
				errno_t ret;
				ret = functionparameter_PrintParameter_ValueString(&fps[fpsindex].parray[pindex], msgstring, STRINGMAXLEN_FPS_LOGMSG);
				
				if(ret == RETURN_SUCCESS)
					cmdOK = 1;
				else
					cmdOK = 0;
				
				/*
                switch(fps[fpsindex].parray[pindex].type)
                {

                    case FPTYPE_INT64:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s INT64      %ld %ld %ld %ld",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.l[0],
                            fps[fpsindex].parray[pindex].val.l[1],
                            fps[fpsindex].parray[pindex].val.l[2],
                            fps[fpsindex].parray[pindex].val.l[3]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_FLOAT64:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s FLOAT64    %f %f %f %f",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.f[0],
                            fps[fpsindex].parray[pindex].val.f[1],
                            fps[fpsindex].parray[pindex].val.f[2],
                            fps[fpsindex].parray[pindex].val.f[3]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_FLOAT32:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s FLOAT32    %f %f %f %f",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.s[0],
                            fps[fpsindex].parray[pindex].val.s[1],
                            fps[fpsindex].parray[pindex].val.s[2],
                            fps[fpsindex].parray[pindex].val.s[3]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_PID:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s PID        %ld",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.l[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_TIMESPEC:
                        //
                        break;

                    case FPTYPE_FILENAME:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s FILENAME   %s",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_FITSFILENAME:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s FITSFILENAME   %s",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_EXECFILENAME:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s EXECFILENAME   %s",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_DIRNAME:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s DIRNAME    %s",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_STREAMNAME:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s STREAMNAME %s",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_STRING:
                        SNPRINTF_CHECK(
                            msgstring,
                            STRINGMAXLEN_FPS_LOGMSG,
                            "%-40s STRING     %s",
                            FPSentryname,
                            fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                    case FPTYPE_ONOFF:
                        if(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ONOFF)
                        {
                            SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "%-40s ONOFF      ON",
                                           FPSentryname);
                        }
                        else
                        {
                            SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "%-40s ONOFF      OFF",
                                           FPSentryname);
                        }
                        cmdOK = 1;
                        break;


                    case FPTYPE_FPSNAME:
                        SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "%-40s FPSNAME   %s",
                                       FPSentryname, fps[fpsindex].parray[pindex].val.string[0]);
                        cmdOK = 1;
                        break;

                }

				*/

                if(cmdOK == 1)
                {
                    if(strcmp(FPScommand, "getval") == 0)
                    {
                        functionparameter_outlog("GETVAL", "%s", msgstring);
                    }
                    if(strcmp(FPScommand, "fwrval") == 0)
                    {

                        FILE *fpouttmp = fopen(FPScmdarg1, "a");
                        functionparameter_outlog_file("FWRVAL", msgstring, fpouttmp);
                        fclose(fpouttmp);

                        functionparameter_outlog("FWRVAL", "%s", msgstring);
                        char msgstring1[STRINGMAXLEN_FPS_LOGMSG];
                        SNPRINTF_CHECK(msgstring1, STRINGMAXLEN_FPS_LOGMSG, "WROTE to file %s",
                                       FPScmdarg1);
                        functionparameter_outlog("FWRVAL", "%s", msgstring1);
                    }
                }

            }
        }


    }


    if(cmdOK == 0)
    {
        SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "\"%s\"", FPScmdline);
        functionparameter_outlog("CMDFAIL", "%s", msgstring);
        *taskstatus |= FPSTASK_STATUS_CMDFAIL;
    }

    if(cmdOK == 1)
    {
        SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "\"%s\"", FPScmdline);
        functionparameter_outlog("CMDOK", "%s", msgstring);
        *taskstatus |= FPSTASK_STATUS_CMDOK;
    }

    if(cmdFOUND == 0)
    {
        SNPRINTF_CHECK(msgstring, STRINGMAXLEN_FPS_LOGMSG, "COMMAND NOT FOUND: %s",
                       FPScommand);
        functionparameter_outlog("ERROR", "%s", msgstring);
        *taskstatus |= FPSTASK_STATUS_CMDNOTFOUND;
    }


    DEBUG_TRACEPOINT(" ");


    return fpsindex;
}







// fill up task list from fifo submissions

int functionparameter_read_fpsCMD_fifo(
    int fpsCTRLfifofd,
    FPSCTRL_TASK_ENTRY *fpsctrltasklist,
    FPSCTRL_TASK_QUEUE *fpsctrlqueuelist
)
{
    int cmdcnt = 0;
    char *FPScmdline = NULL;
    char buff[200];
    int total_bytes = 0;
    int bytes;
    char buf0[1];

    // toggles
    static uint32_t queue = 0;
    static int waitonrun = 0;
    static int waitonconf = 0;



    static uint16_t	cmdinputcnt = 0;

    int lineOK = 1; // keep reading


    DEBUG_TRACEPOINT(" ");

    while(lineOK == 1)
    {
        total_bytes = 0;
        lineOK = 0;
        for(;;)
        {
            bytes = read(fpsCTRLfifofd, buf0, 1);  // read one char at a time
            DEBUG_TRACEPOINT("ERRROR: BUFFER OVERFLOW %d %d\n", bytes, total_bytes);
            if(bytes > 0)
            {
                buff[total_bytes] = buf0[0];
                total_bytes += (size_t)bytes;

            }
            else
            {
                if(errno == EWOULDBLOCK)
                {
                    break;
                }
                else     // read 0 byte
                {
                    //perror("read 0 byte");
                    return cmdcnt;
                }
            }


            DEBUG_TRACEPOINT(" ");


            if(buf0[0] == '\n')    // reached end of line
            {
                buff[total_bytes - 1] = '\0';
                FPScmdline = buff;



                // find next index
                int cmdindex = 0;
                int cmdindexOK = 0;
                while((cmdindexOK == 0) && (cmdindex < NB_FPSCTRL_TASK_MAX))
                {
                    if(fpsctrltasklist[cmdindex].status == 0)
                    {
                        cmdindexOK = 1;
                    }
                    else
                    {
                        cmdindex ++;
                    }
                }


                if(cmdindex == NB_FPSCTRL_TASK_MAX)
                {
                    printf("ERROR: fpscmdarray is full\n");
                    exit(0);
                }


                DEBUG_TRACEPOINT(" ");

                // Some commands affect how the task list is configured instead of being inserted as entries
                int cmdFOUND = 0;


                if((FPScmdline[0] == '#') || (FPScmdline[0] == ' ') || (total_bytes<2))       // disregard line
                {
                    cmdFOUND = 1;
                }

                // set wait on run ON
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "taskcntzero", strlen("taskcntzero")) == 0))
                {
                    cmdFOUND = 1;
                    cmdinputcnt = 0;
                }

                // Set queue index
                // entries will now be placed in queue specified by this command
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "setqindex", strlen("setqindex")) == 0))
                {
                    cmdFOUND = 1;
                    char stringtmp[200];
                    int queue_index;
                    sscanf(FPScmdline, "%s %d", stringtmp, &queue_index);

                    if((queue_index > -1) && (queue_index < NB_FPSCTRL_TASKQUEUE_MAX))
                    {
                        queue = queue_index;
                    }
                }

                // Set queue priority
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "setqprio", strlen("setqprio")) == 0))
                {
                    cmdFOUND = 1;
                    char stringtmp[200];
                    int queue_priority;
                    sscanf(FPScmdline, "%s %d", stringtmp, &queue_priority);

                    if(queue_priority < 0)
                    {
                        queue_priority = 0;
                    }

                    fpsctrlqueuelist[queue].priority = queue_priority;
                }



                // set wait on run ON
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "waitonrunON", strlen("waitonrunON")) == 0))
                {
                    cmdFOUND = 1;
                    waitonrun = 1;
                }

                // set wait on run OFF
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "waitonrunOFF", strlen("waitonrunOFF")) == 0))
                {
                    cmdFOUND = 1;
                    waitonrun = 0;
                }

                // set wait on conf ON
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "waitonconfON", strlen("waitonconfON")) == 0))
                {
                    cmdFOUND = 1;
                    waitonconf = 1;
                }

                // set wait on conf OFF
                if((cmdFOUND == 0)
                        && (strncmp(FPScmdline, "waitonconfOFF", strlen("waitonconfOFF")) == 0))
                {
                    cmdFOUND = 1;
                    waitonconf = 0;
                }


                // set wait point for arbitrary FPS run to have finished

                DEBUG_TRACEPOINT(" ");

                // for all other commands, put in task list
                if(cmdFOUND == 0)
                {
                    strncpy(fpsctrltasklist[cmdindex].cmdstring, FPScmdline,
                            STRINGMAXLEN_FPS_CMDLINE);

                    fpsctrltasklist[cmdindex].status = FPSTASK_STATUS_ACTIVE | FPSTASK_STATUS_SHOW;
                    fpsctrltasklist[cmdindex].inputindex = cmdinputcnt;
                    fpsctrltasklist[cmdindex].queue = queue;
                    clock_gettime(CLOCK_REALTIME, &fpsctrltasklist[cmdindex].creationtime);

                    if(waitonrun == 1)
                    {
                        fpsctrltasklist[cmdindex].flag |= FPSTASK_FLAG_WAITONRUN;
                    }
                    else
                    {
                        fpsctrltasklist[cmdindex].flag &= ~FPSTASK_FLAG_WAITONRUN;
                    }

                    if(waitonconf == 1)
                    {
                        fpsctrltasklist[cmdindex].flag |= FPSTASK_FLAG_WAITONCONF;
                    }
                    else
                    {
                        fpsctrltasklist[cmdindex].flag &= ~FPSTASK_FLAG_WAITONCONF;
                    }

                    cmdinputcnt++;

                    cmdcnt ++;
                }
                lineOK = 1;
                break;
            }
        }

    }

    DEBUG_TRACEPOINT(" ");

    return cmdcnt;
}






/** @brief Find the next task to execute
 *
 * Tasks are arranged in execution queues.
 * Each task belongs to a single queue.
 *
 * This function is run by functionparameter_CTRLscreen() at regular intervals to probe queues and run pending tasks.
 * If a task is found, it is executed by calling functionparameter_FPSprocess_cmdline()
 * 
 * Each queue has a priority index.
 *
 * RULES :
 * - priorities are associated to queues, not individual tasks: changing a queue priority affects all tasks in the queue
 * - If queue priority = 0, no task is executed in the queue: it is paused
 * - Task order within a queue must be respected. Execution order is submission order (FIFO)
 * - Tasks can overlap if they belong to separate queues and have the same priority
 * - A running task waiting to be completed cannot block tasks in other queues
 * - If two tasks are ready with the same priority, the one in the lower queue will be launched
 *
 * CONVENTIONS AND GUIDELINES :
 * - queue #0 is the main queue
 * - Keep queue 0 priority at 10
 * - Do not pause queue 0
 * - Return to queue 0 when done working in other queues
 */

static int function_parameter_process_fpsCMDarray(
    FPSCTRL_TASK_ENTRY         *fpsctrltasklist,
    FPSCTRL_TASK_QUEUE         *fpsctrlqueuelist,
    KEYWORD_TREE_NODE          *keywnode,
    FPSCTRL_PROCESS_VARS       *fpsCTRLvar,
    FUNCTION_PARAMETER_STRUCT  *fps
)
{
    // queue has no task
    int QUEUE_NOTASK = -1;

    // queue has a running task, must waiting for completion
    int QUEUE_WAIT = -2;

    // queue is ready for next scan
    int QUEUE_SCANREADY = -3;



    // the scheduler handles multiple queues
    // in each queue, we look for a task to run, and run it if conditions are met


    int NBtaskLaunched = 0;




    // For each queue, lets find which task is ready
    // results are written in array
    // if no task ready in queue, value = QUEUE_NOTASK
    int queue_nexttask[NB_FPSCTRL_TASKQUEUE_MAX];


    for( uint32_t qi = 0; qi < NB_FPSCTRL_TASKQUEUE_MAX; qi ++)
    {
        queue_nexttask[qi] = QUEUE_SCANREADY;


        while ( queue_nexttask[qi] == QUEUE_SCANREADY )
        {
            // find next command to execute
            uint64_t inputindexmin = UINT_MAX;
            int cmdindexExec;
            int cmdOK = 0;


            queue_nexttask[qi] = QUEUE_NOTASK;
            //
            // Find task with smallest inputindex within this queue
            // This is the one to be executed
            //
            for( int cmdindex = 0; cmdindex < NB_FPSCTRL_TASK_MAX; cmdindex++ )
            {
                if((fpsctrltasklist[cmdindex].status & FPSTASK_STATUS_ACTIVE)
                        && (fpsctrltasklist[cmdindex].queue == qi))
                {
                    if(fpsctrltasklist[cmdindex].inputindex < inputindexmin)
                    {
                        inputindexmin = fpsctrltasklist[cmdindex].inputindex;
                        cmdindexExec = cmdindex;
                        cmdOK = 1;
                    }
                }
            }


            if(cmdOK == 1) // A potential task to be executed has been found
            {
                if(!(fpsctrltasklist[cmdindexExec].status &
                        FPSTASK_STATUS_RUNNING))     // if task not running, launch it
                {
                    queue_nexttask[qi] = cmdindexExec;

                }
                else
                {
                    // if it's already running, lets check if it is completed
                    int task_completed = 1; // default

                    if(fpsctrltasklist[cmdindexExec].flag &
                            FPSTASK_FLAG_WAITONRUN)   // are we waiting for run to be completed ?
                    {
                        if((fps[fpsctrltasklist[cmdindexExec].fpsindex].md->status &
                                FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN))
                        {
                            task_completed = 0; // must wait
                            queue_nexttask[qi] = QUEUE_WAIT;
                        }
                    }

                    if(fpsctrltasklist[cmdindexExec].flag &
                            FPSTASK_FLAG_WAITONCONF)   // are we waiting for conf update to be completed ?
                    {
                        if(fps[fpsctrltasklist[cmdindexExec].fpsindex].md->status &
                                FUNCTION_PARAMETER_STRUCT_SIGNAL_CHECKED)
                        {
                            task_completed = 0; // must wait
                            queue_nexttask[qi] = QUEUE_WAIT;
                        }
                    }

                    if(task_completed == 1)
                    {
                        fpsctrltasklist[cmdindexExec].status &=
                            ~FPSTASK_STATUS_RUNNING; // update status - no longer running
                        fpsctrltasklist[cmdindexExec].status &=
                            ~FPSTASK_STATUS_ACTIVE; //no longer active, remove it from list
                        //   fpsctrltasklist[cmdindexExec].status &= ~FPSTASK_STATUS_SHOW; // and stop displaying

                        clock_gettime(CLOCK_REALTIME, &fpsctrltasklist[cmdindexExec].completiontime);
                        queue_nexttask[qi] = QUEUE_SCANREADY;
                    }
                }
            } // end if(cmdOK==1)
        } // end while QUEUE_SCANREADY


    }



    // find out which task to run among the ones pre-selected above

    int nexttask_priority = -1;
    int nexttask_cmdindex = -1;
    for( uint32_t qi = 0; qi < NB_FPSCTRL_TASKQUEUE_MAX; qi ++)
    {
        if( (queue_nexttask[qi] != QUEUE_NOTASK ) && (queue_nexttask[qi] != QUEUE_WAIT ) )
        {
            if( fpsctrlqueuelist[qi].priority > nexttask_priority )
            {
                nexttask_priority = fpsctrlqueuelist[qi].priority;
                nexttask_cmdindex = queue_nexttask[qi];
            }
        }
    }



    if( nexttask_cmdindex != -1 )
    {
        if(nexttask_priority > 0 )
        { // execute task
            int cmdindexExec = nexttask_cmdindex;

            uint64_t taskstatus = 0;

            fpsctrltasklist[cmdindexExec].fpsindex =
                functionparameter_FPSprocess_cmdline(fpsctrltasklist[cmdindexExec].cmdstring,
                        fpsctrlqueuelist, keywnode, fpsCTRLvar, fps, &taskstatus);
            NBtaskLaunched++;
            
            // update status form cmdline interpreter
            fpsctrltasklist[cmdindexExec].status |= taskstatus;
            
            clock_gettime(CLOCK_REALTIME, &fpsctrltasklist[cmdindexExec].activationtime);
            fpsctrltasklist[cmdindexExec].status |=
                FPSTASK_STATUS_RUNNING; // update status to running
        }
    }


    return NBtaskLaunched;
}






/**
 * @brief FPS start RUN process
 * 
 * Requires setup performed by milk-fpsinit, which performs the following setup
 * - creates the FPS shared memory
 * - create up tmux sessions
 * - create function fpsrunstart, fpsrunstop, fpsconfstart and fpsconfstop
 */ 
errno_t functionparameter_RUNstart(
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsindex
)
{

    if(fps[fpsindex].md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CHECKOK)
    {
        // Move to correct launch directory
        EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:run \"cd %s\" C-m",
                               fps[fpsindex].md->name, fps[fpsindex].md->fpsdirectory);

        EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:run \"fpsrunstart\" C-m",
                               fps[fpsindex].md->name);

        fps[fpsindex].md->status |= FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN;
        fps[fpsindex].md->signal |=
            FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // notify GUI loop to update
    }
    return RETURN_SUCCESS;
}





/** @brief FPS stop RUN process
 * 
 * Run pre-set function fpsrunstop in tmux ctrl window
 */ 
errno_t functionparameter_RUNstop(
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsindex
)
{	
    // Move to correct launch directory
    // 
    EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:ctrl \"cd %s\" C-m",
                           fps[fpsindex].md->name, fps[fpsindex].md->fpsdirectory);

	EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:ctrl \"fpsrunstop\" C-m",
                           fps[fpsindex].md->name);

	// Send C-c in case runstop command is not implemented
	EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:run C-c &> /dev/null",
                fps[fpsindex].md->name);

    fps[fpsindex].md->status &= ~FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN;
    fps[fpsindex].md->signal |=
        FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // notify GUI loop to update

    return RETURN_SUCCESS;
}




/**
 * @brief FPS start CONF process
 * 
 * Requires setup performed by milk-fpsinit, which performs the following setup
 * - creates the FPS shared memory
 * - create up tmux sessions
 * - create function fpsrunstart, fpsrunstop, fpsconfstart and fpsconfstop
 */ 

errno_t functionparameter_CONFstart(
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsindex
)
{
    // Move to correct launch directory
    //
    EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:conf \"cd %s\" C-m",
                           fps[fpsindex].md->name, fps[fpsindex].md->fpsdirectory);

    EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:conf \"fpsconfstart\" C-m",
                           fps[fpsindex].md->name);

    fps[fpsindex].md->status |= FUNCTION_PARAMETER_STRUCT_STATUS_CMDCONF;

    // notify GUI loop to update
    fps[fpsindex].md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE;

    return RETURN_SUCCESS;
}




/**
 * @brief FPS stop CONF process
 * 
 */
errno_t functionparameter_CONFstop(
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsindex
)
{
	// send conf stop signal
	fps[fpsindex].md->signal &= ~FUNCTION_PARAMETER_STRUCT_SIGNAL_CONFRUN;

    return RETURN_SUCCESS;
}









/** @brief remove FPS and associated files
 * 
 * Requires CONF and RUN to be off
 * 
 */ 
errno_t functionparameter_FPSremove(
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsindex
)
{

	// get directory name
    char shmdname[STRINGMAXLEN_DIRNAME];
    function_parameter_struct_shmdirname(shmdname);

    // get FPS shm filename
    char fpsfname[STRINGMAXLEN_FULLFILENAME];
    WRITE_FULLFILENAME(fpsfname, "%s/%s.fps.shm", shmdname, fps[fpsindex].md->name);

    // delete sym links
    //EXECUTE_SYSTEM_COMMAND("find %s -follow -type f -name \"fpslog.*%s\" -exec grep -q \"LOGSTART %s\" {} \\; -delete",
    //                       shmdname, fps[fpsindex].md->name, fps[fpsindex].md->name);

    fps[fpsindex].SMfd = -1;
    close(fps[fpsindex].SMfd);

//    remove(conflogfname);
    int ret = remove(fpsfname);
    int errcode = errno;
    
	// TEST
	FILE *fp;
	fp = fopen("rmlist.txt", "a");
	fprintf(fp, "remove %s  %d\n", fpsfname, ret);	
	if(ret == -1)
	{
		switch (errcode) {
		
		case EACCES:
		fprintf(fp, "EACCES\n");
		break;

		case EBUSY:
		fprintf(fp, "EBUSY\n");
		break;

		case ENOENT:
		fprintf(fp, "ENOENT\n");
		break;

		case EPERM:
		fprintf(fp, "EPERM\n");
		break;

		case EROFS:
		fprintf(fp, "EROFS\n");
		break;
		
		}
	}
	fclose(fp);



    // terminate tmux sessions
    EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:ctrl \"exit\" C-m",
                           fps[fpsindex].md->name);
    EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:conf \"exit\" C-m",
                           fps[fpsindex].md->name);
    EXECUTE_SYSTEM_COMMAND("tmux send-keys -t %s:run \"exit\" C-m",
                           fps[fpsindex].md->name);


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
    time_t now;

    clock_gettime(CLOCK_REALTIME, &tnow);
    now = tnow.tv_sec;
    struct tm *uttime;
    uttime = gmtime(&now);

    char timestring[30];
    sprintf(
        timestring,
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





/*
errno_t functionparameter_outlog(
    char *keyw,
    char *msgstring
)
{
    static int LogOutOpen = 0;
    static FILE *fpout;


    if(LogOutOpen == 0)   // file not open
    {
		char logfname[STRINGMAXLEN_FULLFILENAME];
		getFPSlogfname(logfname);

        fpout = fopen(logfname, "a");
        if(fpout == NULL)
        {
            printf("ERROR: cannot open file\n");
            exit(EXIT_FAILURE);
        }
        LogOutOpen = 1;
    }

    functionparameter_outlog_file(keyw, msgstring, fpout);

    if(strcmp(keyw, "LOGFILECLOSE") == 0)
    {
        fclose(fpout);
        LogOutOpen = 1;
    }

    return RETURN_SUCCESS;
}
*/






errno_t functionparameter_outlog(
    char *keyw,
    const char *fmt, ...
)
{
	// identify logfile and open file

    static int LogOutOpen = 0;
    static FILE *fpout;

    if(LogOutOpen == 0)   // file not open
    {
		char logfname[STRINGMAXLEN_FULLFILENAME];
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
    time_t now;

    clock_gettime(CLOCK_REALTIME, &tnow);
    now = tnow.tv_sec;
    struct tm *uttime;
    uttime = gmtime(&now);

    char timestring[30];
    sprintf(
        timestring,
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
        fclose(fpout);
        LogOutOpen = 1;
    }

    return RETURN_SUCCESS;
}





/**
 * @brief Establish sym link for convenience
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
    WRITE_FULLFILENAME(linkfname, "%s/fpslog.%s", shmdname,
                       data.FPS_PROCESS_TYPE);

    if(symlink(logfname, linkfname) == -1)
    {
		PRINT_ERROR("symlink error");
	}


    return RETURN_SUCCESS;
}













static errno_t functionparameter_scan_fps(
    uint32_t mode,
    char *fpsnamemask,
    FUNCTION_PARAMETER_STRUCT *fps,
    KEYWORD_TREE_NODE *keywnode,
    int *ptr_NBkwn,
    int *ptr_fpsindex,
    long *ptr_pindex,
    int verbose
)
{
    int stringmaxlen = 500;
    int fpsindex;
    int pindex;
    //int fps_symlink[NB_FPS_MAX];
    int kwnindex;
    int NBkwn;
    int l;


    // int nodechain[MAXNBLEVELS];
    //int GUIlineSelected[MAXNBLEVELS];

    // FPS list file
    FILE *fpfpslist;
    int fpslistcnt = 0;
    char FPSlist[200][100];



    // Static variables
    static int shmdirname_init = 0;
    static char shmdname[200];




    // scan filesystem for fps entries

    if(verbose > 0)
    {
        printf("\n\n\n====================== SCANNING FPS ON SYSTEM ==============================\n\n");
        fflush(stdout);
    }


    if(shmdirname_init == 0)
    {
        function_parameter_struct_shmdirname(shmdname);
        shmdirname_init = 1;
    }





    // disconnect previous fps
    for(fpsindex = 0; fpsindex < NB_FPS_MAX; fpsindex++)
    {
        if(fps[fpsindex].SMfd > -1)   // connected
        {
            function_parameter_struct_disconnect(&fps[fpsindex]);
        }
    }



    // request match to file ./fpscomd/fpslist.txt
    if(mode & 0x0001)
    {
        if((fpfpslist = fopen("fpscmd/fpslist.txt", "r")) != NULL)
        {
            char *FPSlistline = NULL;
            size_t len = 0;
            ssize_t read;

            while((read = getline(&FPSlistline, &len, fpfpslist)) != -1)
            {
                if(FPSlistline[0] != '#')
                {
                    char *pch;

                    pch = strtok(FPSlistline, " \t\n\r");
                    if(pch != NULL)
                    {
                        sprintf(FPSlist[fpslistcnt], "%s", pch);
                        fpslistcnt++;
                    }
                }
            }
            fclose(fpfpslist);
        }
        else
        {
            if(verbose > 0)
            {
                printf("Cannot open file fpscmd/fpslist.txt\n");
            }
        }

        int fpsi;
        for(fpsi = 0; fpsi < fpslistcnt; fpsi++)
        {
            if(verbose > 0)
            {
                printf("FPSname must match %s\n", FPSlist[fpsi]);
            }
        }
    }




    //  for(l = 0; l < MAXNBLEVELS; l++) {
    // nodechain[l] = 0;
    // GUIlineSelected[l] = 0;
    //}

    for(int kindex = 0; kindex < NB_KEYWNODE_MAX; kindex++)
    {
        keywnode[kindex].NBchild = 0;
    }




    //    NBparam = function_parameter_struct_connect(fpsname, &fps[fpsindex]);


    // create ROOT node (invisible)
    keywnode[0].keywordlevel = 0;
    sprintf(keywnode[0].keyword[0], "ROOT");
    keywnode[0].leaf = 0;
    keywnode[0].NBchild = 0;
    NBkwn = 1;









    DIR *d;
    struct dirent *dir;
    d = opendir(shmdname);
    if(d)
    {
        fpsindex = 0;
        pindex = 0;
        while(((dir = readdir(d)) != NULL))
        {
            char *pch = strstr(dir->d_name, ".fps.shm");

            int matchOK = 0;
            // name filtering
            if(strcmp(fpsnamemask, "_ALL") == 0)
            {
                matchOK = 1;
            }
            else
            {
                if(strncmp(dir->d_name, fpsnamemask, strlen(fpsnamemask)) == 0)
                {
                    matchOK = 1;
                }
            }


            if(mode & 0x0001)   // enforce match to list
            {
                int matchOKlist = 0;
                int fpsi;

                for(fpsi = 0; fpsi < fpslistcnt; fpsi++)
                    if(strncmp(dir->d_name, FPSlist[fpsi], strlen(FPSlist[fpsi])) == 0)
                    {
                        matchOKlist = 1;
                    }

                matchOK *= matchOKlist;
            }




            if((pch) && (matchOK == 1))
            {

                // is file sym link ?
                struct stat buf;
                int retv;
                char fullname[stringmaxlen];
                char shmdname[stringmaxlen];
                function_parameter_struct_shmdirname(shmdname);

                sprintf(fullname, "%s/%s", shmdname, dir->d_name);

                retv = lstat(fullname, &buf);
                if(retv == -1)
                {
					if( screenprintmode == SCREENPRINT_NCURSES) {
						endwin();
					}
                    printf("File \"%s\"", dir->d_name);
                    perror("Error running lstat on file ");
                    printf("File %s line %d\n", __FILE__, __LINE__);
                    fflush(stdout);
                    exit(EXIT_FAILURE);
                }

                if(S_ISLNK(buf.st_mode))   // resolve link name
                {
                    char fullname[stringmaxlen];
                    char linknamefull[stringmaxlen];
                    char linkname[stringmaxlen];

                    char shmdname[stringmaxlen];
                    function_parameter_struct_shmdirname(shmdname);

                    //fps_symlink[fpsindex] = 1;
                    if(snprintf(fullname, stringmaxlen, "%s/%s", shmdname, dir->d_name) < 0)
                    {
                        PRINT_ERROR("snprintf error");
                    }

                    if(readlink(fullname, linknamefull, 200 - 1) == -1)
                    {
                        // todo: replace with realpath()
                        PRINT_ERROR("readlink() error");
                    }
                    strcpy(linkname, basename(linknamefull));

                    int lOK = 1;
                    unsigned int ii = 0;
                    while((lOK == 1) && (ii < strlen(linkname)))
                    {
                        if(linkname[ii] == '.')
                        {
                            linkname[ii] = '\0';
                            lOK = 0;
                        }
                        ii++;
                    }

                    //                        strncpy(streaminfo[sindex].linkname, linkname, nameNBchar);
                }
                //else {
                //  fps_symlink[fpsindex] = 0;
                //}


                //fps_symlink[fpsindex] = 0;


                char fpsname[STRINGMAXLEN_FPS_NAME];
                long strcplen = strlen(dir->d_name) - strlen(".fps.shm");
                int strcplen1 = STRINGMAXLEN_FPS_NAME-1;
                if(strcplen < strcplen1) {
					strcplen1 =  strcplen;
				}

				strncpy(fpsname, dir->d_name, strcplen1);
				fpsname[strcplen1] = '\0';
				

                if(verbose > 0)
                {
                    printf("FOUND FPS %s - (RE)-CONNECTING  [%d]\n", fpsname, fpsindex);
                    fflush(stdout);
                }


                long NBparamMAX = function_parameter_struct_connect(fpsname, &fps[fpsindex],
                                  FPSCONNECT_SIMPLE);


                long pindex0;
                for(pindex0 = 0; pindex0 < NBparamMAX; pindex0++)
                {
                    if(fps[fpsindex].parray[pindex0].fpflag & FPFLAG_ACTIVE)   // if entry is active
                    {
                        // find or allocate keyword node
                        int level;
                        for(level = 1; level < fps[fpsindex].parray[pindex0].keywordlevel + 1; level++)
                        {

                            // does node already exist ?
                            int scanOK = 0;
                            for(kwnindex = 0; kwnindex < NBkwn;
                                    kwnindex++)   // scan existing nodes looking for match
                            {
                                if(keywnode[kwnindex].keywordlevel == level)   // levels have to match
                                {
                                    int match = 1;
                                    for(l = 0; l < level; l++)   // keywords at all levels need to match
                                    {
                                        if(strcmp(fps[fpsindex].parray[pindex0].keyword[l],
                                                  keywnode[kwnindex].keyword[l]) != 0)
                                        {
                                            match = 0;
                                        }
                                        //                        printf("TEST MATCH : %16s %16s  %d\n", fps[fpsindex].parray[i].keyword[l], keywnode[kwnindex].keyword[l], match);
                                    }
                                    if(match == 1)   // we have a match
                                    {
                                        scanOK = 1;
                                    }
                                    //             printf("   -> %d\n", scanOK);
                                }
                            }



                            if(scanOK == 0)   // node does not exit -> create it
                            {

                                // look for parent
                                int scanparentOK = 0;
                                int kwnindexp = 0;
                                keywnode[kwnindex].parent_index =
                                    0; // default value, not found -> assigned to ROOT

                                while((kwnindexp < NBkwn) && (scanparentOK == 0))
                                {
                                    if(keywnode[kwnindexp].keywordlevel == level - 1)   // check parent has level-1
                                    {
                                        int match = 1;

                                        for(l = 0; l < level - 1; l++)   // keywords at all levels need to match
                                        {
                                            if(strcmp(fps[fpsindex].parray[pindex0].keyword[l],
                                                      keywnode[kwnindexp].keyword[l]) != 0)
                                            {
                                                match = 0;
                                            }
                                        }
                                        if(match == 1)   // we have a match
                                        {
                                            scanparentOK = 1;
                                        }
                                    }
                                    kwnindexp++;
                                }

                                if(scanparentOK == 1)
                                {
                                    keywnode[kwnindex].parent_index = kwnindexp - 1;
                                    int cindex;
                                    cindex = keywnode[keywnode[kwnindex].parent_index].NBchild;
                                    keywnode[keywnode[kwnindex].parent_index].child[cindex] = kwnindex;
                                    keywnode[keywnode[kwnindex].parent_index].NBchild++;
                                }

                                if(verbose > 0)
                                {
                                    printf("CREATING NODE %d ", kwnindex);
                                }
                                keywnode[kwnindex].keywordlevel = level;

                                for(l = 0; l < level; l++)
                                {
                                    char tmpstring[200];
                                    strcpy(keywnode[kwnindex].keyword[l], fps[fpsindex].parray[pindex0].keyword[l]);
                                    printf(" %s", keywnode[kwnindex].keyword[l]);
                                    if(l == 0)
                                    {
                                        strcpy(keywnode[kwnindex].keywordfull, keywnode[kwnindex].keyword[l]);
                                    }
                                    else
                                    {
                                        sprintf(tmpstring, ".%s", keywnode[kwnindex].keyword[l]);
                                        strcat(keywnode[kwnindex].keywordfull, tmpstring);
                                    }
                                }
                                if(verbose > 0)
                                {
                                    printf("   %d %d\n", keywnode[kwnindex].keywordlevel,
                                           fps[fpsindex].parray[pindex0].keywordlevel);
                                }

                                if(keywnode[kwnindex].keywordlevel ==
                                        fps[fpsindex].parray[pindex0].keywordlevel)
                                {
                                    //									strcpy(keywnode[kwnindex].keywordfull, fps[fpsindex].parray[i].keywordfull);

                                    keywnode[kwnindex].leaf = 1;
                                    keywnode[kwnindex].fpsindex = fpsindex;
                                    keywnode[kwnindex].pindex = pindex0;
                                }
                                else
                                {


                                    keywnode[kwnindex].leaf = 0;
                                    keywnode[kwnindex].fpsindex = fpsindex;
                                    keywnode[kwnindex].pindex = 0;
                                }

                                kwnindex ++;
                                NBkwn = kwnindex;
                            }
                        }
                        pindex++;
                    }
                }

                if(verbose > 0)
                {
                    printf("--- FPS %4d  %-20s %ld parameters\n", fpsindex, fpsname,
                           fps[fpsindex].md->NBparamMAX);
                }


                fpsindex ++;
            }
        }
        closedir(d);
    }
    else
    {
        char shmdname[200];
        function_parameter_struct_shmdirname(shmdname);
        printf("ERROR: missing %s directory\n", shmdname);
        printf("File %s line %d\n", __FILE__, __LINE__);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }


    if(verbose > 0)
    {
        printf("\n\n=================[END] SCANNING FPS ON SYSTEM [END]=  %d  ========================\n\n\n",
               fpsindex);
        fflush(stdout);
    }

    *ptr_NBkwn = NBkwn;
    *ptr_fpsindex = fpsindex;
    *ptr_pindex = pindex;



    return RETURN_SUCCESS;
}










void functionparameter_CTRLscreen_atexit()
{
    //printf("exiting CTRLscreen\n");

    // endwin();
}





inline static void print_help_entry(char *key, char *descr)
{
    screenprint_setbold();
    printfw("    %4s", key);
    screenprint_unsetbold();
    printfw("   %s\n", descr);
}





inline static void fpsCTRLscreen_print_DisplayMode_status(
    int fpsCTRL_DisplayMode, int NBfps)
{

    int stringmaxlen = 500;
    char  monstring[stringmaxlen];

    screenprint_setbold();
    if(snprintf(monstring, stringmaxlen,
                "[%d %d] FUNCTION PARAMETER MONITOR: PRESS (x) TO STOP, (h) FOR HELP   PID %d  [%d FPS]",
                wrow, wcol, (int) getpid(), NBfps) < 0)
    {
        PRINT_ERROR("snprintf error");
    }
    function_parameter__print_header(monstring, '-');
    screenprint_unsetbold();
    printfw("\n");

    if(fpsCTRL_DisplayMode == 1)
    {
        screenprint_setreverse();
        printfw("[h] Help");
        screenprint_unsetreverse();
    }
    else
    {
        printfw("[h] Help");
    }
    printfw("   ");

    if(fpsCTRL_DisplayMode == 2)
    {
        screenprint_setreverse();
        printfw("[F2] FPS CTRL");
        screenprint_unsetreverse();
    }
    else
    {
        printfw("[F2] FPS CTRL");
    }
    printfw("   ");

    if(fpsCTRL_DisplayMode == 3)
    {
        screenprint_setreverse();
        printfw("[F3] Sequencer");
        screenprint_unsetreverse();
    }
    else
    {
        printfw("[F3] Sequencer");
    }
    printfw("\n");
}



inline static void fpsCTRLscreen_print_help()
{
    // int attrval = A_BOLD;

    printfw("\n");
    print_help_entry("x", "Exit");

    printfw("\n============ SCREENS \n");
    print_help_entry("h", "Help screen");
    print_help_entry("F2", "FPS control screen");
    print_help_entry("F3", "FPS command list (Sequencer)");

    printfw("\n============ OTHER \n");
    print_help_entry("s",   "rescan");
    print_help_entry("e",   "erase FPS");
    print_help_entry("E",   "erase FPS and tmux sessions");
    print_help_entry("u",   "update CONF process");
    print_help_entry("C/c", "start/stop CONF process");
    print_help_entry("R/r", "start/stop RUN process");
    print_help_entry("l",   "list all entries");
    print_help_entry(">",   "export values to disk");
    print_help_entry("<",   "import values from disk");
    print_help_entry("P",   "(P)rocess input file \"confscript\"");
    printfw("        format: setval <paramfulname> <value>\n");
}





inline static void fpsCTRLscreen_print_nodeinfo(
    FUNCTION_PARAMETER_STRUCT *fps,
    KEYWORD_TREE_NODE *keywnode,
    int nodeSelected,
    int fpsindexSelected,
    int pindexSelected)
{

    DEBUG_TRACEPOINT("Selected node %d in fps %d",
                     nodeSelected,
                     keywnode[nodeSelected].fpsindex);

    printfw("======== FPS info ( # %5d)\n",
           keywnode[nodeSelected].fpsindex);


    char teststring[200];
    sprintf(teststring, "%s", fps[keywnode[nodeSelected].fpsindex].md->sourcefname);
    DEBUG_TRACEPOINT("TEST STRING : %s", teststring);


    DEBUG_TRACEPOINT("TEST LINE : %d",
                     fps[keywnode[nodeSelected].fpsindex].md->sourceline);

    printfw("    FPS source            : %s %d\n",
           fps[keywnode[nodeSelected].fpsindex].md->sourcefname,
           fps[keywnode[nodeSelected].fpsindex].md->sourceline);

    DEBUG_TRACEPOINT(" ");
    printfw("    FPS root directory    : %s\n",
           fps[keywnode[nodeSelected].fpsindex].md->fpsdirectory);

    DEBUG_TRACEPOINT(" ");
    printfw("    FPS tmux sessions     :  %s:conf  %s:run\n",
           fps[keywnode[nodeSelected].fpsindex].md->name,
           fps[keywnode[nodeSelected].fpsindex].md->name);

    DEBUG_TRACEPOINT(" ");
    printfw("======== NODE info ( # %5ld)\n", nodeSelected);
    printfw("%-30s ", keywnode[nodeSelected].keywordfull);

    if(keywnode[nodeSelected].leaf > 0)   // If this is not a directory
    {
        char typestring[100];
        functionparameter_GetTypeString(
            fps[fpsindexSelected].parray[pindexSelected].type,
            typestring);
        printfw("type %s\n", typestring);

        // print binary flag
        printfw("FLAG : ");
        uint64_t mask = (uint64_t) 1 << (sizeof(uint64_t) * CHAR_BIT - 1);
        while(mask)
        {
            int digit = fps[fpsindexSelected].parray[pindexSelected].fpflag & mask ? 1 : 0;
            if(digit == 1)
            {
                screenprint_setcolor(2);
                printfw("%d", digit);
                screenprint_unsetcolor(2);
            }
            else
            {
                printfw("%d", digit);
            }
            mask >>= 1;
        }
    }
    else
    {
        printfw("-DIRECTORY-\n");
    }
    printfw("\n\n");
}







inline static void fpsCTRLscreen_level0node_summary(
    FUNCTION_PARAMETER_STRUCT *fps,
    int fpsindex)
{
    pid_t pid;

    pid = fps[fpsindex].md->confpid;
    if((getpgid(pid) >= 0) && (pid > 0))
    {
        screenprint_setcolor(2);
        printfw("%07d ", (int) pid);
        screenprint_unsetcolor(2);
    }
    else     // PID not active
    {
        if(fps[fpsindex].md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CMDCONF)
        {
            // not clean exit
            screenprint_setcolor(4);
            printfw("%07d ", (int) pid);
            screenprint_unsetcolor(4);
        }
        else
        {
            // All OK
            printfw("%07d ", (int) pid);
        }
    }


    if(fps[fpsindex].md->conferrcnt > 99)
    {
        screenprint_setcolor(4);
        printfw("[XX]");
        screenprint_unsetcolor(4);
    }
    if(fps[fpsindex].md->conferrcnt > 0)
    {
        screenprint_setcolor(4);
        printfw("[%02d]", fps[fpsindex].md->conferrcnt);
        screenprint_unsetcolor(4);
    }
    if(fps[fpsindex].md->conferrcnt == 0)
    {
        screenprint_setcolor(2);
        printfw("[%02d]", fps[fpsindex].md->conferrcnt);
        screenprint_unsetcolor(2);
    }

    pid = fps[fpsindex].md->runpid;
    if((getpgid(pid) >= 0) && (pid > 0))
    {
        screenprint_setcolor(2);
        printfw("%07d ", (int) pid);
        screenprint_unsetcolor(2);
    }
    else
    {
        if(fps[fpsindex].md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN)
        {
            // not clean exit
            screenprint_setcolor(4);
            printfw("%07d ", (int) pid);
            screenprint_unsetcolor(4);
        }
        else
        {
            // All OK
            printfw("%07d ", (int) pid);
        }
    }

}










inline static int fpsCTRLscreen_process_user_key(
    int ch,
    FUNCTION_PARAMETER_STRUCT *fps,
    KEYWORD_TREE_NODE *keywnode,
    FPSCTRL_TASK_ENTRY *fpsctrltasklist,
    FPSCTRL_TASK_QUEUE *fpsctrlqueuelist,
    FPSCTRL_PROCESS_VARS *fpsCTRLvar
)
{
    int stringmaxlen = 500;
    int loopOK = 1;
    int fpsindex;
    int pindex;
    FILE *fpinputcmd;

    char command[stringmaxlen];
    char msg[stringmaxlen];

	FILE *fpoutval;
	errno_t ret;
	char outfpstring[stringmaxlen];
	char fname[STRINGMAXLEN_FULLFILENAME];

	FILE *fpin;

    switch(ch)
    {
        case 'x':     // Exit control screen
            loopOK = 0;
            break;

        // ============ SCREENS

        case 'h': // help
            fpsCTRLvar->fpsCTRL_DisplayMode = 1;
            break;

        case KEY_F(2): // control
            fpsCTRLvar->fpsCTRL_DisplayMode = 2;
            break;

        case KEY_F(3): // scheduler
            fpsCTRLvar->fpsCTRL_DisplayMode = 3;
            break;

        case 's' : // (re)scan
            functionparameter_scan_fps(
                fpsCTRLvar->mode,
                fpsCTRLvar->fpsnamemask,
                fps,
                keywnode,
                &fpsCTRLvar->NBkwn,
                &fpsCTRLvar->NBfps,
                &fpsCTRLvar->NBindex,
                0);
            clear();
            break;

        case 'e' : // erase FPS
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            functionparameter_FPSremove(fps, fpsindex);

            functionparameter_scan_fps(
                fpsCTRLvar->mode,
                fpsCTRLvar->fpsnamemask,
                fps,
                keywnode,
                &fpsCTRLvar->NBkwn,
                &(fpsCTRLvar->NBfps),
                &fpsCTRLvar->NBindex,
                0);
            clear();
            //DEBUG_TRACEPOINT("fpsCTRLvar->NBfps = %d\n", fpsCTRLvar->NBfps);
            // abort();
            fpsCTRLvar->run_display = 0; // skip next display
            fpsCTRLvar->fpsindexSelected =
                0; // safeguard in case current selection disappears
            break;

        case 'E' : // Erase FPS and close tmux sessions
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;

            functionparameter_FPSremove(fps, fpsindex);
            functionparameter_scan_fps(
                fpsCTRLvar->mode,
                fpsCTRLvar->fpsnamemask,
                fps,
                keywnode,
                &fpsCTRLvar->NBkwn,
                &fpsCTRLvar->NBfps,
                &fpsCTRLvar->NBindex, 0);
            clear();
            DEBUG_TRACEPOINT(" ");
            // safeguard in case current selection disappears
            fpsCTRLvar->fpsindexSelected = 0; 
            break;

        case KEY_UP:
            fpsCTRLvar->direction = -1;
            fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] --;
            if(fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] < 0)
            {
                fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] = 0;
            }
            break;


        case KEY_DOWN:
            fpsCTRLvar->direction = 1;
            fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] ++;
            if(fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] > fpsCTRLvar->NBindex -
                    1)
            {
                fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] = fpsCTRLvar->NBindex - 1;
            }
            if(fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] >
                    keywnode[fpsCTRLvar->directorynodeSelected].NBchild - 1)
            {
                fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] =
                    keywnode[fpsCTRLvar->directorynodeSelected].NBchild - 1;
            }
            break;

        case KEY_PPAGE:
            fpsCTRLvar->direction = -1;
            fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] -= 10;
            if(fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] < 0)
            {
                fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] = 0;
            }
            break;

        case KEY_NPAGE:
            fpsCTRLvar->direction = 1;
            fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] += 10;
            while(fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] >
                    fpsCTRLvar->NBindex - 1)
            {
                fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] = fpsCTRLvar->NBindex - 1;
            }
            while(fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] >
                    keywnode[fpsCTRLvar->directorynodeSelected].NBchild - 1)
            {
                fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel] =
                    keywnode[fpsCTRLvar->directorynodeSelected].NBchild - 1;
            }
            break;


        case KEY_LEFT:
            if(fpsCTRLvar->directorynodeSelected != 0)   // ROOT has no parent
            {
                fpsCTRLvar->directorynodeSelected =
                    keywnode[fpsCTRLvar->directorynodeSelected].parent_index;
                fpsCTRLvar->nodeSelected = fpsCTRLvar->directorynodeSelected;
            }
            break;


        case KEY_RIGHT :
            if(keywnode[fpsCTRLvar->nodeSelected].leaf == 0)   // this is a directory
            {
                if(keywnode[keywnode[fpsCTRLvar->directorynodeSelected].child[fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel]]].leaf
                        == 0)
                {
                    fpsCTRLvar->directorynodeSelected =
                        keywnode[fpsCTRLvar->directorynodeSelected].child[fpsCTRLvar->GUIlineSelected[fpsCTRLvar->currentlevel]];
                    fpsCTRLvar->nodeSelected = fpsCTRLvar->directorynodeSelected;
                }
            }
            break;

        case 10 : // enter key
            if(keywnode[fpsCTRLvar->nodeSelected].leaf == 1)   // this is a leaf
            {
				if( screenprintmode == SCREENPRINT_NCURSES) {
					endwin();
				}				
                if(system("clear") != 0)   // clear screen
                {
                    PRINT_ERROR("system() returns non-zero value");
                }
                functionparameter_UserInputSetParamValue(&fps[fpsCTRLvar->fpsindexSelected],
                        fpsCTRLvar->pindexSelected);
                if( screenprintmode == SCREENPRINT_NCURSES) {
					initncurses();
				}
				if( screenprintmode == SCREENPRINT_STDIO ) 
				{
					printf("\e[1;1H\e[2J");
				}
            }
            break;

        case ' ' :
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            pindex = keywnode[fpsCTRLvar->nodeSelected].pindex;

            // toggles ON / OFF - this is a special case not using function functionparameter_UserInputSetParamValue
            if(fps[fpsindex].parray[pindex].fpflag & FPFLAG_WRITESTATUS)
            {
                if(fps[fpsindex].parray[pindex].type == FPTYPE_ONOFF)
                {

                    if(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ONOFF)    // ON -> OFF
                    {
                        fps[fpsindex].parray[pindex].fpflag &= ~FPFLAG_ONOFF;
                    }
                    else     // OFF -> ON
                    {
                        fps[fpsindex].parray[pindex].fpflag |= FPFLAG_ONOFF;
                    }

                    // Save to disk
                    if(fps[fpsindex].parray[pindex].fpflag & FPFLAG_SAVEONCHANGE)
                    {
                        functionparameter_WriteParameterToDisk(&fps[fpsindex], pindex, "setval",
                                                               "UserInputSetParamValue");
                    }
                    fps[fpsindex].parray[pindex].cnt0 ++;
                    fps[fpsindex].md->signal |=
                        FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // notify GUI loop to update
                }
            }

            if(fps[fpsindex].parray[pindex].type == FPTYPE_EXECFILENAME)
            {
                if(snprintf(command, stringmaxlen, "tmux send-keys -t %s:run \"cd %s\" C-m",
                            fps[fpsindex].md->name, fps[fpsindex].md->fpsdirectory) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                if(system(command) != 0)
                {
                    PRINT_ERROR("system() returns non-zero value");
                }
                if(snprintf(command, stringmaxlen, "tmux send-keys -t %s:run \"%s %s\" C-m",
                            fps[fpsindex].md->name, fps[fpsindex].parray[pindex].val.string[0],
                            fps[fpsindex].md->name) < 0)
                {
                    PRINT_ERROR("snprintf error");
                }
                if(system(command) != 0)
                {
                    PRINT_ERROR("system() returns non-zero value");
                }
            }

            break;


        case 'u' : // update conf process
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            fps[fpsindex].md->signal |=
                FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE; // notify GUI loop to update
            if(snprintf(msg, stringmaxlen, "UPDATE %s", fps[fpsindex].md->name) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            functionparameter_outlog("FPSCTRL", "%s", msg);
            //functionparameter_CONFupdate(fps, fpsindex);
            break;

        case 'R' : // start run process if possible
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            if(snprintf(msg, stringmaxlen, "RUNSTART %s", fps[fpsindex].md->name) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            functionparameter_outlog("FPSCTRL", msg);
            functionparameter_RUNstart(fps, fpsindex);
            break;

        case 'r' : // stop run process
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            if(snprintf(msg, stringmaxlen, "RUNSTOP %s", fps[fpsindex].md->name) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            functionparameter_outlog("FPSCTRL", msg);
            functionparameter_RUNstop(fps, fpsindex);
            break;


        case 'C' : // start conf process
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            if(snprintf(msg, stringmaxlen, "CONFSTART %s", fps[fpsindex].md->name) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            functionparameter_outlog("FPSCTRL", msg);
            functionparameter_CONFstart(fps, fpsindex);
            break;

        case 'c': // kill conf process
            fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
            if(snprintf(msg, stringmaxlen, "CONFSTOP %s", fps[fpsindex].md->name) < 0)
            {
                PRINT_ERROR("snprintf error");
            }
            functionparameter_outlog("FPSCTRL", msg);
            functionparameter_CONFstop(fps, fpsindex);
            break;

        case 'l': // list all parameters
			if( screenprintmode == SCREENPRINT_NCURSES) {
				endwin();
			}
            if(system("clear") != 0)
            {
                PRINT_ERROR("system() returns non-zero value");
            }
            printf("FPS entries - Full list \n");
            printf("\n");
            for(int kwnindex = 0; kwnindex < fpsCTRLvar->NBkwn; kwnindex++)
            {
                if(keywnode[kwnindex].leaf == 1)
                {
                    printf("%4d  %4d  %s\n", keywnode[kwnindex].fpsindex, keywnode[kwnindex].pindex,
                           keywnode[kwnindex].keywordfull);
                }
            }
            printf("  TOTAL :  %d nodes\n", fpsCTRLvar->NBkwn);
            printf("\n");
            printf("Press Any Key to Continue\n");
            getchar();
            if( screenprintmode == SCREENPRINT_NCURSES) {
				initncurses();
			}
            break;
        
        
        case '>': // export values to disk
			fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
			sprintf(fname, "%s/fps.%s.outlog", fps[fpsindex].md->fpsdirectory, fps[fpsindex].md->name);
			fpoutval = fopen(fname, "w");
			for(int kwnindex = 0; kwnindex < fpsCTRLvar->NBkwn; kwnindex++)
			{
				if( (keywnode[kwnindex].leaf == 1) && (keywnode[kwnindex].fpsindex==fpsindex) )
				{
					pindex = keywnode[kwnindex].pindex;
					ret = functionparameter_PrintParameter_ValueString(&fps[fpsindex].parray[pindex], outfpstring, stringmaxlen);
					if(ret == RETURN_SUCCESS)
						fprintf(fpoutval, "%s\n", outfpstring);
				}
			}
			fclose(fpoutval);
			break;


        case '<': // import settings
			if( screenprintmode == SCREENPRINT_NCURSES) {
				endwin();
            }
            if(system("clear") != 0)
            {
                PRINT_ERROR("system() returns non-zero value");
            }
			fpsindex = keywnode[fpsCTRLvar->nodeSelected].fpsindex;
			sprintf(fname, "%s/fps.%s.setup", fps[fpsindex].md->fpsdirectory, fps[fpsindex].md->name);		
			printf("READING FILE %s\n", fname);	
			fpin = fopen(fname, "r");
			if(fpin != NULL)
			{				
				char *FPScmdline = NULL;
                size_t len = 0;
                ssize_t read;

                while((read = getline(&FPScmdline, &len, fpin)) != -1)
                {   
					uint64_t taskstatus = 0;
					printf("READING CMD: %s\n", FPScmdline);
                    functionparameter_FPSprocess_cmdline(FPScmdline, fpsctrlqueuelist, keywnode,
                                                         fpsCTRLvar, fps, &taskstatus);
                }				
				fclose(fpin);
			}
			else
			{
				printf("File not found\n");
			}
			sleep(5);
            if( screenprintmode == SCREENPRINT_NCURSES) {
				initncurses();
			}
			break;
			

        case 'F': // process FIFO
			if( screenprintmode == SCREENPRINT_NCURSES) {
				endwin();
            }
            if(system("clear") != 0)
            {
                PRINT_ERROR("system() returns non-zero value");
            }
            printf("Reading FIFO file \"%s\"  fd=%d\n", fpsCTRLvar->fpsCTRLfifoname,
                   fpsCTRLvar->fpsCTRLfifofd);

            if(fpsCTRLvar->fpsCTRLfifofd > 0)
            {
                // int verbose = 1;
                functionparameter_read_fpsCMD_fifo(fpsCTRLvar->fpsCTRLfifofd, fpsctrltasklist,
                                                   fpsctrlqueuelist);
            }

            printf("\n");
            printf("Press Any Key to Continue\n");
            getchar();
            if( screenprintmode == SCREENPRINT_NCURSES) {
				initncurses();
			}
            break;


        case 'P': // process input command file
			if( screenprintmode == SCREENPRINT_NCURSES) {
				endwin();
            }
            if(system("clear") != 0)
            {
                PRINT_ERROR("system() returns non-zero value");
            }
            printf("Reading file confscript\n");
            fpinputcmd = fopen("confscript", "r");
            if(fpinputcmd != NULL)
            {
                char *FPScmdline = NULL;
                size_t len = 0;
                ssize_t read;

                while((read = getline(&FPScmdline, &len, fpinputcmd)) != -1)
                {
					uint64_t taskstatus = 0;
                    printf("Processing line : %s\n", FPScmdline);
                    functionparameter_FPSprocess_cmdline(FPScmdline, fpsctrlqueuelist, keywnode,
                                                         fpsCTRLvar, fps, &taskstatus);
                }
                fclose(fpinputcmd);
            }

            printf("\n");
            printf("Press Any Key to Continue\n");
            getchar();
            if( screenprintmode == SCREENPRINT_NCURSES) {
				initncurses();
			}
            break;
    }


    return(loopOK);
}






/**
 * ## Purpose
 *
 * Automatically build simple ASCII GUI from function parameter structure (fps) name mask
 *
 *
 *
 */
errno_t functionparameter_CTRLscreen(
    uint32_t mode,
    char *fpsnamemask,
    char *fpsCTRLfifoname
)
{
    //int stringmaxlen = 500;

    // function parameter structure(s)
    int fpsindex;

    FPSCTRL_PROCESS_VARS fpsCTRLvar;

    FUNCTION_PARAMETER_STRUCT *fps;


    // function parameters
    long NBpindex = 0;
    long pindex;

    // keyword tree
    KEYWORD_TREE_NODE *keywnode;

    int level;

    int loopOK = 1;
    long long loopcnt = 0;

	long NBtaskLaunchedcnt = 0;

    int nodechain[MAXNBLEVELS];


    // What to run ?
    // disable for testing
    int run_display = 1;
    loopOK = 1;


    struct timespec tnow;
    clock_gettime(CLOCK_REALTIME, &tnow);
    data.FPS_TIMESTAMP = tnow.tv_sec;
    strcpy(data.FPS_PROCESS_TYPE, "ctrl");

    functionparameter_outlog("FPSCTRL", "START\n");

    DEBUG_TRACEPOINT("function start");



    // initialize fpsCTRLvar
    fpsCTRLvar.exitloop              = 0;
    fpsCTRLvar.mode                  = mode;
    fpsCTRLvar.nodeSelected          = 1;
    fpsCTRLvar.run_display           = run_display;
    fpsCTRLvar.fpsindexSelected      = 0;
    fpsCTRLvar.pindexSelected        = 0;
    fpsCTRLvar.directorynodeSelected = 0;
    fpsCTRLvar.currentlevel          = 0;
    fpsCTRLvar.direction             = 1;
    strcpy(fpsCTRLvar.fpsnamemask, fpsnamemask);
    strcpy(fpsCTRLvar.fpsCTRLfifoname, fpsCTRLfifoname);


    fpsCTRLvar.fpsCTRL_DisplayMode = 2;
    // 1: [h]  help
    // 2: [F2] list of conf and run
    // 3: [F3] fpscmdarray





    // allocate memory


    // Array holding fps structures
    //
    fps = (FUNCTION_PARAMETER_STRUCT *) malloc(sizeof(FUNCTION_PARAMETER_STRUCT) *
            NB_FPS_MAX);


    // Initialize file descriptors to -1
    //
    for(fpsindex = 0; fpsindex < NB_FPS_MAX; fpsindex++)
    {
        fps[fpsindex].SMfd = -1;
    }

    // All parameters held in this array
    //
    keywnode = (KEYWORD_TREE_NODE *) malloc(sizeof(KEYWORD_TREE_NODE) *
                                            NB_KEYWNODE_MAX);
    for(int kn = 0; kn < NB_KEYWNODE_MAX; kn++)
    {
        strcpy(keywnode[kn].keywordfull, "");
        for(int ch = 0; ch < MAX_NB_CHILD; ch++)
        {
            keywnode[kn].child[ch] = 0;
        }
    }



    // initialize nodechain
    for(int l = 0; l < MAXNBLEVELS; l++)
    {
        nodechain[l] = 0;
    }



    // Set up instruction buffer to sequence commands
    //
    FPSCTRL_TASK_ENTRY *fpsctrltasklist;
    fpsctrltasklist = (FPSCTRL_TASK_ENTRY *) malloc(sizeof(FPSCTRL_TASK_ENTRY) *
                      NB_FPSCTRL_TASK_MAX);
    for(int cmdindex = 0; cmdindex < NB_FPSCTRL_TASK_MAX; cmdindex++)
    {
        fpsctrltasklist[cmdindex].status = 0;
        fpsctrltasklist[cmdindex].queue = 0;
    }

    // Set up task queue list
    //
    FPSCTRL_TASK_QUEUE *fpsctrlqueuelist;
    fpsctrlqueuelist = (FPSCTRL_TASK_QUEUE *) malloc(sizeof(
                           FPSCTRL_TASK_QUEUE) * NB_FPSCTRL_TASKQUEUE_MAX);
    for(int queueindex = 0; queueindex < NB_FPSCTRL_TASKQUEUE_MAX; queueindex++)
    {
        fpsctrlqueuelist[queueindex].priority = 1; // 0 = not active
    }


#ifndef STANDALONE
    set_signal_catch();
#endif



    // fifo
    fpsCTRLvar.fpsCTRLfifofd = open(fpsCTRLvar.fpsCTRLfifoname,
                                    O_RDWR | O_NONBLOCK);
    long fifocmdcnt = 0;


    for(level = 0; level < MAXNBLEVELS; level++)
    {
        fpsCTRLvar.GUIlineSelected[level] = 0;
    }


	

    functionparameter_scan_fps(
        fpsCTRLvar.mode,
        fpsCTRLvar.fpsnamemask,
        fps,
        keywnode,
        &fpsCTRLvar.NBkwn,
        &fpsCTRLvar.NBfps,
        &NBpindex, 1);
        
    printf("%d function parameter structure(s) imported, %ld parameters\n",
           fpsCTRLvar.NBfps, NBpindex);
    fflush(stdout);


    DEBUG_TRACEPOINT(" ");



    if(fpsCTRLvar.NBfps == 0)
    {
        printf("No function parameter structure found\n");
        printf("File %s line %d\n", __FILE__, __LINE__);
        fflush(stdout);

        return RETURN_SUCCESS;
    }

    fpsCTRLvar.nodeSelected = 1;
    fpsindex = 0;






    // default: use ncurses
    screenprintmode = SCREENPRINT_NCURSES;

    if(getenv("MILK_FPSCTRL_PRINT_STDIO"))
    {
        // use stdio instead of ncurses
        screenprintmode = SCREENPRINT_STDIO;
    }

    if(getenv("MILK_FPSCTRL_NOPRINT"))
    {
        screenprintmode = SCREENPRINT_NONE;
    }





    // INITIALIZE ncurses

    if(run_display == 1)
    {
        if( screenprintmode == SCREENPRINT_NCURSES) // ncurses mode
        {
            initncurses();
            atexit(functionparameter_CTRLscreen_atexit);
            clear();
        }
        else
        {
            inittermios();
            
            // get terminal size
            struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

			wrow = w.ws_row;
			wcol = w.ws_col;            
        }

    }




    fpsCTRLvar.NBindex = 0;
    char shmdname[200];
    function_parameter_struct_shmdirname(shmdname);



    if(run_display == 0)
    {
        loopOK = 0;
    }

	int getchardt_us_ref = 100000;  // how long between getchar probes
	// refresh every 1 sec without input
	int refreshtimeoutus_ref = 1000000;

    int getchardt_us = getchardt_us_ref;        
    int refreshtimeoutus = refreshtimeoutus_ref;

	if( screenprintmode == SCREENPRINT_NCURSES ) // ncurses mode
	{
		refreshtimeoutus_ref = 100000; // 10 Hz
	}

	int refresh_screen = 1; // 1 if screen should be refreshed
    while(loopOK == 1)
    {
		int NBtaskLaunched = 0;
		
        long icnt = 0;
        int ch = -1;


        int timeoutuscnt = 0;
        
        
        while ( refresh_screen == 0 ) // wait for input
        {
			// put input commands from fifo into the task queue
            int fcnt = functionparameter_read_fpsCMD_fifo(fpsCTRLvar.fpsCTRLfifofd,
                       fpsctrltasklist, fpsctrlqueuelist);

            DEBUG_TRACEPOINT(" ");

			// execute next command in the queue
			int taskflag = function_parameter_process_fpsCMDarray(fpsctrltasklist, fpsctrlqueuelist,
                                                   keywnode, &fpsCTRLvar, fps); 
            
            if(taskflag > 0) // task has been performed
            {
				getchardt_us = 1000; // check often
			}
			else
			{
				getchardt_us = (int) (1.01*getchardt_us); // gradually slow down
				if(getchardt_us > getchardt_us_ref) 
				{
					getchardt_us = getchardt_us_ref;
				}
			}
            NBtaskLaunched += taskflag;
            
			NBtaskLaunchedcnt += NBtaskLaunched;
			
            fifocmdcnt += fcnt;




            if(screenprintmode == SCREENPRINT_STDIO) // stdio mode
            {
                usleep(getchardt_us);
            }
            else
            {
                usleep(getchardt_us); 
            }


            // ==================
            // = GET USER INPUT =
            // ==================
			ch = get_singlechar_nonblock();

            if(ch == -1)
            {
                refresh_screen = 0;
            }
            else
            {
                refresh_screen = 2;
            }

            //tcnt ++;
			timeoutuscnt += getchardt_us;
            if (timeoutuscnt > refreshtimeoutus)
            {
                refresh_screen = 1;                
            }
        }

		if ( refresh_screen > 0 )
		{
			refresh_screen --; // will wait next time we enter the loop
		}


        if(screenprintmode == SCREENPRINT_STDIO) // stdio mode
        {
            printf("\e[1;1H\e[2J");
            //printf("[%12lld  %d %d %d ]  ", loopcnt, buffd[0], buffd[1], buffd[2]);
            
            // update terminal size
            struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

			wrow = w.ws_row;
			wcol = w.ws_col; 
        }










        loopOK = fpsCTRLscreen_process_user_key(
                     ch,
                     fps,
                     keywnode,
                     fpsctrltasklist,
                     fpsctrlqueuelist,
                     &fpsCTRLvar
                 );

        if(fpsCTRLvar.exitloop == 1) 
        {
			loopOK = 0;
		}		




        if(fpsCTRLvar.NBfps == 0)
        {
            if( screenprintmode == SCREENPRINT_NCURSES) {
                endwin();
            }

            printf("\n fpsCTRLvar.NBfps = %d ->  No FPS on system - nothing to display\n",
                   fpsCTRLvar.NBfps);
            return RETURN_FAILURE;
        }







        if(fpsCTRLvar.run_display == 1)
        {
            if( screenprintmode == SCREENPRINT_NCURSES)
            {
                erase();
            }

            fpsCTRLscreen_print_DisplayMode_status(fpsCTRLvar.fpsCTRL_DisplayMode,
                                                   fpsCTRLvar.NBfps);



            DEBUG_TRACEPOINT(" ");
            
            printfw("======== FPSCTRL info  ( screen refresh cnt %7ld  scan interval %7ld us)\n", loopcnt, getchardt_us);
            printfw("    INPUT FIFO       :  %s (fd=%d)    fifocmdcnt = %ld   NBtaskLaunched = %d -> %d\n",
                    fpsCTRLvar.fpsCTRLfifoname, fpsCTRLvar.fpsCTRLfifofd, fifocmdcnt, NBtaskLaunched, NBtaskLaunchedcnt);
                    

            DEBUG_TRACEPOINT(" ");
            char logfname[STRINGMAXLEN_FULLFILENAME];
            getFPSlogfname(logfname);
            printfw("    OUTPUT LOG       :  %s\n", logfname);


            DEBUG_TRACEPOINT(" ");


            if(fpsCTRLvar.fpsCTRL_DisplayMode == 1)   // help
            {
                fpsCTRLscreen_print_help();
            }


            if(fpsCTRLvar.fpsCTRL_DisplayMode == 2)   // FPS content
            {


                DEBUG_TRACEPOINT("Check that selected node is OK");
                /* printfw("node selected : %d\n", fpsCTRLvar.nodeSelected);
                 printfw("full keyword :  %s\n", keywnode[fpsCTRLvar.nodeSelected].keywordfull);*/
                if(strlen(keywnode[fpsCTRLvar.nodeSelected].keywordfull) <
                        1)   // if not OK, set to last valid entry
                {
                    fpsCTRLvar.nodeSelected = 1;
                    while((strlen(keywnode[fpsCTRLvar.nodeSelected].keywordfull) < 1)
                            && (fpsCTRLvar.nodeSelected < NB_KEYWNODE_MAX))
                    {
                        fpsCTRLvar.nodeSelected ++;
                    }
                }

                DEBUG_TRACEPOINT("Get info from selected node");
                fpsCTRLvar.fpsindexSelected = keywnode[fpsCTRLvar.nodeSelected].fpsindex;
                fpsCTRLvar.pindexSelected = keywnode[fpsCTRLvar.nodeSelected].pindex;
                fpsCTRLscreen_print_nodeinfo(
                    fps,
                    keywnode,
                    fpsCTRLvar.nodeSelected,
                    fpsCTRLvar.fpsindexSelected,
                    fpsCTRLvar.pindexSelected);



                DEBUG_TRACEPOINT("trace back node chain");
                nodechain[fpsCTRLvar.currentlevel] = fpsCTRLvar.directorynodeSelected;

                printfw("[level %d %d] ", fpsCTRLvar.currentlevel + 1,
                        nodechain[fpsCTRLvar.currentlevel + 1]);

                if(fpsCTRLvar.currentlevel > 0)
                {
                    printfw("[level %d %d] ", fpsCTRLvar.currentlevel,
                            nodechain[fpsCTRLvar.currentlevel]);
                }
                level = fpsCTRLvar.currentlevel - 1;
                while(level > 0)
                {
                    nodechain[level] = keywnode[nodechain[level + 1]].parent_index;
                    printfw("[level %d %d] ", level, nodechain[level]);
                    level --;
                }
                printfw("[level 0 0]\n");
                nodechain[0] = 0; // root

                DEBUG_TRACEPOINT("Get number of lines to be displayed");
                fpsCTRLvar.currentlevel =
                    keywnode[fpsCTRLvar.directorynodeSelected].keywordlevel;
                int GUIlineMax = keywnode[fpsCTRLvar.directorynodeSelected].NBchild;
                for(level = 0; level < fpsCTRLvar.currentlevel; level ++)
                {
                    DEBUG_TRACEPOINT("update GUIlineMax, the maximum number of lines");
                    if(keywnode[nodechain[level]].NBchild > GUIlineMax)
                    {
                        GUIlineMax = keywnode[nodechain[level]].NBchild;
                    }
                }

                printfw("[node %d] level = %d   [%d] NB child = %d",
                        fpsCTRLvar.nodeSelected,
                        fpsCTRLvar.currentlevel,
                        fpsCTRLvar.directorynodeSelected,
                        keywnode[fpsCTRLvar.directorynodeSelected].NBchild
                       );

                printfw("   fps %d",
                        fpsCTRLvar.fpsindexSelected
                       );

                printfw("   pindex %d ",
                        keywnode[fpsCTRLvar.nodeSelected].pindex
                       );

                printfw("\n");

                /*      printfw("SELECTED DIR = %3d    SELECTED = %3d   GUIlineMax= %3d\n\n",
                             fpsCTRLvar.directorynodeSelected,
                             fpsCTRLvar.nodeSelected,
                             GUIlineMax);
                      printfw("LINE: %d / %d\n\n",
                             fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel],
                             keywnode[fpsCTRLvar.directorynodeSelected].NBchild);
                	*/


                //while(!(fps[fpsindexSelected].parray[pindexSelected].fpflag & FPFLAG_VISIBLE)) { // if invisible
                //		fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel]++;
                //}

                //if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_VISIBLE)) { // if invisible


                //              if( !(  fps[keywnode[fpsCTRLvar.nodeSelected].fpsindex].parray[keywnode[fpsCTRLvar.nodeSelected].pindex].fpflag & FPFLAG_VISIBLE)) { // if invisible
                //				if( !(  fps[fpsCTRLvar.fpsindexSelected].parray[fpsCTRLvar.pindexSelected].fpflag & FPFLAG_VISIBLE)) { // if invisible
                if(!(fps[fpsCTRLvar.fpsindexSelected].parray[0].fpflag &
                        FPFLAG_VISIBLE))      // if invisible
                {
                    if(fpsCTRLvar.direction > 0)
                    {
                        fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel] ++;
                    }
                    else
                    {
                        fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel] --;
                    }
                }



                while(fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel] >
                        keywnode[fpsCTRLvar.directorynodeSelected].NBchild - 1)
                {
                    fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel]--;
                }



                int child_index[MAXNBLEVELS];
                for(level = 0; level < MAXNBLEVELS ; level ++)
                {
                    child_index[level] = 0;
                }




                for(int GUIline = 0; GUIline < GUIlineMax;
                        GUIline++)   // GUIline is the line number on GUI display
                {


                    for(level = 0; level < fpsCTRLvar.currentlevel; level ++)
                    {

                        if(GUIline < keywnode[nodechain[level]].NBchild)
                        {
                            int snode = 0; // selected node
                            int knodeindex;

                            knodeindex = keywnode[nodechain[level]].child[GUIline];


                            //TODO: adjust len to string
                            char pword[100];


                            if(level == 0)
                            {
                                DEBUG_TRACEPOINT("provide a fps status summary if at root");
                                fpsindex = keywnode[knodeindex].fpsindex;
                                fpsCTRLscreen_level0node_summary(fps, fpsindex);
                            }

                            // toggle highlight if node is in the chain
                            int v1 = keywnode[nodechain[level]].child[GUIline];
                            int v2 = nodechain[level + 1];
                            if(v1 == v2)
                            {
                                snode = 1;
                                screenprint_setreverse();
                            }

                            // color node if directory
                            if(keywnode[knodeindex].leaf == 0)
                            {
                                screenprint_setcolor(5);
                            }

                            // print keyword
                            if(snprintf(pword, 10, "%s",
                                        keywnode[keywnode[nodechain[level]].child[GUIline]].keyword[level]) < 0)
                            {
                                PRINT_ERROR("snprintf error");
                            }
                            printfw("%-10s ", pword);

                            if(keywnode[knodeindex].leaf == 0)   // directory
                            {
                                screenprint_unsetcolor(5);
                            }

                            screenprint_setreverse();
                            if(snode == 1)
                            {
                                printfw(">");
                            }
                            else
                            {
                                printfw(" ");
                            }
                            screenprint_unsetreverse();
                            screenprint_setnormal();

                        }
                        else     // blank space
                        {
                            if(level == 0)
                            {
                                printfw("                    ");
                            }
                            printfw("            ");
                        }
                    }






                    int knodeindex;
                    knodeindex =
                        keywnode[fpsCTRLvar.directorynodeSelected].child[child_index[level]];
                    if(knodeindex < fpsCTRLvar.NBkwn)
                    {
                        fpsindex = keywnode[knodeindex].fpsindex;
                        pindex = keywnode[knodeindex].pindex;

                        if(child_index[level] > keywnode[fpsCTRLvar.directorynodeSelected].NBchild - 1)
                        {
                            child_index[level] = keywnode[fpsCTRLvar.directorynodeSelected].NBchild - 1;
                        }

                        /*
                                                if(fpsCTRLvar.currentlevel != 0) { // this does not apply to root menu
                                                    while((!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_VISIBLE)) && // if not visible, advance to next one
                                                            (child_index[level] < keywnode[fpsCTRLvar.directorynodeSelected].NBchild-1)) {
                                                        child_index[level] ++;
                                                        DEBUG_TRACEPOINT("knodeindex = %d  child %d / %d",
                                                                  knodeindex,
                                                                  child_index[level],
                                                                  keywnode[fpsCTRLvar.directorynodeSelected].NBchild);
                                                        knodeindex = keywnode[fpsCTRLvar.directorynodeSelected].child[child_index[level]];
                                                        fpsindex = keywnode[knodeindex].fpsindex;
                                                        pindex = keywnode[knodeindex].pindex;
                                                    }
                                                }
                        */

                        DEBUG_TRACEPOINT(" ");

                        if(child_index[level] < keywnode[fpsCTRLvar.directorynodeSelected].NBchild)
                        {

                            if(fpsCTRLvar.currentlevel > 0)
                            {
                                screenprint_setreverse();
                                printfw(" ");
                                screenprint_unsetreverse();
                            }

                            DEBUG_TRACEPOINT(" ");

                            if(keywnode[knodeindex].leaf == 0)   // If this is a directory
                            {
                                DEBUG_TRACEPOINT(" ");
                                if(fpsCTRLvar.currentlevel == 0)   // provide a status summary if at root
                                {
                                    DEBUG_TRACEPOINT(" ");

                                    fpsindex = keywnode[knodeindex].fpsindex;
                                    pid_t pid;

                                    pid = fps[fpsindex].md->confpid;
                                    if((getpgid(pid) >= 0) && (pid > 0))
                                    {
                                        screenprint_setcolor(2);
                                        printfw("%07d ", (int) pid);
                                        screenprint_unsetcolor(2);
                                    }
                                    else     // PID not active
                                    {
                                        if(fps[fpsindex].md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CMDCONF)
                                        {
                                            // not clean exit
                                            screenprint_setcolor(4);
                                            printfw("%07d ", (int) pid);
                                            screenprint_unsetcolor(4);
                                        }
                                        else
                                        {
                                            // All OK
                                            printfw("%07d ", (int) pid);
                                        }
                                    }

                                    if(fps[fpsindex].md->conferrcnt > 99)
                                    {
                                        screenprint_setcolor(4);
                                        printfw("[XX]");
                                        screenprint_unsetcolor(4);
                                    }
                                    if(fps[fpsindex].md->conferrcnt > 0)
                                    {
                                        screenprint_setcolor(4);
                                        printfw("[%02d]", fps[fpsindex].md->conferrcnt);
                                        screenprint_unsetcolor(4);
                                    }
                                    if(fps[fpsindex].md->conferrcnt == 0)
                                    {
                                        screenprint_setcolor(2);
                                        printfw("[%02d]", fps[fpsindex].md->conferrcnt);
                                        screenprint_unsetcolor(2);
                                    }

                                    pid = fps[fpsindex].md->runpid;
                                    if((getpgid(pid) >= 0) && (pid > 0))
                                    {
                                        screenprint_setcolor(2);
                                        printfw("%07d ", (int) pid);
                                        screenprint_unsetcolor(2);
                                    }
                                    else
                                    {
                                        if(fps[fpsindex].md->status & FUNCTION_PARAMETER_STRUCT_STATUS_CMDRUN)
                                        {
                                            // not clean exit
                                            screenprint_setcolor(4);
                                            printfw("%07d ", (int) pid);
                                            screenprint_unsetcolor(4);
                                        }
                                        else
                                        {
                                            // All OK
                                            printfw("%07d ", (int) pid);
                                        }
                                    }
                                }





                                if(GUIline == fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel])
                                {
                                    screenprint_setreverse();
                                    fpsCTRLvar.nodeSelected = knodeindex;
                                    fpsCTRLvar.fpsindexSelected = keywnode[knodeindex].fpsindex;
                                }


                                if(child_index[level + 1] < keywnode[fpsCTRLvar.directorynodeSelected].NBchild)
                                {
                                    screenprint_setcolor(5);
                                    level = keywnode[knodeindex].keywordlevel;
                                    printfw("%-16s", keywnode[knodeindex].keyword[level - 1]);
                                    screenprint_unsetcolor(5);

                                    if(GUIline == fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel])
                                    {
                                        screenprint_unsetreverse();
                                    }
                                }
                                else
                                {
                                    printfw("%-16s", " ");
                                }


                                DEBUG_TRACEPOINT(" ");

                            }
                            else   // If this is a parameter
                            {
                                DEBUG_TRACEPOINT(" ");
                                fpsindex = keywnode[knodeindex].fpsindex;
                                pindex = keywnode[knodeindex].pindex;




                                DEBUG_TRACEPOINT(" ");
                                int isVISIBLE = 1;
                                if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_VISIBLE))   // if invisible
                                {
                                    isVISIBLE = 0;                              
                                    screenprint_setdim();
                                    screenprint_setblink();
                                }


                                //int kl;

                                if(GUIline == fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel])
                                {
                                    fpsCTRLvar.pindexSelected = keywnode[knodeindex].pindex;
                                    fpsCTRLvar.fpsindexSelected = keywnode[knodeindex].fpsindex;
                                    fpsCTRLvar.nodeSelected = knodeindex;

                                    if(isVISIBLE == 1)
                                    {
                                        screenprint_setcolor(10);
                                        screenprint_setbold();
                                    }
                                }
                                DEBUG_TRACEPOINT(" ");

                                if(isVISIBLE == 1)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag & FPFLAG_WRITESTATUS)
                                    {
                                        screenprint_setcolor(10);
                                        screenprint_setblink();
                                        printfw("W "); // writable
                                        screenprint_unsetcolor(10);
                                        screenprint_unsetblink();
                                    }
                                    else
                                    {
                                        screenprint_setcolor(4);
                                        screenprint_setblink();
                                        printfw("NW"); // non writable
                                        screenprint_unsetcolor(4);
                                        screenprint_unsetblink();
                                    }
                                }
                                else
                                {
                                    printfw("  ");
                                }

                                DEBUG_TRACEPOINT(" ");
                                level = keywnode[knodeindex].keywordlevel;

								if(GUIline == fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel])
								{
									screenprint_setreverse();
								}

                                printfw(" %-20s", fps[fpsindex].parray[pindex].keyword[level - 1]);

                                if(GUIline == fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel])
                                {
                                    screenprint_unsetcolor(10);
                                    screenprint_unsetreverse();
                                }
                                DEBUG_TRACEPOINT(" ");
                                printfw("   ");

                                // VALUE

                                int paramsync = 1; // parameter is synchronized

                                if(fps[fpsindex].parray[pindex].fpflag &
                                        FPFLAG_ERROR)   // parameter setting error
                                {
                                    if(isVISIBLE == 1)
                                    {
                                        screenprint_setcolor(4);
                                    }
                                }

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_UNDEF)
                                {
                                    printfw("  %s", "-undef-");
                                }

                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_INT64)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(fps[fpsindex].parray[pindex].val.l[0] !=
                                                    fps[fpsindex].parray[pindex].val.l[3])
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10d", (int) fps[fpsindex].parray[pindex].val.l[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }

                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_FLOAT64)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                        {
                                            double absdiff;
                                            double abssum;
                                            double epsrel = 1.0e-6;
                                            double epsabs = 1.0e-10;

                                            absdiff = fabs(fps[fpsindex].parray[pindex].val.f[0] -
                                                           fps[fpsindex].parray[pindex].val.f[3]);
                                            abssum = fabs(fps[fpsindex].parray[pindex].val.f[0]) + fabs(
                                                         fps[fpsindex].parray[pindex].val.f[3]);


                                            if((absdiff < epsrel * abssum) || (absdiff < epsabs))
                                            {
                                                paramsync = 1;
                                            }
                                            else
                                            {
                                                paramsync = 0;
                                            }
                                        }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10f", (float) fps[fpsindex].parray[pindex].val.f[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }

                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_FLOAT32)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                        {
                                            double absdiff;
                                            double abssum;
                                            double epsrel = 1.0e-6;
                                            double epsabs = 1.0e-10;

                                            absdiff = fabs(fps[fpsindex].parray[pindex].val.s[0] -
                                                           fps[fpsindex].parray[pindex].val.s[3]);
                                            abssum = fabs(fps[fpsindex].parray[pindex].val.s[0]) + fabs(
                                                         fps[fpsindex].parray[pindex].val.s[3]);


                                            if((absdiff < epsrel * abssum) || (absdiff < epsabs))
                                            {
                                                paramsync = 1;
                                            }
                                            else
                                            {
                                                paramsync = 0;
                                            }
                                        }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10f", (float) fps[fpsindex].parray[pindex].val.s[0]);

                                    if(paramsync == 0)
                                    {
                                        screenprint_unsetcolor(3);
                                    }
                                }


                                DEBUG_TRACEPOINT(" ");
                                if(fps[fpsindex].parray[pindex].type == FPTYPE_PID)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(fps[fpsindex].parray[pindex].val.pid[0] !=
                                                    fps[fpsindex].parray[pindex].val.pid[1])
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10d", (float) fps[fpsindex].parray[pindex].val.pid[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }

                                    printfw("  %10d", (int) fps[fpsindex].parray[pindex].val.pid[0]);
                                }


                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_TIMESPEC)
                                {
                                    printfw("  %10s", "-timespec-");
                                }


                                if(fps[fpsindex].parray[pindex].type == FPTYPE_FILENAME)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(strcmp(fps[fpsindex].parray[pindex].val.string[0],
                                                      fps[fpsindex].parray[pindex].val.string[1]))
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10s", fps[fpsindex].parray[pindex].val.string[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }
                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_FITSFILENAME)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(strcmp(fps[fpsindex].parray[pindex].val.string[0],
                                                      fps[fpsindex].parray[pindex].val.string[1]))
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10s", fps[fpsindex].parray[pindex].val.string[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }
                                DEBUG_TRACEPOINT(" ");
                                if(fps[fpsindex].parray[pindex].type == FPTYPE_EXECFILENAME)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(strcmp(fps[fpsindex].parray[pindex].val.string[0],
                                                      fps[fpsindex].parray[pindex].val.string[1]))
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10s", fps[fpsindex].parray[pindex].val.string[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }
                                DEBUG_TRACEPOINT(" ");
                                if(fps[fpsindex].parray[pindex].type == FPTYPE_DIRNAME)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(strcmp(fps[fpsindex].parray[pindex].val.string[0],
                                                      fps[fpsindex].parray[pindex].val.string[1]))
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10s", fps[fpsindex].parray[pindex].val.string[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }

                                DEBUG_TRACEPOINT(" ");
                                if(fps[fpsindex].parray[pindex].type == FPTYPE_STREAMNAME)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            //  if(strcmp(fps[fpsindex].parray[pindex].val.string[0], fps[fpsindex].parray[pindex].val.string[1])) {
                                            //      paramsync = 0;
                                            //  }

                                            if(fps[fpsindex].parray[pindex].info.stream.streamID > -1)
                                            {
                                                if(isVISIBLE == 1)
                                                {
                                                    screenprint_setcolor(2);
                                                }
                                            }

                                    printfw("[%d]  %10s",
                                           fps[fpsindex].parray[pindex].info.stream.stream_sourceLocation,
                                           fps[fpsindex].parray[pindex].val.string[0]);

                                    if(fps[fpsindex].parray[pindex].info.stream.streamID > -1)
                                    {

                                        printfw(" [ %d", fps[fpsindex].parray[pindex].info.stream.stream_xsize[0]);
                                        if(fps[fpsindex].parray[pindex].info.stream.stream_naxis[0] > 1)
                                        {
                                            printfw("x%d", fps[fpsindex].parray[pindex].info.stream.stream_ysize[0]);
                                        }
                                        if(fps[fpsindex].parray[pindex].info.stream.stream_naxis[0] > 2)
                                        {
                                            printfw("x%d", fps[fpsindex].parray[pindex].info.stream.stream_zsize[0]);
                                        }

                                        printfw(" ]");
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(2);
                                        }
                                    }

                                }
                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_STRING)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(strcmp(fps[fpsindex].parray[pindex].val.string[0],
                                                      fps[fpsindex].parray[pindex].val.string[1]))
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(3);
                                        }
                                    }

                                    printfw("  %10s", fps[fpsindex].parray[pindex].val.string[0]);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(3);
                                        }
                                    }
                                }
                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].type == FPTYPE_ONOFF)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ONOFF)
                                    {
                                        screenprint_setcolor(2);
                                        printfw("  ON ");
                                        screenprint_unsetcolor(2);
                                        printfw(" [%15s]", fps[fpsindex].parray[pindex].val.string[0]);
                                    }
                                    else
                                    {
                                        screenprint_setcolor(1);
                                        printfw(" OFF ");
                                        screenprint_unsetcolor(1);
                                        printfw(" [%15s]", fps[fpsindex].parray[pindex].val.string[0]);
                                    }
                                }


                                if(fps[fpsindex].parray[pindex].type == FPTYPE_FPSNAME)
                                {
                                    if(fps[fpsindex].parray[pindex].fpflag &
                                            FPFLAG_FEEDBACK)   // Check value feedback if available
                                        if(!(fps[fpsindex].parray[pindex].fpflag & FPFLAG_ERROR))
                                            if(strcmp(fps[fpsindex].parray[pindex].val.string[0],
                                                      fps[fpsindex].parray[pindex].val.string[1]))
                                            {
                                                paramsync = 0;
                                            }

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(2);
                                        }
                                    }
                                    else
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_setcolor(4);
                                        }
                                    }

                                    printfw(" %10s [%ld %ld %ld]",
                                           fps[fpsindex].parray[pindex].val.string[0],
                                           fps[fpsindex].parray[pindex].info.fps.FPSNBparamMAX,
                                           fps[fpsindex].parray[pindex].info.fps.FPSNBparamActive,
                                           fps[fpsindex].parray[pindex].info.fps.FPSNBparamUsed);

                                    if(paramsync == 0)
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(2);
                                        }
                                    }
                                    else
                                    {
                                        if(isVISIBLE == 1)
                                        {
                                            screenprint_unsetcolor(4);
                                        }
                                    }

                                }

                                DEBUG_TRACEPOINT(" ");

                                if(fps[fpsindex].parray[pindex].fpflag &
                                        FPFLAG_ERROR)   // parameter setting error
                                {
                                    if(isVISIBLE == 1)
                                    {
                                        screenprint_unsetcolor(4);
                                    }
                                }

                                printfw("    %s", fps[fpsindex].parray[pindex].description);



                                if(GUIline == fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel])
                                {
                                    if(isVISIBLE == 1)
                                    {
                                        screenprint_unsetbold();
                                    }
                                }


                                if(isVISIBLE == 0)
                                {
									screenprint_unsetblink();
                                    screenprint_unsetdim();
                                }
                                // END LOOP


                            }


                            DEBUG_TRACEPOINT(" ");
                            icnt++;


                            for(level = 0; level < MAXNBLEVELS ; level ++)
                            {
                                child_index[level] ++;
                            }
                        }
                    }

                    printfw("\n");
                }

                DEBUG_TRACEPOINT(" ");

                fpsCTRLvar.NBindex = icnt;

                if(fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel] > fpsCTRLvar.NBindex -
                        1)
                {
                    fpsCTRLvar.GUIlineSelected[fpsCTRLvar.currentlevel] = fpsCTRLvar.NBindex - 1;
                }

                DEBUG_TRACEPOINT(" ");

                printfw("\n");

                if(fps[fpsCTRLvar.fpsindexSelected].md->status &
                        FUNCTION_PARAMETER_STRUCT_STATUS_CHECKOK)
                {
                    screenprint_setcolor(2);
                    printfw("[%ld] PARAMETERS OK - RUN function good to go\n",
                           fps[fpsCTRLvar.fpsindexSelected].md->msgcnt);
                    screenprint_unsetcolor(2);
                }
                else
                {
                    int msgi;

                    screenprint_setcolor(4);
                    printfw("[%ld] %d PARAMETER SETTINGS ERROR(s) :\n",
                           fps[fpsCTRLvar.fpsindexSelected].md->msgcnt,
                           fps[fpsCTRLvar.fpsindexSelected].md->conferrcnt);
                    screenprint_unsetcolor(4);

                    screenprint_setbold();

                    for(msgi = 0; msgi < fps[fpsCTRLvar.fpsindexSelected].md->msgcnt; msgi++)
                    {
                        pindex = fps[fpsCTRLvar.fpsindexSelected].md->msgpindex[msgi];
                        printfw("%-40s %s\n",
                               fps[fpsCTRLvar.fpsindexSelected].parray[pindex].keywordfull,
                               fps[fpsCTRLvar.fpsindexSelected].md->message[msgi]);
                    }

                    screenprint_unsetbold();
                }


                DEBUG_TRACEPOINT(" ");

            }

            DEBUG_TRACEPOINT(" ");

            if(fpsCTRLvar.fpsCTRL_DisplayMode == 3)   // Task scheduler status
            {
                struct timespec tnow;
                struct timespec tdiff;

                clock_gettime(CLOCK_REALTIME, &tnow);
               
                //int dispcnt = 0;


                // Sort entries from most recent to most ancient, using inputindex
                DEBUG_TRACEPOINT(" ");
                double *sort_evalarray;
                sort_evalarray = (double *) malloc(sizeof(double) * NB_FPSCTRL_TASK_MAX);
                long *sort_indexarray;
                sort_indexarray = (long *) malloc(sizeof(long) * NB_FPSCTRL_TASK_MAX);

                long sortcnt = 0;
                for(int fpscmdindex = 0; fpscmdindex < NB_FPSCTRL_TASK_MAX; fpscmdindex++)
                {
                    if(fpsctrltasklist[fpscmdindex].status & FPSTASK_STATUS_SHOW)
                    {
                        sort_evalarray[sortcnt] = -1.0 * fpsctrltasklist[fpscmdindex].inputindex;
                        sort_indexarray[sortcnt] = fpscmdindex;
                        sortcnt++;
                    }
                }
                DEBUG_TRACEPOINT(" ");
                if(sortcnt > 0)
                {
                    quick_sort2l(sort_evalarray, sort_indexarray, sortcnt);
                }
                free(sort_evalarray);

                DEBUG_TRACEPOINT(" ");
                
                //printfw(" ---------------- %d %d ----- \n", sortcnt, wrow); //TEST

				printfw(" showing   %d / %d  tasks\n", wrow-8, sortcnt);
                
                for(int sortindex = 0; sortindex < sortcnt; sortindex++)
                {


                    DEBUG_TRACEPOINT("iteration %d / %ld", sortindex, sortcnt);

                    int fpscmdindex = sort_indexarray[sortindex];

                    DEBUG_TRACEPOINT("fpscmdindex = %d", fpscmdindex);

                    if(sortindex < wrow-8)     // display
                    {
                        int attron2 = 0;
                        int attrbold = 0;


                        if(fpsctrltasklist[fpscmdindex].status &
                                FPSTASK_STATUS_RUNNING)   // task is running
                        {
                            attron2 = 1;
                            screenprint_setcolor(2);
                        }
                        else if(fpsctrltasklist[fpscmdindex].status &
                                FPSTASK_STATUS_ACTIVE)      // task is queued to run
                        {
                            attrbold = 1;
                            screenprint_setbold();
                        }



                        // measure age since submission
                        tdiff =  timespec_diff(fpsctrltasklist[fpscmdindex].creationtime, tnow);
                        double tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;
                        printfw("%6.2f s ", tdiffv);

                        if(fpsctrltasklist[fpscmdindex].status &
                                FPSTASK_STATUS_RUNNING)   // run time (ongoing)
                        {
                            tdiff =  timespec_diff(fpsctrltasklist[fpscmdindex].activationtime, tnow);
                            tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;
                            printfw(" %6.2f s ", tdiffv);
                        }
                        else if(!(fpsctrltasklist[fpscmdindex].status &
                                  FPSTASK_STATUS_ACTIVE))      // run time (past)
                        {
                            tdiff =  timespec_diff(fpsctrltasklist[fpscmdindex].activationtime,
                                                    fpsctrltasklist[fpscmdindex].completiontime);
                            tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;
                            screenprint_setcolor(3);
                            printfw(" %6.2f s ", tdiffv);
                            screenprint_unsetcolor(3);
                            // age since completion
                            tdiff =  timespec_diff(fpsctrltasklist[fpscmdindex].completiontime, tnow);
                            double tdiffv = tdiffv = 1.0 * tdiff.tv_sec + 1.0e-9 * tdiff.tv_nsec;
                            //printfw("<%6.2f s>      ", tdiffv);

                            //if(tdiffv > 30.0)
                            //fpsctrltasklist[fpscmdindex].status &= ~FPSTASK_STATUS_SHOW;

                        }
                        else
                        {
                            printfw("          ", tdiffv);
                        }


                        if(fpsctrltasklist[fpscmdindex].status & FPSTASK_STATUS_ACTIVE)
                        {
                            printfw(">>");
                        }
                        else
                        {
                            printfw("  ");
                        }

                        if(fpsctrltasklist[fpscmdindex].flag & FPSTASK_FLAG_WAITONRUN)
                        {
                            printfw("WR ");
                        }
                        else
                        {
                            printfw("   ");
                        }

                        if(fpsctrltasklist[fpscmdindex].flag & FPSTASK_FLAG_WAITONCONF)
                        {
                            printfw("WC ");
                        }
                        else
                        {
                            printfw("   ");
                        }

                        printfw("[Q:%02d P:%02d] %4d",
                               fpsctrltasklist[fpscmdindex].queue,
                               fpsctrlqueuelist[fpsctrltasklist[fpscmdindex].queue].priority,
                               fpscmdindex
                               );
                        
                        if(fpsctrltasklist[fpscmdindex].status & FPSTASK_STATUS_RECEIVED)
                        {
							printfw(" R");
						}
						else
						{
							printfw(" -");
						}
                        
                        if(fpsctrltasklist[fpscmdindex].status & FPSTASK_STATUS_CMDNOTFOUND)
                        {
							screenprint_setcolor(3);
							printfw(" NOTCMD");
							screenprint_unsetcolor(3);
						}
						else if (fpsctrltasklist[fpscmdindex].status & FPSTASK_STATUS_CMDFAIL)
						{
							screenprint_setcolor(4);
							printfw(" FAILED");
							screenprint_unsetcolor(4);
						}
						else if (fpsctrltasklist[fpscmdindex].status & FPSTASK_STATUS_CMDOK)
						{
							screenprint_setcolor(2);
							printfw(" PROCOK");
							screenprint_unsetcolor(2);
						}
						else
						{
							screenprint_setcolor(3);
							printfw(" ????  ");
							screenprint_unsetcolor(3);
						}
												

						
                        
                        printfw("  %s\n", fpsctrltasklist[fpscmdindex].cmdstring);
                        

                        if(attron2 == 1)
                        {
                            screenprint_unsetcolor(2);
                        }
                        if(attrbold == 1)
                        {
                            screenprint_unsetbold();
                        }

                    }
                }
                free(sort_indexarray);



            }



            DEBUG_TRACEPOINT(" ");
            
            if( screenprintmode == SCREENPRINT_NCURSES) 
            {
				refresh();
			}			

            DEBUG_TRACEPOINT(" ");

        } // end run_display

        DEBUG_TRACEPOINT("exit from if( fpsCTRLvar.run_display == 1)");

        fpsCTRLvar.run_display = run_display;

        loopcnt++;

#ifndef STANDALONE
        if((data.signal_TERM == 1)
                || (data.signal_INT == 1)
                || (data.signal_ABRT == 1)
                || (data.signal_BUS == 1)
                || (data.signal_SEGV == 1)
                || (data.signal_HUP == 1)
                || (data.signal_PIPE == 1))
        {
            printf("Exit condition met\n");
            loopOK = 0;
        }
#endif
    }


    if(run_display == 1)
    {
		if( screenprintmode == SCREENPRINT_NCURSES) 
		{
			endwin();
		}
    }

    functionparameter_outlog("FPSCTRL", "STOP");

    DEBUG_TRACEPOINT("Disconnect from FPS entries");
    for(fpsindex = 0; fpsindex < fpsCTRLvar.NBfps; fpsindex++)
    {
        function_parameter_struct_disconnect(&fps[fpsindex]);
    }

    free(fps);
    free(keywnode);



    free(fpsctrltasklist);
    free(fpsctrlqueuelist);
    functionparameter_outlog("LOGFILECLOSE", "close log file");

    DEBUG_TRACEPOINT("exit from function");

    return RETURN_SUCCESS;
}








