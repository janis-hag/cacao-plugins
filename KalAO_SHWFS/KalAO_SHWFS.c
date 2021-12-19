#define _GNU_SOURCE

/* ================================================================== */
/* ================================================================== */
/*  MODULE INFO                                                       */
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
/*  HEADER FILES                                                      */
/* ================================================================== */
/* ================================================================== */

#include "CommandLineInterface/CLIcore.h"

//
// Forward declarations are required to connect CLI calls to functions
// If functions are in separate .c files, include here the corresponding .h files
//
#include "process.h"

/* ================================================================== */
/* ================================================================== */
/*  INITIALIZE LIBRARY                                                */
/* ================================================================== */
/* ================================================================== */

// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings
//
INIT_MODULE_LIB(KalAO_SHWFS)

/**
 * @brief Initialize module CLI
 *
 * CLI entries are registered: CLI call names are connected to CLI functions.\n
 * Any other initialization is performed\n
 *
 */
static errno_t init_module_CLI()
{
    CLIADDCMD_KalAO_SHWFS__process();

    return RETURN_SUCCESS;
}

