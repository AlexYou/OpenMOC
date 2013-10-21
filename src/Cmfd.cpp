#include "Cmfd.h"

/**
 * @brief Constructor initializes boundaries and variables that describe
 *          the cmfd solver object.
 * @details The construcor initializes the many variables that describe
 *          the cmfd mesh, the solve method, and flux type. If solve
 *          method is diffusion theory, the fsr volumes, materials,
 *          and fluxes are initialized.
 * @param geometry pointer to the geometry
 * @param criteria convergence criteria on keff
 */
Cmfd::Cmfd(Geometry* geometry, double criteria) {

  /* Create objects */
  _geometry = geometry;
  _mesh = geometry->getMesh();
  _cells_x = _mesh->getCellsX();
  _cells_y = _mesh->getCellsY();
  _quad = new Quadrature(TABUCHI);
  _timer = new Timer();
  
  /* Boolean and Enum flags to toggle features */
  _solve_method = _mesh->getSolveType();
  _assemble_M = true;
  _flux_method = PRIMAL;
  
  /* Global variables used in solving Cmfd problem */
  _l2_norm = 1.0;
  _conv_criteria = criteria;

  /* General problem parameters */
  _num_groups = _mesh->getNumGroups();
  _num_fsrs = _mesh->getNumFSRs();
  
  /* Create Cmfd matrix objects */
  int petsc_err;
  petsc_err = createAMPhi();
  
  /* If solving diffusion problem, create arrays for FSR parameters */
  if (_solve_method == DIFFUSION){
    _FSR_volumes = (FP_PRECISION*)calloc(_num_fsrs, sizeof(FP_PRECISION));
    _FSR_materials = new Material*[_num_fsrs];
    _FSR_fluxes = (FP_PRECISION*)calloc(_num_fsrs*_num_groups, sizeof(FP_PRECISION));
    _mesh->initializeSurfaceCurrents();
    initializeFSRs();
  }

}


/**
 * @brief Destructor deletes arrays of A and M row insertion arrays
 */
Cmfd::~Cmfd() {

}


/**
 * @brief Create the loss matrix (A), fission matrix (A), 
 *        and flux vector (phi)
 * @return petsc_err petsc error flag
 */
int Cmfd::createAMPhi(){

  log_printf(DEBUG, "Creating AMPhi...");
  
  int petsc_err = 0;

#ifdef CMFD
  PetscInt size1 = _cells_x * _cells_y * _num_groups;
  PetscInt size2 = 4 + _num_groups;
  
  /* Create _A (loss) matrix */
  petsc_err = MatCreateSeqAIJ(PETSC_COMM_WORLD, size1, size1, size2, PETSC_NULL, &_A);

  /* Create _M (gain) matrix */
  size2 = size2 - 4;
  petsc_err = MatCreateSeqAIJ(PETSC_COMM_WORLD, size1, size1, size2, PETSC_NULL, &_M);
  
  /* Create flux, source, and residual vectors */
  petsc_err = VecCreateSeq(PETSC_COMM_WORLD, _cells_x*_cells_y*_num_groups, &_phi_new);
  petsc_err = VecCreateSeq(PETSC_COMM_WORLD, _cells_x*_cells_y*_num_groups, &_phi_old);
  petsc_err = VecCreateSeq(PETSC_COMM_WORLD, _cells_x*_cells_y*_num_groups, &_source_old);
  petsc_err = VecCreateSeq(PETSC_COMM_WORLD, _cells_x*_cells_y*_num_groups, &_sold);
  petsc_err = VecCreateSeq(PETSC_COMM_WORLD, _cells_x*_cells_y*_num_groups, &_snew);
  petsc_err = VecCreateSeq(PETSC_COMM_WORLD, _cells_x*_cells_y*_num_groups, &_res);
  CHKERRQ(petsc_err);
#endif

  return petsc_err;
}



/**
 * @brief Create cross sections and fluxes for each cmfd cell by
 *        energy condensing and volume averaging cross sections from
 *        the MOC sweep.
 */
void Cmfd::computeXS(){

  log_printf(INFO, "computing cmfd cross sections...");

  /* split corner currents to side surfaces */
  if (_solve_method == MOC)
    _mesh->splitCorners();
  
  /* initialize variables for FSR properties*/
  double volume, flux, abs, tot, nu_fis, chi, dif_coef;
  double* scat;
  Material** materials = _mesh->getMaterials();
  double* fluxes = _mesh->getFluxes(PREVIOUS);
  
  /* initialize tallies for each parameter */
  double abs_tally, nu_fis_tally, dif_tally, rxn_tally, vol_tally, tot_tally;
  double scat_tally[_num_groups];
  
  /* interator to loop over fsrs in each mesh cell */
  std::vector<int>::iterator iter;
  
  /* create pointers to objects */
  Material* fsr_material;
  Material* cell_material;
  
  /* loop over mesh cells */
  for (int i = 0; i < _cells_x * _cells_y; i++){
    
    /* loop over energy groups */
    for (int e = 0; e < _num_groups; e++) {
      
      /* zero tallies for this group */
      abs_tally = 0;
      nu_fis_tally = 0;
      dif_tally = 0;
      rxn_tally = 0;
      vol_tally = 0;
      tot_tally = 0;
      
      /* zero each group to group scattering tally */
      for (int g = 0; g < _num_groups; g++){
	scat_tally[g] = 0;
      }
      
      /* loop over FSRs in mesh cell */
      for (iter = _mesh->getCellFSRs()->at(i).begin(); iter != _mesh->getCellFSRs()->at(i).end(); ++iter){
	
	/* Gets FSR volume, material, and cross sections */
	fsr_material = _FSR_materials[*iter];
	cell_material = materials[i];
	volume = _FSR_volumes[*iter];
	flux = _FSR_fluxes[(*iter)*_num_groups+e];
	abs = fsr_material->getSigmaA()[e];
	tot = fsr_material->getSigmaT()[e];
	scat = fsr_material->getSigmaS();
	dif_coef = fsr_material->getDifCoef()[e];

	/* if material has a diffusion coefficient, use it; otherwise
	 * estimate the diffusion coefficient with 1 / (3 * sigma_t) */
        if (fsr_material->getDifCoef()[e] > 1e-8){
	  dif_tally += fsr_material->getDifCoef()[e] * flux * volume;
	}
	else
	  dif_tally += flux * volume / (3.0 * tot);
	
	/* if material has a chi, use it; otherwise set to 0 */
	if (fsr_material->getChi()[e] > 1e-10)
	  chi = fsr_material->getChi()[e];
	else
	  chi = 0.0;
	
	/* if material has a nu_sig_f, use it; otherwise set to 0 */
	if (fsr_material->getNuSigmaF()[e] > 1e-8)
	  nu_fis = fsr_material->getNuSigmaF()[e];
	else
	  nu_fis = 0.0;
	
	/* increment tallies for this group */
	abs_tally += abs * flux * volume;
	tot_tally += tot * flux * volume;
	nu_fis_tally += nu_fis * flux * volume;
	rxn_tally += flux * volume;
	vol_tally += volume;
	for (int g = 0; g < _num_groups; g++)
	  scat_tally[g] += scat[e*_num_groups + g] * flux * volume;
	
	/* choose a chi for this group */
	if (chi >= cell_material->getChi()[e])
	  cell_material->setChiByGroup(chi,e);
      }
      
      /* set the mesh cell properties with the tallies */
      _mesh->setVolume(vol_tally, i);
      cell_material->setSigmaAByGroup(abs_tally / rxn_tally, e);
      cell_material->setSigmaTByGroup(tot_tally / rxn_tally, e);
      cell_material->setNuSigmaFByGroup(nu_fis_tally / rxn_tally, e);
      cell_material->setDifCoefByGroup(dif_tally / rxn_tally, e);
      fluxes[i*_num_groups+e] = rxn_tally / vol_tally;      

      log_printf(DEBUG, "cell: %i, group: %i, vol: %f, siga: %f, sigt: %f, nu_sigf: %f, dif_coef: %f, flux: %f", i, e, vol_tally, abs_tally / rxn_tally, 
		 tot_tally / rxn_tally, nu_fis_tally / rxn_tally, dif_tally / rxn_tally, rxn_tally / vol_tally);
      
      for (int g = 0; g < _num_groups; g++){
	cell_material->setSigmaSByGroup(scat_tally[g] / rxn_tally, e, g);
	log_printf(DEBUG, "scattering from %i to %i: %f", e, g, scat_tally[g] / rxn_tally);
      }
    }
  }
}


/**
 * @brief Compute the diffusion coefficients (d_dif - straight 
 *        diffusion coefficient, d_hat - surface diffusion coefficient, 
 *        and d_tilde - surface diffusion coefficient correction factor)
 *        for each mesh while ensuring neutron balance is achieved.
 */
void Cmfd::computeDs(){

  log_printf(INFO, "computing cmfd Ds...");
  
  /* initialize variables */
  double d = 0, d_next = 0, d_hat = 0, d_tilde = 0;
  double current = 0, flux = 0, flux_next = 0, f = 1, f_next = 1;
  double length, length_perpen, next_length_perpen;
  double sense;
  int next_surface;
  int cell, cell_next;

  Material** materials = _mesh->getMaterials();
  double* cell_flux = _mesh->getFluxes(PREVIOUS);
  double* lengths_y = _mesh->getLengthsY();
  double* lengths_x = _mesh->getLengthsX();
  double* currents = _mesh->getCurrents();
  
  /* loop over mesh cells in y direction */
  for (int y = 0; y < _cells_y; y++){
    
    /* loop over mesh cells in x direction */
    for (int x = 0; x < _cells_x; x++){
      
      cell = y*_cells_x+x;
      
      /* loop over surfaces in a cell */
      for (int surface = 0; surface < 4; surface++){
	
	/* loop over groups */
	for (int e = 0; e < _num_groups; e++){
	  
	  /* get diffusivity and flux for mesh cell */
	  d = materials[cell]->getDifCoef()[e];
	  flux = cell_flux[cell*_num_groups+e];
	  cell_next = _mesh->getCellNext(cell, surface);
	  
	  /* set sense of the surface */
	  if (surface == 0 || surface == 3)
	    sense = -1.0;
	  else
	    sense = 1.0;
	  
	  /* set the length of this surface and the
	   * perpendicular surface */
	  if (surface == 0 || surface== 2){
	    length = lengths_y[y];
	    length_perpen = lengths_x[x];
	  }
	  else if (surface == 1 || surface == 3){
	    length = lengths_x[x];
	    length_perpen = lengths_y[y];
	  }
	  
	  /* compute the optical thickness correction factor */
	  f = computeDiffCorrect(d, length_perpen);
	  
	  /* if surface is on a boundary, choose appropriate BCs */
	  if (cell_next == -1){
	    
	    current = sense * currents[cell*_num_groups*8 + surface*_num_groups + e];

	    /* REFLECTIVE BC */
	    if (_mesh->getBoundary(surface) == REFLECTIVE){ 

	      /* set d's */ 
	      d_hat = 0.0;
	      d_tilde = 0.0;
	    }
	    /* VACUUM BC */
	    else if (_mesh->getBoundary(surface) == VACUUM){	      
	      
	      /* set d's */
	      d_hat =  2 * d*f / length_perpen / (1 + 4 * d*f / length_perpen);

	      if (_mesh->getSolveType() == MOC)
		d_tilde = (sense * d_hat * flux - current / length) / flux;
	      else
		d_tilde = 0.0;
	    }
	    /* ZERO_FLUX BC */
	    else if (_mesh->getBoundary(surface) == ZERO_FLUX){

	      /* set d's */
	      d_hat = 2 * d*f / length_perpen;
	      d_tilde = 0.0;
	    }
	  }
	  /* if surface is an interface, use finite differencing */
	  else{
	    
	    /* set properties for cell next to surface */
	    if (surface == 0){
	      next_length_perpen = lengths_x[cell_next % _cells_x];
	      next_surface = 2;
	    }
	    else if (surface == 1){
	      next_length_perpen = _mesh->getLengthsY()[cell_next / _cells_x];
	      next_surface = 3;
	    }
	    else if (surface == 2){
	      next_length_perpen = _mesh->getLengthsX()[cell_next % _cells_x];
	      next_surface = 0;
	    }
	    else if (surface == 3){
	      next_length_perpen = _mesh->getLengthsY()[cell_next / _cells_x];
	      next_surface = 1;
	    }
	    
	    /* set diffusion coefficient and flux for neighboring cell */
	    d_next = materials[cell_next]->getDifCoef()[e];
	    flux_next = cell_flux[cell_next*_num_groups + e];
	    
	    /* get optical thickness correction term for meshCellNext */
	    f_next = computeDiffCorrect(d_next, next_length_perpen);

	    /* compute d_hat */
	    d_hat = 2.0 * d * f * d_next * f_next / (length_perpen * d * f + next_length_perpen * d_next*f_next);
	    
	    /* compute net current */
	    current = sense * currents[cell*_num_groups*8 + surface*_num_groups + e]
	      - sense * currents[cell_next*_num_groups*8 + next_surface*_num_groups + e];
	    
	    /* compute d_tilde */
	    if (_solve_method == MOC)
	      d_tilde = -(sense * d_hat * (flux_next - flux) + current  / length) / (flux_next + flux);
	    else
	      d_tilde = 0.0;

	    /* if the magnitude of d_tilde is greater than the magnitude of d_hat,
	     * select new values d_tilde and d_hat to ensure the course mesh equations
	     * are guaranteed to be diagonally dominant */
	    if (fabs(d_tilde) > fabs(d_hat)){
	      
	      if (sense == -1){
		/* d_tilde is positive */
		if (1 - fabs(d_tilde)/d_tilde < 1e-8){
		  d_hat   = - current/(2*flux*length);
		  d_tilde = - current/(2*flux*length);
		}
		/* if d_tilde is negative */
		else{
		  d_hat   = current/(2*flux_next*length);
		  d_tilde = - current/(2*flux_next*length);
		}
	      }
	      else{
		/* d_tilde is positive */
		if (1 - fabs(d_tilde)/d_tilde < 1e-8){
		  d_hat   = - current/(2*flux_next*length);
		  d_tilde = - current/(2*flux_next*length);
		}
		/* if d_tilde is negative */
		else{
		  d_hat   = current/(2*flux*length);
		  d_tilde = - current/(2*flux*length);
		}
	      }
	    }
	  }  
	  
	  /* perform underrelaxation on d_tilde */
	  d_tilde = materials[cell]->getDifTilde()[surface*_num_groups + e] * (1 - _mesh->getRelaxFactor()) + _mesh->getRelaxFactor() * d_tilde;

	  /* set d_hat and d_tilde */
	  materials[cell]->setDifHatByGroup(d_hat, e, surface);
	  materials[cell]->setDifTildeByGroup(d_tilde, e, surface);

	  log_printf(DEBUG, "cell: %i, group: %i, side: %i, flux: %f, current: %f, d: %f, dhat: %f, dtilde: %f", y*_cells_x + x, e, surface, flux, current, d, d_hat, d_tilde);
	  
	}
      }
    }
  }
}


/*
 * @brief CMFD solver that solves the diffusion problem
 * @return k-effective
 */
double Cmfd::computeKeff(){

  log_printf(INFO, "Running diffusion solver...");

  /* if solving diffusion problem, initialize timer */
  if (_solve_method == DIFFUSION)
    _timer->startTimer();
  
  /* initialize variables */
  int petsc_err = 0;

#ifdef CMFD
  int max_outer, iter = 0;
  PetscScalar sumold, sumnew, scale_val;
  PetscReal rtol = 1e-10;
  PetscReal atol = 1e-10;
  PetscReal eps = 0.0;
  Vec sold, snew, res;
  KSP ksp;

  /* compute the cross sections and surface 
   * diffusion coefficients */
  if (_solve_method == MOC)
    computeXS();
  
  computeDs();

  /* assemble matrices and flux vector */  
  petsc_err = MatZeroEntries(_A);
  
  if (_assemble_M)
    petsc_err = MatZeroEntries(_M);
    
  petsc_err = constructMatrices();
  CHKERRQ(petsc_err);

  /* assembly petsc objects */
  petsc_err = VecAssemblyBegin(_phi_old);
  petsc_err = VecAssemblyEnd(_phi_old);
  petsc_err = VecAssemblyBegin(_phi_new);
  petsc_err = VecAssemblyEnd(_phi_new);
  petsc_err = VecAssemblyBegin(_source_old);
  petsc_err = VecAssemblyEnd(_source_old);
  petsc_err = VecAssemblyBegin(_sold);
  petsc_err = VecAssemblyEnd(_sold);
  petsc_err = VecAssemblyBegin(_snew);
  petsc_err = VecAssemblyEnd(_snew);
  petsc_err = VecAssemblyBegin(_res);
  petsc_err = VecAssemblyEnd(_res);
  petsc_err = MatAssemblyBegin(_A, MAT_FINAL_ASSEMBLY);
  petsc_err = MatAssemblyEnd(_A, MAT_FINAL_ASSEMBLY);  

  if (_assemble_M){
    petsc_err = MatAssemblyBegin(_M, MAT_FINAL_ASSEMBLY);
    petsc_err = MatAssemblyEnd(_M, MAT_FINAL_ASSEMBLY);
  }
  CHKERRQ(petsc_err);
  
  /* create petsc ksp objects */
  petsc_err = KSPCreate(PETSC_COMM_WORLD, &ksp);
  petsc_err = KSPSetTolerances(ksp, rtol, atol, PETSC_DEFAULT, PETSC_DEFAULT);
  petsc_err = KSPSetType(ksp, KSPGMRES);
  petsc_err = KSPSetInitialGuessNonzero(ksp, PETSC_TRUE);
  petsc_err = KSPSetOperators(ksp, _A, _A, SAME_NONZERO_PATTERN);
  petsc_err = KSPSetUp(ksp);
  petsc_err = KSPSetFromOptions(ksp);
  CHKERRQ(petsc_err);  

  /* compute the normalize the initial source */
  petsc_err = MatMult(_M, _phi_old, _sold);
  petsc_err = VecSum(_sold, &sumold);
  scale_val = (_cells_x * _cells_y * _num_groups) / sumold;
  petsc_err = VecScale(_sold, scale_val);
  sumold = _cells_x * _cells_y * _num_groups;
  CHKERRQ(petsc_err);

  /* power iteration diffusion solver */
  for (iter = 0; iter < 1000; iter++){

    /* Solve phi = A^-1 * old_source */
    petsc_err = KSPSolve(ksp, _sold, _phi_new);

    /* compute the new source */
    petsc_err = MatMult(_M, _phi_new, _snew);
    petsc_err = VecSum(_snew, &sumnew);
    CHKERRQ(petsc_err);
    
    /* compute and set keff */
    _k_eff = sumnew / sumold;

    /* scale the old source by keff */
    petsc_err = VecScale(_sold, _k_eff);
    
    /* compute the L2 norm of source error */
    scale_val = 1e-15;
    petsc_err = VecShift(_snew, scale_val);
    petsc_err = VecShift(_sold, scale_val);
    petsc_err = VecPointwiseDivide(_res, _sold, _snew);
    scale_val = -1;
    petsc_err = VecShift(_res, scale_val);
    scale_val = -1e-15;
    petsc_err = VecShift(_snew, scale_val);
    petsc_err = VecShift(_sold, scale_val);
    CHKERRQ(petsc_err);
    petsc_err = VecNorm(_res, NORM_2, &eps);
    eps = eps / (_cells_x * _cells_y * _num_groups);

    /* normalize the new source */
    scale_val = (_cells_x * _cells_y * _num_groups) / sumnew;
    petsc_err = VecScale(_snew, scale_val);
    CHKERRQ(petsc_err);
    
    /* set old source to new source */
    petsc_err = VecCopy(_snew, _sold);
    CHKERRQ(petsc_err);
    
    log_printf(INFO, "CMFD iter: %i, keff: %f, error: %f", iter + 1, _k_eff, eps);

    /* check for convergence */
    if (eps < _conv_criteria)
      break;
  }

  /* destroy KSP object */
  petsc_err = KSPDestroy(&ksp);

  /* rescale the old and new flux */
  petsc_err = rescaleFlux();

  /* give the petsc flux array to the mesh cell flux array */
  petsc_err = setMeshCellFlux();

  /* update the MOC flux */
  updateMOCFlux();  
  
  /* If solving diffusion problem, print timing results */
  if (_solve_method == DIFFUSION){
    std::string msg_string;
    log_printf(TITLE, "TIMING REPORT");
    _timer->stopTimer();
    _timer->recordSplit("Total time to solve diffusion eigenvalue problem");
    
    double tot_time = _timer->getSplit("Total time to solve diffusion eigenvalue problem");
    msg_string = "Total time to solve diffusion eigenvalue problem";
    msg_string.resize(53, '.');
    log_printf(RESULT, "%s%1.4E sec", msg_string.c_str(), tot_time);
  }

#endif
  
  return _k_eff;
}


/**
 * @brief rescale the initial and converged flux arrays
 * @return petsc_err petsc error flag
 */
int Cmfd::rescaleFlux(){

  int petsc_err = 0;

#ifdef CMFD
  PetscScalar sumnew, sumold, scale_val;

  /* rescale the new and old flux to have an avg source of 1.0 */
  petsc_err = MatMult(_M, _phi_new, _snew);
  petsc_err = VecSum(_snew, &sumnew);
  scale_val = _cells_x*_cells_y*_num_groups / sumnew;
  petsc_err = VecScale(_phi_new, scale_val);
  petsc_err = MatMult(_M, _phi_old, _sold);
  petsc_err = VecSum(_sold, &sumold);
  scale_val = _cells_x*_cells_y*_num_groups / sumold;
  petsc_err = VecScale(_phi_old, scale_val);
  CHKERRQ(petsc_err);
#endif
  
  return petsc_err;
}


/**
 * @brief Pass the petsc flux array flux to the mesh cell flux array
 * @return petsc_err petsc error flag
 */
int Cmfd::setMeshCellFlux(){

  /* initialize variables */
  int petsc_err = 0;

#ifdef CMFD
  PetscScalar *old_phi;
  PetscScalar *new_phi;
  petsc_err = VecGetArray(_phi_new, &new_phi);
  petsc_err = VecGetArray(_phi_old, &old_phi);
  CHKERRQ(petsc_err);
  
  for (int i = 0; i < _cells_x*_cells_y; i++){
    for (int e = 0; e < _num_groups; e++){
      _mesh->getFluxes(CURRENT)[i*_num_groups + e] = double(new_phi[i*_num_groups + e]);
      _mesh->getFluxes(PREVIOUS)[i*_num_groups + e] = double(old_phi[i*_num_groups + e]);
    }
  }
  
  petsc_err = VecRestoreArray(_phi_new, &new_phi);
  petsc_err = VecRestoreArray(_phi_old, &old_phi);
  CHKERRQ(petsc_err);
#endif

  return petsc_err;
}


/* Fill in the values in the A matrix, M matrix, and phi_old vector
 * @return petsc_err petsc error flag
 */
int Cmfd::constructMatrices(){

  log_printf(INFO,"Constructing AMPhi...");
  
  /* initialized variables */
  int petsc_err = 0;

#ifdef CMFD
  PetscScalar value;
  int cell;
  PetscInt row, col;

  /* get arrays */
  Material** materials = _mesh->getMaterials();
  double* old_flux = _mesh->getFluxes(PREVIOUS);
  double* heights = _mesh->getLengthsY();
  double* widths = _mesh->getLengthsX();
  
  /* loop over cells */
  for (int y = 0; y < _cells_y; y++){
    for (int x = 0; x < _cells_x; x++){
      
      cell = y*_cells_x + x;
      
      /* loop over groups */
      for (int e = 0; e < _num_groups; e++){

	row = cell*_num_groups + e;
	
	/* flux */
	value = old_flux[cell*_num_groups+e];
	petsc_err = VecSetValue(_phi_old, row, value, INSERT_VALUES);

	/* absorption term */
	value = materials[cell]->getSigmaA()[e] * _mesh->getVolumes()[cell];
	col = cell*_num_groups + e;
	petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);

	/* out (1st) and in (2nd) scattering */
	if (_flux_method == PRIMAL){
	  for (int g = 0; g < _num_groups; g++){
	    if (e != g){
	      col = cell*_num_groups+e;
	      value = materials[cell]->getSigmaS()[g*_num_groups+e] * _mesh->getVolumes()[cell]; 
	      petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	      col = cell*_num_groups+g;
	      value = - materials[cell]->getSigmaS()[e*_num_groups + g] * _mesh->getVolumes()[cell];
	      petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	    }
	  }
	}
	else{
	  for (int g = 0; g < _num_groups; g++){
	    if (e != g){
	      col = cell*_num_groups+e;
	      value = materials[cell]->getSigmaS()[g*_num_groups + e] * _mesh->getVolumes()[cell];
	      petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	      col = cell*_num_groups+g;
	      value = - materials[cell]->getSigmaS()[e*_num_groups + g] * _mesh->getVolumes()[cell];
	      petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	    }
	  }
	}
	
	/* RIGHT SURFACE */
       
	/* set transport term on diagonal */

	value = (materials[cell]->getDifHat()[2*_num_groups + e] 
			    - materials[cell]->getDifTilde()[2*_num_groups + e]) 
	                    * heights[cell / _cells_x];

	col = cell*_num_groups+e;
	petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	
	/* set transport term on off diagonal */
	if (x != _cells_x - 1){
	  value = - (materials[cell]->getDifHat()[2*_num_groups + e] 
					+ materials[cell]->getDifTilde()[2*_num_groups + e]) 
	                                * heights[cell / _cells_x];

	  col = (cell+1)*_num_groups+e;
	  petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	}

	/* LEFT SURFACE */
	
	/* set transport term on diagonal */
	value = (materials[cell]->getDifHat()[0*_num_groups + e] 
			    + materials[cell]->getDifTilde()[0*_num_groups + e]) 
	                    * heights[cell / _cells_x];

	col = cell*_num_groups+e;	
	petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	
	/* set transport term on off diagonal */
	if (x != 0){
	    value = - (materials[cell]->getDifHat()[0*_num_groups + e] 
			    - materials[cell]->getDifTilde()[0*_num_groups + e]) 
	                    * heights[cell / _cells_x];

	  col = (cell-1)*_num_groups+e;
	  petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	}
	
	/* BOTTOM SURFACE */
	
	/* set transport term on diagonal */
	value = (materials[cell]->getDifHat()[1*_num_groups + e] 
			    - materials[cell]->getDifTilde()[1*_num_groups + e]) 
	                    * widths[cell % _cells_x];

	col = cell*_num_groups+e;
	petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
       
	/* set transport term on off diagonal */
	if (y != _cells_y - 1){
	  value = - (materials[cell]->getDifHat()[1*_num_groups + e] 
			  + materials[cell]->getDifTilde()[1*_num_groups + e]) 
	                  * widths[cell % _cells_x];

	  col = (cell+_cells_x)*_num_groups+e;
	  petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	}
	
	/* TOP SURFACE */
	
	/* set transport term on diagonal */
        value = (materials[cell]->getDifHat()[3*_num_groups + e] 
			    + materials[cell]->getDifTilde()[3*_num_groups + e]) 
	                    * widths[cell % _cells_x];

	col = cell*_num_groups+e;
	petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	
	/* set transport term on off diagonal */
	if (y != 0){
	  value = - (materials[cell]->getDifHat()[3*_num_groups + e] 
					- materials[cell]->getDifTilde()[3*_num_groups + e]) 
	                                * widths[cell % _cells_x];

	  col = (cell-_cells_x)*_num_groups+e;
	  petsc_err = MatSetValues(_A, 1, &row, 1, &col, &value, ADD_VALUES);
	}

	/* source term */
	if (_assemble_M){
	  for (int g = 0; g < _num_groups; g++){	    
	    int f = g;
	    
	    if (_flux_method == ADJOINT){
	      g = e;
	      e = f;
	    }

	    col = cell*_num_groups+f;
	    value = materials[cell]->getChi()[e] * materials[cell]->getNuSigmaF()[g] * _mesh->getVolumes()[cell];
	    petsc_err = MatSetValues(_M, 1, &row, 1,&col, &value, ADD_VALUES);
	    
	    if (_flux_method == ADJOINT){
	      e = g;
	      g = f;
	    }
	  }
	}
      }
    }
  }

#endif
  
  log_printf(INFO,"Done constructing AMPhi...");
  
  return petsc_err;
}


/**
 * @brief Update the MOC flux in each FSR
 */
void Cmfd::updateMOCFlux(){

  log_printf(INFO, "Updating MOC flux...");
  
  /* initialize variables */
  std::vector<int>::iterator iter;
  double* old_flux = _mesh->getFluxes(PREVIOUS);
  double* new_flux = _mesh->getFluxes(CURRENT);
  double old_cell_flux, new_cell_flux;
  
  /* loop over mesh cells */
  for (int i = 0; i < _cells_x*_cells_y; i++){
    
    /* loop over groups */
    for (int e = 0; e < _num_groups; e++){
      
      /* get the old and new meshCell flux */
      old_cell_flux = old_flux[i*_num_groups + e];
      new_cell_flux = new_flux[i*_num_groups + e];
      
      /* loop over FRSs in mesh cell */
      for (iter = _mesh->getCellFSRs()->at(i).begin(); iter != _mesh->getCellFSRs()->at(i).end(); ++iter) {
	
	/* set new flux in FSR */
	_FSR_fluxes[*iter*_num_groups+e] = new_cell_flux / old_cell_flux * _FSR_fluxes[*iter*_num_groups+e];
	
	log_printf(DEBUG, "Updating flux in FSR: %i, cell: %i, group: %i, ratio: %f", *iter ,i, e, new_cell_flux / old_cell_flux);
      }
    }
  }
}


/**
 * @brief Compute diffusion correction factors to correct 
 * diffusion coefficients in optically thick mesh cells
 * @param d old diffusion coefficient
 * @param h height of cell
 * @return f correction factor
 */
double Cmfd::computeDiffCorrect(double d, double h){

  if (_mesh->getOpticallyThick() && _solve_method == MOC){
    
    /* initialize variables */
    double alpha, mu, expon;
    double rho, F;
    rho = 0.0;
    
    /* loop over polar angles */
    for (int p = 0; p < 3; p++){
      mu = cos(asin(_quad->getSinTheta(p)));
      expon = exp(- h / (3 * d * mu));
      alpha = (1 + expon) / (1 - expon) - 2 * mu / h;
      rho += mu * _quad->getWeight(p) * alpha;
    }
    
    /* compute correction factor, F */
    F = 1.0 + h * rho / (2 * d);
    
    return F;
  }
  else
    return 1.0;
  
}


#ifdef CMFD
/**
 * @brief get pointer to loss matrix, A
 * @return _A pointer to loss matrix, A
 */
Mat Cmfd::getA(){
  return _A;
}


/**
 * @brief get pointer to source matrix, M
 * @return _M pointer to source matrix, M
 */
Mat Cmfd::getM(){
  return _M;
}
#endif

/**
 * @brief get k_eff
 * @return _k_eff k_eff
 */
double Cmfd::getKeff(){
  return _k_eff;
}


/**
 * @brief initialize the fsrs 
 */
void Cmfd::initializeFSRs(){

  log_printf(INFO, "Initialize FSRs...");
  
  /* intialize variables */
  int fsr_id;
  CellBasic* cell;
  Material* material;
  Universe* univ_zero = _geometry->getUniverse(0);
  double* heights = _mesh->getLengthsY();
  double* widths = _mesh->getLengthsX();
  
  for (int i = 0; i < _cells_x * _cells_y; i++){
    
    /* get mesh cell and fsr volume */
    fsr_id = _mesh->getCellFSRs()->at(i).front();
    _FSR_volumes[fsr_id] = heights[i / _cells_x] * widths[i % _cells_x];
    
    /* initialize the fsr fluxes to 1.0 */
    for (int e = 0; e < _num_groups; e++)
      _FSR_fluxes[fsr_id*_num_groups+e] = 1.0;
    
    /* Get the cell corresponding to this FSR from the geometry */
    cell = static_cast<CellBasic*>(_geometry->findCell(univ_zero, fsr_id));
    
    /* Get the cell's material and assign it to the FSR */
    material = _geometry->getMaterial(cell->getMaterial());
    _FSR_materials[fsr_id] = material;
    
    log_printf(DEBUG, "cell %i with FSR id = %d has cell id = %d and material id = %d "
	       "and volume = %f", i, fsr_id, cell->getId(),
	       _FSR_materials[fsr_id]->getUid(), _FSR_volumes[fsr_id]);
    
  }
  
  log_printf(INFO, "Done initializing FSRs");
}


/**
 * @brief Set flag to determine whether to assemble M
 *        whenever we solve a diffusion problem
 * @param assemble_M assemble M flag
 */
void Cmfd::assembleM(bool assemble_M){
  _assemble_M = assemble_M;
}


/**
 * @brief Set the fsr materials array pointer
 * @param FSR_materials pointer to fsr materials array
 */
void Cmfd::setFSRMaterials(Material** FSR_materials){
  _FSR_materials = FSR_materials;
}


/**
 * @brief Set the fsr volumes by summing the volumes of 
 *        the fsrs contained in each cell
 * @param FSR_volumes array of fsr volumes
 */
void Cmfd::setFSRVolumes(FP_PRECISION* FSR_volumes){
  _FSR_volumes = FSR_volumes;
  
  std::vector<int>::iterator iter;
  double volume;
  
  /* set volume of mesh cells */
  for (int y = 0; y < _cells_y; y++){
    for (int x = 0; x < _cells_x; x++){
      volume = 0.0;
      
      for (iter = _mesh->getCellFSRs()->at(y*_cells_x+x).begin(); iter != _mesh->getCellFSRs()->at(y*_cells_x+x).end(); ++iter)
	volume += _FSR_volumes[*iter];
      
      _mesh->setVolume(volume, y*_cells_x+x);
      log_printf(DEBUG, "set cell %i volume to: %f", y*_cells_x+x, volume);
    }
  }
}


/**
 * @brief Set pointer to fsr flux array
 * @param scalar_flux pointer to fsr flux array
 */
void Cmfd::setFSRFluxes(FP_PRECISION* scalar_flux){
  _FSR_fluxes = scalar_flux;
}


/**
 * @brief Get pointer to the mesh 
 * @return _mesh pointer to mesh
 */
Mesh* Cmfd::getMesh(){
  return _mesh;
}


/**
 * @brief Set the flux method
 * @param flux_method enum for flux type
 */
void Cmfd::toggleFluxType(fluxType flux_method){
  _flux_method = flux_method;
}



