/**
 * @file radar_bx.c
 * @brief Radar behaviour
 */
#include "radar_bx.h"
#include <stdint.h>
#include <stdbool.h>

static bool radar_bx_init(void);
static void radar_process(void);

void radar_bx_start(void)
{
    bool success;
    success = radar_bx_init();

    for(;;)
    {
        radar_process();
    }
}

static bool radar_bx_init(void)
{
    return true;
}


static void radar_process(void)
{
    return;
}


