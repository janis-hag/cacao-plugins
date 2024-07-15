/* ================================================================== */
/* ================================================================== */
/*            DEPENDENCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"
#include "CommandLineInterface/fps/fps_GetParamIndex.h"

#include "COREMOD_iofits/file_exists.h"
#include "COREMOD_iofits/is_fits_file.h"
#include "COREMOD_iofits/loadfits.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

/* ================================================================== */
/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */

typedef struct
{
    int64_t emgain;
    float exposuretime;
    float exposure;

} NUVU_AUTOGAIN_PARAMS;

#define MAXNB_AUTOGAIN_PARAMS 100
#define EPSILON 0.01
#define FPFLAG_KALAO_AUTOGAIN 0x1000000000000000
#define DYNAMIC_BIAS_SIZE 8
#define READOUT_TIME 0.5538

static int64_t *temperature;
static long fpi_temperature;

static int64_t *readoutmode;
static long fpi_readoutmode;

static int64_t *binning;
static long fpi_binning;

static int64_t *emgain;
static long fpi_emgain = 0;

static float *exposuretime;
static long fpi_exposuretime;

static char *bias_fname;
static long fpi_bias_fname;

static char *flat_fname;
static long fpi_flat_fname;

static uint64_t *dynamic_bias;
static long fpi_dynamic_bias;

static int64_t *dynamic_bias_algorithm;
static long fpi_dynamic_bias_algorithm;

static uint64_t *autogain;
static long fpi_autogain;

static int64_t *autogain_setting;
static long fpi_autogain_setting;

static char *autogain_params_fname;
static long fpi_autogain_params_fname;

static char *autogain_flux_param;
static long fpi_autogain_flux_param;

static int64_t *autogain_lowgain_lower;
static long fpi_autogain_lowgain_lower;

static int64_t *autogain_lowgain_upper;
static long fpi_autogain_lowgain_upper;

static int64_t *autogain_highgain_lower;
static long fpi_autogain_highgain_lower;

static int64_t *autogain_highgain_upper;
static long fpi_autogain_highgain_upper;

static int64_t *autogain_wait;
static long fpi_autogain_wait;

static CLICMDARGDEF farg[] =
    {
        {
            CLIARG_INT64,
            ".temperature",
            "Temperature (unused)",
            "-60",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&temperature,
            &fpi_temperature,
        },
        {
            CLIARG_INT64,
            ".readoutmode",
            "Readout mode (unused)",
            "1",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&readoutmode,
            &fpi_readoutmode,
        },
        {
            CLIARG_INT64,
            ".binning",
            "Binning (unused)",
            "2",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&binning,
            &fpi_binning,
        },
        {
            CLIARG_INT64,
            ".emgain",
            "EM Gain",
            "1",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&emgain,
            &fpi_emgain,
        },
        {
            CLIARG_FLOAT32,
            ".exposuretime",
            "Exposure Time [ms]",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&exposuretime,
            &fpi_exposuretime,
        },
        {
            CLIARG_FITSFILENAME,
            ".bias",
            "Bias files",
            "bias/bias_%02ldC_%02ldrom_%01ldb_%04ldg.fits",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&bias_fname,
            &fpi_bias_fname,
        },
        {
            CLIARG_FITSFILENAME,
            ".flat",
            "Flat files",
            "flat/flat_%02ldC_%02ldrom_%01ldb_%04ldg.fits",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&flat_fname,
            &fpi_flat_fname,
        },
        {
            CLIARG_ONOFF,
            ".dynamic_bias_on",
            "Dynamic bias ON/OFF",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&dynamic_bias,
            &fpi_dynamic_bias,
        },
        {
            CLIARG_INT64,
            ".dynamic_bias_algorithm",
            "Dynamic bias algorithm (0 = Average, 1 = Bilinear)",
            "1",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&dynamic_bias_algorithm,
            &fpi_dynamic_bias_algorithm,
        },
        {
            CLIARG_ONOFF,
            ".autogain_on",
            "Auto-gain ON/OFF",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain,
            &fpi_autogain,
        },
        {
            CLIARG_INT64,
            ".autogain_setting",
            "Current Auto-gain setting",
            "0",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_setting,
            &fpi_autogain_setting,
        },
        {
            CLIARG_FILENAME,
            ".autogain.params",
            "Exposure parameters for Auto-gain",
            "filename",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_params_fname,
            &fpi_autogain_params_fname,
        },
        {
            CLIARG_STR,
            ".autogain.flux_param",
            "Flux param to use for autogain",
            "",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_flux_param,
            &fpi_autogain_flux_param,
        },
        {
            CLIARG_INT64,
            ".autogain.lowgain_lower",
            "Auto-gain Lower Limit in low gain regime [ADU]",
            "25000",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_lowgain_lower,
            &fpi_autogain_lowgain_lower,
        },
        {
            CLIARG_INT64,
            ".autogain.lowgain_upper",
            "Auto-gain Upper Limit in low gain regime [ADU]",
            "60000",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_lowgain_upper,
            &fpi_autogain_lowgain_upper,
        },
        {
            CLIARG_INT64,
            ".autogain.highgain_lower",
            "Auto-gain Lower Limit in high gain regime [ADU]",
            "1000",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_highgain_lower,
            &fpi_autogain_highgain_lower,
        },
        {
            CLIARG_INT64,
            ".autogain.highgain_upper",
            "Auto-gain Upper Limit in high gain regime [ADU]",
            "3000",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_highgain_upper,
            &fpi_autogain_highgain_upper,
        },
        {
            CLIARG_INT64,
            ".autogain.wait_time",
            "Time to wait after a change to exposure [ms]",
            "750",
            CLIARG_HIDDEN_DEFAULT,
            (void **)&autogain_wait,
            &fpi_autogain_wait,
        },
};

static CLICMDDATA CLIcmddata =
    {
        "acquire",
        "Connect to camera and start acqusition",
        CLICMD_FIELDS_DEFAULTS,
};

/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                                                                                                        	   */
/* ================================================================== */
/* ================================================================== */

static errno_t help_function() {
    return RETURN_SUCCESS;
}

static errno_t customCONFsetup() {
    if (data.fpsptr != NULL) {
        data.fpsptr->parray[fpi_temperature].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_temperature].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_temperature].val.i64[1] = -90; // min
        data.fpsptr->parray[fpi_temperature].val.i64[2] = 20;  // max

        data.fpsptr->parray[fpi_readoutmode].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_readoutmode].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_readoutmode].val.i64[1] = 1;  // min
        data.fpsptr->parray[fpi_readoutmode].val.i64[2] = 12; // max

        data.fpsptr->parray[fpi_binning].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_binning].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_binning].val.i64[1] = 1;  // min
        data.fpsptr->parray[fpi_binning].val.i64[2] = 16; // max

        data.fpsptr->parray[fpi_emgain].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_emgain].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_emgain].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_emgain].val.i64[1] = 1;    // min
        data.fpsptr->parray[fpi_emgain].val.i64[2] = 1000; // max

        data.fpsptr->parray[fpi_exposuretime].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_exposuretime].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_exposuretime].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_exposuretime].val.f32[1] = 0;    // min
        data.fpsptr->parray[fpi_exposuretime].val.f32[2] = 1000; // max

        data.fpsptr->parray[fpi_dynamic_bias].fpflag |= FPFLAG_WRITERUN;

        data.fpsptr->parray[fpi_dynamic_bias_algorithm].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_dynamic_bias_algorithm].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_dynamic_bias_algorithm].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_dynamic_bias_algorithm].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_dynamic_bias_algorithm].val.i64[2] = 1; // max

        data.fpsptr->parray[fpi_autogain].fpflag |= FPFLAG_WRITERUN;

        data.fpsptr->parray[fpi_autogain_setting].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_autogain_setting].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_setting].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_setting].val.i64[1] = 0;                     // min
        data.fpsptr->parray[fpi_autogain_setting].val.i64[2] = MAXNB_AUTOGAIN_PARAMS; // max

        data.fpsptr->parray[fpi_autogain_lowgain_lower].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_autogain_lowgain_lower].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_lowgain_lower].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_lowgain_lower].val.i64[1] = 0;     // min
        data.fpsptr->parray[fpi_autogain_lowgain_lower].val.i64[2] = 65535; // max

        data.fpsptr->parray[fpi_autogain_lowgain_upper].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_autogain_lowgain_upper].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_lowgain_upper].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_lowgain_upper].val.i64[1] = 0;         // min
        data.fpsptr->parray[fpi_autogain_lowgain_upper].val.i64[2] = 4 * 65535; // max

        data.fpsptr->parray[fpi_autogain_highgain_lower].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_autogain_highgain_lower].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_highgain_lower].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_highgain_lower].val.i64[1] = 0;     // min
        data.fpsptr->parray[fpi_autogain_highgain_lower].val.i64[2] = 65535; // max

        data.fpsptr->parray[fpi_autogain_highgain_upper].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_autogain_highgain_upper].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_highgain_upper].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_highgain_upper].val.i64[1] = 0;         // min
        data.fpsptr->parray[fpi_autogain_highgain_upper].val.i64[2] = 4 * 65535; // max

        data.fpsptr->parray[fpi_autogain_wait].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_autogain_wait].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_wait].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_wait].val.i64[1] = 0;    // min
        data.fpsptr->parray[fpi_autogain_wait].val.i64[2] = 10e6; // max
    }

    return RETURN_SUCCESS;
}

static int read_exposure_params(NUVU_AUTOGAIN_PARAMS *autogain_params, int64_t *max_gain, float *min_exposuretime) {
    int NBautogain_params = 0;

    FILE *fp;

    fp = fopen(autogain_params_fname, "r");
    if (fp == NULL) {
        perror("Unable to open file!"); // %s",  autogain_params_fname);
        exit(1);
    }

    int64_t emgain;
    float exposuretime;

    char keyw[16];

    int loopOK = 1;
    while (loopOK == 1) {
        int ret = fscanf(fp, "%s %ld %f", keyw, &emgain, &exposuretime);
        if (ret == EOF) {
            loopOK = 0;
        } else {
            if ((ret == 3) && (strcmp(keyw, "EXP") == 0)) {
                printf("Found EXP %5ld %5f\n", emgain, exposuretime);
                autogain_params[NBautogain_params].emgain = emgain;
                autogain_params[NBautogain_params].exposuretime = exposuretime;
                autogain_params[NBautogain_params].exposure = emgain * exposuretime;

                if (emgain > *max_gain) {
                    *max_gain = emgain;
                }

                if (exposuretime < *min_exposuretime) {
                    *min_exposuretime = exposuretime;
                }

                NBautogain_params++;
            }
        }
    }
    printf("Loaded %d exposure parameters\n", NBautogain_params);

    fclose(fp);

    data.fpsptr->parray[fpi_autogain_setting].val.i64[2] = NBautogain_params - 1;

    return NBautogain_params;
}

void increase_autogain(int NBautogain_params) {
    if (*autogain_setting < NBautogain_params - 1) {
        (*autogain_setting)++;
        data.fpsptr->parray[fpi_autogain_setting].cnt0++;
    }
}

void decrease_autogain(int NBautogain_params) {
    if (*autogain_setting > 0) {
        (*autogain_setting)--;
        data.fpsptr->parray[fpi_autogain_setting].cnt0++;
    }
}

void update_exposure_parameters(
    NUVU_AUTOGAIN_PARAMS *autogain_params) {
    // Signal that emgain and exposuretime will be updated by autogain
    data.fpsptr->parray[fpi_emgain].userflag |= FPFLAG_KALAO_AUTOGAIN;
    data.fpsptr->parray[fpi_exposuretime].userflag |= FPFLAG_KALAO_AUTOGAIN;

    *emgain = autogain_params[*autogain_setting].emgain;
    *exposuretime = autogain_params[*autogain_setting].exposuretime;

    data.fpsptr->parray[fpi_emgain].cnt0++;
    data.fpsptr->parray[fpi_exposuretime].cnt0++;
}

int update_exposuretime() {
    printf("Exposure time to be set: %f\n", *exposuretime);

    char set_exposuretime[255];
    sprintf(set_exposuretime, "tmux send-keys -t kalaocam_ctrl \"SetExposureTime(%f)\" Enter", *exposuretime);

    int status = system(set_exposuretime);
    (void)status;

    return RETURN_SUCCESS;
}

int update_emgain() {
    printf("EMgain to be set: %ld\n", *emgain);

    char set_emgain[255];
    sprintf(set_emgain, "tmux send-keys -t kalaocam_ctrl \"SetEMCalibratedGain(%ld)\" Enter", *emgain);

    int status = system(set_emgain);
    (void)status;

    return RETURN_SUCCESS;
}

void load_bias_and_flat(
    PROCESSINFO *processinfo,
    imageID biasID,
    imageID flatID) {
    char biasfile[255];
    char flatfile[255];

    sprintf(biasfile, bias_fname, *temperature, *readoutmode, *binning, *emgain);
    sprintf(flatfile, flat_fname, *temperature, *readoutmode, *binning, *emgain);

    imageID biastmpID = -1;
    imageID flattmpID = -1;

    /********** Load bias **********/

    if (!file_exists(biasfile)) {
        printf("Bias file %s not found\n", biasfile);
    } else if (!is_fits_file(biasfile)) {
        printf("Bias file %s is not a valid FITS file\n", biasfile);
    } else {
        load_fits(biasfile, "nuvu_bias_tmp", 1, &biastmpID);

        if (data.image[biasID].md[0].datatype != data.image[biastmpID].md[0].datatype) {
            printf("Wrong data type for bias file %s\n", biasfile);
            biastmpID = -1;
        } else if (data.image[biasID].md[0].nelement != data.image[biastmpID].md[0].nelement) {
            printf("Wrong size for bias file %s\n", biasfile);
            biastmpID = -1;
        }
    }

    data.image[biasID].md[0].write = 1;

    // Note: DO NOT use memset() as it will be optimized away
    if (biastmpID != -1) {
        for (uint64_t i = 0; i < data.image[biasID].md[0].nelement; i++)
            data.image[biasID].array.F[i] = data.image[biastmpID].array.F[i];

        data.fpsptr->parray[fpi_dynamic_bias].fpflag &= ~FPFLAG_ONOFF;
        data.fpsptr->parray[fpi_dynamic_bias].cnt0++;
    } else {
        for (uint64_t i = 0; i < data.image[biasID].md[0].nelement; i++)
            data.image[biasID].array.F[i] = 0;

        data.fpsptr->parray[fpi_dynamic_bias].fpflag |= FPFLAG_ONOFF;
        data.fpsptr->parray[fpi_dynamic_bias].cnt0++;
    }

    processinfo_update_output_stream(processinfo, biasID);

    /********** Load flat **********/

    if (!file_exists(flatfile)) {
        printf("Flat file %s not found\n", flatfile);
    } else if (!is_fits_file(flatfile)) {
        printf("Flat file %s is not a valid FITS file\n", flatfile);
    } else {
        load_fits(flatfile, "nuvu_flat_tmp", 1, &flattmpID);

        if (data.image[flatID].md[0].datatype != data.image[flattmpID].md[0].datatype) {
            printf("Wrong data type for flat file %s\n", flatfile);
            flattmpID = -1;
        } else if (data.image[flatID].md[0].nelement != data.image[flattmpID].md[0].nelement) {
            printf("Wrong size for flat file %s\n", flatfile);
            flattmpID = -1;
        }
    }

    data.image[flatID].md[0].write = 1;

    // Note: DO NOT use memset() as it will be optimized away
    if (flattmpID != -1) {
        for (uint64_t i = 0; i < data.image[flatID].md[0].nelement; i++)
            data.image[flatID].array.F[i] = data.image[flattmpID].array.F[i];
    } else {
        for (uint64_t i = 0; i < data.image[flatID].md[0].nelement; i++)
            data.image[flatID].array.F[i] = 1;
    }

    processinfo_update_output_stream(processinfo, flatID);
}

#define WIDTH_IN 520
#define HEIGHT_IN 70

#define WIDTH 64
#define HEIGHT 64

// With parenthesis around the arguments and the expression to avoid operator precedence issues
#define RAW_PX_INDEX(i, j) (((j) + 4) * WIDTH_IN + 8 * (WIDTH - (i)))

static errno_t compute_function() {
    DEBUG_TRACE_FSTART();

    int width = 64;
    int height = 64;

    FUNCTION_PARAMETER_STRUCT shwfs_fps;

    char shwfs_tmp[255];
    char *shwfs_proc_name;
    char *shwfs_flux_param;

    strcpy(shwfs_tmp, autogain_flux_param);

    shwfs_proc_name = strtok(shwfs_tmp, ".");
    shwfs_flux_param = strtok(NULL, ".");

    function_parameter_struct_connect(shwfs_proc_name, &shwfs_fps, FPSCONNECT_SIMPLE);

    float *flux = functionparameter_GetParamPtr_FLOAT32(&shwfs_fps, shwfs_flux_param);
    long *flux_cnt0_ptr = &shwfs_fps.parray[functionparameter_GetParamIndex(&shwfs_fps, shwfs_flux_param)].cnt0;

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Allocate streams **********/

    processinfo_WriteMessage(processinfo, "Allocating streams");

    imageID inID = processinfo->triggerstreamID;
    imageID outID = image_ID("nuvu_stream");
    imageID flatID = image_ID("nuvu_flat");
    imageID biasID = image_ID("nuvu_bias");
    imageID dynamicBiasID = image_ID("nuvu_dynamic_bias");
    {
        uint32_t *imsize = (uint32_t *)malloc(sizeof(uint32_t) * 2);

        imsize[0] = width;
        imsize[1] = height;

        create_image_ID("nuvu_stream", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &outID);
        create_image_ID("nuvu_flat", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &flatID);
        create_image_ID("nuvu_bias", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &biasID);
        create_image_ID("nuvu_dynamic_bias", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &dynamicBiasID);

        free(imsize);
    }

    /********** Configure camera **********/

    processinfo_WriteMessage(processinfo, "Configuring camera");

    // error =
    update_exposuretime();
    long exposuretime_cnt0 = data.fpsptr->parray[fpi_exposuretime].cnt0;

    // error =
    update_emgain();
    long emgain_cnt0 = data.fpsptr->parray[fpi_emgain].cnt0;

    /********** Load bias and flat **********/

    processinfo_WriteMessage(processinfo, "Loading flat and bias");

    load_bias_and_flat(processinfo, biasID, flatID);

    /********** Load auto-gain parameters **********/

    processinfo_WriteMessage(processinfo, "Loading auto-gain parameters");

    NUVU_AUTOGAIN_PARAMS *autogain_params = (NUVU_AUTOGAIN_PARAMS *)malloc(sizeof(NUVU_AUTOGAIN_PARAMS) * MAXNB_AUTOGAIN_PARAMS);
    int64_t max_gain = 0;
    float min_exposuretime = 1e6;

    int NBautogain_params = read_exposure_params(autogain_params, &max_gain, &min_exposuretime);
    long flux_cnt0 = *flux_cnt0_ptr;
    long autogain_cnt0 = data.fpsptr->parray[fpi_autogain].cnt0;

    if (data.fpsptr->parray[fpi_autogain].fpflag & FPFLAG_ONOFF) {
        // If autogain is on, apply setting
        update_exposure_parameters(autogain_params);
    } else {
        // Needed to avoid race condition when enabling auto-gain
        // Explanation: cacao set flag to on and THEN increment cnt0, and our code can run in-between this two actions
        autogain_cnt0--;
    }

    long autogain_setting_cnt0 = data.fpsptr->parray[fpi_autogain_setting].cnt0;

    /********** Start camera **********/

    int ii, jj;
    uint64_t autogain_wait_frame;

    processinfo_WriteMessage(processinfo, "Looping");

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

    if (data.fpsptr->parray[fpi_autogain_setting].cnt0 != autogain_setting_cnt0) {
        autogain_setting_cnt0 = data.fpsptr->parray[fpi_autogain_setting].cnt0;

        update_exposure_parameters(autogain_params);
    }

    if (data.fpsptr->parray[fpi_exposuretime].cnt0 != exposuretime_cnt0) {
        exposuretime_cnt0 = data.fpsptr->parray[fpi_exposuretime].cnt0;

        if (data.fpsptr->parray[fpi_exposuretime].userflag & FPFLAG_KALAO_AUTOGAIN) {
            // If updated by autogain, remove flag
            data.fpsptr->parray[fpi_exposuretime].userflag &= ~FPFLAG_KALAO_AUTOGAIN;
        } else {
            // If not (updated by user), deactivate autogain
            data.fpsptr->parray[fpi_autogain].fpflag &= ~FPFLAG_ONOFF;
            data.fpsptr->parray[fpi_autogain].cnt0++;
        }

        processinfo_WriteMessage(processinfo, "New exposure time");

        // error =
        update_exposuretime();

        processinfo_WriteMessage(processinfo, "Looping");
    }

    if (data.fpsptr->parray[fpi_emgain].cnt0 != emgain_cnt0) {
        emgain_cnt0 = data.fpsptr->parray[fpi_emgain].cnt0;

        if (data.fpsptr->parray[fpi_emgain].userflag & FPFLAG_KALAO_AUTOGAIN) {
            // If updated by autogain, remove flag
            data.fpsptr->parray[fpi_emgain].userflag &= ~FPFLAG_KALAO_AUTOGAIN;
        } else {
            // If not (updated by user), deactivate autogain
            data.fpsptr->parray[fpi_autogain].fpflag &= ~FPFLAG_ONOFF;
            data.fpsptr->parray[fpi_autogain].cnt0++;
        }

        processinfo_WriteMessage(processinfo, "New EM gain");

        // error =
        update_emgain();

        load_bias_and_flat(processinfo, biasID, flatID);

        processinfo_WriteMessage(processinfo, "Looping");
    }

    /***** Write output stream *****/

    data.image[outID].md[0].write = 1;
    data.image[dynamicBiasID].md[0].write = 1;

    if (data.fpsptr->parray[fpi_dynamic_bias].fpflag & FPFLAG_ONOFF) {
        float bias[4] = {0, 0, 0, 0};

        int ii_0[] = {0, width - DYNAMIC_BIAS_SIZE};
        int jj_0[] = {0, height - DYNAMIC_BIAS_SIZE};

        for (int k = 0; k < 2; k++) {
            for (int l = 0; l < 2; l++) {
                for (ii = 0; ii < DYNAMIC_BIAS_SIZE; ii++)
                    for (jj = 0; jj < DYNAMIC_BIAS_SIZE; jj++)
                        bias[l * 2 + k] += data.image[inID].array.UI16[RAW_PX_INDEX(ii_0[k] + ii, jj_0[l] + jj)];

                bias[l * 2 + k] /= DYNAMIC_BIAS_SIZE * DYNAMIC_BIAS_SIZE;
            }
        }

        if (*dynamic_bias_algorithm == 0) {
            // Subtract mean
            float bias_mean = (bias[0] + bias[1] + bias[2] + bias[3]) / 4;

            for (ii = 0; ii < width; ii++) {
                for (jj = 0; jj < height; jj++) {
                    data.image[outID].array.F[jj * width + ii] = (data.image[inID].array.UI16[RAW_PX_INDEX(ii, jj)] - bias_mean) * data.image[flatID].array.F[jj * width + ii];
                    data.image[dynamicBiasID].array.F[jj * width + ii] = bias_mean;
                }
            }
        } else {
            // Subtract bilinear fit
            float x1 = (DYNAMIC_BIAS_SIZE - 1) / 2;
            float y1 = (DYNAMIC_BIAS_SIZE - 1) / 2;
            float x2 = (width - 1) - (DYNAMIC_BIAS_SIZE - 1) / 2;
            float y2 = (height - 1) - (DYNAMIC_BIAS_SIZE - 1) / 2;

            float C = 1 / ((x2 - x1) * (y2 - y1));

            float a00 = C * (x2 * y2 * bias[0] - x2 * y1 * bias[1] - x1 * y2 * bias[2] + x1 * y1 * bias[3]);
            float a10 = C * (-y2 * bias[0] + y1 * bias[1] + y2 * bias[2] - y1 * bias[3]);
            float a01 = C * (-x2 * bias[0] + x2 * bias[1] + x1 * bias[2] - x1 * bias[3]);
            float a11 = C * (bias[0] - bias[1] - bias[2] + bias[3]);

            float bias_bilinear;
            for (ii = 0; ii < width; ii++) {
                for (jj = 0; jj < height; jj++) {
                    bias_bilinear = a00 + a10 * jj + a01 * ii + a11 * jj * ii;
                    data.image[outID].array.F[jj * width + ii] = (data.image[inID].array.UI16[RAW_PX_INDEX(ii, jj)] - bias_bilinear) * data.image[flatID].array.F[jj * width + ii];
                    data.image[dynamicBiasID].array.F[jj * width + ii] = bias_bilinear;
                }
            }
        }

    } else {
        for (ii = 0; ii < width; ii++) {
            for (jj = 0; jj < height; jj++) {
                data.image[outID].array.F[jj * width + ii] = (data.image[inID].array.UI16[RAW_PX_INDEX(ii, jj)] - data.image[biasID].array.F[jj * width + ii]) * data.image[flatID].array.F[jj * width + ii];
                data.image[dynamicBiasID].array.F[jj * width + ii] = 0;
            }
        }
    }

    processinfo_update_output_stream(processinfo, outID);
    processinfo_update_output_stream(processinfo, dynamicBiasID);

    /***** Autogain *****/

    if (data.fpsptr->parray[fpi_autogain].fpflag & FPFLAG_ONOFF) {
        autogain_wait_frame = *autogain_wait;
        if (*exposuretime < READOUT_TIME) {
            autogain_wait_frame /= READOUT_TIME;
        } else {
            autogain_wait_frame /= *exposuretime;
        }

        if (data.fpsptr->parray[fpi_autogain].cnt0 != autogain_cnt0) {
            // Autogain was enabled
            autogain_cnt0 = data.fpsptr->parray[fpi_autogain].cnt0;
            update_exposure_parameters(autogain_params);
            flux_cnt0 = *flux_cnt0_ptr;
        } else if (*flux_cnt0_ptr < flux_cnt0) {
            // Wrap-around or restart
            flux_cnt0 = *flux_cnt0_ptr;
        } else if (*flux_cnt0_ptr > flux_cnt0 + autogain_wait_frame) {
            // Enough frames passed
            if (*emgain == max_gain && fabs(*exposuretime - min_exposuretime) < EPSILON) {
                // We are in the intermediate gain regime
                if (*flux > *autogain_lowgain_upper) {
                    decrease_autogain(NBautogain_params);
                    flux_cnt0 = *flux_cnt0_ptr;
                } else if (*flux < *autogain_highgain_lower) {
                    increase_autogain(NBautogain_params);
                    flux_cnt0 = *flux_cnt0_ptr;
                }
            } else if (*emgain < max_gain) {
                // We are in the low gain regime
                if (*flux > *autogain_lowgain_upper) {
                    decrease_autogain(NBautogain_params);
                    flux_cnt0 = *flux_cnt0_ptr;
                } else if (*flux < *autogain_lowgain_lower) {
                    increase_autogain(NBautogain_params);
                    flux_cnt0 = *flux_cnt0_ptr;
                }
            } else {
                // We are in the high gain regime
                if (*flux > *autogain_highgain_upper) {
                    decrease_autogain(NBautogain_params);
                    flux_cnt0 = *flux_cnt0_ptr;
                } else if (*flux < *autogain_highgain_lower) {
                    increase_autogain(NBautogain_params);
                    flux_cnt0 = *flux_cnt0_ptr;
                }
            }
        }
    }

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    function_parameter_struct_disconnect(&shwfs_fps);

    free(autogain_params);

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t
CLIADDCMD_KalAO_Nuvu__acquire() {
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
