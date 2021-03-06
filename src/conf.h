/*
	conf
	Written by Mike Perron (2013)

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Functions for handling configuration for Kraknet.
*/
#ifndef KRAKNET_CONF_H
#define KRAKNET_CONF_H
#include "generic.h"

extern void change_root(const char *path);

extern void set_path(char **dest, char *src);
extern int set_env_from_conf();

extern int change_log_owner(uid_t uid, gid_t gid);
extern void calibrate_path();

#endif
