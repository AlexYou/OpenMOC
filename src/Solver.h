/**
 * @file Solver.h
 * @brief The Solver class.
 * @date February 7, 2012
 * @author William Boyd, MIT, Course 22 (wboyd@mit.edu)
 */

#ifndef SOLVER_H_
#define SOLVER_H_

#define NUM_SECTIONS 1

#ifdef __cplusplus
#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <string>
#include "PapiProfiler.h"
#include "Timer.h"
#include "Quadrature.h"
#include "TrackGenerator.h"
#include "pairwise_sum.h"
#endif

#define NUM_CODE_SECTIONS 1

#define _scalar_flux(r,e) (_scalar_flux[(r)*_num_groups + (e)])

#define _source(r,e) (_source[(r)*_num_groups + (e)])

#define _old_source(r,e) (_old_source[(r)*_num_groups + (e)])

#define _ratios(r,e) (_ratios[(r)*_num_groups + (e)])

#define _polar_weights(i,p) (_polar_weights[(i)*_num_polar + (p)])

#define _boundary_flux(i,j,p,e) (_boundary_flux[(i)*2*_polar_times_groups + (j)*_polar_times_groups + (p)*_num_groups + (e)])

#define _boundary_leakage(i,pe2) (_boundary_leakage[2*(i)*_polar_times_groups+(pe2)])

#define _fission_source(r,e) (_fission_source[(r)*_num_groups + (e)])

#define source_residuals(r,e) (source_residuals[(r)*_num_groups + (e)])

//#define prefactorindex(tau) (int(tau * _inverse_prefactor_spacing) * _two_times_num_polar)

//#define prefactor(index,p,tau) (1. - (_prefactor_array[index+2 * p] * tau + _prefactor_array[index + 2 * p +1]))

/** The value of 4pi: \f$ 4\pi \f$ */
#define FOUR_PI 12.5663706143

/** The values of 1 divided by 4pi: \f$ \frac{1}{4\pi} \f$ */
#define ONE_OVER_FOUR_PI 0.0795774715

/**
 * @class Solver Solver.h "openmoc/src/host/Solver.h"
 * @brief This is an abstract base class from which different types of Solvers subclass for
 *        different architectures or using different algorithms.
 */
class Solver {

protected:

    /******* PAPI *******/
    PapiProfiler *_papiProfiler;

    /********************/

    /** The number of azimuthal angles */
    int _num_azim;

    /** The number of energy groups */
    int _num_groups;

    /** The number of flat source regions */
    int _num_FSRs;

    /** The flat source region "volumes" (ie, areas) index by FSR UID */
    FP_PRECISION* _FSR_volumes;

    /** The flat source region material pointers index by FSR UID */
    Material** _FSR_materials;

    /** A pointer to a trackgenerator which contains tracks */
    TrackGenerator* _track_generator;

    /** A pointer to a geometry with initialized flat source region maps */
    Geometry* _geometry;

    /** The number of materials */
    int _num_materials;

    /** A pointer to a polar quadrature */
    Quadrature* _quad;

    /** The number of polar angles */
    int _num_polar;

    /** Twice the number of polar angles */
    int _two_times_num_polar;

    /** The number of polar angles times energy groups */
    int _polar_times_groups;

    /** The type of polar quadrature (TABUCHI or LEONARD) */
    quadratureType _quadrature_type;

    /** A pointer to the 2D ragged array of tracks */
    Track** _tracks;

    /** A pointer to an array with the number of tracks per azimuthal angle */
    int* _num_tracks;

    /** The total number of tracks */
    int _tot_num_tracks;

    /** The weights for each azimuthal angle */
    double* _azim_weights;

    /** The weights for each polar angle in the polar angle quadrature */
    FP_PRECISION* _polar_weights;

    /** The angular fluxes for each track for all energy groups, polar angles,
     *  and azimuthal angles. This array stores the boundary fluxes for a
     *  a track along both "forward" and "reverse" directions. */
    FP_PRECISION* _boundary_flux;

    /** The angular leakages for each track for all energy groups, polar angles,
     *  and azimuthal angles. This array stores the weighted outgoing fluxes 
     *  for a track along both "forward" and "reverse" directions. */
    FP_PRECISION* _boundary_leakage;

    /* Flat source regions */
    /** The scalar flux for each energy group in each flat source region */
    FP_PRECISION* _scalar_flux;

    /** The fission source in each energy group in each flat source region */
    FP_PRECISION* _fission_source;

    /** The source in each energy group in each flat source region */
    FP_PRECISION* _source;

    /** The source in each energy group in each flat source region from the 
     *  previous iteration */
    FP_PRECISION* _old_source;

    /** Pre-computed Ratio of source / sigma_t for each energy group in each
     *  flat source region */
    FP_PRECISION* _ratios;

    /** The normalized power in each flat source region */
    FP_PRECISION* _FSRs_to_powers;

    /** The normalized power corresponding to the pin which each flat source
     * region is within (0 for all moderator cells) */
    FP_PRECISION* _FSRs_to_pin_powers;

    /** The current iteration's approximation to k-effective */
    FP_PRECISION _k_eff; 

    /** The total leakage across vacuum boundaries */
    FP_PRECISION _leakage;

    /** The number of transport sweeps to convergence */
    int _num_iterations;

    /** Whether or not the Solver has converged the source */
    bool _converged_source;

    /** The tolerance for converging the source */
    FP_PRECISION _source_convergence_thresh;

    /* Exponential pre-factor hash table */
    /** The hashtable of exponential prefactors from the transport equation */
    FP_PRECISION* _prefactor_array;

    /** The size of the exponential prefactor array */
    int _prefactor_array_size;

    /** The maximum index of the exponential prefactor array */
    int _prefactor_max_index;

    /** The spacing for the exponential prefactor array */
    FP_PRECISION _prefactor_spacing;

    /** The inverse spacing for the exponential prefactor array */
    FP_PRECISION _inverse_prefactor_spacing;

    /** A timer to record timing data for a simulation */
    Timer* _timer;

    virtual void initializePolarQuadrature() =0;
    virtual void initializeFSRs() =0;
    virtual void initializeFluxArrays() =0;
    virtual void initializeSourceArrays() =0;
    virtual void initializePowerArrays() =0;
    virtual void precomputePrefactors() =0;
    virtual void checkTrackSpacing();

    virtual void zeroTrackFluxes() =0;
    virtual void flattenFSRFluxes(FP_PRECISION value) =0;
    virtual void flattenFSRSources(FP_PRECISION value) =0;
    virtual void normalizeFluxes() =0;
    virtual FP_PRECISION computeFSRSources() =0;
    virtual void computeKeff() =0;
    virtual void addSourceToScalarFlux() =0;
    virtual void transportSweep() =0;

    void clearTimerSplits();

public:
    Solver(Geometry* geom=NULL, TrackGenerator* track_generator=NULL);
    virtual ~Solver();

    Geometry* getGeometry();
    TrackGenerator* getTrackGenerator();
    int getNumPolarAngles();
    quadratureType getPolarQuadratureType();
    int getNumIterations();
    FP_PRECISION getSourceConvergenceThreshold();
    virtual FP_PRECISION getFSRScalarFlux(int fsr_id, int energy_group) =0;
    virtual FP_PRECISION* getFSRScalarFluxes() =0;
    virtual FP_PRECISION* getFSRPowers() =0;
    virtual FP_PRECISION* getFSRPinPowers() =0;

    virtual void setGeometry(Geometry* geometry);
    virtual void setTrackGenerator(TrackGenerator* track_generator);
    virtual void setPolarQuadratureType(quadratureType quadrature_type);
    virtual void setNumPolarAngles(int num_polar);
    virtual void setSourceConvergenceThreshold(FP_PRECISION source_thresh);

    virtual FP_PRECISION convergeSource(int max_iterations);
    virtual void computePinPowers() =0;

    /*** PapiProfiler wrappers ***/

    virtual int addPapiEvent(char *event);
    virtual int clearPapiEvents();
    virtual void printPapiEventCounts(int reduce);

    /*****************************/


    void printTimerReport();
};


#endif /* SOLVER_H_ */
