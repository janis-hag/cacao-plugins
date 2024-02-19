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

#define DATAPOINTS 1800 * 10

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

    /********** Open streams **********/

    processinfo_WriteMessage(processinfo, "Connecting to streams");

    imageID TTMinID = image_ID(TTMin_streamname);

    /********** Allocate streams **********/

    processinfo_WriteMessage(processinfo, "Allocating streams");

    // Identifiers for output streams
    imageID outID = image_ID("telemetry_ttm");

    uint32_t *imsizearray = (uint32_t *)malloc(sizeof(uint32_t) * 2);
    {
        // slopes
        imsizearray[0] = DATAPOINTS;
        imsizearray[1] = 3;
        create_image_ID("telemetry_ttm", 2, imsizearray, _DATATYPE_DOUBLE, 1, 10, 0, &outID);

        free(imsizearray);
    }

    for (int i = 0; i < 3 * DATAPOINTS; i++) {
        data.image[outID].array.F[i] = 0;
    }

    /********** Setup and loop **********/

    processinfo_WriteMessage(processinfo, "Looping");

    struct timespec ts;
    int i = 0;

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    /***** Write flux stream *****/

    data.image[outID].md[0].write = 1;

    timespec_get(&ts, TIME_UTC);

    data.image[outID].array.D[i] = 1.0 * ts.tv_sec + 0.000000001 * ts.tv_nsec;
    data.image[outID].array.D[DATAPOINTS + i] = data.image[TTMinID].array.F[0];
    data.image[outID].array.D[2 * DATAPOINTS + i] = data.image[TTMinID].array.F[1];

    processinfo_update_output_stream(processinfo, outID);

    i += 1;
    i %= DATAPOINTS;

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

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
