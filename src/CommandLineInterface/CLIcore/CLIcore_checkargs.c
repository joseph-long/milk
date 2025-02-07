/**
 * @file CLIcore_checkargs.c
 *
 * @brief Check CLI command line arguments
 *
 */

#include <stdio.h>

#include "CommandLineInterface/CLIcore.h"

#include "COREMOD_memory/COREMOD_memory.h"

// keep processing if 1
static int argcheck_process_flag = 1;


// toggles to 1 if function help called
static int functionhelp_called = 0;




/**
 * @brief check that input CLI argument matches required function argument type
 *
 * @param CLIargnum    CLI argument / token index
 * @param funcargtype   function argument type
 * @param errmsg    error message printing flag (1 if printing errors)
 * @return int
 */
static int CLI_checkarg0(
    int       CLIargnum,
    uint32_t  funcargtype,
    int       errmsg
)
{
    DEBUG_TRACE_FSTART();

    int     rval; // 0 if OK, 1 if not, 2: do not process other args
    imageID IDv;

    rval = 2;

    if(strcmp(data.cmdargtoken[CLIargnum].val.string, "?") == 0)
    {
        argcheck_process_flag = 0; // stop processing arguments, will call help
        help_command(data.cmdargtoken[0].val.string);
        snprintf(data.cmdargtoken[CLIargnum].val.string,
                 STRINGMAXLEN_CMDARGTOKEN_VAL,
                 " "); // avoid re-running help
        functionhelp_called = 1;
        DEBUG_TRACE_FEXIT();
        return 1;
    }

    switch(funcargtype) // & 0x0000FFFF)
    {


        // Argument should be float32 or float64
        //
        case CLIARG_FLOAT32: // should be float32
        case CLIARG_FLOAT64: // should be float64
        case CLIARG_FLOAT: // to be deprecated
            switch(data.cmdargtoken[CLIargnum].type)
            {

                // Token is floatXX
                // -> OK
                //
                case CLIARG_FLOAT32 & 0x0000FFFF :
                    data.cmdargtoken[CLIargnum].val.numl =
                        (long) data.cmdargtoken[CLIargnum].val.numf;
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%f",
                             data.cmdargtoken[CLIargnum].val.numf);
                    rval = 0;
                    break;


                // Token is xintxx
                // -> convert long to float
                //
                case CLIARG_INT64  & 0x0000FFFF :
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%ld",
                             data.cmdargtoken[CLIargnum].val.numl);
                    data.cmdargtoken[CLIargnum].val.numf =
                        (double) data.cmdargtoken[CLIargnum].val.numl;
                    data.cmdargtoken[CLIargnum].type = CLIARG_FLOAT64;
                    rval = 0;
                    break;


                // Token is string not image
                //
                case CLIARG_STR_NOT_IMG:
                    IDv = variable_ID(data.cmdargtoken[CLIargnum].val.string);
                    if(IDv == -1)  // if not a variable -> not OK
                    {
                        if(errmsg == 1)
                        {
                            printf(
                                "arg %d is string (=\"%s\"), but "
                                "should be integer\n",
                                CLIargnum - 1,
                                data.cmdargtoken[CLIargnum].val.string);
                        }
                        rval = 1;
                    }
                    else // if variable, read it -> OK
                    {
                        switch(data.variable[IDv].type)
                        {
                            case CLIARG_FLOAT64:
                                data.cmdargtoken[CLIargnum].val.numf =
                                    data.variable[IDv].value.f;
                                data.cmdargtoken[CLIargnum].type = CLIARG_FLOAT64;
                                rval                          = 0;
                                break;
                            case CLIARG_INT64:
                                data.cmdargtoken[CLIargnum].val.numf =
                                    1.0 * data.variable[IDv].value.l;
                                data.cmdargtoken[CLIargnum].type = CLIARG_FLOAT64;
                                rval                          = 0;
                                break;
                            default:
                                if(errmsg == 1)
                                {
                                    printf(
                                        "  arg %d (string \"%s\") not "
                                        "an integer\n",
                                        CLIargnum - 1,
                                        data.cmdargtoken[CLIargnum].val.string);
                                }
                                rval = 1;
                                break;
                        }
                    }
                    break;


                // Token is image
                // -> not OK
                //
                case CLIARG_IMG:
                    if(errmsg == 1)
                    {
                        printf(
                            "  arg %d (image \"%s\") not a floating point "
                            "number\n",
                            CLIargnum - 1,
                            data.cmdargtoken[CLIargnum].val.string);
                    }
                    rval = 1;
                    break;


                // Token is string
                // -> not OK
                //
                case CLIARG_STR:
                    if(errmsg == 1)
                    {
                        printf(
                            "  arg %d (command \"%s\") not a floating "
                            "point number\n",
                            CLIargnum - 1,
                            data.cmdargtoken[CLIargnum].val.string);
                    }
                    rval = 1;
                    break;


                // Token is ??
                //
                case 6:
                    data.cmdargtoken[CLIargnum].val.numf =
                        atof(data.cmdargtoken[CLIargnum].val.string);
                    snprintf(data.cmdargtoken[CLIargnum].val.string, STRINGMAXLEN_CMDARGTOKEN_VAL,
                             " ");
                    data.cmdargtoken[CLIargnum].type = CLIARG_FLOAT64;
                    rval = 0;
                    break;
            }
            break;



        // Argument should be integer
        //
        case CLIARG_INT32:  // should be int32
        case CLIARG_INT64:  // should be int64
        case CLIARG_UINT32: // should be  uint32
        case CLIARG_UINT64: // should be uint64
        case CLIARG_ONOFF:  // should be on/off (= uint64)
        case CLIARG_LONG: // to be deprecated
            printf("ARG is CLIARG_XINTXX\n");
            switch(data.cmdargtoken[CLIargnum].type)
            {

                // Token is float
                // -> convert to int
                //
                case CLIARG_FLOAT32  & 0x0000FFFF :
                    //printf("token is CLIARG_FLOATXX\n");
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%f",
                             data.cmdargtoken[CLIargnum].val.numf);
                    if(errmsg == 1)
                    {
                        printf("converting floating point arg %d to integer\n",
                               CLIargnum - 1);
                    }
                    data.cmdargtoken[CLIargnum].val.numl =
                        (long)(data.cmdargtoken[CLIargnum].val.numf + 0.5);
                    data.cmdargtoken[CLIargnum].type = CLIARG_INT64;
                    rval = 0;
                    break;


                // Token is int
                // -> OK
                //
                case CLIARG_INT64  & 0x0000FFFF :
                    //printf("token is CLIARG_XINTXX\n");
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%ld",
                             data.cmdargtoken[CLIargnum].val.numl);
                    rval = 0;
                    break;


                // Token is string, not image
                //
                case CLIARG_STR_NOT_IMG:
                    //printf("token is CLIARG_STR_NOT_IMG\n");

                    if(funcargtype == CLIARG_ONOFF)
                    {
                        // if argument is ONOFF, check if string is "on", "off", "ON", "OFF"
                        // and convert to ONOFF integer
                        //
                        if((strcmp(data.cmdargtoken[CLIargnum].val.string, "on") == 0)
                                || (strcmp(data.cmdargtoken[CLIargnum].val.string, "ON") == 0))
                        {
                            printf("CONVERTING on to 1\n");
                            data.cmdargtoken[CLIargnum].val.numl = 1;
                            rval = 0;
                        }
                        if((strcmp(data.cmdargtoken[CLIargnum].val.string, "off") == 0)
                                || (strcmp(data.cmdargtoken[CLIargnum].val.string, "OFF") == 0))
                        {
                            printf("CONVERTING on to 0\n");
                            data.cmdargtoken[CLIargnum].val.numl = 0;
                            rval = 0;
                        }
                    }
                    else
                    {
                        // check if can be converted to int
                        //
                        IDv = variable_ID(data.cmdargtoken[CLIargnum].val.string);
                        if(IDv == -1)
                        {
                            // if not a variable name, not OK
                            //
                            if(errmsg == 1)
                            {
                                printf(
                                    "  arg %d (string \"%s\") not an "
                                    "integer\n",
                                    CLIargnum - 1,
                                    data.cmdargtoken[CLIargnum].val.string);
                            }
                            rval = 1;
                        }
                        else
                        {
                            // If variable, convert to int
                            //
                            switch(data.variable[IDv].type)
                            {

                                case CLIARG_FLOAT32: // float
                                case CLIARG_FLOAT64: // double
                                    data.cmdargtoken[CLIargnum].val.numl =
                                        (long)(data.variable[IDv].value.f);
                                    data.cmdargtoken[CLIargnum].type = CLIARG_INT64;
                                    rval = 0;
                                    break;

                                case CLIARG_INT32: // int32
                                case CLIARG_INT64: // int64
                                case CLIARG_UINT32: // uint32
                                case CLIARG_UINT64: // uint64
                                    data.cmdargtoken[CLIargnum].val.numl =
                                        data.variable[IDv].value.l;
                                    data.cmdargtoken[CLIargnum].type = CLIARG_INT64;
                                    rval = 0;
                                    break;

                                default:
                                    if(errmsg == 1)
                                    {
                                        printf(
                                            "  arg %d (string \"%s\") not "
                                            "an integer\n",
                                            CLIargnum - 1,
                                            data.cmdargtoken[CLIargnum].val.string);
                                    }
                                    rval = 1;
                                    break;
                            }
                        }
                    }
                    break;


                case CLIARG_IMG:
                    //printf("token is CLIARG_IMG\n");
                    if(errmsg == 1)
                    {
                        printf("  arg %d (image \"%s\") not an integer\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.string);
                    }
                    rval = 1;
                    break;

                case CLIARG_STR:
                    //printf("token is CLIARG_STR\n");
                    if(funcargtype == CLIARG_ONOFF)
                    {
                        if((strcmp(data.cmdargtoken[CLIargnum].val.string, "on") == 0)
                                || (strcmp(data.cmdargtoken[CLIargnum].val.string, "ON") == 0))
                        {
                            printf("CONVERTING on to 1\n");
                            data.cmdargtoken[CLIargnum].val.numl = 1;
                            rval = 0;
                        }
                        if((strcmp(data.cmdargtoken[CLIargnum].val.string, "off") == 0)
                                || (strcmp(data.cmdargtoken[CLIargnum].val.string, "OFF") == 0))
                        {
                            printf("CONVERTING on to 0\n");
                            data.cmdargtoken[CLIargnum].val.numl = 0;
                            rval = 0;
                        }
                    }
                    else
                    {
                        if(errmsg == 1)
                        {
                            printf("  arg %d (command \"%s\") not an integer\n",
                                   CLIargnum - 1,
                                   data.cmdargtoken[CLIargnum].val.string);
                        }
                        rval = 1;
                    }
                    break;
            }
            break;



        // Argument should be string, but not image
        //
        case CLIARG_STR_NOT_IMG:
            switch(data.cmdargtoken[CLIargnum].type)
            {

                // Token is floatxx
                // -> not OK
                //
                case CLIARG_FLOAT64 & 0x0000FFFF :
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%f",
                             data.cmdargtoken[CLIargnum].val.numf);
                    if(errmsg == 1)
                    {
                        printf("  arg %d (float %f) not a non-img-string\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.numf);
                    }
                    rval = 1;
                    break;

                // Token is xintxx
                // -> not OK
                //
                case CLIARG_INT64 & 0x0000FFFF :
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%ld",
                             data.cmdargtoken[CLIargnum].val.numl);
                    if(errmsg == 1)
                    {
                        printf("  arg %d (integer %ld) not a non-img-string\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.numl);
                    }
                    rval = 1;
                    break;


                // Token is string, but not image
                // -> OK
                //
                case CLIARG_STR_NOT_IMG: // OK
                    rval = 0;
                    break;


                // Token is image
                // -> not OK
                //
                case CLIARG_IMG:
                    if(errmsg == 1)
                    {
                        printf("  arg %d (image %s) not a non-img-string\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.string);
                    }
                    rval = 1;
                    break;


                // Token is string
                // -> not OK
                //
                case CLIARG_STR:
                    printf("arg %d is command (=\"%s\"), but should be string\n",
                           CLIargnum,
                           data.cmdargtoken[CLIargnum].val.string);
                    rval = 1;
                    break;



                case 6:
                    rval = 0;
                    break;
            }
            break;





        // Argument should be existing image
        //
        case CLIARG_IMG:
            switch(data.cmdargtoken[CLIargnum].type)
            {


                // Token is floatxx
                // -> not OK
                //
                case CLIARG_FLOAT32 & 0x0000FFFF :
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%f",
                             data.cmdargtoken[CLIargnum].val.numf);
                    if(errmsg == 1)
                    {
                        printf("  arg %d (float %f) not an image\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.numf);
                    }
                    rval = 1;
                    break;


                // Token is xintxx
                // -> not OK
                //
                case CLIARG_INT64 & 0x0000FFFF:
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%ld",
                             data.cmdargtoken[CLIargnum].val.numl);
                    if(errmsg == 1)
                    {
                        printf("  arg %d (integer %ld) not an image\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.numl);
                    }
                    rval = 1;
                    break;


                // Token is string not image
                // -> not OK
                //
                case CLIARG_STR_NOT_IMG:
                    if(errmsg == 1)
                    {
                        printf("  arg %d (string \"%s\") not an image\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.string);
                    }
                    rval = 1;
                    break;


                // Token is image
                // -> OK
                //
                case CLIARG_IMG:
                    rval = 0;
                    break;


                // Token is string
                // -> not OK
                //
                case CLIARG_STR:
                    if(errmsg == 1)
                    {
                        printf("  arg %d (string \"%s\") not an image\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.string);
                    }
                    rval = 1;
                    break;


                case 6:
                    rval = 0;
                    break;
            }
            break;




        // Argument should be string (image or not)
        //
        case CLIARG_STR:
            switch(data.cmdargtoken[CLIargnum].type)
            {

                // Token is floatxx
                // -> not OK
                //
                case CLIARG_FLOAT32 & 0x0000FFFF :
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%f",
                             data.cmdargtoken[CLIargnum].val.numf);
                    if(errmsg == 1)
                    {
                        printf("  arg %d (float %f) not a string or image\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.numf);
                    }
                    rval = 1;
                    break;


                // Token is xintxx
                // -> not OK
                //
                case CLIARG_INT64 & 0x0000FFFF :
                    snprintf(data.cmdargtoken[CLIargnum].val.string,
                             STRINGMAXLEN_CMDARGTOKEN_VAL,
                             "%ld",
                             data.cmdargtoken[CLIargnum].val.numl);
                    if(errmsg == 1)
                    {
                        printf("  arg %d (integer %ld) not string or image\n",
                               CLIargnum - 1,
                               data.cmdargtoken[CLIargnum].val.numl);
                    }
                    rval = 1;
                    break;


                // Token is string not image
                // -> OK
                //
                case CLIARG_STR_NOT_IMG:
                    rval = 0;
                    break;


                // Token is image
                // -> OK
                //
                case CLIARG_IMG:
                    rval = 0;
                    break;


                // Token is string
                // -> OK
                //
                case CLIARG_STR:
                    rval = 0;
                    break;

                case 6:
                    rval = 0;
                    break;
            }
            break;

        default :
            printf("Can't resolve arg type\n");
            break;
    }

    if(rval == 2)
    {
        if(errmsg == 1)
        {
            printf("arg %d: wrong arg type 0x%X ->  0x%X  vs 0x%X\n",
                   CLIargnum,
                   funcargtype,
                   funcargtype & 0x0000FFFF,
                   data.cmdargtoken[CLIargnum].type);
        }
        rval = 1;
    }

    DEBUG_TRACE_FEXIT();
    return rval;
}




/**
 * @brief Check that input CLI argument matches required argument type
 *
 * @param CLIargnum
 * @param funcargtype
 * @return int
 */
int CLI_checkarg(int CLIargnum, uint32_t funcargtype)
{
    DEBUG_TRACE_FSTART();

    int rval;

    if(CLIargnum == 1)
    {
        argcheck_process_flag = 1;
    }

    if(argcheck_process_flag == 1)
    {
        rval = CLI_checkarg0(CLIargnum, funcargtype, 1);
    }
    else
    {
        rval = 1;
    }

    DEBUG_TRACE_FEXIT();
    return rval;
}

/**
 * @brief Check that input CLI argument matches required argument type - do not print error message
 *
 * @param CLIargnum
 * @param funcargtype
 * @return int
 */
int CLI_checkarg_noerrmsg(int CLIargnum, uint32_t funcargtype)
{
    DEBUG_TRACE_FSTART();

    int rval;

    if(CLIargnum == 1)
    {
        argcheck_process_flag = 1;
    }

    if(argcheck_process_flag == 1)
    {
        rval = CLI_checkarg0(CLIargnum, funcargtype, 0);
    }
    else
    {
        rval = 1;
    }

    DEBUG_TRACE_FEXIT();
    return rval;
}




/** @brief Check array of command line (CLI) arguments
 *
 * Use list of arguments in fpscliarg[].
 * Skip arguments that have CLICMDARG_FLAG_NOCLI flag.
 *
 * CLIarg keep count of argument position in CLI call
 *
 */
errno_t CLI_checkarg_array(CLICMDARGDEF fpscliarg[], int nbarg)
{
    DEBUG_TRACE_FSTART();

    // initialize arg check
    argcheck_process_flag = 1;

    int argindexmatch = -1;
    // check if CLI argument 1 is one of the function parameters keys
    // if it is, set argindexmatch to the function parameter index
    for(int arg = 0; arg < nbarg; arg++)
    {
        if(strcmp(data.cmdargtoken[1].val.string, fpscliarg[arg].fpstag) == 0)
        {
            argindexmatch = arg;
        }
    }

    // if CLI arg 1 is a function parameter, set function parameter to value entered in CLI arg 2
    if(argindexmatch != -1)
    {
        //printf("match to arg %s\n", fpscliarg[argindexmatch].fpstag); //TEST

        if(data.cmdargtoken[2].type == CLIARG_MISSING)
        {
            printf("Setting arg %s : input missing\n",
                   fpscliarg[argindexmatch].fpstag);
            DEBUG_TRACE_FEXIT();
            return RETURN_CLICHECKARGARRAY_FAILURE;
        }

        DEBUG_TRACEPOINT("calling CLI_checkarg");
        if(CLI_checkarg(2, fpscliarg[argindexmatch].type) == 0)
        {
            int cmdi = data.cmdindex;
            switch(fpscliarg[argindexmatch].type)  // & 0x0000FFFF)
            {
                case CLIARG_FLOAT32:
                    data.cmd[cmdi].argdata[argindexmatch].val.f32 =
                        data.cmdargtoken[2].val.numf;
                    break;
                case CLIARG_FLOAT64:
                    data.cmd[cmdi].argdata[argindexmatch].val.f64 =
                        data.cmdargtoken[2].val.numf;
                    break;
                case CLIARG_INT32:
                    data.cmd[cmdi].argdata[argindexmatch].val.i32 =
                        data.cmdargtoken[2].val.numl;
                    break;
                case CLIARG_INT64:
                    data.cmd[cmdi].argdata[argindexmatch].val.i64 =
                        data.cmdargtoken[2].val.numl;
                    break;
                case CLIARG_UINT32:
                    data.cmd[cmdi].argdata[argindexmatch].val.ui32 =
                        data.cmdargtoken[2].val.numl;
                    break;
                case CLIARG_UINT64:
                    data.cmd[cmdi].argdata[argindexmatch].val.ui64 =
                        data.cmdargtoken[2].val.numl;
                    break;
                case CLIARG_ONOFF:
                    data.cmd[cmdi].argdata[argindexmatch].val.i64 =
                        data.cmdargtoken[2].val.numl;
                    break;
                case CLIARG_STR_NOT_IMG:
                    strncpy(data.cmd[cmdi].argdata[argindexmatch].val.s,
                            data.cmdargtoken[2].val.string,
                            STRINGMAXLEN_CLICMDARG - 1);
                    break;
                case CLIARG_IMG:
                    strncpy(data.cmd[cmdi].argdata[argindexmatch].val.s,
                            data.cmdargtoken[2].val.string,
                            STRINGMAXLEN_CLICMDARG - 1);
                    break;
                case CLIARG_STR:
                    strncpy(data.cmd[cmdi].argdata[argindexmatch].val.s,
                            data.cmdargtoken[2].val.string,
                            STRINGMAXLEN_CLICMDARG - 1);
                    break;
            }
        }
        else
        {
            printf("Setting arg %s : Wrong type\n",
                   fpscliarg[argindexmatch].fpstag);
            DEBUG_TRACE_FEXIT();
            return RETURN_CLICHECKARGARRAY_FAILURE;
        }

        printf("Argument %s value updated\n", fpscliarg[argindexmatch].fpstag);

        //printf("arg 1: [%d] %s %f %ld\n", data.cmdargtoken[2].type, data.cmdargtoken[2].val.string, data.cmdargtoken[2].val.numf, data.cmdargtoken[2].val.numl);
        DEBUG_TRACE_FEXIT();
        return RETURN_CLICHECKARGARRAY_FUNCPARAMSET;
    }

    //printf("arg 1: %s %f %ld\n", data.cmdargtoken[2].val.string);

    int nberr  = 0;
    int CLIarg = 0; // index of argument in CLI call
    for(int arg = 0; arg < nbarg; arg++)
    {
        char argtypestring[16];
        switch(fpscliarg[arg].type)
        {
            case CLIARG_FLOAT32:
                strcpy(argtypestring, "FLT32");
                break;
            case CLIARG_FLOAT64:
                strcpy(argtypestring, "FLT64");
                break;
            case CLIARG_ONOFF:
                strcpy(argtypestring, "ONOFF");
                break;
            case CLIARG_INT32:
                strcpy(argtypestring, "INT32");
                break;
            case CLIARG_UINT32:
                strcpy(argtypestring, "UINT32");
                break;
            case CLIARG_INT64:
                strcpy(argtypestring, "INT64");
                break;
            case CLIARG_UINT64:
                strcpy(argtypestring, "UINT64");
                break;
            case CLIARG_STR_NOT_IMG:
                strcpy(argtypestring, "STRnIMG");
                break;
            case CLIARG_IMG:
                strcpy(argtypestring, "IMG");
                break;
            case CLIARG_STREAM:
                strcpy(argtypestring, "STREAM");
                break;
            case CLIARG_STR:
                strcpy(argtypestring, "STRING");
                break;
        }

        if(!(fpscliarg[arg].flag & CLICMDARG_FLAG_NOCLI))
        {
            int cmdi = data.cmdindex;

            DEBUG_TRACEPOINT("  arg %d  CLI %2d  [%7s]  %s",
                             arg,
                             CLIarg,
                             argtypestring,
                             fpscliarg[arg].fpstag);

            if(strcmp(data.cmdargtoken[CLIarg + 1].val.string, ".") == 0)
            {
                // if arg token starts with "."
                DEBUG_TRACEPOINT("ADOPTING DEFAULT/LAST VALUE");
                switch(fpscliarg[arg].type)  // & 0x0000FFFF)
                {

                    case CLIARG_FLOAT32: // if desired type is float single precision
                        data.cmdargtoken[CLIarg + 1].val.numf =
                            data.cmd[cmdi].argdata[arg].val.f32;
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_FLOAT32;
                        break;

                    case CLIARG_FLOAT64: // if desired type is float double precision
                        data.cmdargtoken[CLIarg + 1].val.numf =
                            data.cmd[cmdi].argdata[arg].val.f64;
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_FLOAT64;
                        break;

                    case CLIARG_INT32:
                        data.cmdargtoken[CLIarg + 1].val.numl =
                            data.cmd[cmdi].argdata[arg].val.i32;
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_INT32;
                        break;

                    case CLIARG_INT64:
                        data.cmdargtoken[CLIarg + 1].val.numl =
                            data.cmd[cmdi].argdata[arg].val.i64;
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_INT64;
                        break;

                    case CLIARG_UINT32:
                        data.cmdargtoken[CLIarg + 1].val.numl =
                            data.cmd[cmdi].argdata[arg].val.ui32;
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_UINT32;
                        break;

                    case CLIARG_UINT64:
                        data.cmdargtoken[CLIarg + 1].val.numl =
                            data.cmd[cmdi].argdata[arg].val.ui64;
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_UINT64;
                        break;

                    case CLIARG_STR_NOT_IMG: // if desired is string not image
                        strncpy(data.cmdargtoken[CLIarg + 1].val.string,
                                data.cmd[cmdi].argdata[arg].val.s,
                                STRINGMAXLEN_CMDARGTOKEN_VAL - 1);
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_STR_NOT_IMG;
                        break;

                    case CLIARG_IMG: // should be image
                        strncpy(data.cmdargtoken[CLIarg + 1].val.string,
                                data.cmd[cmdi].argdata[arg].val.s,
                                STRINGMAXLEN_CMDARGTOKEN_VAL - 1);
                        if(image_ID(data.cmd[cmdi].argdata[arg].val.s) != -1)
                        {
                            // if image exists
                            data.cmdargtoken[CLIarg + 1].type = CLIARG_IMG;
                        }
                        else
                        {
                            data.cmdargtoken[CLIarg + 1].type = CLIARG_STR_NOT_IMG;
                        }
                        //printf("arg %d IMG        : %s\n", CLIarg+1, data.cmdargtoken[CLIarg+1].val.string);
                        break;

                    case CLIARG_STR:
                        strncpy(data.cmdargtoken[CLIarg + 1].val.string,
                                data.cmd[cmdi].argdata[arg].val.s,
                                STRINGMAXLEN_CMDARGTOKEN_VAL - 1);
                        data.cmdargtoken[CLIarg + 1].type = CLIARG_STR;
                        break;
                }
            }

            DEBUG_TRACEPOINT("calling CLI_checkarg");
            if(CLI_checkarg(CLIarg + 1, fpscliarg[arg].type) == 0)
            {
                DEBUG_TRACEPOINT("successful parsing, update default to last");
                switch(fpscliarg[arg].type)  // & 0x0000FFFF)
                {
                    case CLIARG_FLOAT32:
                        data.cmd[cmdi].argdata[arg].val.f32 =
                            data.cmdargtoken[CLIarg + 1].val.numf;
                        break;
                    case CLIARG_FLOAT64:
                        data.cmd[cmdi].argdata[arg].val.f64 =
                            data.cmdargtoken[CLIarg + 1].val.numf;
                        break;
                    case CLIARG_INT32:
                        data.cmd[cmdi].argdata[arg].val.i32 =
                            data.cmdargtoken[CLIarg + 1].val.numl;
                        break;
                    case CLIARG_INT64:
                        data.cmd[cmdi].argdata[arg].val.i64 =
                            data.cmdargtoken[CLIarg + 1].val.numl;
                        break;
                    case CLIARG_UINT32:
                        data.cmd[cmdi].argdata[arg].val.ui32 =
                            data.cmdargtoken[CLIarg + 1].val.numl;
                        break;
                    case CLIARG_UINT64:
                        data.cmd[cmdi].argdata[arg].val.ui64 =
                            data.cmdargtoken[CLIarg + 1].val.numl;
                        break;
                    case CLIARG_STR_NOT_IMG:
                        strncpy(data.cmd[cmdi].argdata[arg].val.s,
                                data.cmdargtoken[CLIarg + 1].val.string,
                                STRINGMAXLEN_CLICMDARG - 1);
                        break;
                    case CLIARG_IMG:
                        strncpy(data.cmd[cmdi].argdata[arg].val.s,
                                data.cmdargtoken[CLIarg + 1].val.string,
                                STRINGMAXLEN_CLICMDARG - 1);
                        break;
                    case CLIARG_STR:
                        strncpy(data.cmd[cmdi].argdata[arg].val.s,
                                data.cmdargtoken[CLIarg + 1].val.string,
                                STRINGMAXLEN_CLICMDARG - 1);
                        break;
                }
            }
            else
            {
                if(functionhelp_called == 1)
                {
                    DEBUG_TRACE_FEXIT();
                    return RETURN_CLICHECKARGARRAY_HELP;
                }
                nberr++;
            }
            CLIarg++;
        }
        else
        {
            DEBUG_TRACEPOINT("argument not part of CLI");
            DEBUG_TRACEPOINT("  arg %d  IGNORED [%7s]  %s",
                             arg,
                             argtypestring,
                             fpscliarg[arg].fpstag);
        }
    }

    DEBUG_TRACEPOINT("Number of arg error(s): %d / %d", nberr, CLIarg);

    if(nberr == 0)
    {
        DEBUG_TRACE_FEXIT();
        return RETURN_CLICHECKARGARRAY_SUCCESS;
    }
    else
    {
        DEBUG_TRACE_FEXIT();
        return RETURN_CLICHECKARGARRAY_FAILURE;
    }
}





/** @brief Build FPS content from FPSCLIARG list
 *
 * All CLI arguments converted to FPS parameters
 *
 */
int CLIargs_to_FPSparams_setval(CLICMDARGDEF               fpscliarg[],
                                int                        nbarg,
                                FUNCTION_PARAMETER_STRUCT *fps)
{
    DEBUG_TRACE_FSTART();

    int NBarg_processed = 0;

    for(int arg = 0; arg < nbarg; arg++)
    {
        if(!(fpscliarg[arg].flag & CLICMDARG_FLAG_NOFPS))
        {
            // if argument is part of FPS
            switch(fpscliarg[arg].type)
            {
                case CLIARG_FLOAT32:
                    functionparameter_SetParamValue_FLOAT32(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.numf);
                    NBarg_processed++;
                    break;

                case CLIARG_FLOAT64:
                    functionparameter_SetParamValue_FLOAT64(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.numf);
                    NBarg_processed++;
                    break;

                case CLIARG_ONOFF:
                    functionparameter_SetParamValue_ONOFF(
                        fps,
                        fpscliarg[arg].fpstag,
                        (int) data.cmdargtoken[arg + 1].val.numl);
                    NBarg_processed++;
                    break;

                case CLIARG_INT32:
                    functionparameter_SetParamValue_INT32(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.numl);
                    NBarg_processed++;
                    break;

                case CLIARG_UINT32:
                    functionparameter_SetParamValue_UINT32(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.numl);
                    NBarg_processed++;
                    break;

                case CLIARG_INT64:
                    functionparameter_SetParamValue_INT64(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.numl);
                    NBarg_processed++;
                    break;

                case CLIARG_UINT64:
                    functionparameter_SetParamValue_UINT64(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.numl);
                    NBarg_processed++;
                    break;

                case CLIARG_STR_NOT_IMG:
                    functionparameter_SetParamValue_STRING(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.string);
                    NBarg_processed++;
                    break;

                case CLIARG_IMG:
                    functionparameter_SetParamValue_STRING(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.string);
                    NBarg_processed++;
                    break;

                case CLIARG_STREAM:
                    functionparameter_SetParamValue_STRING(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.string);
                    NBarg_processed++;
                    break;

                case CLIARG_STR:
                    functionparameter_SetParamValue_STRING(
                        fps,
                        fpscliarg[arg].fpstag,
                        data.cmdargtoken[arg + 1].val.string);
                    NBarg_processed++;
                    break;
            }
        }
    }

    DEBUG_TRACE_FEXIT();
    return NBarg_processed;
}




/** @brief Build FPS from command args
 */
int CMDargs_to_FPSparams_create(
    FUNCTION_PARAMETER_STRUCT *fps
)
{
    DEBUG_TRACE_FSTART();

    int  NBarg_processed = 0;
    long fpi             = 0;


    for(int argi = 0; argi < data.cmd[data.cmdindex].nbarg; argi++)
    {
        if(!(data.cmd[data.cmdindex].argdata[argi].flag &
                CLICMDARG_FLAG_NOFPS))
        {
            // if argument is part of FPS
            long tmpvall = 0;

            switch(data.cmd[data.cmdindex].argdata[argi].type)
            {
                // float point types

                case CLIARG_FLOAT32:
                {
                    float tmpf = data.cmd[data.cmdindex].argdata[argi].val.f32;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_FLOAT32,
                        FPFLAG_DEFAULT_INPUT,
                        &tmpf,
                        NULL);
                    NBarg_processed++;
                }
                break;

                case CLIARG_FLOAT64:
                {
                    double tmplf = data.cmd[data.cmdindex].argdata[argi].val.f64;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_FLOAT64,
                        FPFLAG_DEFAULT_INPUT,
                        &tmplf,
                        NULL);
                    NBarg_processed++;
                }
                break;

                // integer typtes

                case CLIARG_ONOFF: // default to INT64
                {
                    tmpvall = data.cmd[data.cmdindex].argdata[argi].val.ui64;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_ONOFF,
                        FPFLAG_DEFAULT_INPUT,
                        &tmpvall,
                        NULL);
                    NBarg_processed++;
                }
                break;

                /* case CLIARG_LONG: // default to INT64
                 {
                     tmpvall = data.cmd[data.cmdindex].argdata[argi].val.l;
                     function_parameter_add_entry(fps, data.cmd[data.cmdindex].argdata[argi].fpstag,
                                                  data.cmd[data.cmdindex].argdata[argi].descr,
                                                  FPTYPE_INT64, FPFLAG_DEFAULT_INPUT, &tmpvall,
                                                  NULL);
                     NBarg_processed++;
                 }
                 break;*/

                case CLIARG_INT32:
                {
                    int32_t tmpi32 = data.cmd[data.cmdindex].argdata[argi].val.i32;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_INT32,
                        FPFLAG_DEFAULT_INPUT,
                        &tmpi32,
                        NULL);
                    NBarg_processed++;
                }
                break;

                case CLIARG_UINT32:
                {
                    uint32_t tmpui32 =
                        data.cmd[data.cmdindex].argdata[argi].val.ui32;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_UINT32,
                        FPFLAG_DEFAULT_INPUT,
                        &tmpui32,
                        NULL);
                    NBarg_processed++;
                }
                break;

                case CLIARG_INT64:
                {
                    int64_t tmpi64 = data.cmd[data.cmdindex].argdata[argi].val.i64;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_INT64,
                        FPFLAG_DEFAULT_INPUT,
                        &tmpi64,
                        NULL);
                    NBarg_processed++;
                }
                break;

                case CLIARG_UINT64:
                {
                    uint64_t tmpui64 =
                        data.cmd[data.cmdindex].argdata[argi].val.ui64;
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_UINT64,
                        FPFLAG_DEFAULT_INPUT,
                        &tmpui64,
                        NULL);
                    NBarg_processed++;
                }
                break;

                case CLIARG_STR_NOT_IMG:
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_STRING,
                        FPFLAG_DEFAULT_INPUT,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        NULL);
                    NBarg_processed++;
                    break;

                case CLIARG_IMG:
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_STREAMNAME,
                        FPFLAG_DEFAULT_INPUT,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        NULL);
                    NBarg_processed++;
                    break;

                case CLIARG_STREAM:
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_STREAMNAME,
                        FPFLAG_DEFAULT_INPUT,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        NULL);
                    NBarg_processed++;
                    break;

                case CLIARG_STR:
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_STRING,
                        FPFLAG_DEFAULT_INPUT,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        NULL);
                    NBarg_processed++;
                    break;

                case CLIARG_FILENAME:
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_FILENAME,
                        FPFLAG_DEFAULT_INPUT,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        NULL);
                    NBarg_processed++;
                    break;

                case CLIARG_FITSFILENAME:
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_FITSFILENAME,
                        FPFLAG_DEFAULT_INPUT,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        NULL);
                    NBarg_processed++;
                    break;

                case CLIARG_FPSNAME:
                    //printf("ADDING FPS ENTRY %s at index %d\n", data.cmd[data.cmdindex].argdata[argi].fpstag, argi);
                    function_parameter_add_entry(
                        fps,
                        data.cmd[data.cmdindex].argdata[argi].fpstag,
                        data.cmd[data.cmdindex].argdata[argi].descr,
                        FPTYPE_FPSNAME,
                        FPFLAG_DEFAULT_INPUT | FPFLAG_FPS_RUN_REQUIRED,
                        data.cmd[data.cmdindex].argdata[argi].val.s,
                        &fpi);
                    //printf("fpi = %ld\n", fpi);
                    fps->parray[fpi].info.fps.FPSNBparamMAX = 0;
                    NBarg_processed++;
                    break;
            }
        }
    }

    DEBUG_TRACE_FEXIT();
    return NBarg_processed;
}




/** @brief get FPS pointer to function argument/parameter
 */
void *get_farg_ptr(
    char *tag,
    long *fpsi
)
{
    DEBUG_TRACE_FSTART();

    void *ptr = NULL;

    DEBUG_TRACEPOINT("looking for pointer %s", tag);
    DEBUG_TRACEPOINT("FPS_CMDCODE = %d", data.FPS_CMDCODE);

    if(data.FPS_CMDCODE != 0)
    {
        // look for pointer in FPS
        // We use INT64 type as default
        ptr = (void *) functionparameter_GetParamPtr_generic(data.fpsptr,
                tag,
                fpsi);
    }
    else
    {
        for(int argi = 0; argi < data.cmd[data.cmdindex].nbarg; argi++)
        {
            if(strcmp(data.cmd[data.cmdindex].argdata[argi].fpstag, tag) == 0)
            {
                ptr = (void *)(&data.cmd[data.cmdindex].argdata[argi].val);
                break;
            }
        }
    }
    DEBUG_TRACEPOINT("found pointer");

    DEBUG_TRACE_FEXIT();
    return ptr;
}
