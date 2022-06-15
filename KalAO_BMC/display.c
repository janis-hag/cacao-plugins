/* ================================================================== */
/* ================================================================== */
/*            DEPENDANCIES                                            */
/* ================================================================== */
/* ================================================================== */

#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"
#include "CommandLineInterface/fps/fps_GetParamIndex.h"
#include "ZernikePolyn/ZernikePolyn.h"

#include "BMCApi.h"

#include <math.h>

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

} ZERNIKE_PARAMS;

static char *DMin_streamname;
static long fpi_DMin_streamname;

static char *TTMin_streamname;
static long fpi_TTMin_streamname;

static float *ttm_tip_offset;
static long fpi_ttm_tip_offset;

static float *ttm_tilt_offset;
static long fpi_ttm_tilt_offset;

static float *zernike_piston;
static long fpi_zernike_piston;

static float *zernike_tip;
static long fpi_zernike_tip;

static float *zernike_tilt;
static long fpi_zernike_tilt;

static float *zernike_defocus;
static long fpi_zernike_defocus;

static float *zernike_verticalastigmatism;
static long fpi_zernike_verticalastigmatism;

static float *zernike_obliqueastigmatism;
static long fpi_zernike_obliqueastigmatism;

static CLICMDARGDEF farg[] =
{
    {
        CLIARG_IMG,
        ".DMin",
        "Actuators stream",
        "",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &DMin_streamname,
        &fpi_DMin_streamname
    },
    {
        CLIARG_IMG,
        ".TTMin",
        "Tip-Tilt stream",
        "",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &TTMin_streamname,
        &fpi_TTMin_streamname
    },
    {
        CLIARG_FLOAT32,
        ".ttm_tip_offset",
        "Tip offset on TTM",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &ttm_tip_offset,
        &fpi_ttm_tip_offset
    },
    {
        CLIARG_FLOAT32,
        ".ttm_tilt_offset",
        "Tilt offset on TTM",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &ttm_tilt_offset,
        &fpi_ttm_tilt_offset
    },
    {
        CLIARG_FLOAT32,
        ".zernike_piston",
        "Piston",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &zernike_piston,
        &fpi_zernike_piston
    },
    {
        CLIARG_FLOAT32,
        ".zernike_tip",
        "Tip (on DM)",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &zernike_tip,
        &fpi_zernike_tip
    },
    {
        CLIARG_FLOAT32,
        ".zernike_tilt",
        "Tilt (on DM)",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &zernike_tilt,
        &fpi_zernike_tilt
    },
    {
        CLIARG_FLOAT32,
        ".zernike_defocus",
        "Defocus",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &zernike_defocus,
        &fpi_zernike_defocus
    },
    {
        CLIARG_FLOAT32,
        ".zernike_vert_astig",
        "Vertical Astigmatism",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &zernike_verticalastigmatism,
        &fpi_zernike_verticalastigmatism
    },
    {
        CLIARG_FLOAT32,
        ".zernike_obli_astig",
        "Oblique Astigmatism",
        "0",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &zernike_obliqueastigmatism,
        &fpi_zernike_obliqueastigmatism
    }
};


static CLICMDDATA CLIcmddata =
{
    "display",
    "Connect to deformable mirror and display",
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
        data.fpsptr->parray[fpi_ttm_tip_offset].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_ttm_tip_offset].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_ttm_tip_offset].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_ttm_tip_offset].val.f32[1] = -2.5; // min
        data.fpsptr->parray[fpi_ttm_tip_offset].val.f32[2] = 2.5; // max

        data.fpsptr->parray[fpi_ttm_tilt_offset].fpflag |= FPFLAG_MINLIMIT;
        data.fpsptr->parray[fpi_ttm_tilt_offset].fpflag |= FPFLAG_MAXLIMIT;
        data.fpsptr->parray[fpi_ttm_tilt_offset].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_ttm_tilt_offset].val.f32[1] = -2.5; // min
        data.fpsptr->parray[fpi_ttm_tilt_offset].val.f32[2] = 2.5; // max

        data.fpsptr->parray[fpi_zernike_piston].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_zernike_tip].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_zernike_tilt].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_zernike_defocus].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_zernike_verticalastigmatism].fpflag |= FPFLAG_WRITERUN;
        data.fpsptr->parray[fpi_zernike_obliqueastigmatism].fpflag |= FPFLAG_WRITERUN;
    }

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
    value += (*zernike_tip)*compute_zernike(i, 1, params);
    value += (*zernike_tilt)*compute_zernike(i, 2, params);
    value += (*zernike_obliqueastigmatism)*compute_zernike(i, 3, params);
    value += (*zernike_defocus)*compute_zernike(i, 4, params);
    value += (*zernike_verticalastigmatism)*compute_zernike(i, 5, params);

    return value;
}

static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();

    INSERT_STD_PROCINFO_COMPUTEFUNC_INIT

    char DMzernike_streamname[FUNCTION_PARAMETER_STRMAXLEN + 3];
    sprintf(DMzernike_streamname, "%s11", DMin_streamname);

    char TTMoffset_streamname[FUNCTION_PARAMETER_STRMAXLEN + 3];
    sprintf(TTMoffset_streamname, "%s00", TTMin_streamname);

    /********** Variables **********/

    int error = NO_ERR;
    DM dm = {};
    uint32_t *map_lut;
    double *dm_array;
    int k;
    ZERNIKE_PARAMS zernike_params;

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

    int ii;
    long ttmOffsetCnt_old = 0;
    long zernikeCnt_old = 0;
    double zernike = 0;

    INSERT_STD_PROCINFO_COMPUTEFUNC_LOOPSTART

            for(ii=0; ii<10; ii++){
                dm_array[ii] = (data.image[IDDMin].array.F[ii+1] + *zernike_piston)/3.5+0.5;
                if (dm_array[ii] > 1) {
					dm_array[ii] = 1;
				}
                else if (dm_array[ii] < 0) {
					dm_array[ii] = 0;
				}
			}

            for(; ii<130; ii++){
                dm_array[ii] = (data.image[IDDMin].array.F[ii+2] + *zernike_piston)/3.5+0.5;
                if (dm_array[ii] > 1) {
					dm_array[ii] = 1;
				}
                else if (dm_array[ii]) < 0 {
					dm_array[ii] = 0;
				}
			}

            for(; ii<140; ii++){
                dm_array[ii] = (data.image[IDDMin].array.F[ii+3] + *zernike_piston)/3.5+0.5;
                if (dm_array[ii] > 1) {
					dm_array[ii] = 1;
				}
                else if (dm_array[ii] < 0) {
					dm_array[ii] = 0;
				}
			}

            dm_array[155] = data.image[IDTTMin].array.F[0]/5.0+0.5;
            if (dm_array[155] > 1) {
				dm_array[155] = 1;
			}
			else if (dm_array[155] < 0) {
				dm_array[155] = 0;
			}

            dm_array[156] = data.image[IDTTMin].array.F[1]/5.0+0.5;
            if (dm_array[156] > 1) {
				dm_array[156] = 1;
			}
			else if (dm_array[156] < 0) {
				dm_array[156] = 0;
			}

            error = BMCSetArray(&dm, dm_array, map_lut);
            if (error) {
                printf("\nThe error %d happened while setting array for deformable mirror\n", error);
                return error;
            }

            /***** Output offset in TTM stream *****/

            long ttmOffsetCnt_sum = data.fpsptr->parray[fpi_ttm_tip_offset].cnt0
                                  + data.fpsptr->parray[fpi_ttm_tilt_offset].cnt0;

            if(ttmOffsetCnt_old != ttmOffsetCnt_sum) {
                ttmOffsetCnt_old = ttmOffsetCnt_sum;

                data.image[IDTTMoffset].md[0].write = 1;

                data.image[IDTTMoffset].array.F[0] = *ttm_tip_offset;
                data.image[IDTTMoffset].array.F[1] = *ttm_tilt_offset;

                processinfo_update_output_stream(processinfo, IDTTMoffset);
            }

            /***** Output zernike in DM stream *****/

            long zernikeCnt_sum = data.fpsptr->parray[fpi_zernike_tip].cnt0
                                + data.fpsptr->parray[fpi_zernike_tilt].cnt0
                                + data.fpsptr->parray[fpi_zernike_defocus].cnt0
                                + data.fpsptr->parray[fpi_zernike_verticalastigmatism].cnt0
                                + data.fpsptr->parray[fpi_zernike_obliqueastigmatism].cnt0;

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

    INSERT_STD_PROCINFO_COMPUTEFUNC_END

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

    DEBUG_TRACE_FEXIT();

    return RETURN_SUCCESS;
}

INSERT_STD_FPSCLIfunctions

// Register function in CLI
errno_t CLIADDCMD_KalAO_BMC__display()
{
    CLIcmddata.FPS_customCONFsetup = customCONFsetup;
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}

