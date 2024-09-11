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

static uint64_t *stroke_mode;
static long fpi_stroke_mode;

static float *target_stroke;
static long fpi_target_stroke;

static CLICMDARGDEF farg[] =
    {
        {
            CLIARG_IMG,
            ".DMin",
            "Actuators stream",
            "",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&DMin_streamname,
            &fpi_DMin_streamname,
        },
        {
            CLIARG_IMG,
            ".TTMin",
            "Tip-Tilt stream",
            "",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&TTMin_streamname,
            &fpi_TTMin_streamname,
        },
        {
            CLIARG_FLOAT32,
            ".max_stroke",
            "Maximum stroke of DM [-]",
            "0.9",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&max_stroke,
            &fpi_max_stroke,
        },
        {
            CLIARG_INT64,
            ".stroke_mode",
            "Stroke mode (0 = Mid-stroke, 1 = Minimize stroke)",
            "1",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&stroke_mode,
            &fpi_stroke_mode,
        },
        {
            CLIARG_FLOAT32,
            ".target_stroke",
            "Target stroke for minimize mode [-]",
            "0.2",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&target_stroke,
            &fpi_target_stroke,
        },
};

static CLICMDDATA CLIcmddata =
    {
        "display",
        "Connect to deformable mirror and display",
        CLICMD_FIELDS_DEFAULTS,
};

/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                         */
/* ================================================================== */
/* ================================================================== */

static errno_t help_function() {
    return RETURN_SUCCESS;
}

static errno_t customCONFsetup() {
    if (data.fpsptr != NULL) {
        data.fpsptr->parray[fpi_max_stroke].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_max_stroke].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_max_stroke].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_max_stroke].val.f32[1] = 0; // min
        data.fpsptr->parray[fpi_max_stroke].val.f32[2] = 1; // max

        data.fpsptr->parray[fpi_stroke_mode].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_stroke_mode].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_stroke_mode].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_stroke_mode].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_stroke_mode].val.i64[2] = 1; // max

        data.fpsptr->parray[fpi_target_stroke].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_target_stroke].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_target_stroke].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_target_stroke].val.f32[1] = 0; // min
        data.fpsptr->parray[fpi_target_stroke].val.f32[2] = 1; // max
    }

    return RETURN_SUCCESS;
}

static errno_t compute_function() {
    DEBUG_TRACE_FSTART();

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Variables **********/

    int error = NO_ERR;
    DM dm = {};
    uint32_t *map_lut;
    double *dm_array;
    int k;

    /********** Open streams **********/

    processinfo_WriteMessage(processinfo, "Connecting to streams");

    imageID DMinID = image_ID(DMin_streamname);
    imageID TTMinID = image_ID(TTMin_streamname);

    /********** Allocate streams **********/

    processinfo_WriteMessage(processinfo, "Allocating streams");

    imageID DMoutID = image_ID("bmc_commands_dm");
    imageID TTMoutID = image_ID("bmc_commands_ttm");

    {
        uint32_t *imsize = (uint32_t *)malloc(sizeof(uint32_t) * 2);

        imsize[0] = 12;
        imsize[1] = 12;
        create_image_ID("bmc_commands_dm", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &DMoutID);

        imsize[0] = 2;
        imsize[1] = 1;
        create_image_ID("bmc_commands_ttm", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &TTMoutID);

        free(imsize);
    }

    /********** Open BMC **********/

    processinfo_WriteMessage(processinfo, "Opening DM");

    error = BMCOpen(&dm, "17DW019#50D");
    if (error) {
        printf("\nThe error %d happened while opening deformable mirror\n", error);
        return error;
    }

    processinfo_WriteMessage(processinfo, "Creating DM LUT Map");

    map_lut = (uint32_t *)malloc(sizeof(uint32_t) * MAX_DM_SIZE);

    for (k = 0; k < (int)dm.ActCount; k++)
        map_lut[k] = 0;

    error = BMCLoadMap(&dm, NULL, map_lut);
    if (error) {
        printf("\nThe error %d happened while loading map for deformable mirror\n", error);
        return error;
    }

    processinfo_WriteMessage(processinfo, "Creating DM array");

    dm_array = malloc(sizeof(double) * (int)dm.ActCount);

    for (k = 0; k < (int)dm.ActCount; k++)
        dm_array[k] = 0;

    error = BMCSetArray(&dm, dm_array, map_lut);
    if (error) {
        printf("\nThe error %d happened while setting array for deformable mirror\n", error);
        return error;
    }

    /********** Loop **********/

    int ii;
    float full_stroke, half_stroke, min_stroke, offset;
    long cnt0sum;
    long cnt0sumref = 0;

    processinfo_WriteMessage(processinfo, "Looping");

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    cnt0sum = data.image[DMinID].md->cnt0 + data.image[TTMinID].md->cnt0;

    if (cnt0sum != cnt0sumref) {
        cnt0sumref = cnt0sum;

        full_stroke = *max_stroke;
        half_stroke = *max_stroke / 2;

        for (ii = 0; ii < 10; ii++)
            dm_array[ii] = data.image[DMinID].array.F[ii + 1] / 3.5 + half_stroke;

        for (; ii < 130; ii++)
            dm_array[ii] = data.image[DMinID].array.F[ii + 2] / 3.5 + half_stroke;

        for (; ii < 140; ii++)
            dm_array[ii] = data.image[DMinID].array.F[ii + 3] / 3.5 + half_stroke;

        dm_array[155] = data.image[TTMinID].array.F[0] / 5.0 + 0.5;
        dm_array[156] = data.image[TTMinID].array.F[1] / 5.0 + 0.5;

        // Prevent values to be out of range
        for (ii = 0; ii < 140; ii++) {
            if (dm_array[ii] > full_stroke)
                dm_array[ii] = full_stroke;

            if (dm_array[ii] < 0)
                dm_array[ii] = 0;
        }

        for (; ii < 160; ii++) {
            if (dm_array[ii] > 1)
                dm_array[ii] = 1;

            if (dm_array[ii] < 0)
                dm_array[ii] = 0;
        }

        // Apply stroke mode

        if (*stroke_mode == 1) {
            min_stroke = 1;

            for (ii = 0; ii < 140; ii++) {
                if (dm_array[ii] < min_stroke)
                    min_stroke = dm_array[ii];
            }

            offset = min_stroke - *target_stroke;

            if (offset > 0) {
                for (ii = 0; ii < 140; ii++)
                    dm_array[ii] -= offset;
            }
        }

        // Send command to DM

        error = BMCSetArray(&dm, dm_array, map_lut);
        if (error) {
            printf("\nThe error %d happened while setting array for deformable mirror\n", error);
            return error;
        }

        // Write command sent to DM

        data.image[DMoutID].md->write = 1;

        data.image[DMoutID].array.F[0] = 0;
        data.image[DMoutID].array.F[11] = 0;
        data.image[DMoutID].array.F[132] = 0;
        data.image[DMoutID].array.F[143] = 0;

        for (ii = 0; ii < 10; ii++)
            data.image[DMoutID].array.F[ii + 1] = dm_array[ii];

        for (; ii < 130; ii++)
            data.image[DMoutID].array.F[ii + 2] = dm_array[ii];

        for (; ii < 140; ii++)
            data.image[DMoutID].array.F[ii + 3] = dm_array[ii];

        processinfo_update_output_stream(processinfo, DMoutID);

        // Write command sent to TTM

        data.image[TTMoutID].md->write = 1;

        data.image[TTMoutID].array.F[0] = dm_array[155];
        data.image[TTMoutID].array.F[1] = dm_array[156];

        processinfo_update_output_stream(processinfo, TTMoutID);
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
errno_t
CLIADDCMD_KalAO_BMC__display() {
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
