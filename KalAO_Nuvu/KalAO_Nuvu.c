/* ================================================================== */
/* ================================================================== */
/*            MODULE INFO                                             */
/* ================================================================== */
/* ================================================================== */

// module default short name
#define MODULE_SHORTNAME_DEFAULT "KalAO_Nuvu"

// Module short description
#define MODULE_DESCRIPTION       "Module to control the Wavefront Sensor Camera of KalAO"

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
#include "COREMOD_memory/image_ID.h"
#include "COREMOD_memory/stream_sem.h"
#include "COREMOD_memory/create_image.h"
#include "COREMOD_iofits/loadfits.h"
#include "COREMOD_iofits/file_exists.h"
#include "COREMOD_iofits/is_fits_file.h"

#include "nc_driver.h"

//
// Forward declarations are required to connect CLI calls to functions
// If functions are in separate .c files, include here the corresponding .h files
//

errno_t KalAO_Nuvu__acquire_FPCONF();
errno_t KalAO_Nuvu__acquire_RUN();
errno_t KalAO_Nuvu__acquire();

/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */

typedef struct
{
	int64_t emgain;
	float exposuretime;

} NUVU_AUTOGAIN_PARAMS;

#define MAXNB_AUTOGAIN_PARAMS 100

#define FPFLAG_KALAO_AUTOGAIN    0x1000000000000000
#define FPFLAG_KALAO_UPDATED     0x2000000000000000

/* ================================================================== */
/* ================================================================== */
/*            INITIALIZE LIBRARY                                      */
/* ================================================================== */
/* ================================================================== */

// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings
//

INIT_MODULE_LIB(KalAO_Nuvu)

/* ================================================================== */
/* ================================================================== */
/*            COMMAND LINE INTERFACE (CLI) FUNCTIONS                  */
/* ================================================================== */
/* ================================================================== */

errno_t KalAO_Nuvu__acquire__cli()
{
	function_parameter_getFPSargs_from_CLIfunc("acquire");

    if(data.FPS_CMDCODE != 0) { // use FPS implementation
        // set pointers to CONF and RUN functions
        data.FPS_CONFfunc = KalAO_Nuvu__acquire_FPCONF;
        data.FPS_RUNfunc  = KalAO_Nuvu__acquire_RUN;
        function_parameter_execFPScmd();
        return RETURN_SUCCESS;
    }

    // call non FPS implementation - all parameters specified at function launch
    if(0
        == 0) {
        KalAO_Nuvu__acquire();
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
        "acquire",                                 // function call name from CLI
        __FILE__,                                  // this file, used to track where function comes from
        KalAO_Nuvu__acquire__cli,                  // function to call
        "Connect to camera and start acqusition",  // short description
        "",                                        // arguments
        "acquire",                                 // example use
        "KalAO_Nuvu__acquire()");                  // source code call

    return RETURN_SUCCESS;
}

/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                         */
/* ================================================================== */
/* ================================================================== */

errno_t KalAO_Nuvu__acquire_FPCONF()
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
	void *pNull = NULL;

    long temperatureDefault[4] = { -60, -90, 20, -60 };
    __attribute__((unused)) long fpi_temperature = function_parameter_add_entry(&fps, ".temperature", "Temperature",
                            FPTYPE_INT64, FPFLAG, &temperatureDefault);

    long readoutmodeDefault[4] = { 1, 1, 12, 1 };
    __attribute__((unused)) long fpi_readoutmode = function_parameter_add_entry(&fps, ".readoutmode", "Readout mode",
                            FPTYPE_INT64, FPFLAG, &readoutmodeDefault);

    long binningDefault[4] = { 2, 1, 16, 2 };
    __attribute__((unused)) long fpi_binning = function_parameter_add_entry(&fps, ".binning", "Binning",
                            FPTYPE_INT64, FPFLAG, &binningDefault);

	long emgainDefault[4] = { 1, 1, 1000, 1 };
	__attribute__((unused)) long fpi_emgain = function_parameter_add_entry(&fps, ".emgain", "EM Gain",
                            FPTYPE_INT64, FPFLAG | FPFLAG_WRITERUN, &emgainDefault);

	float exposuretimeDefault[4] = { 0, 0, 1e3, 0 };
	__attribute__((unused)) long fpi_exposuretime = function_parameter_add_entry(&fps, ".exposuretime", "Exposure Time",
                            FPTYPE_FLOAT32, FPFLAG | FPFLAG_WRITERUN, &exposuretimeDefault);

    __attribute__((unused)) long fpi_bias = function_parameter_add_entry(&fps, ".bias", "Bias files",
                           FPTYPE_STRING, FPFLAG, "bias/bias_%02ldC_%02ldrom_%01ldb_%04ldg.fits");

    __attribute__((unused)) long fpi_flat = function_parameter_add_entry(&fps, ".flat", "Flat files",
                           FPTYPE_STRING, FPFLAG, "flat/flat_%02ldC_%02ldrom_%01ldb_%04ldg.fits");

    __attribute__((unused)) long fpi_autogain = function_parameter_add_entry(&fps, ".autogain_on", "Auto-gain ON/OFF",
                            FPTYPE_ONOFF, FPFLAG | FPFLAG_WRITERUN, pNull);

    FPS_ADDPARAM_FILENAME_IN (fpi_autogain_paramsfname, ".autogain_params", "Exposure Parameters for Auto-gain", NULL);

    long autogainLowerLimit[4] = { 60000, 0, 65535, 60000 };
    __attribute__((unused)) long fpi_autogainLowerLimit = function_parameter_add_entry(&fps, ".autogain_low", "Auto-gain Lower Limit (ADU)",
                            FPTYPE_INT64, FPFLAG, &autogainLowerLimit);

    long autogainUpperLimit[4] = { 75000, 0, 4*65535, 75000 };
    __attribute__((unused)) long fpi_autogainUpperLimit = function_parameter_add_entry(&fps, ".autogain_high", "Auto-gain Upper Limit (ADU)",
                            FPTYPE_INT64, FPFLAG, &autogainUpperLimit);

    long autogainFramewait[4] = { 500, 0, 5000, 500 };
    __attribute__((unused)) long fpi_autogainFramewait = function_parameter_add_entry(&fps, ".autogain_framewait", "Number of frames to wait after a change to exposure",
                            FPTYPE_INT64, FPFLAG, &autogainFramewait);

	FPS_ADDPARAM_FLT64_OUT(fpi_temp_ccd, ".temp_ccd", "CCD Temperature");
	FPS_ADDPARAM_FLT64_OUT(fpi_temp_controller, ".temp_controller", "Controller Temperature");
	FPS_ADDPARAM_FLT64_OUT(fpi_temp_power_supply, ".temp_power_supply", "Power Supply Temperature");
	FPS_ADDPARAM_FLT64_OUT(fpi_temp_fpga, ".temp_fpga", "FPGA Temperature");
	FPS_ADDPARAM_FLT64_OUT(fpi_temp_heatsink, ".temp_heatsink", "Heatsink Temperature");

    long emgain_cnt0 = fps.parray[fpi_emgain].cnt0;
    long exposuretime_cnt0 = fps.parray[fpi_exposuretime].cnt0;

    // ==============================================
    // ======== START FPS CONF LOOP =================
    // ==============================================
    FPS_CONFLOOP_START  // macro in function_parameter.h

	if(fps.parray[fpi_emgain].cnt0 != emgain_cnt0)
	{
		emgain_cnt0 = fps.parray[fpi_emgain].cnt0;

		fps.parray[fpi_emgain].fpflag |= FPFLAG_KALAO_UPDATED;

		if(fps.parray[fpi_emgain].fpflag & FPFLAG_KALAO_AUTOGAIN) {
			fps.parray[fpi_emgain].fpflag &= ~FPFLAG_KALAO_AUTOGAIN;
		} else {
			fps.parray[fpi_autogain].fpflag &= ~FPFLAG_ONOFF;
			fps.parray[fpi_autogain].cnt0++;
		}
	}

	if(fps.parray[fpi_exposuretime].cnt0 != exposuretime_cnt0)
	{
		exposuretime_cnt0 = fps.parray[fpi_exposuretime].cnt0;

		fps.parray[fpi_exposuretime].fpflag |= FPFLAG_KALAO_UPDATED;

		if(fps.parray[fpi_exposuretime].fpflag & FPFLAG_KALAO_AUTOGAIN) {
			fps.parray[fpi_exposuretime].fpflag &= ~FPFLAG_KALAO_AUTOGAIN;
		} else {
			fps.parray[fpi_autogain].fpflag &= ~FPFLAG_ONOFF;
			fps.parray[fpi_autogain].cnt0++;
		}
	}

    // ==============================================
    // ======== STOP FPS CONF LOOP ==================
    // ==============================================
    FPS_CONFLOOP_END  // macro in function_parameter.h

    return RETURN_SUCCESS;
}

static int read_exposure_params(NUVU_AUTOGAIN_PARAMS *autogain_params, char *fname)
{
    int NBautogain_params = 0;

    FILE *fp;

    fp = fopen(fname, "r");
    if(fp == NULL)
    {
        perror("Unable to open file!");
        exit(1);
    }

	int64_t emgain;
    float exposuretime;

    char keyw[16];

    int loopOK = 1;
    while(loopOK == 1)
    {
        int ret = fscanf(fp, "%s %ld %f", keyw, &emgain, &exposuretime);
        if(ret == EOF)
        {
            loopOK = 0;
        }
        else
        {
            if((ret==3) && (strcmp(keyw, "EXP") == 0))
            {
                printf("Found EXP %5ld %5f\n", emgain, exposuretime);
                autogain_params[NBautogain_params].emgain = emgain;
                autogain_params[NBautogain_params].exposuretime = exposuretime;
                NBautogain_params++;
            }
        }
    }
	printf("Loaded %d exposure parameters\n", NBautogain_params);

    fclose(fp);

    return NBautogain_params;
}

void update_exposure_parameters(FUNCTION_PARAMETER_STRUCT *fps, NUVU_AUTOGAIN_PARAMS *autogain_params, int current_autogain_param, uint64_t *emgainFlag, uint64_t *exposuretimeFlag)
{
	*emgainFlag |= FPFLAG_KALAO_AUTOGAIN;
	*exposuretimeFlag |= FPFLAG_KALAO_AUTOGAIN;
	functionparameter_SetParamValue_INT64(fps, ".emgain", autogain_params[current_autogain_param].emgain);
	functionparameter_SetParamValue_FLOAT32(fps, ".exposuretime", autogain_params[current_autogain_param].exposuretime);
	fps->md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE;
}

void load_bias_and_flat(PROCESSINFO *processinfo, FUNCTION_PARAMETER_STRUCT *fps, imageID biasID, imageID flatID, long temperature, long readoutmode, long binning, long emgain)
{
	char biasfile[255];
	char flatfile[255];

	sprintf(biasfile, functionparameter_GetParamPtr_STRING(fps, ".bias"), temperature, readoutmode, binning, emgain);
	sprintf(flatfile, functionparameter_GetParamPtr_STRING(fps, ".flat"), temperature, readoutmode, binning, emgain);

	imageID biastmpID = -1;
	imageID flattmpID = -1;

	/********** Load bias **********/

	if(!file_exists(biasfile)) {
		printf("File %s not found\n", biasfile);
	} else if(!is_fits_file(biasfile)) {
		printf("File %s is not a valid FITS file\n", biasfile);
	} else
	{
		biastmpID = load_fits(biasfile, "nuvu_bias_tmp", 1);

		if(data.image[biasID].md[0].datatype != data.image[biastmpID].md[0].datatype)
		{
			printf("Wrong data type for file %s\n", biasfile);
			biastmpID = -1;
		}
		else if (data.image[biasID].md[0].nelement != data.image[biastmpID].md[0].nelement)
		{
			printf("Wriong size for file %s\n", biasfile);
			biastmpID = -1;
		}
	}

	data.image[biasID].md[0].write = 1;

	// Note: DO NOT use memset() as it will be optimized away
	if(biastmpID != -1)
	{
		for(uint64_t i = 0; i < data.image[biasID].md[0].nelement; i++)
			data.image[biasID].array.F[i] = data.image[biastmpID].array.F[i];
	}
	else
	{
		for(uint64_t i = 0; i < data.image[biasID].md[0].nelement; i++)
			data.image[biasID].array.F[i] = 0;
	}

	processinfo_update_output_stream(processinfo, biasID);

	/********** Load flat **********/

	if(!file_exists(flatfile)) {
		printf("File %s not found\n", flatfile);
	} else if(!is_fits_file(flatfile)) {
		printf("File %s is not a valid FITS file\n", flatfile);
	}
	else
	{
		flattmpID = load_fits(flatfile, "nuvu_flat_tmp", 1);

		if(data.image[flatID].md[0].datatype != data.image[flattmpID].md[0].datatype)
		{
			printf("Wrong data type for file %s\n", flatfile);
			flattmpID = -1;
		}
		else if (data.image[flatID].md[0].nelement != data.image[flattmpID].md[0].nelement)
		{
			printf("Wriong size for file %s\n", flatfile);
			flattmpID = -1;
		}
	}

	data.image[flatID].md[0].write = 1;

	// Note: DO NOT use memset() as it will be optimized away
	if(flattmpID != -1)
	{
		for(uint64_t i = 0; i < data.image[flatID].md[0].nelement; i++)
			data.image[flatID].array.F[i] = data.image[flattmpID].array.F[i];
	}
	else
	{
		for(uint64_t i = 0; i < data.image[flatID].md[0].nelement; i++)
			data.image[flatID].array.F[i] = 1;
	}

	processinfo_update_output_stream(processinfo, flatID);
}

errno_t KalAO_Nuvu__acquire_RUN()
{
	// ===========================
    // ### Connect to FPS
    // ===========================
    FPS_CONNECT(data.FPS_name, FPSCONNECT_RUN);

    // ===================================
    // ### GET FUNCTION PARAMETER VALUES
    // ===================================
    long temperature = functionparameter_GetParamValue_INT64(&fps, ".temperature");
    long readoutmode = functionparameter_GetParamValue_INT64(&fps, ".readoutmode");
    long binning = functionparameter_GetParamValue_INT64(&fps, ".binning");
    long autogain_low = functionparameter_GetParamValue_INT64(&fps, ".autogain_low");
    long autogain_high = functionparameter_GetParamValue_INT64(&fps, ".autogain_high");
    long autogain_framewait = functionparameter_GetParamValue_INT64(&fps, ".autogain_framewait");

    long* emgainPtr = functionparameter_GetParamPtr_INT64(&fps, ".emgain");
    uint64_t* emgainFlag = functionparameter_GetParamPtr_fpflag(&fps, ".emgain");

    float* exposuretimePtr = functionparameter_GetParamPtr_FLOAT32(&fps, ".exposuretime");
    uint64_t* exposuretimeFlag = functionparameter_GetParamPtr_fpflag(&fps, ".exposuretime");

    uint64_t* autogainFlag = functionparameter_GetParamPtr_fpflag(&fps, ".autogain_on");
    long* autogainCnt = &fps.parray[functionparameter_GetParamIndex(&fps, ".autogain_on")].cnt0;

    char autogain_params_fname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(autogain_params_fname, functionparameter_GetParamPtr_STRING(&fps, ".autogain_params"), FUNCTION_PARAMETER_STRMAXLEN);

    double* temp_ccd_ptr = functionparameter_GetParamPtr_FLOAT64(&fps, ".temp_ccd");
	double* temp_controller_ptr = functionparameter_GetParamPtr_FLOAT64(&fps, ".temp_controller");
	double* temp_power_supply_ptr = functionparameter_GetParamPtr_FLOAT64(&fps, ".temp_power_supply");
	double* temp_fpga_ptr = functionparameter_GetParamPtr_FLOAT64(&fps, ".temp_fpga");
	double* temp_heatsink_ptr = functionparameter_GetParamPtr_FLOAT64(&fps, ".temp_heatsink");

    FUNCTION_PARAMETER_STRUCT fps_shwfs;
    function_parameter_struct_connect("shwfs_process", &fps_shwfs, FPSCONNECT_SIMPLE);
    float* fluxPtr = functionparameter_GetParamPtr_FLOAT32(&fps_shwfs, ".flux_subaperture");
    long* fluxCnt = &fps_shwfs.parray[functionparameter_GetParamIndex(&fps_shwfs, ".flux_subaperture")].cnt0;

    // ===========================
    // ### processinfo support
    // ===========================
	PROCESSINFO *processinfo;

	processinfo = processinfo_setup(
		data.FPS_name,           // short name for the processinfo instance, no spaces, no dot, name should be human-readable
		"Read frames from Nuvu", // description
		"Starting setup",        // message on startup
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

	int	error = NC_SUCCESS;
    NcCam cam = NULL;
    uint32_t* image = NULL;
    int width = 0;
    int height = 0;

	/********** Open and configure camera **********/

	processinfo_WriteMessage(processinfo, "Opening camera");
	error = ncCamOpen(NC_AUTO_UNIT, NC_AUTO_CHANNEL, 4, &cam);
	if (error) {
		printf("\nThe error %d happened while opening camera\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting readout mode");
	error = ncCamSetReadoutMode(cam, readoutmode);
	if (error) {
		printf("\nThe error %d happened while setting readout mode\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting detector temperature");
	error = ncCamSetTargetDetectorTemp(cam, temperature);
	if (error) {
		printf("\nThe error %d happened while setting detector temperature\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting binning");
	error = ncCamSetBinningMode(cam, binning, binning);
	if (error) {
		printf("\nThe error %d happened while setting binning\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting exposure time");
	error = ncCamSetExposureTime(cam, *exposuretimePtr);
	if (error) {
		printf("\nThe error %d happened while setting exposure time\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting waiting time");
	error = ncCamSetWaitingTime(cam, 0);
	if (error) {
		printf("\nThe error %d happened while setting waiting time\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting timeout");
	error = ncCamSetTimeout(cam, 1000);
	if (error) {
		printf("\nThe error %d happened while setting timeout\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Setting EM gain");
    error = ncCamSetCalibratedEmGain(cam, *emgainPtr);
	if (error) {
        printf("\nThe error %d happened while setting EM gain\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Allocating the image");
	error = ncCamAllocUInt32Image(cam, &image);
	if (error) {
		printf("\nThe error %d happened while allocating the image\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Getting image size");
	error = ncCamGetSize(cam, &width, &height);
	if (error) {
		printf("\nThe error %d happened while getting image size\n", error);
		return error;
	}

	/********** Allocate streams **********/

	processinfo_WriteMessage(processinfo, "Allocating streams");

	uint32_t *imsize = (uint32_t *) malloc(sizeof(uint32_t)*2);

	imsize[0] = width;
	imsize[1] = height;

	create_image_ID("nuvu_stream", 2, imsize, _DATATYPE_FLOAT, 1, 10);
	create_image_ID("nuvu_flat", 2, imsize, _DATATYPE_FLOAT, 1, 10);
	create_image_ID("nuvu_bias", 2, imsize, _DATATYPE_FLOAT, 1, 10);

	imageID flatID = image_ID("nuvu_flat");
	imageID biasID = image_ID("nuvu_bias");

	free(imsize);

	/********** Load bias and flat **********/

	processinfo_WriteMessage(processinfo, "Loading flat and bias");
	load_bias_and_flat(processinfo, &fps, biasID, flatID, temperature, readoutmode, binning, *emgainPtr);

	/********** Load exposure parameters **********/

	NUVU_AUTOGAIN_PARAMS *autogain_params = (NUVU_AUTOGAIN_PARAMS*) malloc(sizeof(NUVU_AUTOGAIN_PARAMS)*MAXNB_AUTOGAIN_PARAMS);

	int NBautogain_params = read_exposure_params(autogain_params, autogain_params_fname);
	int current_autogain_param = 0;
 	long fluxCnt_old = *fluxCnt;
 	long autogainCnt_old = *autogainCnt;

	printf("INIT AUTOGAIN %ld\n", autogainCnt_old); //TODO

	if(*autogainFlag & FPFLAG_ONOFF) {
		update_exposure_parameters(&fps, autogain_params, current_autogain_param, emgainFlag, exposuretimeFlag);
    } else {
    	// Needed to avoid race condition when enabling auto-gain
    	// Explanation: cacao set flag to on and THEN increment cnt0, and our code can run in-between this two actions
    	autogainCnt_old--;
    }

	/********** Start camera **********/

	processinfo_WriteMessage(processinfo, "Opening the shutter");
    error = ncCamSetShutterMode(cam, OPEN);
    if (error) {
		printf("\nThe error %d happened while opening the shutter\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Starting the acquisition");
	error = ncCamStart(cam, 0);
	if (error) {
		printf("\nThe error %d happened while starting the acquisition\n", error);
		return error;
	}

	imageID IDout = image_ID("nuvu_stream");

	processinfo_WriteMessage(processinfo, "Looping");
	processinfo_loopstart(processinfo); // Notify processinfo that we are entering loop

    int ii, jj;
	while(loopOK == 1) {
		loopOK = processinfo_loopstep(processinfo);

		processinfo_exec_start(processinfo);

		if(processinfo_compute_status(processinfo) == 1)
		{
			/***** Update exposure and emgain if needed *****/

            if(*exposuretimeFlag & FPFLAG_KALAO_UPDATED)
            {
                processinfo_WriteMessage(processinfo, "New exposure time");

	            error = ncCamSetExposureTime(cam, *exposuretimePtr);
	            if (error) {
	            	printf("\nThe error %d happened while setting exposure time\n", error);
		            //return error;
	            }

	            *exposuretimeFlag &= ~FPFLAG_KALAO_UPDATED;

                processinfo_WriteMessage(processinfo, "Looping");
            }

            if(*emgainFlag & FPFLAG_KALAO_UPDATED)
            {
                processinfo_WriteMessage(processinfo, "New EM gain");

                error = ncCamSetCalibratedEmGain(cam, *emgainPtr);
	            if (error) {
                    printf("\nThe error %d happened while setting EM gain\n", error);
		            //return error;
	            }

                load_bias_and_flat(processinfo, &fps, biasID, flatID, temperature, readoutmode, binning, *emgainPtr);

                *emgainFlag &= ~FPFLAG_KALAO_UPDATED;

                processinfo_WriteMessage(processinfo, "Looping");
            }

            /***** Read image from camera *****/

			error = ncCamReadUInt32(cam, image);
			if (error) {
				printf("\nThe error %d happened while reading the image\n", error);
				//return error;
			}

			/***** Write output stream *****/

			data.image[IDout].md[0].write = 1;

			for(ii=0; ii<width; ii++)
				for(jj=0; jj<height; jj++)
					data.image[IDout].array.F[jj*width+ii] = ((image[jj*width+ii] >> 16) - data.image[biasID].array.F[jj*width+ii]) * data.image[flatID].array.F[jj*width+ii];

			processinfo_update_output_stream(processinfo, IDout);

			/***** Read temperatures *****/

			error = ncCamGetComponentTemp(cam, NC_TEMP_CCD, temp_ccd_ptr);
			if (error) {
				printf("\nThe error %d happened while reading the ccd temperature\n", error);
				//return error;
			}

			error = ncCamGetComponentTemp(cam, NC_TEMP_CONTROLLER, temp_controller_ptr);
			if (error) {
				printf("\nThe error %d happened while reading the controller temperature\n", error);
				//return error;
			}

			error = ncCamGetComponentTemp(cam, NC_TEMP_POWER_SUPPLY, temp_power_supply_ptr);
			if (error) {
				printf("\nThe error %d happened while reading the power supply temperature\n", error);
				//return error;
			}

			error = ncCamGetComponentTemp(cam, NC_TEMP_FPGA, temp_fpga_ptr);
			if (error) {
				printf("\nThe error %d happened while reading the fpga temperature\n", error);
				//return error;
			}

			error = ncCamGetComponentTemp(cam, NC_TEMP_HEATINK, temp_heatsink_ptr);
			if (error) {
				printf("\nThe error %d happened while reading the heatsink temperature\n", error);
				//return error;
			}

			/***** Autogain *****/

			if(*autogainFlag & FPFLAG_ONOFF && (*fluxPtr > autogain_high || *fluxPtr < autogain_low))
			{
				if(*autogainCnt != autogainCnt_old)
				{
					printf("RESET AUTOGAIN %ld\n", *autogainCnt); //TODO
					current_autogain_param = 0;
					update_exposure_parameters(&fps, autogain_params, current_autogain_param, emgainFlag, exposuretimeFlag);
					fluxCnt_old = *fluxCnt;
					autogainCnt_old = *autogainCnt;
				}
				else if(*exposuretimeFlag & (FPFLAG_KALAO_UPDATED|FPFLAG_KALAO_AUTOGAIN) || *emgainFlag & (FPFLAG_KALAO_UPDATED|FPFLAG_KALAO_AUTOGAIN))
				{
					// Wait because exposure parameters will be changed
					fluxCnt_old = *fluxCnt;
				}
				else if(   (fluxCnt_old < LONG_MAX - autogain_framewait && *fluxCnt > fluxCnt_old + autogain_framewait)
				        || (fluxCnt_old > LONG_MAX - autogain_framewait && *fluxCnt > LONG_MIN - (LONG_MAX - fluxCnt_old) + autogain_framewait))
				{
					if(*fluxPtr > autogain_high)
					{
						printf("REDUCE AUTOGAIN %ld\n", *autogainCnt); //TODO
						if(current_autogain_param > 0) {
							current_autogain_param--;
						}

						update_exposure_parameters(&fps, autogain_params, current_autogain_param, emgainFlag, exposuretimeFlag);
						fluxCnt_old = *fluxCnt;
            		}
            		else if (*fluxPtr < autogain_low)
            		{
            			printf("AUGMENT AUTOGAIN %ld\n", *autogainCnt); //TODO
						if(current_autogain_param < NBautogain_params-1) {
							current_autogain_param++;
						}

						update_exposure_parameters(&fps, autogain_params, current_autogain_param, emgainFlag, exposuretimeFlag);
						fluxCnt_old = *fluxCnt;
            		}
            	}
        	}
		}

		// process signals, increment loop counter
		processinfo_exec_end(processinfo);
	}

	printf("\nExiting loop\n");

	processinfo_WriteMessage(processinfo, "Aborting the acquisition");
	error = ncCamAbort(cam);
	if (error) {
		printf("\nThe error %d happened while aborting the acquisition\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Freeing the image buffer");
	error = ncCamFreeUInt32Image(&image);
	if (error) {
		printf("\nThe error %d happened while freeing the image buffer\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Closing the shutter");
    error = ncCamSetShutterMode(cam, CLOSE);
    if (error) {
		printf("\nThe error %d happened while closing the shutter\n", error);
		return error;
	}

	processinfo_WriteMessage(processinfo, "Closing the camera");
	error = ncCamClose(cam);
	if (error) {
		printf("\nThe error %d happened while closing the camera\n", error);
		return error;
	}
	cam = NULL;

	// ==================================
	// ENDING LOOP
	// ==================================
	function_parameter_struct_disconnect(&fps_shwfs);

	processinfo_cleanExit(processinfo);
	function_parameter_RUNexit(&fps);

	return RETURN_SUCCESS;
}

errno_t KalAO_Nuvu__acquire()
{
    long pindex = (long) getpid();  // index used to differentiate multiple calls to function
    // if we don't have anything more informative, we use PID

    FUNCTION_PARAMETER_STRUCT fps;

    // create FPS
    sprintf(data.FPS_name, "nuvu_acquire-%06ld", pindex);
    data.FPS_CMDCODE = FPSCMDCODE_FPSINIT;
    KalAO_Nuvu__acquire_FPCONF();

    KalAO_Nuvu__acquire_RUN();
    return RETURN_SUCCESS;
}
