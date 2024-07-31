/* ================================================================== */
/* ================================================================== */
/*            DEPENDENCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"

#include <math.h>

/* ================================================================== */
/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */

typedef struct
{
    // lower index pixel coords in input raw image
    uint32_t Xraw;
    uint32_t Yraw;

    // output 2D coordinates
    uint32_t Xout;
    uint32_t Yout;

    // precomputed indices for speed
    uint64_t XYout_dx;
    uint64_t XYout_dy;
    uint64_t fluxout;

    // signal
    float dx;
    float dy;
    float flux;

} SHWFS_SPOTS;

#define MAXNB_SPOT 1000

// With parenthesis around the arguments and the expression to avoid operator precedence issues
#define RMA(n, avg, value) (((value) + (avg) * ((n)-1)) / n)
#define EMA(c, avg, value) ((c) * (value) + (1.0 - (c)) * (avg))

static char *spotcoords_fname;
static long fpi_spotcoords_fname;

static int64_t *algorithm;
static long fpi_algorithm;

static int64_t *averaging_method;
static long fpi_averaging_method;

static int64_t *averaging_length;
static long fpi_averaging_length;

static float *flux_max;
static long fpi_flux_max;

static float *flux_avg;
static long fpi_flux_avg;

static float *residual_rms;
static long fpi_residual_rms;

static float *slope_x_avg;
static long fpi_slope_x_avg;

static float *slope_y_avg;
static long fpi_slope_y_avg;

static char *wfsref_streamname;
static long fpi_wfsref_streamname;

static CLICMDARGDEF farg[] =
    {
        {
            CLIARG_FILENAME,
            ".spotcoords",
            "SH spot coordinates",
            "spots.txt",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&spotcoords_fname,
            &fpi_spotcoords_fname,
        },
        {
            CLIARG_INT64,
            ".algorithm",
            "Algorithm (0 = Quad-cell, 1 = Center of mass)",
            "1",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&algorithm,
            &fpi_algorithm,
        },
        {
            CLIARG_INT64,
            ".averaging_method",
            "Moving average method (0 = Rolling, 1 = Exponential)",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&averaging_method,
            &fpi_averaging_method,
        },
        {
            CLIARG_INT64,
            ".averaging_length",
            "Moving average (equivalent) length",
            "1000",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&averaging_length,
            &fpi_averaging_length,
        },
        {
            CLIARG_FLOAT32,
            ".flux_avg",
            "Avg. flux in active subapertures",
            "0",
            CLIARG_OUTPUT_DEFAULT,
            (void **)&flux_avg,
            &fpi_flux_avg,
        },
        {
            CLIARG_FLOAT32,
            ".flux_max",
            "Max. flux in active subapertures",
            "0",
            CLIARG_OUTPUT_DEFAULT,
            (void **)&flux_max,
            &fpi_flux_max,
        },
        {
            CLIARG_FLOAT32,
            ".residual_rms",
            "RMS residual slopes",
            "0",
            CLIARG_OUTPUT_DEFAULT,
            (void **)&residual_rms,
            &fpi_residual_rms,
        },
        {
            CLIARG_FLOAT32,
            ".slope_x_avg",
            "Average slope in X direction",
            "0",
            CLIARG_OUTPUT_DEFAULT,
            (void **)&slope_x_avg,
            &fpi_slope_x_avg,
        },
        {
            CLIARG_FLOAT32,
            ".slope_y_avg",
            "Average slope in Y direction",
            "0",
            CLIARG_OUTPUT_DEFAULT,
            (void **)&slope_y_avg,
            &fpi_slope_y_avg,
        },
        {
            CLIARG_IMG,
            ".wfsref",
            "WFS reference",
            "",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&wfsref_streamname,
            &fpi_wfsref_streamname,
        },
};

static CLICMDDATA CLIcmddata =
    {
        "process",
        "Computes slopes/centroids from SHWFS frames",
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
        data.fpsptr->parray[fpi_algorithm].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_algorithm].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_algorithm].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_algorithm].val.i64[2] = 1; // max

        data.fpsptr->parray[fpi_averaging_length].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_averaging_length].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_averaging_length].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_averaging_length].val.i64[1] = 1;       // min
        data.fpsptr->parray[fpi_averaging_length].val.i64[2] = 1000000; // max
    }

    return RETURN_SUCCESS;
}

static int read_spots_coords(SHWFS_SPOTS *spotcoord) {
    int NBspot = 0;

    FILE *fp;

    fp = fopen(spotcoords_fname, "r");
    if (fp == NULL) {
        perror("Unable to open file!");
        exit(1);
    }

    int xin, yin, xout, yout;

    char keyw[16];

    int loopOK = 1;
    while (loopOK == 1) {
        int ret = fscanf(fp, "%s %d %d %d %d", keyw, &xin, &yin, &xout, &yout);
        if (ret == EOF) {
            loopOK = 0;
        } else {
            if ((ret == 5) && (strcmp(keyw, "SPOT") == 0)) {
                printf("Found SPOT %5d %5d   %5d %5d\n", xin, yin, xout, yout);
                spotcoord[NBspot].Xraw = xin;
                spotcoord[NBspot].Yraw = yin;
                spotcoord[NBspot].Xout = xout;
                spotcoord[NBspot].Yout = yout;
                NBspot++;
            }
        }
    }
    printf("Loaded %d spots\n", NBspot);

    fclose(fp);

    return NBspot;
}

static errno_t compute_function() {
    DEBUG_TRACE_FSTART();

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Load spots coordinates **********/

    processinfo_WriteMessage(processinfo, "Loading spots coordinates");

    // Allocate spots
    SHWFS_SPOTS *spotcoord = (SHWFS_SPOTS *)malloc(sizeof(SHWFS_SPOTS) * MAXNB_SPOT);

    char msgstring[200];
    sprintf(msgstring, "Loading spot <- %s", spotcoords_fname);
    processinfo_WriteMessage(processinfo, msgstring);

    int NBspot = read_spots_coords(spotcoord);

    // size of output 2D representation
    imageID inID = processinfo->triggerstreamID;
    uint32_t sizeinX = data.image[inID].md[0].size[0];
    uint32_t sizeinY = data.image[inID].md[0].size[1];
    uint32_t sizeoutX = 0;
    uint32_t sizeoutY = 0;

    for (int spot = 0; spot < NBspot; spot++) {
        if (spotcoord[spot].Xout + 1 > sizeoutX) {
            sizeoutX = spotcoord[spot].Xout + 1;
        }
        if (spotcoord[spot].Yout + 1 > sizeoutY) {
            sizeoutY = spotcoord[spot].Yout + 1;
        }
    }

    for (int spot = 0; spot < NBspot; spot++) {
        spotcoord[spot].XYout_dx = spotcoord[spot].Yout * (2 * sizeoutX) + spotcoord[spot].Xout;
        spotcoord[spot].XYout_dy = spotcoord[spot].Yout * (2 * sizeoutX) + spotcoord[spot].Xout + sizeoutX;
        spotcoord[spot].fluxout = spotcoord[spot].Yout * (sizeoutX) + spotcoord[spot].Xout;
    }

    printf("Output 2D representation: %d x %d\n", sizeoutX, sizeoutY);

    /********** Open streams **********/

    processinfo_WriteMessage(processinfo, "Connecting to streams");

    imageID wfsrefID = image_ID(wfsref_streamname);

    /********** Allocate streams **********/

    processinfo_WriteMessage(processinfo, "Allocating streams");

    // Identifiers for output streams
    imageID slopesID = image_ID("shwfs_slopes");
    imageID fluxID = image_ID("shwfs_flux");

    uint32_t *imsizearray = (uint32_t *)malloc(sizeof(uint32_t) * 2);
    {
        // slopes
        imsizearray[0] = sizeoutX * 2;
        imsizearray[1] = sizeoutY;
        create_image_ID("shwfs_slopes", 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &slopesID);

        // flux
        imsizearray[0] = sizeoutX;
        imsizearray[1] = sizeoutY;
        create_image_ID("shwfs_flux", 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &fluxID);

        free(imsizearray);
    }

    /********** Setup and loop **********/

    processinfo_WriteMessage(processinfo, "Looping");

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    float averaging_coeff = 0;

    float new_flux_max = 0;
    float new_flux_avg = 0;
    float new_residual_rms = 0;
    float new_slope_x_avg = 0;
    float new_slope_y_avg = 0;

    for (int spot = 0; spot < NBspot; spot++) {
        float dx = 0;
        float dy = 0;
        float flux = 0;

        /***** Quad-cell *****/

        if (*algorithm == 0) {
            // clang-format off
            float f00 = data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1];

            float f01 = data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3];

            float f10 = data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1];

            float f11 = data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];
            // clang-format on

            flux = f00 + f01 + f10 + f11;
            dx = (f01 + f11) - (f00 + f10);
            dy = (f10 + f11) - (f00 + f01);
        }

        /***** Center of mass *****/

        else {
            // clang-format off
            dx =
               - 1.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
               - 0.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1]
               + 0.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
               + 1.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];

            dy =
               - 1.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
               - 1.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
               - 1.5 * data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
               - 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
               + 1.5 * data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];

            flux = data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[inID].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[inID].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];
            // clang-format on
        }

        /***** Common part *****/

        if (flux > 1) {
            dx /= flux;
            dy /= flux;
        } else {
            dx = 0;
            dy = 0;
        }

        if (dx > 2) {
            dx = 2;
        } else if (dx < -2) {
            dx = -2;
        }

        if (dy > 2) {
            dy = 2;
        } else if (dy < -2) {
            dy = -2;
        }

        spotcoord[spot].dx = dx;
        spotcoord[spot].dy = dy;
        spotcoord[spot].flux = flux;

        dx -= data.image[wfsrefID].array.F[spotcoord[spot].XYout_dx];
        dy -= data.image[wfsrefID].array.F[spotcoord[spot].XYout_dy];

        if (flux > new_flux_max) {
            new_flux_max = flux;
        }

        new_flux_avg += flux;
        new_residual_rms += dx * dx + dy * dy;
        new_slope_x_avg += dx;
        new_slope_y_avg += dy;
    }

    if (*averaging_method == 0) {
        *flux_max = RMA(*averaging_length, *flux_max, new_flux_max);
        *flux_avg = RMA(*averaging_length, *flux_avg, new_flux_avg / NBspot);
        *residual_rms = RMA(*averaging_length, *residual_rms, sqrt(new_residual_rms / NBspot));
        *slope_x_avg = RMA(*averaging_length, *slope_x_avg, new_slope_x_avg / NBspot);
        *slope_y_avg = RMA(*averaging_length, *slope_y_avg, new_slope_y_avg / NBspot);
    } else {
        averaging_coeff = 2 / (*averaging_length + 1);

        *flux_max = EMA(averaging_coeff, *flux_max, new_flux_max);
        *flux_avg = EMA(averaging_coeff, *flux_avg, new_flux_avg / NBspot);
        *residual_rms = EMA(averaging_coeff, *residual_rms, sqrt(new_residual_rms / NBspot));
        *slope_x_avg = EMA(averaging_coeff, *slope_x_avg, new_slope_x_avg / NBspot);
        *slope_y_avg = EMA(averaging_coeff, *slope_y_avg, new_slope_y_avg / NBspot);
    }

    data.fpsptr->parray[fpi_flux_max].cnt0++;
    data.fpsptr->parray[fpi_flux_avg].cnt0++;
    data.fpsptr->parray[fpi_residual_rms].cnt0++;
    data.fpsptr->parray[fpi_slope_x_avg].cnt0++;
    data.fpsptr->parray[fpi_slope_y_avg].cnt0++;

    /***** Write slopes *****/

    data.image[slopesID].md[0].write = 1;

    for (int spot = 0; spot < NBspot; spot++) {
        data.image[slopesID].array.F[spotcoord[spot].XYout_dx] = spotcoord[spot].dx;
        data.image[slopesID].array.F[spotcoord[spot].XYout_dy] = spotcoord[spot].dy;
    }

    processinfo_update_output_stream(processinfo, slopesID);

    /***** Write flux stream *****/

    data.image[fluxID].md[0].write = 1;

    for (int spot = 0; spot < NBspot; spot++) {
        data.image[fluxID].array.F[spotcoord[spot].fluxout] = spotcoord[spot].flux;
    }

    processinfo_update_output_stream(processinfo, fluxID);

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    free(spotcoord);

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t
CLIADDCMD_KalAO_SHWFS__process() {
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
