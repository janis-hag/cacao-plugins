/* ================================================================== */
/* ================================================================== */
/*            DEPENDENCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"
#include "CommandLineInterface/fps/fps_GetParamIndex.h"

#include "BMCApi.h"

#include <math.h>

/* ================================================================== */
/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */

static char *DMin_streamname;
static long fpi_DMin_streamname;

static char *TTMin_streamname;
static long fpi_TTMin_streamname;

static float *max_stroke;
static long fpi_max_stroke;

static CLICMDARGDEF farg[] =
{
    {
        CLIARG_IMG,
        ".DMin",
        "Actuators stream",
        "",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &DMin_streamname,
        &fpi_DMin_streamname
    },
    {
        CLIARG_IMG,
        ".TTMin",
        "Tip-Tilt stream",
        "",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &TTMin_streamname,
        &fpi_TTMin_streamname
    },
    {
        CLIARG_FLOAT32,
        ".max_stroke",
        "Maximum stroke of DM [-]",
        "0.9",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &max_stroke,
        &fpi_max_stroke
    },
};


static CLICMDDATA CLIcmddata =
{
    "display",
    "Connect to deformable mirror and display",
    CLICMD_FIELDS_DEFAULTS
};

/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                         */
/* ================================================================== */
/* ================================================================== */

static errno_t help_function()
{
    return RETURN_SUCCESS;
}

static errno_t customCONFsetup()
{
    if (data.fpsptr != NULL)
    {
        data.fpsptr->parray[fpi_max_stroke].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_max_stroke].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_max_stroke].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_max_stroke].val.f32[1] = 0; // min
        data.fpsptr->parray[fpi_max_stroke].val.f32[2] = 1; // max
    }

    return RETURN_SUCCESS;
}

static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Variables **********/

    int error = NO_ERR;
    DM dm = {};
    uint32_t *map_lut;
    double *dm_array;
    int k;

    /********** Open BMC **********/

    processinfo_WriteMessage(processinfo, "Code version: 12.04.2023");

    processinfo_WriteMessage(processinfo, "Opening DM");
    error = BMCOpen(&dm, "17BW023#065");
    if (error) {
        printf("\nThe error %d happened while opening deformable mirror\n", error);
        return error;
    }

    processinfo_WriteMessage(processinfo, "Creating DM LUT Map");
    map_lut = (uint32_t *)malloc(sizeof(uint32_t)*MAX_DM_SIZE);

    for(k=0; k<(int)dm.ActCount; k++)
        map_lut[k] = 0;

    error = BMCLoadMap(&dm, NULL, map_lut);
    if (error) {
        printf("\nThe error %d happened while loading map for deformable mirror\n", error);
        return error;
    }

    processinfo_WriteMessage(processinfo, "Creating DM array");
    dm_array = malloc(sizeof(double)*(int)dm.ActCount);

    for(k=0; k<(int)dm.ActCount; k++)
        dm_array[k] = 0.5;

    error = BMCSetArray(&dm, dm_array, map_lut);
    if (error) {
        printf("\nThe error %d happened while setting array for deformable mirror\n", error);
        return error;
    }

    /********** Open streams **********/

    // create output arrays
    uint32_t *imsizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);

    processinfo_WriteMessage(processinfo, "Connecting to stream");
    imageID IDDMin = image_ID(DMin_streamname);
    imageID IDTTMin = image_ID(TTMin_streamname);

    /********** Loop **********/

    processinfo_WriteMessage(processinfo, "Looping");

    int ii;

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

            for(ii=0; ii<10; ii++)
                dm_array[ii] = (data.image[IDDMin].array.F[ii+1])/3.5+0.5;

            for(; ii<130; ii++)
                dm_array[ii] = (data.image[IDDMin].array.F[ii+2])/3.5+0.5;

            for(; ii<140; ii++)
                dm_array[ii] = (data.image[IDDMin].array.F[ii+3])/3.5+0.5;

            dm_array[155] = data.image[IDTTMin].array.F[0]/5.0+0.5;
            dm_array[156] = data.image[IDTTMin].array.F[1]/5.0+0.5;

            // Prevent values to be out of range
            for(ii=0; ii<140; ii++){
                if (dm_array[ii] > *max_stroke)
                    dm_array[ii] = *max_stroke;

                if (dm_array[ii] < 0)
                    dm_array[ii] = 0;
            }

            for(; ii<160; ii++){
                if (dm_array[ii] > 1)
                    dm_array[ii] = 1;

                if (dm_array[ii] < 0)
                    dm_array[ii] = 0;
            }

            error = BMCSetArray(&dm, dm_array, map_lut);
            if (error) {
                printf("\nThe error %d happened while setting array for deformable mirror\n", error);
                return error;
            }

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    free(dm_array);

    processinfo_WriteMessage(processinfo, "Clearing array");
    error = BMCClearArray(&dm);
    if (error) {
        printf("\nThe error %d happened while clearing deforamble mirror\n", error);
        return error;
    }

    processinfo_WriteMessage(processinfo, "Closing DM");
    error = BMCClose(&dm);
    if (error) {
        printf("\nThe error %d happened while closing the shutter\n", error);
        return error;
    }

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t CLIADDCMD_KalAO_BMC__display()
{
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}

