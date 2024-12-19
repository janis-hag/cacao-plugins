/* ================================================================== */
/* ================================================================== */
/*            DEPENDENCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"

#include <math.h>
#include <time.h>

/* ================================================================== */
/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */

#define DATAPOINTS 1800 * 3

static char *TTMin_streamname;
static long fpi_TTMin_streamname;

static CLICMDARGDEF farg[] =
    {
        {
            CLIARG_IMG,
            ".TTMin",
            "Tip-Tilt stream",
            "",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&TTMin_streamname,
            &fpi_TTMin_streamname,
        },
};

static CLICMDDATA CLIcmddata =
    {
        "gather",
        "Save telemetry for KalAO GUI",
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
        // Nothing
    }

    return RETURN_SUCCESS;
}

static errno_t compute_function() {
    DEBUG_TRACE_FSTART();

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Open fps **********/

    FUNCTION_PARAMETER_STRUCT shwfs_fps;

    function_parameter_struct_connect("shwfs_process-1", &shwfs_fps, FPSCONNECT_SIMPLE);

    float *flux_avg = functionparameter_GetParamPtr_FLOAT32(&shwfs_fps, "flux_avg");
    float *flux_max = functionparameter_GetParamPtr_FLOAT32(&shwfs_fps, "flux_max");
    float *residual_rms = functionparameter_GetParamPtr_FLOAT32(&shwfs_fps, "residual_rms");
    float *slope_x_avg = functionparameter_GetParamPtr_FLOAT32(&shwfs_fps, "slope_x_avg");
    float *slope_y_avg = functionparameter_GetParamPtr_FLOAT32(&shwfs_fps, "slope_y_avg");

    /********** Open streams **********/

    processinfo_WriteMessage(processinfo, "Connecting to streams");

    imageID TTMinID = image_ID(TTMin_streamname);

    /********** Allocate streams **********/

    processinfo_WriteMessage(processinfo, "Allocating streams");

    // Identifiers for output streams
    imageID outID = image_ID("kalao_telemetry");

    uint32_t *imsizearray = (uint32_t *)malloc(sizeof(uint32_t) * 2);
    {
        // slopes
        imsizearray[0] = DATAPOINTS;
        imsizearray[1] = 9;
        create_image_ID("kalao_telemetry", 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &outID);

        free(imsizearray);
    }

    for (int i = 0; i < 9 * DATAPOINTS; i++) {
        data.image[outID].array.F[i] = 0;
    }

    /********** Loop **********/

    struct timespec ts;
    double timestamp;
    int i = 0;

    timespec_get(&ts, TIME_UTC);

    float timestamp_offset_float = (float)ts.tv_sec;

    processinfo_WriteMessage(processinfo, "Looping");

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    /***** Write flux stream *****/

    data.image[outID].md->write = 1;

    timespec_get(&ts, TIME_UTC);

    // Note: splitting timestamp in a big integer part representable by a float32
    // and a small non-integer part also representable by a float32
    timestamp = 1.0 * ts.tv_sec + 0.000000001 * ts.tv_nsec - timestamp_offset_float;

    // clang-format off
    data.image[outID].array.F[                 i] = timestamp_offset_float;
    data.image[outID].array.F[    DATAPOINTS + i] = (float) timestamp;
    data.image[outID].array.F[2 * DATAPOINTS + i] = data.image[TTMinID].array.F[0];
    data.image[outID].array.F[3 * DATAPOINTS + i] = data.image[TTMinID].array.F[1];
    data.image[outID].array.F[4 * DATAPOINTS + i] = *flux_avg;
    data.image[outID].array.F[5 * DATAPOINTS + i] = *flux_max;
    data.image[outID].array.F[6 * DATAPOINTS + i] = *residual_rms;
    data.image[outID].array.F[7 * DATAPOINTS + i] = *slope_x_avg;
    data.image[outID].array.F[8 * DATAPOINTS + i] = *slope_y_avg;
    // clang-format on

    processinfo_update_output_stream(processinfo, outID);

    i += 1;
    i %= DATAPOINTS;

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    function_parameter_struct_disconnect(&shwfs_fps);

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t
CLIADDCMD_KalAO_Telemetry__gather() {
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
