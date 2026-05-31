#ifndef BOOT_PANEL_SPLASH_H
#define BOOT_PANEL_SPLASH_H

#include "display_validation.h"

void boot_panel_splash_init(void);
void boot_panel_splash_show(const char *title, const char *subtitle);

#ifdef KEYSWITCH_HOST_TEST
struct BootPanelRenderSnapshot {
	uint8_t framebuffer[128U * 8U];
	DisplayValidationReport validation;
};

void boot_panel_splash_host_render(const char *title, const char *subtitle, BootPanelRenderSnapshot *snapshot);
#endif

#endif