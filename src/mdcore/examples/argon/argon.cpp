/*******************************************************************************
 * This file is part of mdcore.
 * Coypright (c) 2010 Pedro Gonnet (gonnet@maths.ox.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

// include some standard headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <float.h>
#include <pthread.h>
#include <time.h>
#include "cycle.h"
#include "mdcore_single.h"
#include "Mechanica.h"

/* MPI headers. */
#ifdef HAVE_MPI
#include <mpi.h>
#endif

/* FFTW3 headers. */
#ifdef HAVE_FFTW3
#include <complex.h>
#include <fftw3.h>
#endif



/* What to do if ENGINE_FLAGS was not defined? */
#ifndef ENGINE_FLAGS
#define ENGINE_FLAGS engine_flag_none
#endif
#ifndef CPU_TPS
#define CPU_TPS 2.67e+9
#endif



int main ( int argc , char *argv[] ) {

    Mx_Initialize(0);

    const double origin[3] = { 0.0 , 0.0 , 0.0 };
    // double dim[3] = { 20.0 , 20.0 , 20.0 };
    // int nr_parts = 121600;
    double dim[3] = { 10.0 , 10.0 , 10.0 };
    // int nr_parts = 20712;
    int nr_parts = 32680;
    // int nr_parts = 15200;
    double Temp = 100.0;
    // double Temp = 94.4;

    double x[3], vtot[3] = { 0.0 , 0.0 , 0.0 };
    double epot, ekin, v2, temp, cutoff = 1.0, cellwidth;
    // FPTYPE ee, eff;
    struct engine& e = _Engine;
    struct MxParticle pAr;
    struct MxPotential *pot_ArAr;
    // struct potential *pot_ee;
    int i, j, k, cid, pid, nr_runners = 1, nr_steps = 1000;
    int nx, ny, nz;
    double hx, hy, hz, w;
    double L[] = { cutoff , cutoff , cutoff };
    // struct cellpair cp;

    ticks tic, toc, toc_step, toc_temp;

    tic = getticks();

    // did the user supply a cutoff?
    if ( argc > 4 ) {
        cellwidth = atof( argv[4] );
        nr_parts *= ( cellwidth * cellwidth * cellwidth );
        for ( k = 0 ; k < 3 ; k++ ) {
            L[k] = cellwidth;
            dim[k] *= cellwidth * (1.0 + DBL_EPSILON);
        }
    }
    else
        cellwidth = 1.0;


    // initialize the engine
    printf("main: initializing the engine... ");
    printf("main: requesting origin = [ %f , %f , %f ].\n", origin[0], origin[1], origin[2] );
    printf("main: requesting dimensions = [ %f , %f , %f ].\n", dim[0], dim[1], dim[2] );
    printf("main: requesting cell size = [ %f , %f , %f ].\n", L[0], L[1], L[2] );
    printf("main: requesting cutoff = %22.16e.\n", cutoff);
    fflush(stdout);

    if ( engine_init( &e , origin , dim , L , cutoff , space_periodic_full , 2 , engine_flag_none ) != 0 ) {
        printf("main: engine_init failed with engine_err=%i.\n",engine_err);
        errs_dump(stdout);
        return 1;
    }

    printf("main: engine_init done.\n"); fflush(stdout);

    // set the interaction cutoff
    printf("main: cell dimensions = [ %i , %i , %i ].\n", e.s.cdim[0] , e.s.cdim[1] , e.s.cdim[2] );
    printf("main: cell size = [ %e , %e , %e ].\n" , e.s.h[0] , e.s.h[1] , e.s.h[2] );
    printf("main: cutoff set to %22.16e.\n", cutoff);
    printf("main: nr tasks: %i.\n",e.s.nr_tasks);

    /* mix-up the pair list just for kicks
    printf("main: shuffling the interaction pairs... "); fflush(stdout);
    srand(6178);
    for ( i = 0 ; i < e.s.nr_pairs ; i++ ) {
        j = rand() % e.s.nr_pairs;
        if ( i != j ) {
            cp = e.s.pairs[i];
            e.s.pairs[i] = e.s.pairs[j];
            e.s.pairs[j] = cp;
            }
        }
    printf("done.\n"); fflush(stdout); */


    // initialize the Ar-Ar potential
    if ( ( pot_ArAr = potential_create_LJ126( 0.275 , 1.0 , 9.5075e-06 , 6.1545e-03 , 1.0e-3 ) ) == NULL ) {
        printf("main: potential_create_LJ126 failed with potential_err=%i.\n",potential_err);
        errs_dump(stdout);
        return 1;
    }
    printf("main: constructed ArAr-potential with %i intervals.\n",pot_ArAr->n); fflush(stdout);


    /* register the particle types. */
    if ( ( pAr.typeId = engine_addtype( &e , 39.948 , 0.0 , "Ar" , "Ar" ) ) < 0 ) {
        printf("main: call to engine_addtype failed.\n");
        errs_dump(stdout);
        return 1;
    }

    // register these potentials.
    if ( engine_addpot( &e , pot_ArAr , pAr.typeId , pAr.typeId ) < 0 ){
        printf("main: call to engine_addpot failed.\n");
        errs_dump(stdout);
        return 1;
    }

    // set fields for all particles
    srand(6178);
    pAr.flags = PARTICLE_FLAG_NONE;
    for ( k = 0 ; k < 3 ; k++ ) {
        pAr.v[k] = 0.0;
        pAr.f[k] = 0.0;
    }
#ifdef VECTORIZE
    pAr.v[3] = 0.0; pAr.f[3] = 0.0; pAr.x[3] = 0.0;
#endif

    // create and add the particles
    printf("main: initializing particles... "); fflush(stdout);
    nx = ceil( pow( nr_parts , 1.0/3 ) ); hx = dim[0] / nx;
    ny = ceil( sqrt( ((double)nr_parts) / nx ) ); hy = dim[1] / ny;
    nz = ceil( ((double)nr_parts) / nx / ny ); hz = dim[2] / nz;
    for ( i = 0 ; i < nx ; i++ ) {
        x[0] = 0.05 + i * hx;
        for ( j = 0 ; j < ny ; j++ ) {
            x[1] = 0.05 + j * hy;
            for ( k = 0 ; k < nz && k + nz * ( j + ny * i ) < nr_parts ; k++ ) {
                pAr.id = k + nz * ( j + ny * i );
                x[2] = 0.05 + k * hz;
                pAr.v[0] = ((double)rand()) / RAND_MAX - 0.5;
                pAr.v[1] = ((double)rand()) / RAND_MAX - 0.5;
                pAr.v[2] = ((double)rand()) / RAND_MAX - 0.5;
                temp = 0.275 / sqrt( pAr.v[0]*pAr.v[0] + pAr.v[1]*pAr.v[1] + pAr.v[2]*pAr.v[2] );
                pAr.v[0] *= temp; pAr.v[1] *= temp; pAr.v[2] *= temp;
                vtot[0] += pAr.v[0]; vtot[1] += pAr.v[1]; vtot[2] += pAr.v[2];
                if ( engine_addpart( &e , &pAr , x, NULL ) != 0 ) {
                    printf("main: space_addpart failed with space_err=%i.\n",space_err);
                    errs_dump(stdout);
                    return 1;
                }
            }
        }
    }
    for ( cid = 0 ; cid < e.s.nr_cells ; cid++ )
        for ( pid = 0 ; pid < e.s.cells[cid].count ; pid++ )
            for ( k = 0 ; k < 3 ; k++ )
                e.s.cells[cid].parts[pid].v[k] -= vtot[k] / nr_parts;
    printf("done.\n"); fflush(stdout);
    printf("main: inserted %i particles.\n", e.s.nr_parts);


    // set the time and time-step by hand
    e.time = 0;
    if ( argc > 3 )
        e.dt = atof( argv[3] );
    else
        e.dt = 0.005;
    printf("main: dt set to %f fs.\n", e.dt*1000 );

    toc = getticks();

    printf("main: setup took %.3f ms.\n",(double)(toc-tic) * 1000 / CPU_TPS);

    // did the user specify a number of runners?
    if ( argc > 1 ) {
        nr_runners = atoi( argv[1] );
        //omp_set_num_threads( nr_runners );
    }

    // start the engine

    printf("main: starting engine with nr_runners: %i.\n",nr_runners);
    if ( engine_start( &e , nr_runners , nr_runners ) != 0 ) {
        printf("main: engine_start failed with engine_err=%i.\n",engine_err);
        errs_dump(stdout);
        return 1;
    }


    // did the user specify a number of steps?
    if ( argc > 2 )
        nr_steps = atoi( argv[2] );

    // do a few steps
    for ( i = 0 ; i < nr_steps ; i++ ) {

        // take a step
        tic = getticks();

        if ( engine_step( &e ) != 0 ) {
            printf("main: engine_step failed with engine_err=%i.\n",engine_err);
            errs_dump(stdout);
            return 1;
        }

        toc_step = getticks();

        /* Check virtual/local ids. */
        /* for ( cid = 0 ; cid < e.s.nr_cells ; cid++ )
            for ( pid = 0 ; pid < e.s.cells[cid].count ; pid++ )
                if ( e.s.cells[cid].parts[pid].id != e.s.cells[cid].parts[pid].vid )
                    printf( "main: inconsistent particle id/vid (%i/%i)!\n",
                        e.s.cells[cid].parts[pid].id, e.s.cells[cid].parts[pid].vid ); */

        /* Verify integrity of partlist. */
        /* for ( k = 0 ; k < nr_mols*3 ; k++ )
            if ( e.s.partlist[k]->id != k )
                printf( "main: inconsistent particle id/partlist (%i/%i)!\n", e.s.partlist[k]->id, k );
        fflush(stdout); */


        // get the total COM-velocities and ekin
        epot = e.s.epot; ekin = 0.0;
#pragma omp parallel for schedule(static,100), private(cid,pid,k,v2), reduction(+:epot,ekin)
        for ( cid = 0 ; cid < e.s.nr_cells ; cid++ ) {
            for ( pid = 0 ; pid < e.s.cells[cid].count ; pid++ ) {
                for ( v2 = 0.0 , k = 0 ; k < 3 ; k++ )
                    v2 += e.s.cells[cid].parts[pid].v[k] * e.s.cells[cid].parts[pid].v[k];
                ekin += 0.5 * 39.948 * v2;
            }
        }

        // compute the temperature and scaling
        temp = ekin / ( 1.5 * 6.022045E23 * 1.380662E-26 * nr_parts );
        w = sqrt( 1.0 + 0.1 * ( Temp / temp - 1.0 ) );

        // scale the velocities
        if ( i < 10000 ) {
#pragma omp parallel for schedule(static,100), private(cid,pid,k), reduction(+:epot,ekin)
            for ( cid = 0 ; cid < e.s.nr_cells ; cid++ ) {
                for ( pid = 0 ; pid < e.s.cells[cid].count ; pid++ ) {
                    for ( k = 0 ; k < 3 ; k++ )
                        e.s.cells[cid].parts[pid].v[k] *= w;
                }
            }
        }

        toc_temp = getticks();

        printf("%i %e %e %e %i %i %.3f %.3f %.3f ms\n",
                e.time,epot,ekin,temp,e.s.nr_swaps,e.s.nr_stalls,
                (double)(toc_temp-tic) * 1000 / CPU_TPS,
                (double)(toc_step-tic) * 1000 / CPU_TPS,
                (double)(toc_temp-toc_step) * 1000 / CPU_TPS);
        fflush(stdout);

        // print some particle data
        // printf("main: part 13322 is at [ %e , %e , %e ].\n",
        //     e.s.partlist[13322]->x[0], e.s.partlist[13322]->x[1], e.s.partlist[13322]->x[2]);

    }

    // dump the particle positions, just for the heck of it
    // for ( cid = 0 ; cid < e.s.nr_cells ; cid++ )
    //     for ( pid = 0 ; pid < e.s.cells[cid].count ; pid++ ) {
    //         for ( k = 0 ; k < 3 ; k++ )
    //             x[k] = e.s.cells[cid].origin[k] + e.s.cells[cid].parts[pid].x[k];
    //         printf("%i %e %e %e\n",e.s.cells[cid].parts[pid].id,x[0],x[1],x[2]);
    //         }

    // clean break
    return 0;

}
