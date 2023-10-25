/* ================================================================== */
/* ================================================================== */
/*            DEPENDANCIES                                            */
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

static char *slopes_streamname;
static long fpi_slopes_streamname;

static char *spotcoords_fname;
static long fpi_spotcoords_fname;

static int64_t *algorithm;
static long fpi_algorithm;

static float *flux_averagecoeff;
static long fpi_flux_averagecoeff;

static float *flux_subaperture_brightest;
static long fpi_flux_subaperture_brightest;

static float *flux_subaperture_avg;
static long fpi_flux_subaperture_avg;

static float *residual;
static long fpi_residual;

static float *slope_x;
static long fpi_slope_x;

static float *slope_y;
static long fpi_slope_y;

static CLICMDARGDEF farg[] =
{
    {
        CLIARG_STREAM,
        ".outWFS",
        "Output stream",
        "",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &slopes_streamname,
        &fpi_slopes_streamname
    },
    {
        CLIARG_FILENAME,
        ".spotcoords",
        "SH spot coordinates",
        "spots.txt",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &spotcoords_fname,
        &fpi_spotcoords_fname
    },
    {
        CLIARG_INT64,
        ".algorithm",
        "Algorithm (0 = Quadcell, 1 = Center of mass)",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &algorithm,
        &fpi_algorithm
    },
    {
        CLIARG_FLOAT32,
        ".flux_averagecoeff",
        "Flux averaging coefficient",
        "0.1",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &flux_averagecoeff,
        &fpi_flux_averagecoeff
    },
    {
        CLIARG_FLOAT32,
        ".flux_subaperture_avg",
        "Avg. flux in a subaperture",
        "0",
        CLIARG_OUTPUT_DEFAULT,
        (void **) &flux_subaperture_avg,
        &fpi_flux_subaperture_avg
    },
    {
        CLIARG_FLOAT32,
        ".flux_subaperture_brightest",
        "Flux in brightest subaperture",
        "0",
        CLIARG_OUTPUT_DEFAULT,
        (void **) &flux_subaperture_brightest,
        &fpi_flux_subaperture_brightest
    },
    {
        CLIARG_FLOAT32,
        ".residual",
        "RMS Residual",
        "0",
        CLIARG_OUTPUT_DEFAULT,
        (void **) &residual,
        &fpi_residual
    },
    {
        CLIARG_FLOAT32,
        ".slope_x",
        "Average Tip",
        "0",
        CLIARG_OUTPUT_DEFAULT,
        (void **) &slope_x,
        &fpi_slope_x
    },
    {
        CLIARG_FLOAT32,
        ".slope_y",
        "Average Tilt",
        "0",
        CLIARG_OUTPUT_DEFAULT,
        (void **) &slope_y,
        &fpi_slope_y
    }
};


static CLICMDDATA CLIcmddata =
{
    "process",
    "Computes slopes/centroids from SHWFS frames",
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
        data.fpsptr->parray[fpi_algorithm].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_algorithm].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_algorithm].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_algorithm].val.i64[2] = 1; // max

        data.fpsptr->parray[fpi_flux_averagecoeff].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_flux_averagecoeff].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_flux_averagecoeff].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_flux_averagecoeff].val.f32[1] = 0; // min
        data.fpsptr->parray[fpi_flux_averagecoeff].val.f32[2] = 1; // max
    }

    return RETURN_SUCCESS;
}

static int read_spots_coords(SHWFS_SPOTS *spotcoord)
{
    int NBspot = 0;

    FILE *fp;

    fp = fopen(spotcoords_fname, "r");
    if(fp == NULL)
    {
        perror("Unable to open file!");
        exit(1);
    }

    int xin, yin, xout, yout;

    char keyw[16];

    int loopOK = 1;
    while(loopOK == 1)
    {
        int ret = fscanf(fp, "%s %d %d %d %d", keyw, &xin, &yin, &xout, &yout);
        if(ret == EOF)
        {
            loopOK = 0;
        }
        else
        {
            if((ret==5) && (strcmp(keyw, "SPOT") == 0))
            {
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

static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Load spots coordinates **********/

    // Allocate spots
    SHWFS_SPOTS *spotcoord = (SHWFS_SPOTS*) malloc(sizeof(SHWFS_SPOTS)*MAXNB_SPOT);

    char msgstring[200];
    sprintf(msgstring, "Loading spot <- %s", spotcoords_fname);
    processinfo_WriteMessage(processinfo, msgstring);

    int NBspot = read_spots_coords(spotcoord);

    char flux_streamname[FUNCTION_PARAMETER_STRMAXLEN + 6];
    sprintf(flux_streamname, "%s_flux", slopes_streamname);

    // size of output 2D representation
    imageID IDin = processinfo->triggerstreamID;
    uint32_t sizeinX = data.image[IDin].md[0].size[0];
    uint32_t sizeinY = data.image[IDin].md[0].size[1];
    uint32_t sizeoutX = 0;
    uint32_t sizeoutY = 0;
    for(int spot=0; spot<NBspot; spot++)
    {
        if ( spotcoord[spot].Xout + 1 > sizeoutX )
        {
            sizeoutX = spotcoord[spot].Xout+1;
        }
        if ( spotcoord[spot].Yout + 1 > sizeoutY )
        {
            sizeoutY = spotcoord[spot].Yout+1;
        }
    }

    for(int spot=0; spot<NBspot; spot++)
    {
        spotcoord[spot].XYout_dx = spotcoord[spot].Yout * (2*sizeoutX) + spotcoord[spot].Xout;
        spotcoord[spot].XYout_dy = spotcoord[spot].Yout * (2*sizeoutX) + spotcoord[spot].Xout + sizeoutX;
        spotcoord[spot].fluxout  = spotcoord[spot].Yout * (sizeoutX)   + spotcoord[spot].Xout;
    }

    printf("Output 2D representation: %d x %d\n", sizeoutX, sizeoutY);

    // create output arrays
    uint32_t *imsizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);

    // Identifiers for output streams
    imageID IDslopes = image_ID(slopes_streamname);
    imageID IDflux = image_ID(flux_streamname);

    {
        // slopes
        imsizearray[0] = sizeoutX * 2;
        imsizearray[1] = sizeoutY;
        create_image_ID(slopes_streamname, 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &IDslopes);

        // flux
        imsizearray[0] = sizeoutX;
        imsizearray[1] = sizeoutY;
        create_image_ID(flux_streamname, 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &IDflux);

        free(imsizearray);
    }

    /********** Setup and loop **********/

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    float new_flux_brightest = 0;
    float new_flux_avg = 0;
    float new_residual = 0;
    float new_slope_x = 0;
    float new_slope_y = 0;

    for(int spot=0; spot<NBspot; spot++)
    {
        float dx = 0;
        float dy = 0;
        float flux = 0;

        /***** Quadcell *****/

        if(algorithm == 0) {
            float f00 = data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1];

            float f01 = data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3];

            float f10 = data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1];

            float f11 = data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
                      + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];

            flux = f00 + f01 + f10 + f11;
            dx = (f01 + f11) - (f00 + f10);
            dx /= flux;
            dy = (f10 + f11) - (f00 + f01);
            dy /= flux;
        }

        /***** Center of mass *****/

        else {
            dx =
               - 1.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
               - 0.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1]
               + 0.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
               + 1.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];

            dy =
               - 1.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
               - 1.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
               - 1.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
               - 1.5 * data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
               - 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
               + 0.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
               + 1.5 * data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];

            flux = data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[IDin].array.F[ spotcoord[spot].Yraw      * sizeinX + spotcoord[spot].Xraw + 3]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 3]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 2) * sizeinX + spotcoord[spot].Xraw + 3]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw    ]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 1]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 2]
                 + data.image[IDin].array.F[(spotcoord[spot].Yraw + 3) * sizeinX + spotcoord[spot].Xraw + 3];

            dx /= flux;
            dy /= flux;
        }

        /***** Common part *****/

        spotcoord[spot].dx = dx;
        spotcoord[spot].dy = dy;
        spotcoord[spot].flux = flux;

        if(flux > new_flux_brightest) {
            new_flux_brightest = flux;
        }

        new_flux_avg += flux;
        new_residual += dx*dx + dy*dy;
        new_slope_x += dx;
        new_slope_y += dy;
    }

    *flux_subaperture_brightest = *flux_averagecoeff * new_flux_brightest + (1.0-*flux_averagecoeff) * *flux_subaperture_brightest;
    *flux_subaperture_avg = new_flux_avg/NBspot;
    *residual = sqrt(new_residual / NBspot);
    *slope_x = new_slope_x/NBspot;
    *slope_y = new_slope_y/NBspot;

    /***** Write slopes *****/

    data.image[IDslopes].md[0].write = 1;

    for(int spot=0; spot<NBspot; spot++)
    {
        data.image[IDslopes].array.F[spotcoord[spot].XYout_dx] = spotcoord[spot].dx;
        data.image[IDslopes].array.F[spotcoord[spot].XYout_dy] = spotcoord[spot].dy;
    }

    processinfo_update_output_stream(processinfo, IDslopes);

    /***** Write flux stream *****/

    data.image[IDflux].md[0].write = 1;

    for(int spot=0; spot<NBspot; spot++)
    {
        data.image[IDflux].array.F[spotcoord[spot].fluxout] = spotcoord[spot].flux;
    }

    processinfo_update_output_stream(processinfo, IDflux);

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    free(spotcoord);

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t CLIADDCMD_KalAO_SHWFS__process()
{
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
