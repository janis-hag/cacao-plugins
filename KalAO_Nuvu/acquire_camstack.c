/* ================================================================== */
/* ================================================================== */
/*            DEPENDANCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"
#include "CommandLineInterface/fps/fps_GetParamIndex.h"
#include "COREMOD_iofits/loadfits.h"
#include "COREMOD_iofits/file_exists.h"
#include "COREMOD_iofits/is_fits_file.h"

#include <stdlib.h>
#include <limits.h>

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

static uint64_t *autogain;
static long fpi_autogain;

static char *autogain_params_fname;
static long fpi_autogain_params_fname;

static int64_t *autogain_low;
static long fpi_autogain_low;

static int64_t *autogain_high;
static long fpi_autogain_high;

static int64_t *autogain_framewait;
static long fpi_autogain_framewait;



static long emgain_cnt0 = 0;
static long exposuretime_cnt0 = 0;


static CLICMDARGDEF farg[] =
{
    {
        CLIARG_INT64,
        ".temperature",
        "Temperature",
        "-60",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &temperature,
        &fpi_temperature
    },
    {
        CLIARG_INT64,
        ".readoutmode",
        "readoutmode",
        "1",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &readoutmode,
        &fpi_readoutmode
    },
    {
        CLIARG_INT64,
        ".binning",
        "binning",
        "2",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &binning,
        &fpi_binning
    },
    {
        CLIARG_INT64,
        ".emgain",
        "EM Gain",
        "1",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &emgain,
        &fpi_emgain
    },
    {
        CLIARG_FLOAT32,
        ".exposuretime",
        "Exposure Time [milliseconds]",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &exposuretime,
        &fpi_exposuretime
    },
    {
        CLIARG_FILENAME,
        ".bias",
        "Bias files",
        "bias/bias_%02ldC_%02ldrom_%01ldb_%04ldg.fits",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &bias_fname,
        &fpi_bias_fname
    },
    {
        CLIARG_FILENAME,
        ".flat",
        "Flat files",
        "flat/flat_%02ldC_%02ldrom_%01ldb_%04ldg.fits",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &flat_fname,
        &fpi_flat_fname
    },
    {
        CLIARG_ONOFF,
        ".autogain_on",
        "Auto-gain ON/OFF",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &autogain,
        &fpi_autogain
    },
    {
        CLIARG_FILENAME,
        ".autogain_params",
        "Exposure Parameters for Auto-gain",
        "filename",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &autogain_params_fname,
        &fpi_autogain_params_fname
    },
    {
        CLIARG_INT64,
        ".autogain_low",
        "Auto-gain Lower Limit (ADU)",
        "60000",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &autogain_low,
        &fpi_autogain_low
    },
    {
        CLIARG_INT64,
        ".autogain_high",
        "Auto-gain Upper Limit (ADU)",
        "75000",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &autogain_high,
        &fpi_autogain_high
    },
    {
        CLIARG_INT64,
        ".autogain_framewait",
        "Number of frames to wait after a change to exposure",
        "500",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &autogain_framewait,
        &fpi_autogain_framewait
    }
};


static CLICMDDATA CLIcmddata =
{
    "acquire",
    "Connect to camera and start acqusition",
    CLICMD_FIELDS_DEFAULTS
};


/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                                                                                                        	   */
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
        data.fpsptr->parray[fpi_temperature].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_temperature].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_temperature].val.i64[1] = -90; // min
        data.fpsptr->parray[fpi_temperature].val.i64[2] = 20; // max

        data.fpsptr->parray[fpi_readoutmode].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_readoutmode].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_readoutmode].val.i64[1] = 1; // min
        data.fpsptr->parray[fpi_readoutmode].val.i64[2] = 12; // max

        data.fpsptr->parray[fpi_binning].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_binning].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_binning].val.i64[1] = 1; // min
        data.fpsptr->parray[fpi_binning].val.i64[2] = 16; // max

        data.fpsptr->parray[fpi_emgain].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_emgain].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_emgain].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_emgain].val.i64[1] = 1; // min
        data.fpsptr->parray[fpi_emgain].val.i64[2] = 1000; // max

        data.fpsptr->parray[fpi_exposuretime].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_exposuretime].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_exposuretime].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_exposuretime].val.f32[1] = 0; // min
        data.fpsptr->parray[fpi_exposuretime].val.f32[2] = 1000; // max

        data.fpsptr->parray[fpi_autogain].fpflag |= FPFLAG_WRITERUN;

        data.fpsptr->parray[fpi_autogain_low].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_low].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_low].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_autogain_low].val.i64[2] = 65535; // max

        data.fpsptr->parray[fpi_autogain_high].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_high].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_high].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_autogain_high].val.i64[2] = 4*65535; // max

        data.fpsptr->parray[fpi_autogain_framewait].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_autogain_framewait].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_autogain_framewait].val.i64[1] = 0; // min
        data.fpsptr->parray[fpi_autogain_framewait].val.i64[2] = 10e3; // max

        emgain_cnt0 = data.fpsptr->parray[fpi_emgain].cnt0;
        exposuretime_cnt0 = data.fpsptr->parray[fpi_exposuretime].cnt0;
    }

    return RETURN_SUCCESS;
}


static errno_t customCONFcheck() {
    if(data.fpsptr->parray[fpi_emgain].cnt0 != emgain_cnt0)
    {
        emgain_cnt0 = data.fpsptr->parray[fpi_emgain].cnt0;

        data.fpsptr->parray[fpi_emgain].userflag |= FPFLAG_KALAO_UPDATED;

        if(data.fpsptr->parray[fpi_emgain].userflag & FPFLAG_KALAO_AUTOGAIN) {
            data.fpsptr->parray[fpi_emgain].userflag &= ~FPFLAG_KALAO_AUTOGAIN;
        } else {
            data.fpsptr->parray[fpi_autogain].userflag &= ~FPFLAG_ONOFF;
            data.fpsptr->parray[fpi_autogain].cnt0++;
        }
    }

    if(data.fpsptr->parray[fpi_exposuretime].cnt0 != exposuretime_cnt0)
    {
        exposuretime_cnt0 = data.fpsptr->parray[fpi_exposuretime].cnt0;

        data.fpsptr->parray[fpi_exposuretime].userflag |= FPFLAG_KALAO_UPDATED;

        if(data.fpsptr->parray[fpi_exposuretime].userflag & FPFLAG_KALAO_AUTOGAIN) {
            data.fpsptr->parray[fpi_exposuretime].userflag &= ~FPFLAG_KALAO_AUTOGAIN;
        } else {
            data.fpsptr->parray[fpi_autogain].userflag &= ~FPFLAG_ONOFF;
            data.fpsptr->parray[fpi_autogain].cnt0++;
        }
    }

    return RETURN_SUCCESS;
}











static int read_exposure_params(NUVU_AUTOGAIN_PARAMS *autogain_params)
{
    int NBautogain_params = 0;

    FILE *fp;

    fp = fopen(autogain_params_fname, "r");
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





void update_exposure_parameters(
    NUVU_AUTOGAIN_PARAMS *autogain_params,
    int current_autogain_param,
    uint64_t *emgainFlag,
    uint64_t *exposuretimeFlag
)
{
    *emgainFlag |= FPFLAG_KALAO_AUTOGAIN;
    *exposuretimeFlag |= FPFLAG_KALAO_AUTOGAIN;

    *emgain = autogain_params[current_autogain_param].emgain;
    *exposuretime = autogain_params[current_autogain_param].exposuretime;

    // Update GUI
    data.fpsptr->md->signal |= FUNCTION_PARAMETER_STRUCT_SIGNAL_UPDATE;
}





int update_exposuretime(float etime)
{
    // send tmux command
    printf("Exposure time to be set: %f\n", etime);

    char *set_exp = (char *)malloc(64*sizeof(char));
    sprintf(set_exp, "tmux send-keys -t nuvu_ctrl \"SetExposureTime(%f)\" Enter", *exposuretime);


    // TODO check if status is equal to the exposuretime.

    return RETURN_SUCCESS;
}




int update_emgain(long egain)
{
    printf("EMgain time to be set: %ld\n", egain);

    char *set_emgain = (char *)malloc(64*sizeof(char));
    sprintf(set_emgain, "tmux send-keys -t nuvu_ctrl \"SetEMCalibratedGain(%ld)\" Enter", egain);

    int status = system(set_emgain);
    (void) status;

    return RETURN_SUCCESS;
}





void load_bias_and_flat(
    PROCESSINFO *processinfo,
    imageID biasID,
    imageID flatID
)
{
    char biasfile[255];
    char flatfile[255];

    sprintf(biasfile, bias_fname, *temperature, *readoutmode, *binning, *emgain);
    sprintf(flatfile, flat_fname, *temperature, *readoutmode, *binning, *emgain);

    imageID biastmpID = -1;
    imageID flattmpID = -1;

    /********** Load bias **********/

    if(!file_exists(biasfile)) {
        printf("File %s not found\n", biasfile);
    } else if(!is_fits_file(biasfile)) {
        printf("File %s is not a valid FITS file\n", biasfile);
    } else
    {
        load_fits(biasfile, "nuvu_bias_tmp", 1, &biastmpID);

        if(data.image[biasID].md[0].datatype != data.image[biastmpID].md[0].datatype)
        {
            printf("Wrong data type for file %s\n", biasfile);
            biastmpID = -1;
        }
        else if (data.image[biasID].md[0].nelement != data.image[biastmpID].md[0].nelement)
        {
            printf("Wrong size for file %s\n", biasfile);
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
        load_fits(flatfile, "nuvu_flat_tmp", 1, &flattmpID);

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





static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();

    int width = 64;
    int height = 64;
    int width_in = 520;
    int height_in = 70;

    FUNCTION_PARAMETER_STRUCT fps_shwfs;
    function_parameter_struct_connect("shwfs_process", &fps_shwfs, FPSCONNECT_SIMPLE);
    float* fluxPtr = functionparameter_GetParamPtr_FLOAT32(&fps_shwfs, ".flux_subaperture");
    long* fluxCnt = &fps_shwfs.parray[functionparameter_GetParamIndex(&fps_shwfs, ".flux_subaperture")].cnt0;

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    /********** Allocate streams **********/
    processinfo_WriteMessage(processinfo, "Allocating streams");

    imageID IDin = processinfo->triggerstreamID;
    imageID IDout = image_ID("nuvu_stream");
    imageID flatID = image_ID("nuvu_flat");
    imageID biasID = image_ID("nuvu_bias");
    {
        uint32_t *imsize = (uint32_t *) malloc(sizeof(uint32_t)*2);

        imsize[0] = width;
        imsize[1] = height;


        create_image_ID("nuvu_stream", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &IDout);
        create_image_ID("nuvu_flat", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &flatID);
        create_image_ID("nuvu_bias", 2, imsize, _DATATYPE_FLOAT, 1, 10, 0, &biasID);

        free(imsize);
    }
    /********** Configure camera **********/

    //error =
    update_exposuretime(*exposuretime);

    //error =
    update_emgain(*emgain);

    /********** Load bias and flat **********/

    processinfo_WriteMessage(processinfo, "Loading flat and bias");
    load_bias_and_flat(processinfo, biasID, flatID);

    /********** Load exposure parameters **********/

    NUVU_AUTOGAIN_PARAMS *autogain_params = (NUVU_AUTOGAIN_PARAMS*) malloc(sizeof(NUVU_AUTOGAIN_PARAMS)*MAXNB_AUTOGAIN_PARAMS);

    int NBautogain_params = read_exposure_params(autogain_params);
    int current_autogain_param = 0;
    long fluxCnt_old = *fluxCnt;
    long autogainCnt_old = data.fpsptr->parray[fpi_autogain].cnt0;

    printf("INIT AUTOGAIN %ld\n", autogainCnt_old); //TODO

    if(data.fpsptr->parray[fpi_autogain].fpflag & FPFLAG_ONOFF) {
        update_exposure_parameters(autogain_params, current_autogain_param,
                                   &data.fpsptr->parray[fpi_emgain].userflag,
                                   &data.fpsptr->parray[fpi_exposuretime].userflag
                                  );
    } else {
        // Needed to avoid race condition when enabling auto-gain
        // Explanation: cacao set flag to on and THEN increment cnt0, and our code can run in-between this two actions
        autogainCnt_old--;
    }

    /********** Start camera **********/

    processinfo_WriteMessage(processinfo, "Looping");


    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART


    if(data.fpsptr->parray[fpi_exposuretime].userflag & FPFLAG_KALAO_UPDATED)
    {
        processinfo_WriteMessage(processinfo, "New exposure time");

        //error =
        update_exposuretime(*exposuretime);

        data.fpsptr->parray[fpi_exposuretime].userflag &= ~FPFLAG_KALAO_UPDATED;

        processinfo_WriteMessage(processinfo, "Looping");
    }

    if(data.fpsptr->parray[fpi_emgain].userflag & FPFLAG_KALAO_UPDATED)
    {
        processinfo_WriteMessage(processinfo, "New EM gain");

        //error =
        update_emgain(*emgain);

        load_bias_and_flat(processinfo, biasID, flatID);

        data.fpsptr->parray[fpi_emgain].userflag &= ~FPFLAG_KALAO_UPDATED;

        processinfo_WriteMessage(processinfo, "Looping");
    }

    /***** Write output stream *****/

    data.image[IDout].md[0].write = 1;

    for(int ii=0; ii<width; ii++)
        for(int jj=0; jj<height; jj++)
            data.image[IDout].array.F[jj*width+ii] = (data.image[IDin].array.UI16[(jj+4)*width_in+8*(width-ii)] - data.image[biasID].array.F[jj*width+ii])  * data.image[flatID].array.F[jj*width+ii];

    processinfo_update_output_stream(processinfo, IDout);

    /***** Autogain *****/

    if(data.fpsptr->parray[fpi_autogain].fpflag & FPFLAG_ONOFF && (*fluxPtr > (float) *autogain_high || *fluxPtr < (float) *autogain_low))
    {
        if(data.fpsptr->parray[fpi_autogain].cnt0 != autogainCnt_old)
        {
            printf("RESET AUTOGAIN %ld\n", data.fpsptr->parray[fpi_autogain].cnt0); //TODO
            current_autogain_param = 0;
            update_exposure_parameters(autogain_params, current_autogain_param,
                                       &data.fpsptr->parray[fpi_emgain].userflag,
                                       &data.fpsptr->parray[fpi_exposuretime].userflag);
            fluxCnt_old = *fluxCnt;
            autogainCnt_old = data.fpsptr->parray[fpi_autogain].cnt0;
        }
        else if(data.fpsptr->parray[fpi_exposuretime].userflag & (FPFLAG_KALAO_UPDATED|FPFLAG_KALAO_AUTOGAIN) || data.fpsptr->parray[fpi_emgain].userflag & (FPFLAG_KALAO_UPDATED|FPFLAG_KALAO_AUTOGAIN))
        {
            // Wait because exposure parameters will be changed
            fluxCnt_old = *fluxCnt;
        }
        else if(   (fluxCnt_old < LONG_MAX - *autogain_framewait  && *fluxCnt > fluxCnt_old + *autogain_framewait)
                   || (fluxCnt_old > LONG_MAX - *autogain_framewait && *fluxCnt > LONG_MIN - (LONG_MAX - fluxCnt_old) + *autogain_framewait))
        {
            if(*fluxPtr > (float) *autogain_high)
            {
                printf("REDUCE AUTOGAIN %ld\n", data.fpsptr->parray[fpi_autogain].cnt0); //TODO
                if(current_autogain_param > 0) {
                    current_autogain_param--;
                }

                update_exposure_parameters(autogain_params, current_autogain_param,
                                           &data.fpsptr->parray[fpi_emgain].userflag,
                                           &data.fpsptr->parray[fpi_exposuretime].userflag);

                fluxCnt_old = *fluxCnt;
            }
            else if (*fluxPtr < (float) *autogain_low)
            {
                printf("AUGMENT AUTOGAIN %ld\n", data.fpsptr->parray[fpi_autogain].cnt0); //TODO
                if(current_autogain_param < NBautogain_params-1) {
                    current_autogain_param++;
                }

                update_exposure_parameters(autogain_params, current_autogain_param,
                                           &data.fpsptr->parray[fpi_emgain].userflag,
                                           &data.fpsptr->parray[fpi_exposuretime].userflag);
                fluxCnt_old = *fluxCnt;
            }
        }
    }

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

    function_parameter_struct_disconnect(&fps_shwfs);

    free(autogain_params);

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}




INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t CLIADDCMD_KalAO_Nuvu__acquire()
{
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    CLIcmddata.FPS_customCONFcheck = customCONFcheck;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}

