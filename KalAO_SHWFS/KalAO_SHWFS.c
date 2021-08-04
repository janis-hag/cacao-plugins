/* ================================================================== */
/* ================================================================== */
/*            MODULE INFO                                             */
/* ================================================================== */
/* ================================================================== */

// module default short name
#define MODULE_SHORTNAME_DEFAULT "KalAO_SHWFS"

// Module short description
#define MODULE_DESCRIPTION       "Compute slopes for Shack-Hartmann WFS"

// Application to which module belongs
#define MODULE_APPLICATION       "cacao"

/* ================================================================== */
/* ================================================================== */
/*            DEPENDANCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"
#include "COREMOD_memory/image_ID.h"
#include "COREMOD_memory/stream_sem.h"
#include "COREMOD_memory/create_image.h"

#include <math.h>

//
// Forward declarations are required to connect CLI calls to functions
// If functions are in separate .c files, include here the corresponding .h files
//

errno_t KalAO_SHWFS__process_FPCONF();
errno_t KalAO_SHWFS__process_RUN();
errno_t KalAO_SHWFS__process();

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

/* ================================================================== */
/* ================================================================== */
/*            INITIALIZE LIBRARY                                      */
/* ================================================================== */
/* ================================================================== */

// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings
//

INIT_MODULE_LIB(KalAO_SHWFS)

/* ================================================================== */
/* ================================================================== */
/*            COMMAND LINE INTERFACE (CLI) FUNCTIONS                  */
/* ================================================================== */
/* ================================================================== */

static errno_t KalAO_SHWFS__process__cli()
{
    function_parameter_getFPSargs_from_CLIfunc("process");

    if(data.FPS_CMDCODE != 0) { // use FPS implementation
        // set pointers to CONF and RUN functions
        data.FPS_CONFfunc = KalAO_SHWFS__process_FPCONF;
        data.FPS_RUNfunc  = KalAO_SHWFS__process_RUN;
        function_parameter_execFPScmd();
        return RETURN_SUCCESS;
    }

    // call non FPS implementation - all parameters specified at function launch
    if(0
        == 0) {
        KalAO_SHWFS__process();
        return RETURN_SUCCESS;
    } else {
        return CLICMD_INVALID_ARG;
    }
}

/* ================================================================== */
/* ================================================================== */
/*  MODULE CLI INITIALIZATION                                         */
/* ================================================================== */
/* ================================================================== */

static errno_t init_module_CLI()
{
    RegisterCLIcommand(
        "process",
        __FILE__,
        KalAO_SHWFS__process__cli,
        "Computes slopes/centroids from SHWFS frames",
        "",
        "process imSHWFS SHconf",
        "KalAO_SHWFS__process(char *imstream, char *conffile)");

    return RETURN_SUCCESS;
}

/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                         */
/* ================================================================== */
/* ================================================================== */

errno_t KalAO_SHWFS__process_FPCONF()
{
	// ===========================
    // SETUP FPS
    // ===========================
    FPS_SETUP_INIT(data.FPS_name, data.FPS_CMDCODE); // macro in function_parameter.h
    fps_add_processinfo_entries(&fps); // include real-time settings

    // ==============================================
    // ========= ALLOCATE FPS ENTRIES ===============
    // ==============================================
	uint64_t FPFLAG = FPFLAG_DEFAULT_INPUT | FPFLAG_MINLIMIT | FPFLAG_MAXLIMIT;  // required to enforce the min and max

    FPS_ADDPARAM_STREAM_IN   (fpi_stream_inname,  ".rawWFSin", "input raw WFS stream", NULL);
    FPS_ADDPARAM_STREAM_OUT  (fpi_stream_outname, ".outWFS",   "output stream");
    FPS_ADDPARAM_FILENAME_IN (fpi_spotsfname, ".spotcoords", "SH spot coordinates", NULL);

    long algorithmDefault[4] = { 1, 0, 1, 1 };
	__attribute__((unused)) long fpi_algorithm = function_parameter_add_entry(&fps, ".algorithm", "Algorithm (0 = Quadcell, 1 = Center of mass)",
                            FPTYPE_INT64, FPFLAG, &algorithmDefault);

    float flux_averagecoeffDefault[4] = { 0.1, 0, 1, 0.1 };
	__attribute__((unused)) long fpi_tilt = function_parameter_add_entry(&fps, ".flux_averagecoeff", "Flux averaging coefficient",
                            FPTYPE_FLOAT32, FPFLAG, &flux_averagecoeffDefault);

    FPS_ADDPARAM_FLT32_OUT   (fpi_flux, ".flux_subaperture", "Max flux in a subaperture");
    FPS_ADDPARAM_FLT32_OUT   (fpi_residual, ".residual", "RMS Residual");
    FPS_ADDPARAM_FLT32_OUT   (fpi_slope_x, ".slope_x", "Average Tip");
    FPS_ADDPARAM_FLT32_OUT   (fpi_slope_y, ".slope_y", "Average Tilt");

	// ==============================================
    // ======== START FPS CONF LOOP =================
    // ==============================================
    FPS_CONFLOOP_START  // macro in function_parameter.h
	// here goes the logic



    // ==============================================
    // ======== STOP FPS CONF LOOP ==================
    // ==============================================
    FPS_CONFLOOP_END  // macro in function_parameter.h

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

errno_t KalAO_SHWFS__process_RUN()
{
	// ===========================
    // ### Connect to FPS
    // ===========================
    printf("connecting to fps")
    FPS_CONNECT(data.FPS_name, FPSCONNECT_RUN);
	printf("connected to fps")

    // ===================================
    // ### GET FUNCTION PARAMETER VALUES
    // ===================================
    char rawWFS_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(rawWFS_streamname, functionparameter_GetParamPtr_STRING(&fps, ".rawWFSin"), FUNCTION_PARAMETER_STRMAXLEN);

    char slopes_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(slopes_streamname, functionparameter_GetParamPtr_STRING(&fps, ".outWFS"), FUNCTION_PARAMETER_STRMAXLEN);

    char spotcoords_fname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(spotcoords_fname, functionparameter_GetParamPtr_STRING(&fps, ".spotcoords"), FUNCTION_PARAMETER_STRMAXLEN);

    float flux_averagecoeff = functionparameter_GetParamValue_FLOAT32(&fps, ".flux_averagecoeff");
    long algorithm = functionparameter_GetParamValue_INT64(&fps, ".algorithm");

	printf("starting processinfo support")
    // ===========================
    // ### processinfo support
    // ===========================
    PROCESSINFO *processinfo;

    processinfo = processinfo_setup(
		data.FPS_name,                                 // short name for the processinfo instance, no spaces, no dot, name should be human-readable
		"Computes slopes/centroids from SHWFS frames", // description
		"Sartup",                                      // message on startup
		__FUNCTION__, __FILE__, __LINE__
		);

	// OPTIONAL SETTINGS
	processinfo->MeasureTiming = 1; // Measure timing
	processinfo->RT_priority = 20;  // RT_priority, 0-99. Larger number = higher priority. If <0, ignore
	processinfo->loopcntMax = -1;   // -1 if infinite loop
	processinfo->CTRLval = 0;

	// apply relevant FPS entries to PROCINFO
    // see macro code for details
    fps_to_processinfo(&fps, processinfo);

    int loopOK = 1;

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
	imageID IDin = image_ID(rawWFS_streamname);
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

    // Specify input stream trigger
    IDin = image_ID(rawWFS_streamname);
    processinfo_waitoninputstream_init(processinfo, IDin, PROCESSINFO_TRIGGERMODE_SEMAPHORE, 0);

    printf("IDin = %ld\n", (long) IDin);



    // Notify processinfo that we are entering loop
    processinfo_loopstart(processinfo);

    float flux_subaperture = 0;
    while(loopOK==1)
    {
        loopOK = processinfo_loopstep(processinfo);

        processinfo_waitoninputstream(processinfo);

        processinfo_exec_start(processinfo);
        if(processinfo_compute_status(processinfo)==1)
        {
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
        }

        // process signals, increment loop counter
        processinfo_exec_end(processinfo);
    }

    // ==================================
    // ### ENDING LOOP
    // ==================================

    processinfo_cleanExit(processinfo);
	function_parameter_RUNexit(&fps);

	free(spotcoord);

    return RETURN_SUCCESS;
}

errno_t KalAO_SHWFS__process()
{
    long pindex = (long) getpid();  // index used to differentiate multiple calls to function
    // if we don't have anything more informative, we use PID

    FUNCTION_PARAMETER_STRUCT fps;

    // create FPS
    sprintf(data.FPS_name, "shwfsproc-%06ld", pindex);
    data.FPS_CMDCODE = FPSCMDCODE_FPSINIT;
    KalAO_SHWFS__process_FPCONF();

    KalAO_SHWFS__process_RUN();
    return RETURN_SUCCESS;
}
