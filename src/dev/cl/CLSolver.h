/**
 * @file CLSolver.h
 * @brief The CLSolver class and GPU solver routines.
 * @date August 5, 2012
 * @author Robert Sloan, MIT, Course 8 (rsloan@mit.edu)
 */

#ifndef CLSOLVER_H_
#define CLSOLVER_H_

#include "../../Solver.h"
#include "CLQuery.h"
#include "framework/CLInstance.hpp"
#include <vector>

#include <sm_13_double_functions.h>
#include <sm_20_atomic_functions.h>
#include "clone.h"


/** Indexing macro for the scalar flux in each flat source region and 
 *  in each energy group */
#define scalar_flux(tid,e) (scalar_flux[(tid)*(*num_groups) + (e)])

/** Indexing macro for the total source in each flat source region and
 *  in each energy group */
#define source(tid,e) (source[(tid)*(*num_groups) + (e)])

/** Indexing macro for the total source from the previous source iteration
 *  in each flat source region and each energy group */
#define old_source(tid,e) (old_source[(tid)*(*num_groups) + (e)])

/** Indexing macro for the total source divided by the total cross-section,
 *  \f$ \frac{Q}{\Sigma_t} \f$, in each flat source region and each energy
 *  group */
#define reduced_source(tid,e) (reduced_source[(tid)*(*num_groups) + (e)])

/** Indexing macro for the azimuthal and polar weights */
#define polar_weights(i,p) (polar_weights[(i)*(*num_polar) + (p)])

/** Indexing macro for the angular fluxes for each polar angle and energy
 *  group for a given track */ 
#define boundary_flux(tid,pe2) (boundary_flux[2*(tid)*(*polar_times_groups)+(pe2)])

/** The value of 4pi: \f$ 4\pi \f$ */
#define FOUR_PI 12.5663706143

/** The values of 1 divided by 4pi: \f$ \frac{1}{4\pi} \f$ */
#define ONE_OVER_FOUR_PI 0.0795774715

/** The maximum number of polar angles to reserve constant memory on GPU */
#define MAX_POLAR_ANGLES 3

/** The maximum number of azimuthal angles to reserve constant memory on GPU */
#define MAX_AZIM_ANGLES 256


/**
 * @class GPUSolver GPUSolver.h "openmoc/src/dev/gpu/GPUSolver.h"
 * @brief This a subclass of the Solver class for NVIDIA Graphics
 *        Processing Units (GPUs).
 * @details The source code for this class includes C++ coupled with
 *          compute intensive CUDA kernels for execution on the GPU.
 */
class CLSolver : public Solver {

private:

    CLInstance * inst;

    /**************************************************************************/
    /*                             Data on the host                           */
    /**************************************************************************/

    /** The number of threadblocks */
    int _B;
    
    /** The number of threads per threadblock */
    int _T;


    /**************************************************************************/
    /*                           Data on the device                           */
    /**************************************************************************/

    /** The flat source region material pointers index by FSR UID */
    int* _FSR_materials;

    /** A pointer to an array of the materials on the device */
    dev_material* _materials;

    /** A pointer to the array of tracks on the device */
    dev_track* _dev_tracks;

    /** An array of the cumulative number of tracks for each azimuthal angle */
    int* _track_index_offsets;

    /** A pointer to the Thrust vector of absorption rates in each FSR */
    FP_PRECISION* _tot_absorption;

    /** A pointer to the Thrust vector of fission rates in each FSR */
    FP_PRECISION* _tot_fission;

    /** A pointer to the Thrust vector of leakages for each track */
    FP_PRECISION* _leakage;

    /** Thrust vector of fission sources in each FSR */
    compute::vector<FP_PRECISION> _fission_sources_vec;

    /** Thrust vector of fission rates in each FSR */
    compute::vector<FP_PRECISION> _tot_fission_vec;

    /** Thrust vector of absorption rates in each FSR */
    compute::vector<FP_PRECISION> _tot_absorption_vec;

    /** Thrust vector of source residuals in each FSR */
    compute::vector<FP_PRECISION> _source_residuals_vec;

    /** Thrust vector of leakages for each track */
    compute::vector<FP_PRECISION> _leakage_vec;

    void initializePolarQuadrature();
    void initializeFSRs();
    void initializeMaterials();
    void initializeTracks();
    void initializeFluxArrays();
    void initializeSourceArrays();
    void initializeThrustVectors();
    void precomputePrefactors();

    void zeroTrackFluxes();
    void flattenFSRFluxes(FP_PRECISION value);
    void flattenFSRSources(FP_PRECISION value);
    void normalizeFluxes();
    FP_PRECISION computeFSRSources();
    void addSourceToScalarFlux();
    void computeKeff();
    void transportSweep();

public:

    CLSolver(Geometry* geometry=NULL, TrackGenerator* track_generator=NULL);
    virtual ~CLSolver();
    
    FP_PRECISION getFSRScalarFlux(int fsr_id, int energy_group);
    FP_PRECISION* getFSRScalarFluxes();

    void setNumThreadBlocks(int num_blocks);
    void setNumThreadsPerBlock(int num_threads);
    void setGeometry(Geometry* geometry);
    void setTrackGenerator(TrackGenerator* track_generator);

    int computeScalarTrackIndex(int i, int j);

    void computeFSRFissionRates(double* fission_rates, int num_FSRs);
};


#endif /* CLSOLVER_H_ */
