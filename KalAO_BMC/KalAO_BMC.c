/* ================================================================== */
/* ================================================================== */
/*            MODULE INFO                                             */
/* ================================================================== */
/* ================================================================== */

// module default short name
#define MODULE_SHORTNAME_DEFAULT "KalAO_BMC"

// Module short description
#define MODULE_DESCRIPTION       "Module to control the Deformable Mirror of KalAO"

// Application to which module belongs
#define MODULE_APPLICATION       "cacao"

/* ================================================================== */
/* ================================================================== */
/*            DEPENDANCIES                                            */
/* ================================================================== */
/* ================================================================== */


#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"
#include "CommandLineInterface/fps_GetParamIndex.h"
#include "COREMOD_memory/COREMOD_memory.h"
#include "ZernikePolyn/ZernikePolyn.h"

#include "BMCApi.h"

#include <math.h>

//
// Forward declarations are required to connect CLI calls to functions
// If functions are in separate .c files, include here the corresponding .h files
//

errno_t KalAO_BMC__display_FPCONF();
errno_t KalAO_BMC__display_RUN();
errno_t KalAO_BMC__display();

/* ================================================================== */
/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */

typedef struct
{
	uint32_t size_x;
	uint32_t size_y;
	double offset_x;
	double offset_y;
	double radius;

	float* pistonPtr;
	float* tipPtr;
	float* tiltPtr;
	float* defocusPtr;
	float* verticalastigmatismPtr;
	float* obliqueastigmatismPtr;

	long* pistonCnt;
	long* tipCnt;
	long* tiltCnt;
	long* defocusCnt;
	long* verticalastigmatismCnt;
	long* obliqueastigmatismCnt;

} ZERNIKE_PARAMS;

/* ================================================================== */
/* ================================================================== */
/*            INITIALIZE LIBRARY                                      */
/* ================================================================== */
/* ================================================================== */

// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings

INIT_MODULE_LIB(KalAO_BMC)

/* ================================================================== */
/* ================================================================== */
/*            COMMAND LINE INTERFACE (CLI) FUNCTIONS                  */
/* ================================================================== */
/* ================================================================== */

errno_t KalAO_BMC__display__cli()
{
	function_parameter_getFPSargs_from_CLIfunc("display");

    if(data.FPS_CMDCODE != 0) { // use FPS implementation
        // set pointers to CONF and RUN functions
        data.FPS_CONFfunc = KalAO_BMC__display_FPCONF;
        data.FPS_RUNfunc  = KalAO_BMC__display_RUN;
        function_parameter_execFPScmd();
        return RETURN_SUCCESS;
    }

    // call non FPS implementation - all parameters specified at function launch
    if(0
        == 0) {
        KalAO_BMC__display();
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
        "display",
        __FILE__,
        KalAO_BMC__display__cli,
        "Connect to deformable mirror and display",
        "",
        "display",
        "KalAO_BMC__display()");

    return RETURN_SUCCESS;
}

/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                         */
/* ================================================================== */
/* ================================================================== */

errno_t KalAO_BMC__display_FPCONF()
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

    FPS_ADDPARAM_STREAM_IN (fpi_DMin_streamname,  ".DMin", "Actuators stream", NULL);
    FPS_ADDPARAM_STREAM_IN (fpi_TTMin_streamname,  ".TTMin", "Tip-Tilt stream", NULL);

    float tipOffsetDefault[4] = { 0, -2.500, 2.500, 0 };
	__attribute__((unused)) long fpi_ttm_tip_offset = function_parameter_add_entry(&fps, ".ttm_tip_offset", "Tip offset on TTM",
                            FPTYPE_FLOAT32, FPFLAG | FPFLAG_WRITERUN, &tipOffsetDefault);

    float tiltOffsetDefault[4] = { 0, -2.500, 2.500, 0 };
	__attribute__((unused)) long fpi_ttm_tilt_offset = function_parameter_add_entry(&fps, ".ttm_tilt_offset", "Tilt offset on TTM",
                            FPTYPE_FLOAT32, FPFLAG | FPFLAG_WRITERUN, &tiltOffsetDefault);

    float zernikeDefault[4] = { 0, 0, 0, 0 };
	__attribute__((unused)) long fpi_zernike_piston = function_parameter_add_entry(&fps, ".zernike_piston", "Piston",
                            FPTYPE_FLOAT32, FPFLAG_DEFAULT_INPUT | FPFLAG_WRITERUN, &zernikeDefault);

	__attribute__((unused)) long fpi_zernike_tip = function_parameter_add_entry(&fps, ".zernike_tip", "Tip (on DM)",
                            FPTYPE_FLOAT32, FPFLAG_DEFAULT_INPUT | FPFLAG_WRITERUN, &zernikeDefault);

	__attribute__((unused)) long fpi_zernike_tilt = function_parameter_add_entry(&fps, ".zernike_tilt", "Tilt (on DM)",
                            FPTYPE_FLOAT32, FPFLAG_DEFAULT_INPUT | FPFLAG_WRITERUN, &zernikeDefault);

	__attribute__((unused)) long fpi_zernike_defocus = function_parameter_add_entry(&fps, ".zernike_defocus", "Defocus",
                            FPTYPE_FLOAT32, FPFLAG_DEFAULT_INPUT | FPFLAG_WRITERUN, &zernikeDefault);

	__attribute__((unused)) long fpi_zernike_verticalastigmatism = function_parameter_add_entry(&fps, ".zernike_vert_astig", "Vertical Astigmatism",
                            FPTYPE_FLOAT32, FPFLAG_DEFAULT_INPUT | FPFLAG_WRITERUN, &zernikeDefault);

	__attribute__((unused)) long fpi_zernike_obliqueastigmatism = function_parameter_add_entry(&fps, ".zernike_obli_astig", "Oblique Astigmatism",
                            FPTYPE_FLOAT32, FPFLAG_DEFAULT_INPUT | FPFLAG_WRITERUN, &zernikeDefault);

    // ==============================================
    // ======== START FPS CONF LOOP =================
    // ==============================================
    FPS_CONFLOOP_START  // macro in function_parameter.h



    // ==============================================
    // ======== STOP FPS CONF LOOP ==================
    // ==============================================
    FPS_CONFLOOP_END  // macro in function_parameter.h

    return RETURN_SUCCESS;
}

double compute_zernike(long i, long n, ZERNIKE_PARAMS params) {
	double x = (i % params.size_x) - params.offset_x;
	double y = (int)(i / params.size_x) - params.offset_y;

	double r = sqrt(x * x + y * y) / params.radius;
    double theta = atan2(y, x);

    return Zernike_value(n, r, theta);
}

double compute_zernike_total(long i, ZERNIKE_PARAMS params) {
	// 1 = tip
	// 2 = tilt
	// 3 = oblique astigmatism
	// 4 = defocus
	// 5 = vertical astigmatism

	double value = 0.0;
	value += (*params.tipPtr)*compute_zernike(i, 1, params);
	value += (*params.tiltPtr)*compute_zernike(i, 2, params);
	value += (*params.obliqueastigmatismPtr)*compute_zernike(i, 3, params);
	value += (*params.defocusPtr)*compute_zernike(i, 4, params);
	value += (*params.verticalastigmatismPtr)*compute_zernike(i, 5, params);

    return value;
}

errno_t KalAO_BMC__display_RUN()
{
	// ===========================
    // ### Connect to FPS
    // ===========================
    FPS_CONNECT( data.FPS_name, FPSCONNECT_RUN );

    // ===================================
    // ### GET FUNCTION PARAMETER VALUES
    // ===================================
    char DMin_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(DMin_streamname, functionparameter_GetParamPtr_STRING(&fps, ".DMin"), FUNCTION_PARAMETER_STRMAXLEN);

    char TTMin_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(TTMin_streamname, functionparameter_GetParamPtr_STRING(&fps, ".TTMin"), FUNCTION_PARAMETER_STRMAXLEN);

	char DMzernike_streamname[FUNCTION_PARAMETER_STRMAXLEN + 3];
	sprintf(DMzernike_streamname, "%s11", DMin_streamname);

    char TTMoffset_streamname[FUNCTION_PARAMETER_STRMAXLEN + 3];
	sprintf(TTMoffset_streamname, "%s00", TTMin_streamname);

	float* ttmTipOffsetPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".ttm_tip_offset");
	float* ttmTiltOffsetPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".ttm_tilt_offset");

	long* ttmTipOffsetCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".ttm_tip_offset")].cnt0;
	long* ttmTiltOffsetCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".ttm_tilt_offset")].cnt0;

	ZERNIKE_PARAMS zernike_params;

	zernike_params.pistonPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".zernike_piston");
	zernike_params.tipPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".zernike_tip");
	zernike_params.tiltPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".zernike_tilt");
	zernike_params.defocusPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".zernike_defocus");
	zernike_params.verticalastigmatismPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".zernike_vert_astig");
	zernike_params.obliqueastigmatismPtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".zernike_obli_astig");

	zernike_params.pistonCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".zernike_piston")].cnt0;
	zernike_params.tipCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".zernike_tip")].cnt0;
	zernike_params.tiltCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".zernike_tilt")].cnt0;
	zernike_params.defocusCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".zernike_defocus")].cnt0;
	zernike_params.verticalastigmatismCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".zernike_vert_astig")].cnt0;
	zernike_params.obliqueastigmatismCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".zernike_obli_astig")].cnt0;

    // ===========================
    // ### processinfo support
    // ===========================
	PROCESSINFO *processinfo;

	processinfo = processinfo_setup(
		data.FPS_name,          // short name for the processinfo instance, no spaces, no dot, name should be human-readable
		"Read frames from BMC", // description
		"Starting setup",       // message on startup
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

	/********** Variables **********/

	int	error = NO_ERR;
    DM dm = {};
	uint32_t *map_lut;
	double *dm_array;
	int k;

	/********** Open BMC **********/

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

	processinfo_WriteMessage(processinfo, "Connecting to stream");
	imageID IDDMin = image_ID(DMin_streamname);
	imageID IDTTMin = image_ID(TTMin_streamname);
	imageID IDDMzernike = read_sharedmem_image(DMzernike_streamname);
	imageID IDTTMoffset = read_sharedmem_image(TTMoffset_streamname);

	/********** Zernike parameters **********/

	zernike_params.size_x = data.image[IDDMin].md[0].size[0];
	zernike_params.size_y = data.image[IDDMin].md[0].size[1];
	zernike_params.offset_x = (zernike_params.size_x - 1.0)/ 2.0;
	zernike_params.offset_y = (zernike_params.size_y - 1.0)/ 2.0;
	zernike_params.radius = ((zernike_params.size_x + zernike_params.size_y)/2.0 - 1.0) / 2.0;

	zernike_init();

	/********** Loop **********/

	processinfo_WriteMessage(processinfo, "Looping");
	processinfo_loopstart(processinfo); // Notify processinfo that we are entering loop

	int ii;
	long ttmOffsetCnt_old = 0;
	long zernikeCnt_old = 0;
	double zernike = 0;
	while(loopOK == 1) {
		loopOK = processinfo_loopstep(processinfo);

		processinfo_exec_start(processinfo);

		if(processinfo_compute_status(processinfo) == 1)
		{
			for(ii=0; ii<10; ii++)
				dm_array[ii] = (data.image[IDDMin].array.F[ii+1] + *(zernike_params.pistonPtr))/3.5+0.5;

			for(; ii<130; ii++)
				dm_array[ii] = (data.image[IDDMin].array.F[ii+2] + *(zernike_params.pistonPtr))/3.5+0.5;

			for(; ii<140; ii++)
				dm_array[ii] = (data.image[IDDMin].array.F[ii+3] + *(zernike_params.pistonPtr))/3.5+0.5;

			dm_array[155] = data.image[IDTTMin].array.F[0]/5.0+0.5;
			dm_array[156] = data.image[IDTTMin].array.F[1]/5.0+0.5;

			error = BMCSetArray(&dm, dm_array, map_lut);
			if (error) {
				printf("\nThe error %d happened while setting array for deformable mirror\n", error);
				return error;
			}

			/***** Output offset in TTM stream *****/

			long ttmOffsetCnt_sum = *ttmTipOffsetCnt + *ttmTiltOffsetCnt;

			if(ttmOffsetCnt_old != ttmOffsetCnt_sum) {
				ttmOffsetCnt_old = ttmOffsetCnt_sum;

				data.image[IDTTMoffset].md[0].write = 1;

				data.image[IDTTMoffset].array.F[0] = *ttmTipOffsetPtr;
				data.image[IDTTMoffset].array.F[1] = *ttmTiltOffsetPtr;

				processinfo_update_output_stream(processinfo, IDTTMoffset);
			}

			/***** Output zernike in DM stream *****/

			long zernikeCnt_sum = *zernike_params.tipCnt + *zernike_params.tiltCnt + *zernike_params.defocusCnt
			                    + *zernike_params.verticalastigmatismCnt + *zernike_params.obliqueastigmatismCnt;

			if(zernikeCnt_old != zernikeCnt_sum) {
				zernikeCnt_old = zernikeCnt_sum;

				data.image[IDDMzernike].md[0].write = 1;

				for(ii=0; ii<10; ii++)
					data.image[IDDMzernike].array.F[ii+1] = compute_zernike_total(ii+1, zernike_params);

				for(; ii<130; ii++)
					data.image[IDDMzernike].array.F[ii+2] = compute_zernike_total(ii+2, zernike_params);

				for(; ii<140; ii++)
					data.image[IDDMzernike].array.F[ii+3] = compute_zernike_total(ii+3, zernike_params);

				processinfo_update_output_stream(processinfo, IDDMzernike);
			}
		}

		// process signals, increment loop counter
		processinfo_exec_end(processinfo);
	}

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

	// ==================================
	// ENDING LOOP
	// ==================================
	processinfo_cleanExit(processinfo);
	function_parameter_RUNexit(&fps);

	return RETURN_SUCCESS;
}

errno_t KalAO_BMC__display()
{
    long pindex = (long) getpid();  // index used to differentiate multiple calls to function
    // if we don't have anything more informative, we use PID

    FUNCTION_PARAMETER_STRUCT fps;

    // create FPS
    sprintf(data.FPS_name, "bmc_display-%06ld", pindex);
    data.FPS_CMDCODE = FPSCMDCODE_FPSINIT;
    KalAO_BMC__display_FPCONF();

    KalAO_BMC__display_RUN();
    return RETURN_SUCCESS;
}

