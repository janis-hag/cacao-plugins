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


static CLICMDARGDEF farg[] =
{
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
	FPS_CONNECT(data.FPS_name, FPSCONNECT_CONF);

	uint64_t FPFLAG = FPFLAG_DEFAULT_INPUT | FPFLAG_MINLIMIT | FPFLAG_MAXLIMIT;  // required to enforce the min and max

    //FPS_ADDPARAM_STREAM_IN   (fpi_stream_inname,  ".rawWFSin", "input raw WFS stream", NULL);
    FPS_ADDPARAM_STREAM_OUT  (fpi_stream_outname, ".outWFS",   "output stream");
    FPS_ADDPARAM_FILENAME_IN (fpi_spotsfname, ".spotcoords", "SH spot coordinates", NULL);

    long algorithmDefault[4] = { 1, 0, 1, 1 };
	long fpi_algorithm = 0;
	function_parameter_add_entry(&fps, ".algorithm", "Algorithm (0 = Quadcell, 1 = Center of mass)",
                            FPTYPE_INT64, FPFLAG, &algorithmDefault, &fpi_algorithm);

    float flux_averagecoeffDefault[4] = { 0.1, 0, 1, 0.1 };
	long fpi_tilt = 0;
	function_parameter_add_entry(&fps, ".flux_averagecoeff", "Flux averaging coefficient",
                            FPTYPE_FLOAT32, FPFLAG, &flux_averagecoeffDefault, &fpi_tilt);

    FPS_ADDPARAM_FLT32_OUT   (fpi_flux, ".flux_subaperture", "Max flux in a subaperture");
    FPS_ADDPARAM_FLT32_OUT   (fpi_residual, ".residual", "RMS Residual");
    FPS_ADDPARAM_FLT32_OUT   (fpi_slope_x, ".slope_x", "Average Tip");
    FPS_ADDPARAM_FLT32_OUT   (fpi_slope_y, ".slope_y", "Average Tilt");

    return RETURN_SUCCESS;
}

static int read_spots_coords(SHWFS_SPOTS *spotcoord, char *fname)
{
    int NBspot = 0;

    FILE *fp;

    fp = fopen(fname, "r");
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
	FPS_CONNECT(data.FPS_name, FPSCONNECT_RUN);
	INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    // ===================================
    // ### GET FUNCTION PARAMETER VALUES
    // ===================================
    //char rawWFS_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    //strncpy(rawWFS_streamname, functionparameter_GetParamPtr_STRING(&fps, ".rawWFSin"), FUNCTION_PARAMETER_STRMAXLEN);

    char slopes_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(slopes_streamname, functionparameter_GetParamPtr_STRING(&fps, ".outWFS"), FUNCTION_PARAMETER_STRMAXLEN);

    char spotcoords_fname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(spotcoords_fname, functionparameter_GetParamPtr_STRING(&fps, ".spotcoords"), FUNCTION_PARAMETER_STRMAXLEN);

    float flux_averagecoeff = functionparameter_GetParamValue_FLOAT32(&fps, ".flux_averagecoeff");
    long algorithm = functionparameter_GetParamValue_INT64(&fps, ".algorithm");

	/********** Load spots coordinates **********/

	// Allocate spots
	SHWFS_SPOTS *spotcoord = (SHWFS_SPOTS*) malloc(sizeof(SHWFS_SPOTS)*MAXNB_SPOT);

    char msgstring[200];
    sprintf(msgstring, "Loading spot <- %s", spotcoords_fname);
    processinfo_WriteMessage(processinfo, msgstring);

	int NBspot = read_spots_coords(spotcoord, spotcoords_fname);

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

	// slopes
	imsizearray[0] = sizeoutX * 2;
	imsizearray[1] = sizeoutY;
	create_image_ID(slopes_streamname, 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &IDslopes);

	// flux
	imsizearray[0] = sizeoutX;
	imsizearray[1] = sizeoutY;
	create_image_ID(flux_streamname, 2, imsizearray, _DATATYPE_FLOAT, 1, 10, 0, &IDflux);

	free(imsizearray);

	/********** Setup and loop **********/

    float flux_subaperture = 0;

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

            float flux_max = 0;
            float residual = 0;
            float slope_x = 0;
            float slope_y = 0;

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

				if(flux > flux_max) {
				    flux_max = flux;
				}

				residual += dx*dx + dy*dy;
				slope_x += dx;
				slope_y += dy;
			}

			flux_subaperture = flux_averagecoeff * flux_max + (1.0-flux_averagecoeff) * flux_subaperture;
			residual = sqrt(residual / NBspot);
			slope_x /= NBspot;
			slope_y /= NBspot;

			functionparameter_SetParamValue_FLOAT32(&fps, ".flux_subaperture", flux_subaperture);
			functionparameter_SetParamValue_FLOAT32(&fps, ".residual", residual);
			functionparameter_SetParamValue_FLOAT32(&fps, ".slope_x", slope_x);
			functionparameter_SetParamValue_FLOAT32(&fps, ".slope_y", slope_y);

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
