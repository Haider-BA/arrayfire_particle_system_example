#pragma once
#include <arrayfire.h>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern const float explosion_speed;
extern const int num_explosion_particles;
extern const int num_spark_particles;
enum particle_system_t { SPARK_SYSTEM, EXPLOSION_SYSTEM };

/*
 * function converting x,y positions into image offsets
 * used to determine which pixels to draw
 */
static af::array ids_from_pos(int img_width, int img_height, std::vector<af::array> &pos) {
    af::array in_bounds = af::where(pos[1] > 0 && pos[1] < img_height && pos[0] > 0 && pos[0] < img_width);
    if(in_bounds.elements() > 0)
        return ((pos[0].as(u32) * img_height) + pos[1].as(u32))(in_bounds);
    return af::array();
}

/*
 * Abstract Class representing a particle system
 * provides simple update mechanisms for common particle features
 */
class particle_system
{
public:
    //particle data
    static const int dims = 2;
    std::vector<af::array> pos;
    std::vector<af::array> vel;
    std::vector<af::array> accel;
    af::array lifetime;
    af::array is_active;

    //particle system data
    af::array texture;
    int num_particles;
    int particle_size;
    int system_type;
    float system_pos[2];

    //initialize particles' af::arrays in constructor
    particle_system(int num_particles_) {
        num_particles = num_particles_;
        system_pos[0] = 0; system_pos[1] = 0; system_pos[2] = 0;

        for(int dim=0; dim<dims; ++dim) {
            pos.push_back(af::constant(0, num_particles));
            vel.push_back(af::constant(0, num_particles));
            accel.push_back(af::constant(0, num_particles));
        }
        lifetime = af::constant(0, num_particles);
        is_active = af::constant(0, num_particles);
    }

    virtual ~particle_system() {}

    //offsets the whole particle system
    void move_system(float x, float y) {
        system_pos[0] += x;
        system_pos[1] += y;
        pos[0] += x;
        pos[1] += y;
    }

    // updates positions, velocities, and lifetime of particles in system
    virtual bool update() {
        if(af::where(is_active > 0).elements() > 0 ) {
            for(int i=0; i<pos.size(); ++i) {
                pos[i](is_active>0) += vel[i](is_active>0);
                vel[i](is_active>0) += accel[i](is_active>0);
                lifetime += 1;
            }
            return true;
        }
        return false;
    }

    virtual void render(af::array &image) = 0;
};

class spark_system : public particle_system
{
public:
    bool keep_alive;

    spark_system(int num_particles_) : particle_system::particle_system(num_particles_)
    {
        is_active = af::constant(1, num_particles);
        keep_alive = true;
        particle_size = 2;

        pos[0]   = af::constant(system_pos[0], num_particles);
        pos[1]   = af::constant(system_pos[1], num_particles);
        vel[0]   = af::randn(num_particles) * 0.05f;
        vel[1]   = af::randu(num_particles);
        accel[0] = af::constant(0.f, num_particles);
        accel[1] = af::constant(0.1f, num_particles);
        lifetime = af::randu(num_particles) * 50;

        system_type = SPARK_SYSTEM;
    }

    bool update() {
        lifetime -= 1;
        if(af::where(lifetime < 0).elements() > 0 ) {
            is_active(lifetime < 0) = 0;
        }
        if(af::where(is_active <= 0).elements() > 0 ) {
            pos[0](is_active <= 0) = system_pos[0];
            pos[1](is_active <= 0) = system_pos[1];
            int randint = std::rand();
            vel[0](is_active <= 0) = 0.1f * (float)(randint - RAND_MAX/2)/(RAND_MAX/2);
            vel[1](is_active <= 0) = 0;

            if(keep_alive) {
                lifetime(is_active <= 0) = 20 + randint % 50;
                is_active(is_active <= 0) = 1;
            }
        }

        af::array ids = (is_active > 0);
        if(af::where(ids).elements() > 0 ) {
            for(int i=0; i<pos.size(); ++i) {
                pos[i](ids) += vel[i](ids);
                vel[i](ids) += accel[i](ids);
            }
            return true;
        }
        return false;
    }

    void deactivate_system() {
        keep_alive = false;
    }

    void render(af::array &image) {
        int image_width = image.dims(0);
        int image_height = image.dims(1);
        af::array ids = ids_from_pos(image_width, image_height, pos);
        if(af::where(is_active > 0).elements() > 0 ) {
            if(ids.elements() > 0) {
                ids = ids(is_active > 0);
                for(int j=0; j<particle_size; ++j) {
                    for(int i=0; i<particle_size; ++i) {
                        image(ids + i + j * image_height) += 25 / lifetime(is_active>0);
                    }
                }
            }
        }
    }
};

class explosion_system : public particle_system
{
public:
    float expl_speed;
    explosion_system(int num_particles_, float expl_speed_) : particle_system::particle_system(num_particles_)
    {
        particle_size = 5;
        expl_speed = expl_speed_;

        is_active = af::constant(1, num_particles);

        pos[0]   = af::constant(system_pos[0], num_particles);
        pos[1]   = af::constant(system_pos[1], num_particles);
        vel[0]   = (af::randn(num_particles)) * expl_speed;
        vel[1]   = (af::randn(num_particles)) * expl_speed;
        accel[0] = (af::constant(0.f, num_particles));
        accel[1] = (af::constant(0.09f, num_particles));

        system_type = EXPLOSION_SYSTEM;
    }

    bool update() {
        //kill off particles that are older than 250 frames
        if(af::where(lifetime > 250).elements() > 0 ) {
            is_active(lifetime > 250) = 0;
        }
        //limit top speed of particles
        if(af::where(vel[1] > 3.f).elements() > 0) {
            vel[0](vel[1] > 3.f) *= 0.9f;
            vel[1](vel[1] > 3.f) *= 0.9f;
        }
        return particle_system::update();
    }

    //render active particles while fading w/respect to lifetime
    void render(af::array &image) {
        int image_width = image.dims(0);
        int image_height = image.dims(1);
        af::array ids = ids_from_pos(image_width, image_height, pos);
        if(af::where(is_active > 0).elements() > 0 ) {
            if(ids.elements() > 0) {
                ids = ids(is_active>0);
                for(int j=0; j<particle_size; ++j)
                    for(int i=0; i<particle_size; ++i)
                        image(ids + i + j * image_height) += 5/lifetime(is_active>0);
            }
        }
    }
};

/*
 * Particle Manager Class
 * keeps track of all living particle systems
 */

class particle_manager
{
public:
    std::vector<particle_system*> systems;

    particle_manager() {

    }

    void addSystem(particle_system *sys) {
        systems.push_back(sys);
    }

    void update() {
        for(auto ps : systems) {
            if(ps->system_type == SPARK_SYSTEM) {
                if(ps->system_pos[1] < 200)  {
                    if(((spark_system*)ps)->keep_alive == true){
                        ((spark_system*)ps)->deactivate_system();
                        explosion_system *explosion = new explosion_system(num_explosion_particles, explosion_speed);
                        explosion->move_system(ps->system_pos[0], ps->system_pos[1]);
                        addSystem(explosion);
                    }
                } else {
                    ps->move_system(0, -5);
                }
            }
        }
        systems.erase(std::remove_if(systems.begin(), systems.end(),
                    [](particle_system* ps){bool alive = ps->update(); if(!alive) delete ps; return !alive;}), systems.end());
    }

    void render(af::array &image) {
        for(auto sys : systems)
            sys->render(image);
    }
};

