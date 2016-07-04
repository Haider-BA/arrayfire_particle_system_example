/*******************************************************
 * Copyright (c) 2016, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#include <arrayfire.h>
#include <iostream>
#include <cstdio>
#include <vector>
#include "particle_systems.h"

using namespace af;
using namespace std;

static const int width = 768, height = 768;
const float explosion_speed = 1.1f;
const int num_explosion_particles = 400;
const int num_spark_particles = 50;

int main(int argc, char *argv[])
{
    try {
        af::info();

        af::Window myWindow(width, height, "Particle Engine using ArrayFire");
        myWindow.setColorMap(AF_COLORMAP_HEAT);

        int frame_count = 0;

        af::array image = af::constant(0, width, height);

        particle_manager particles;

        while(!myWindow.close()) {
            //launch a new firework every 50 frames
            if(frame_count % 50 == 0) {
                spark_system *sparks = new spark_system(num_spark_particles);
                sparks->move_system(frame_count*2 % width, height);
                particles.addSystem(sparks);
            }

            //render particles to image using the image manager
            particles.render(image);
            //and draw using an arrayfire window
            myWindow.image(image);
            //clear the image
            image = af::constant(0, image.dims());

            //simulate particles
            particles.update();

            frame_count++;
        }
    } catch (af::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        throw;
    }

    return 0;
}
