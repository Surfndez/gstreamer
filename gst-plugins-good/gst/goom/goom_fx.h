/* Goom Project
 * Copyright (C) <2003> iOS-Software
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef _GOOM_FX_H
#define _GOOM_FX_H

#include "goom_visual_fx.h"
#include "goom_plugin_info.h"

VisualFX convolve_create (void);
VisualFX flying_star_create (void);

void zoom_filter_c(int sizeX, int sizeY, Pixel *src, Pixel *dest, int *brutS, int *brutD, int buffratio, int precalCoef[16][16]);

#endif
