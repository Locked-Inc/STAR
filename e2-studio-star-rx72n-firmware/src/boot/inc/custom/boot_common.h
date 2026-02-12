/***********************************************************************************************************************
* STAR Project - Boot Support Header
*
* This file provides minimal definitions needed by boot files moved from smc_gen/.
* Provides INTERNAL_NOT_USED macro needed by SMC-generated boot files.
***********************************************************************************************************************/

#pragma once

/***********************************************************************************************************************
* Macro to suppress "unused parameter" warnings for function parameters that must exist for API compatibility
* but are not used in the implementation.
***********************************************************************************************************************/
#define INTERNAL_NOT_USED(p) ((void)(p))
