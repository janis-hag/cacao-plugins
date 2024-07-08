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

static uint64_t *adc_synchronisation;
static long fpi_adc_synchronisation;

static uint64_t *ttm_offloading;
static long fpi_ttm_offloading;

static CLICMDARGDEF farg[] =
    {
        {
            CLIARG_ONOFF,
            ".adc_synchronisation",
            "ADC synchronisation ON/OFF",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&adc_synchronisation,
            &fpi_adc_synchronisation,
        },
        {
            CLIARG_ONOFF,
            ".ttm_offloading",
            "TTM offloading ON/OFF",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&ttm_offloading,
            &fpi_ttm_offloading,
        },
};

static CLICMDDATA CLIcmddata =
    {
        "dynconfig",
        "Dynamic config of KalAO-ICS",
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

    /********** Setup and loop **********/

    processinfo_WriteMessage(processinfo, "Looping");

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t
CLIADDCMD_KalAO_Config__dynconfig() {
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
