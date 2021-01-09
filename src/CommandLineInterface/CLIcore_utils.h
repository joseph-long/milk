/**
 * @file    CLIcore_utils.h
 * @brief   Util functions and macros for coding convenience
 *
 */

#ifndef CLICORE_UTILS_H
#define CLICORE_UTILS_H

#include "CommandLineInterface/CLIcore.h"

#include "COREMOD_memory/COREMOD_memory.h"
#include "CommandLineInterface/IMGID.h"







/** @brief Standard Function call wrapper
 *
 * CLI argument(s) is(are) parsed and checked with CLI_checkarray(), then
 * passed to the compute function call.
 *
 * Custom code may be added for more complex processing of function arguments.
 *
 * If CLI call arguments check out, go ahead with computation.
 * Arguments not contained in CLI call line are extracted from the
 * command argument list
 */
#define INSERT_STD_CLIfunction \
static errno_t CLIfunction(void)\
{\
    if(CLI_checkarg_array(farg, CLIcmddata.nbarg) == RETURN_SUCCESS)\
    {\
        variables_link();\
        compute_function();\
\
        return RETURN_SUCCESS;\
    }\
    else\
    {\
        return CLICMD_INVALID_ARG;\
    }\
}



/** @brief FPS conf function
 * Sets up the FPS and its parameters.\n
 * Optional parameter checking can be included.\n
 *
 * ### ADD PARAMETERS
 *
 * The function function_parameter_add_entry() is called to add
 * each parameter.
 *
 * Macros are provided for convenience, named "FPS_ADDPARAM_...".\n
 * The macros are defined in fps_add_entry.h, and provide a function
 * parameter identifier variable (int) for each parameter added.
 *
 * parameters for FPS_ADDPARAM macros:
 * - key/variable name
 * - tag name
 * - description
 * - default initial value
 *
 * Equivalent code without using macro :
 *      function_parameter_add_entry(&fps, ".delayus", "Delay [us]", FPTYPE_INT64, FPFLAG_DEFAULT_INPUT|FPFLAG_WRITERUN, NULL);
 * ### START CONFLOOP
 *
 * start function parameter conf loop\n
 * macro defined in function_parameter.h
 *
 * Optional code to handle/check parameters is included after this
 * statement
 *
 * ### STOP CONFLOOP
 * stop function parameter conf loop\n
 * macro defined in function_parameter.h
 */
#define INSERT_STD_FPSCONFfunction \
static errno_t FPSCONFfunction()\
{\
    FPS_SETUP_INIT(data.FPS_name, data.FPS_CMDCODE);\
    data.fps = &fps;\
    CMDargs_to_FPSparams_create(&fps);\
    variables_link();\
    FPS_CONFLOOP_START\
    data.fps = NULL;\
    FPS_CONFLOOP_END\
    return RETURN_SUCCESS;\
}





/** @brief FPS run function
 *
 * The FPS name is taken from data.FPS_name, which has to
 * have been set up by either the stand-alone function, or the
 * CLI.
 *
 * Running FPS_CONNECT macro in FPSCONNECT_RUN mode.
 *
 * ### GET FUNCTION PARAMETER VALUES
 *
 * Parameters are addressed by their tag name\n
 * These parameters are read once, before running the loop.\n
 *
 * FPS_GETPARAM... macros are wrapper to functionparameter_GetParamValue
 * and functionparameter_GetParamPtr functions, all defined in
 * fps_paramvalue.h.
 *
 * Each of the FPS_GETPARAM macro creates a variable with "_" prepended
 * to the first macro argument.
 *
 * Equivalent code without using macros:
 *
 * char _IDin_name[200];
 * strncpy(_IDin_name,  functionparameter_GetParamPtr_STRING(&fps, ".in_name"), FUNCTION_PARAMETER_STRMAXLEN);
 * long _delayus = functionparameter_GetParamValue_INT64(&fps, ".delayus");
 */
#define INSERT_STD_FPSRUNfunction \
static errno_t FPSRUNfunction()\
{\
FPS_CONNECT(data.FPS_name, FPSCONNECT_RUN);\
data.fps = &fps;\
variables_link();\
compute_function();\
data.fps = NULL;\
function_parameter_RUNexit(&fps);\
return RETURN_SUCCESS;\
}




/** @brief FPSCLI function
 *
 * GET ARGUMENTS AND PARAMETERS
 * Try FPS implementation
 *
 * Set data.fpsname, providing default value as first arg, and set data.FPS_CMDCODE value.
 * Default FPS name will be used if CLI process has NOT been named.
 * See code in function_parameter.h for detailed rules.
 */
#define INSERT_STD_FPSCLIfunction \
static errno_t FPSCLIfunction(void)\
{\
function_parameter_getFPSargs_from_CLIfunc(CLIcmddata.key);\
if(data.FPS_CMDCODE != 0)\
    {\
        data.FPS_CONFfunc = FPSCONFfunction;\
        data.FPS_RUNfunc  = FPSRUNfunction;\
        function_parameter_execFPScmd();\
        return RETURN_SUCCESS;\
    }\
    else\
    {\
        if(CLI_checkarg_array(farg, CLIcmddata.nbarg) == RETURN_SUCCESS)\
        {\
            variables_link();\
            compute_function();\
        }\
        else\
        {\
            return CLICMD_INVALID_ARG;\
        }\
    }\
    return RETURN_SUCCESS;\
}











static inline IMGID makeIMGID(
    const char *restrict name
)
{
    IMGID img;

    img.ID = -1;
    strcpy(img.name, name);

    img.im = NULL;
    img.md = NULL;
    img.createcnt = -1;

    return img;
}


static inline imageID resolveIMGID(
    IMGID *img,
    int ERRMODE
)
{
    if(img->ID == -1)
    {
        // has not been previously resolved -> resolve
        img->ID = image_ID(img->name);
        if(img->ID > -1)
        {
            img->im = &data.image[img->ID];
            img->md = &data.image[img->ID].md[0];
            img->createcnt = data.image[img->ID].createcnt;
        }
    }
    else
    {
        // check that create counter matches and image is in use
        if((img->createcnt != data.image[img->ID].createcnt)
                || (data.image[img->ID].used != 1))
        {
            // create counter mismatch -> need to re-resolve
            img->ID = image_ID(img->name);
            if(img->ID > -1)
            {
                img->im = &data.image[img->ID];
                img->md = &data.image[img->ID].md[0];
                img->createcnt = data.image[img->ID].createcnt;
            }
        }

    }

    if(img->ID == -1)
    {
        if((ERRMODE == ERRMODE_FAIL) || (ERRMODE == ERRMODE_ABORT))
        {
            printf("ERROR: %c[%d;%dm Cannot resolve image %s %c[%d;m\n", (char) 27, 1, 31,
                   img->name, (char) 27, 0);
            abort();
        }
        else if(ERRMODE == ERRMODE_WARN)
        {
            printf("WARNING: %c[%d;%dm Cannot resolve image %s %c[%d;m\n", (char) 27, 1, 35,
                   img->name, (char) 27, 0);
        }
    }

    return img->ID;
}


#endif
