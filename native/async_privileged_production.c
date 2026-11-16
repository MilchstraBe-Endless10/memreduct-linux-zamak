/* SPDX-License-Identifier: GPL-3.0-or-later
 * Production wrapper: compile the fixed page-cache bridge without its
 * same-UID CI authorizer entry point.
 */
#define MEMREDUCT_PRODUCTION_BUILD 1
#include "async_privileged.c"
