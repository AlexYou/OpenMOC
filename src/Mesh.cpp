#include "Mesh.h"



/**
 * @brief Constructor initializes boundaries and variables that describe
 *          the mesh.
 * @details The construcor initializes the many variables to zero,
 *          initializes cmfd acceleration to off, and initializes 
 *          the boundary conditions to REFLECTIVE
 * @param solve_type solve method (MOC or DIFFUSION)
 * @param cmfd_on an optional boolean to turn on cmfd
 * @param relax_factor relaxation factor
 * @param cmfd_level cmfd mesh level
 */
Mesh::Mesh(solveType solve_type, bool cmfd_on, double relax_factor, int mesh_level){

  if (solve_type == DIFFUSION)
    cmfd_on = true;

  /* initialize variables */
  _cmfd_on = cmfd_on;
  _acceleration = cmfd_on;
  _num_groups = 0;
  _num_fsrs = 0;
  _num_azim = 0;
  _num_currents = 0;
  _cells_x = 0;
  _cells_y = 0;
  _mesh_level = mesh_level;
  _optically_thick = false; 
  _relax_factor = relax_factor;
  _solve_method = solve_type;

  /* initialize boundaries to be reflective */
  _boundaries = new boundaryType[4];
  _boundaries[0] = REFLECTIVE;
  _boundaries[1] = REFLECTIVE;
  _boundaries[2] = REFLECTIVE;
  _boundaries[3] = REFLECTIVE;

  _volumes = NULL;
  _bounds_x = NULL;
  _bounds_y = NULL;
  _lengths_x = NULL;
  _lengths_y = NULL;

}


/**
 * @brief Destructor deletes arrays of boundaries, volumes,
 *        lengths, and bounds
 * @details Deallocates memory for all arrays allocated by the cmfd
 *        module including boundaries, volumes, lengths, and bounds
 */
Mesh::~Mesh(){
  

  if (_boundaries != NULL)
    delete [] _boundaries;

  if (_volumes != NULL)
    delete [] _volumes;

  if (_bounds_x != NULL)
    delete [] _bounds_x;

  if (_bounds_y != NULL)
    delete [] _bounds_y;

  if (_lengths_x != NULL)
    delete [] _lengths_x;

  if (_lengths_y != NULL)
    delete [] _lengths_y;

}


/**
 * @brief Initializes the mesh by allocating memory for various arrays
 * @details This method is called by the geometry once the width of
 *          the mesh has been determined. This method allocates memory
 *          for the volumes, old and new flux arrays, lengths, bounds,
 *          and cell fsr vectors
 */
void Mesh::initialize(){

  /* allocate memory for volumes */
  _volumes = new double[_cells_x*_cells_y];
  
  /* allocate memory for fluxes */
  double* new_flux;
  double* old_flux;
  double* adj_flux;
  new_flux = new double[_cells_x*_cells_y*_num_groups];
  old_flux = new double[_cells_x*_cells_y*_num_groups];
  adj_flux = new double[_cells_x*_cells_y*_num_groups];
  _fluxes.insert(std::pair<materialState, double*>(CURRENT, new_flux));
  _fluxes.insert(std::pair<materialState, double*>(PREVIOUS, old_flux));
  _fluxes.insert(std::pair<materialState, double*>(ADJ, adj_flux));
  
  /* allocate memory for cell widths, heights, and bounds */
  _lengths_x = new double[_cells_x];
  _lengths_y = new double[_cells_y];
  _bounds_x  = new double[_cells_x+1];
  _bounds_y  = new double[_cells_y+1];
  _num_currents = _cells_x*_cells_y*8;
  
  /* set initial mesh cell flux to 1.0 and allocate memory for fsr vectors */
  for (int y = 0; y < _cells_y; y++){
    for (int x = 0; x < _cells_x; x++){
      
        for (int g = 0; g < _num_groups; g++){
	    new_flux[(y*_cells_x+x)*_num_groups + g] = 1.0;
	    old_flux[(y*_cells_x+x)*_num_groups + g] = 1.0;
	    adj_flux[(y*_cells_x+x)*_num_groups + g] = 1.0;
	}

	/* allocate memory for fsr vector */
	std::vector<int> *fsrs = new std::vector<int>;
	_cell_fsrs.push_back(*fsrs);      
    }
  }  
}


/**
 * @brief Get mesh cell width
 * @return mesh cell width
 */
int Mesh::getCellsX(){
  return _cells_x;
}


/**
 * @brief Get mesh cell height
 * @return mesh cell height
 */
int Mesh::getCellsY(){
  return _cells_y;
}


/**
 * @brief Set the mesh cell width for a particular cell
 * @param cell_num mesh cell number
 * @param length_x width of mesh cell
 */
void Mesh::setCellLengthX(int cell_num, double length_x){
  int x = cell_num % _cells_x;
  _lengths_x[x] = length_x;
}


/**
 * @brief Set the mesh cell height for a particular cell
 * @param cell_num mesh cell number
 * @param length_y height of mesh cell
 */
void Mesh::setCellLengthY(int cell_num, double length_y){
  int y = cell_num / _cells_x;
  _lengths_y[y] = length_y;
}


/**
 * @brief Set the number of mesh cells in a row
 * @param cells_x cell width
 */
void Mesh::setCellsX(int cells_x){
  _cells_x = cells_x;
}


/**
 * @brief Set the number of mesh cells in a column
 * @param cells_y cell height
 */
void Mesh::setCellsY(int cells_y){
  _cells_y = cells_y;
}


/**
 * @brief given an x,y coordinate, find what mesh cell the point is in
 * @param x_coord coordinate
 * @param y_coord coordinate
 * @return cell cell id
 */
int Mesh::findMeshCell(double x_coord, double y_coord){
  
  int x = 0, y = 0;

  /* loop over cells in y direction */
  for (y = 0; y < _cells_y; y++){
    if (y_coord - _bounds_y[y+1] >= -1.e-8 && y_coord - _bounds_y[y] <= 1.e-8){
      break;
    }
  }
  
  /* loop over cells in y direction */
  for (x = 0; x < _cells_x; x++){
    if (x_coord - _bounds_x[x] >= -1.e-8 && x_coord - _bounds_x[x+1] <= 1.e-8){
      break;
    }
  }
  
  int cell = (y*_cells_x + x);
  return cell;
}


/**
 * @brief Set mesh width
 * @param length_x physical width of mesh
 */
void Mesh::setLengthX(double length_x){
  _length_x = length_x;
}


/**
 * @brief Set mesh height
 * @param length_y physical height of mesh
 */
void Mesh::setLengthY(double length_y){
  _length_y = length_y;
}


/**
 * @brief Get mesh width
 * @return _length_x physical width of mesh
 */
double Mesh::getLengthX(){
  return _length_x;
}


/**
 * @brief Get mesh height
 * @return _length_y physical height of mesh
 */
double Mesh::getLengthY(){
  return _length_y;
}


/**
 * @brief Set the fsr bounds for each mesh cell
 */
void Mesh::setFSRBounds(){
  
  /* initialize variables */
  int min;
  int max;
  std::vector<int>::iterator iter;
  
  /* create arrays of fsr indices, cell bounds, and surfaces */
  _fsr_indices = new int[2 * _cells_x * _cells_y];
  
  /* loop over mesh cells */
  for (int i = 0; i < _cells_y * _cells_x; i++){
    
    /* set fsr min and max bounds */
    min = _cell_fsrs.at(i).front();
    max = _cell_fsrs.at(i).front();
    
    /* loop over fsrs and update min and max bounds */
    for (iter = _cell_fsrs.at(i).begin(); iter != _cell_fsrs.at(i).end(); ++iter) {
      min = std::min(*iter, min);
      max = std::max(*iter, max);
    }
    
    /* set fsr bounds */
    _fsr_indices[2*i] = min;
    _fsr_indices[2*i+1] = max;
  }
}


/**
 * @brief Using an fsr_id and coordinate, find which surface 
 *        a coordinate is on
 * @param fsr_id Uid of fsr
 * @param coord coordinate of segment on mesh surface
 * @param angle azimuthal angle of track segment is on
 * @return surface Uid representing surface
 */
int Mesh::findMeshSurface(int fsr_id, LocalCoords* coord, int angle){

  /* initialize variables */
  int surface = -1;
  double x = coord->getX();
  double y = coord->getY();
  int i;
  bool break_cells = false;
  
  std::vector<int>::iterator iter;
  
  /* loop over mesh cells */
  for (i = 0; i < _cells_x * _cells_y; i++){
    
    break_cells = false;
    
    /* find if fsr is within bounds of cell */
    if (fsr_id >= _fsr_indices[2*i] && fsr_id <= _fsr_indices[2*i+1]){
     
      /* loop over fsrs in cell to see if fsr is actually in the cell since the
       * fsr bounds can overlap */
      for (iter = _cell_fsrs.at(i).begin(); iter < _cell_fsrs.at(i).end(); iter++){
	
	if (fsr_id == *iter){
	  
	  /* check if coordinate is on left surface, left top, or left bottom corner */
	  if (fabs(x -  _bounds_x[i % _cells_x]) < 1e-6){
	    
	    /* check if coordinate is on left surface */
	    if ((y - _bounds_y[i / _cells_x + 1]) > 1e-6 && (y - _bounds_y[i/_cells_x]) < -1e-6){
	      surface = i*8+0;
	      break_cells = true;
	      break;
	    }
	    /* check if coordinate is on left top corner */
	    else if (fabs(y - _bounds_y[i/_cells_x]) < 1e-6){
	      surface = i*8+7;
	      break_cells = true;
	      break;
	    }
	    /* check if coordinate is on left bottom corner */
	    else{
	      surface = i*8+4;
	      break_cells = true;
	      break;
	    }
	  }
	  /* check if coordinate is on right surface, right top corner, or right bottom corner */
	  else if (fabs(x - _bounds_x[i%_cells_x + 1]) < 1e-6){
	    /* check if coordinate is on right surface */
	    if ((y - _bounds_y[i/_cells_x+1]) > 1e-6 && (y - _bounds_y[i/_cells_x]) < -1e-6){
	      surface = i*8+2;
	      break_cells = true;
	      break;
	    }
	    /* check if coordinate is on right top surface */
	    else if (fabs(y - _bounds_y[i/_cells_x]) < 1e-6){
	      surface = i*8+6;
	      break_cells = true;
	      break;
	    }
	    /* coordinate is on right bottom surface */
	    else{
	      surface = i*8+5;
	      break_cells = true;
	      break;
	    }
	  }
	  /* check if coordinate is on top surface */
	  else if (fabs(y - _bounds_y[i/_cells_x]) < 1e-6){
	    surface = i*8+3;
	    break_cells = true;
	    break;
	  }
	  /* coordinate is on bottom surface */
	  else if (fabs(y - _bounds_y[i/_cells_x + 1]) < 1e-6){
	    surface = i*8+1;
	    break_cells = true;
	    break;
	  }
	}
      }
    }
    
    if (break_cells)
      break;
  }
  
  /* give each azimuthal angle unique surface id */
  if (surface != -1)
    surface = _num_currents*angle + surface;

  return surface;
}


/**
 * @brief Print the surface currents */
void Mesh::printCurrents(){

  double current;
  
  /* loop over cells */
  for (int i = 0; i < _cells_x * _cells_y; i++){
    /* loop over surfaces */
    for (int s = 0; s < 8; s++){
      /* loop over groups */
      for (int g = 0; g < _num_groups; g++){
	current = _currents[i*_num_groups*8 + s*_num_groups + g];
	log_printf(NORMAL, "cell: %i, surface: %i, group: %i, current: %f", i, s, g, current);
      }
    }
  }
}


/**
 * @brief Set the mesh boundary type for left surface
 * @param side the Uid for the mesh surface side
 * @param boundary the boundary type enum
 */
void Mesh::setBoundary(int side, boundaryType boundary){
  _boundaries[side] = boundary;
}


/* @brief split the currents of the mesh cell corners to the 
 *        nearby surfaces
 * @detail left bottom corner -> bottom surface and left surface 
 *         of mesh cell below; right bottom corner -> bottom surface
 *         and right surface of mesh cell below; right top corner -> 
 *         right surface and top surface of mesh cell to the right; 
 *         left top corner -> left surface and top surface of mesh 
 *         cell to the left
 */
void Mesh::splitCorners(){

  log_printf(INFO, "splitting corners...");

  for (int x = 0; x < _cells_x; x++){
    for (int y = 0; y < _cells_y; y++){
      
      /* split the LEFT BOTTOM CORNER */
      
      /* if cell is not on left or bottom geometry edge
       * give to bottom surface and left surface of mesh cell below */
      if (x > 0 && y < _cells_y - 1){
	
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, LEFT BOTTOM current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 4*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + 1*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 4*_num_groups + e];
	  _currents[((y+1)*_cells_x+x)*_num_groups*8 + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 4*_num_groups + e];
	}
      }
      /* if cell is on left or bottom geometry edge
       * give to bottom surface and left surface */
      else{
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, LEFT BOTTOM current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 4*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + 1*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 4*_num_groups + e];
	  _currents[(y*_cells_x+x)*_num_groups*8 + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 4*_num_groups + e];
	}
      }
      
      /* split the RIGHT BOTTOM CORNER */
      
      /* if cell is not on right or bottom geometry edge
       * give to bottom surface and right surface of mesh cell below */
      if (x < _cells_x - 1 && y < _cells_y - 1){
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, RIGHT BOTTOM current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 5*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + 1*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 5*_num_groups + e];
	  _currents[((y+1)*_cells_x+x)*_num_groups*8 + 2*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 5*_num_groups + e];
	}
      }
      /* if cell is on right or bottom geometry edge
       * give to bottom surface and right surface */
      else{
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, RIGHT BOTTOM current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 5*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + 1*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 5*_num_groups + e];
	  _currents[(y*_cells_x+x)*_num_groups*8 + 2*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 5*_num_groups + e];
	}
      }
      
      /* split the RIGHT TOP CORNER */
      
      /* if cell is not on right or top geometry edge
       * give to right surface and top surface of mesh cell to the right */
      if (x < _cells_x - 1 && y > 0){
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, RIGHT TOP current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 6*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + 2*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 6*_num_groups + e];
	  _currents[(y*_cells_x+x+1)*_num_groups*8 + 3*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 6*_num_groups + e];
	}
      }
      /* if cell is on right or top geometry edge
       * give to right surface and top surface */
      else{
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, RIGHT TOP current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 6*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + 2*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 6*_num_groups + e];
	  _currents[(y*_cells_x+x)*_num_groups*8 + 3*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 6*_num_groups + e];
	}
      }

      /* split the LEFT TOP CORNER */
      
      /* if cell is not on left or top geometry edge
       * give to left surface and top surface of mesh cell to the left */
      if (x > 0 && y > 0){
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, LEFT TOP current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 7*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 7*_num_groups + e];
	  _currents[(y*_cells_x+x-1)*_num_groups*8 + 3*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 7*_num_groups + e];
	}
      }
      /* if cell is on left or top geometry edge
       * give to top surface and left surface */
      else{
	for (int e = 0; e < _num_groups; e++){
	  log_printf(DEBUG, "cell: %i, group: %i, LEFT TOP current: %f", y*_cells_x+x,e, _currents[(y*_cells_x+x)*_num_groups*8 + 7*_num_groups + e]);
	  _currents[(y*_cells_x+x)*_num_groups*8 + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 7*_num_groups + e];
	  _currents[(y*_cells_x+x)*_num_groups*8 + 3*_num_groups + e] += _currents[(y*_cells_x+x)*_num_groups*8 + 7*_num_groups + e];
	}
      }
    }
  }
}


/**
 * @brief Get the number of cells in the geometry
 * @return num_cells the number of cells 
 **/
int Mesh::getNumCells(){
  int num_cells = _cells_x * _cells_y; 
  return num_cells;
}


/**
 * @brief Set the number of energy groups
 * @param num_groups number of energy groups
 **/
void Mesh::setNumGroups(int num_groups){
  _num_groups = num_groups;
}


/**
 * @brief Get the number of energy groups
 * @return _num_cells the number of groups
 **/
int Mesh::getNumGroups(){
  return _num_groups;
}


/**
 * @brief Set the number of fsrs
 * @param num_fsrs the number of fsrs 
 **/
void Mesh::setNumFSRs(int num_fsrs){
  _num_fsrs = num_fsrs;
}


/**
 * @brief Get the number of fsrs
 * @return _num_fsrs the number of fsrs 
 **/
int Mesh::getNumFSRs(){
  return _num_fsrs;
}


/**
 * @brief Get the boundary type
 * @param side the surface id 
 * @return _boundaries[side] the boundary type
 **/
boundaryType Mesh::getBoundary(int side){
  return _boundaries[side];
}


/**
 * @brief Get the flux for a certain cell and energy group
 * @param flux_name name of the flux
 * @param cell_id Uid of cell
 * @param group energy group
 * @return flux the scalar flux
 **/
double Mesh::getFlux(int cell_id, int group, materialState state){
  double* fluxes = _fluxes.find(state)->second;
  return fluxes[cell_id*_num_groups + group];
}

/**
 * @brief Get the cell id given a LocalCoords object
 * @param coord local coords object
 * @return num_cells the number of cells 
 **/
int Mesh::findCellId(LocalCoords* coord){

  /* initialize variables */
  double x_coord = coord->getX();
  double y_coord = coord->getY();
  int x,y;


  /* loop over cells in y direction */
  for (y = 0; y < _cells_y; y++){
    if (y_coord - _bounds_y[y+1] >= -1.e-8 && y_coord - _bounds_y[y] <= 1.e-8){
      break;
    }
  }
  
  /* loop over cells in y direction */
  for (x = 0; x < _cells_x; x++){
    if (x_coord - _bounds_x[x] >= -1.e-8 && x_coord - _bounds_x[x+1] <= 1.e-8){
      break;
    }
  }
  
  return (y*_cells_x + x);
}


/**
 * @brief Set the pointer to the surface currents array
 * @param surface_currents pointer to surface currents array
 **/
void Mesh::setSurfaceCurrents(double* surface_currents){
  _currents = surface_currents;
}


/**
 * @brief Get the cmfd on flag
 * @return _cmfd_on the cmfd_on flag 
 **/
bool Mesh::getCmfdOn(){
  return _cmfd_on;
}


/**
 * @brief Get the acceleration flag
 * @return _acceleration the acceleration flag 
 **/
bool Mesh::getAcceleration(){
  return _acceleration;
}


/**
 * @brief Set the acceleration flag
 * @parap accel the acceleration flag 
 **/
void Mesh::setAcceleration(bool accel){
  _acceleration = accel;
}


/**
 * @brief Get pointer to the mesh cell fsrs
 * @return _cell_fsrs point to nested vector of cell fsrs
 **/
std::vector<std::vector<int> >* Mesh::getCellFSRs(){
  return &_cell_fsrs;
}


/**
 * @brief Set the physical bounds of the cell
 **/
void Mesh::setCellBounds(){

  _bounds_x[0] = -_length_x / 2.0;
  _bounds_y[0] =  _length_y / 2.0;  

  log_printf(DEBUG, "bounds x: %f", _bounds_x[0]);

  /* set x bounds */
  for (int x = 1; x < _cells_x+1; x++){
    _bounds_x[x] = _bounds_x[x-1] + _lengths_x[x-1];
    log_printf(DEBUG, "bounds x: %f", _bounds_x[x]);
  }  

  log_printf(DEBUG, "bounds y: %f", _bounds_y[0]);

  /* set y bounds */
  for (int y = 1; y < _cells_y+1; y++){
    _bounds_y[y] = _bounds_y[y-1] - _lengths_y[y-1];
    log_printf(DEBUG, "bounds y: %f", _bounds_y[y]);
  }
  
  for (int x = 0; x < _cells_x; x++){
    for (int y = 0; y < _cells_y; y++){
      _volumes[y*_cells_x+x] = _lengths_x[x] * _lengths_y[y];
    }
  }

}


/**
 * @brief Get pointer to materials array
 * @return _materials pointer to materials array 
 **/
Material** Mesh::getMaterials(){
  return _materials;
}


/**
 * @brief Get pointer to volume array
 * @return _volumes pointer to volume array
 **/
double* Mesh::getVolumes(){
  return _volumes;
}


/**
 * @brief Set the volume of a cell
 * @param volume volume of cell
 * @param cell_num cell id
 **/
void Mesh::setVolume(double volume, int cell_num){
  _volumes[cell_num] = volume;
}


/**
 * @brief Get a flux array
 * @param flux_name name of flux array
 * @return fluxes array of fluxes 
 **/
double* Mesh::getFluxes(materialState state){
  return _fluxes.at(state);
}


/**
 * @brief Get array of mesh lengths in x direction
 * @return _lengths_x array of mesh lengths in x direction
 **/
double* Mesh::getLengthsX(){
  return _lengths_x;
}


/**
 * @brief Get array of mesh lengths in y direction
 * @return _lenghts_y array of mesh lengths in y direction
 **/
double* Mesh::getLengthsY(){
  return _lengths_y;
}


/**
 * @brief Get the id of cell next to given cell
 * @param cell_num current cell id
 * @param surface_id surface id to look across for 
 *         neighboring cell 
 * @return cell_next cell id of neighbor cell
 **/
int Mesh::getCellNext(int cell_num, int surface_id){

  int cell_next = -1;
  
  if (surface_id == 0){
    if (cell_num % _cells_x != 0)
      cell_next = cell_num - 1;
  }
  else if (surface_id == 1){
    if (cell_num / _cells_x != _cells_y - 1)
      cell_next = cell_num + _cells_x;
  }
  else if (surface_id == 2){
    if (cell_num % _cells_x != _cells_x - 1)
      cell_next = cell_num + 1;
  }
  else if (surface_id == 3){
    if (cell_num / _cells_x != 0)
      cell_next = cell_num - _cells_x;
  }
  
  return cell_next;
}


/**
 * @brief Get array of surface currents
 * @return _currents array of surface currents
 **/
double* Mesh::getCurrents(){
  return _currents;
}


/**
 * @brief Initialize the mesh cell materials
 * @param materials map of fsr materials
 * @param fsrs_to_mats array of material ids indexed by fsr id 
 **/
void Mesh::initializeMaterials(std::map<int, Material*>* materials, int* fsrs_to_mats){

  _materials = new Material*[_cells_x*_cells_y];
  std::vector<int>::iterator iter;
  
  for (int y = 0; y < _cells_y; y++){
    for (int x = 0; x < _cells_x; x++){
      if (materials->at(fsrs_to_mats[_cell_fsrs.at(y*_cells_x+x).at(0)])->getType() == BASE){
	_materials[y*_cells_x+x] = materials->at(fsrs_to_mats[_cell_fsrs.at(y*_cells_x+x).at(0)])->clone();
      }
      else{
	_materials[y*_cells_x+x] = static_cast<FunctionalMaterial*>(materials->at(fsrs_to_mats[_cell_fsrs.at(y*_cells_x+x).at(0)]))->clone();
      }
    }
  }
}



/**
 * @brief Set the number of azim angles
 * @param num_azim number of azim angles
 **/
void Mesh::setNumAzim(int num_azim){
  _num_azim = num_azim;
}


/**
 * @brief Get the number of surface currents
 * @return _num_currents number of surface currents
 **/
int Mesh::getNumCurrents(){
  return _num_currents;
}


/**
 * @brief Initializes the surface currents
 **/
void Mesh::initializeSurfaceCurrents(){
  _currents = new double[8*_cells_x*_cells_y*_num_groups];

  for (int i = 0; i < 8*_cells_x*_cells_y*_num_groups; i++)
    _currents[i] = 0.0;
}


/**
 * @brief Gets the mesh level
 * @return _mesh_level mesh level
 **/
int Mesh::getMeshLevel(){
  return _mesh_level;
}


/**
 * @brief Sets the cmfd level
 * @parap cmfd_level cmfd level
 **/
void Mesh::setMeshLevel(int mesh_level){
  _mesh_level = mesh_level;
}


/**
 * @brief Set flag to determine whether we use
 *        optically thick diffusion correction factor.
 * @param thick flag to turn on correction factor.
 */
void Mesh::setOpticallyThick(bool thick){
  _optically_thick = thick;
}


/**
 * @brief Get flag to determine whether we use
 *        optically thick diffusion correction factor.
 * @return _optically_thick flag to turn on 
 *         optically thick correction factor.
 */
bool Mesh::getOpticallyThick(){
  return _optically_thick;
}


/**
 * @brief Set the relaxation factor
 * @param relax_factor the relaxation factor
 */
void Mesh::setRelaxFactor(double relax_factor){
  _relax_factor = relax_factor;
}


/**
 * @brief Get the relaxation factor
 * @return _relax_factor the relaxation factor
 */
double Mesh::getRelaxFactor(){
  return _relax_factor;
}


/**
 * @brief Get the solve type
 * @return _solve_method the solve type
 */
solveType Mesh::getSolveType(){
  return _solve_method;
}


void Mesh::createNewFlux(materialState state){
  double* flux = new double[_cells_x*_cells_y*_num_groups];
  _fluxes.insert(std::pair<materialState, double*>(state, flux));
}


void Mesh::copyFlux(materialState from_state, materialState to_state){

  double* from_flux = _fluxes.at(from_state);
  double* to_flux = _fluxes.at(to_state);

  for (int i = 0; i < _cells_x*_cells_y*_num_groups; i++)
    to_flux[i] = from_flux[i];

}


double Mesh::getLeakage(materialState state, int group, bool adj_weight){

  double leakage = 0.0;
  double* flux = _fluxes.at(state);
  double* adj_flux;
  if (adj_weight)
    adj_flux = _fluxes.at(ADJ);

  int cell;

  /* loop over cells */
  for (int y = 0; y < _cells_y; y++){
    for (int x = 0; x < _cells_x; x++){ 
      cell = y*_cells_x+x;      
      if (adj_weight){
	
	/* LEFT SURFACE */
	leakage -= (_materials[cell]->getDifHat()[0*_num_groups + group] 
		    + _materials[cell]->getDifTilde()[0*_num_groups + group]) 
	  * _lengths_y[y] * flux[cell*_num_groups+group] * adj_flux[cell*_num_groups+group];

	if (x != 0){
	  leakage += (_materials[cell]->getDifHat()[0*_num_groups + group] 
		    + _materials[cell]->getDifTilde()[0*_num_groups + group]) 
	    * _lengths_y[y] * flux[(cell-1)*_num_groups+group] * adj_flux[cell*_num_groups+group];
	}
	  
	/* BOTTOM SURFACE */
	leakage -= (_materials[cell]->getDifHat()[1*_num_groups + group] 
		    - _materials[cell]->getDifTilde()[1*_num_groups + group]) 
	  * _lengths_x[x] * flux[cell*_num_groups+group] * adj_flux[cell*_num_groups+group];

	if (y != _cells_y-1)
	  leakage += (_materials[cell]->getDifHat()[1*_num_groups + group] 
		      - _materials[cell]->getDifTilde()[1*_num_groups + group]) 
	    * _lengths_x[x] * flux[(cell+_cells_x)*_num_groups+group] * adj_flux[cell*_num_groups+group];
	  

	/* RIGHT SURFACE */
	leakage -= (_materials[cell]->getDifHat()[2*_num_groups + group] 
		    - _materials[cell]->getDifTilde()[2*_num_groups + group]) 
	  * _lengths_y[y] * flux[cell*_num_groups+group] * adj_flux[cell*_num_groups+group];
	
	if (x != _cells_x-1){
	  leakage += (_materials[cell]->getDifHat()[2*_num_groups + group] 
		      - _materials[cell]->getDifTilde()[2*_num_groups + group]) 
	    * _lengths_y[y] * flux[(cell+1)*_num_groups+group] * adj_flux[cell*_num_groups+group];
	}	 
	
	/* TOP SURFACE */
	leakage -= (_materials[cell]->getDifHat()[3*_num_groups + group] 
		    + _materials[cell]->getDifTilde()[3*_num_groups + group]) 
	  * _lengths_x[x] * flux[cell*_num_groups+group] * adj_flux[cell*_num_groups+group];

	if (y != 0){
	  leakage += (_materials[cell]->getDifHat()[3*_num_groups + group] 
		      + _materials[cell]->getDifTilde()[3*_num_groups + group]) 
	    * _lengths_x[x] * flux[(cell-_cells_x)*_num_groups+group] * adj_flux[cell*_num_groups+group];
	}
      }
      else{

	/* LEFT SURFACE */
	leakage -= (_materials[cell]->getDifHat()[0*_num_groups + group] 
		    + _materials[cell]->getDifTilde()[0*_num_groups + group]) 
	  * _lengths_y[y] * flux[cell*_num_groups+group];

	if (x != 0){
	  leakage += (_materials[cell]->getDifHat()[0*_num_groups + group] 
		      + _materials[cell]->getDifTilde()[0*_num_groups + group]) 
	    * _lengths_y[y] * flux[(cell-1)*_num_groups+group];
	}
	
	/* BOTTOM SURFACE */
	leakage -= (_materials[cell]->getDifHat()[1*_num_groups + group] 
		    - _materials[cell]->getDifTilde()[1*_num_groups + group]) 
	  * _lengths_x[x] * flux[cell*_num_groups+group];

	if (y != _cells_y-1){
	  leakage += (_materials[cell]->getDifHat()[1*_num_groups + group] 
		      - _materials[cell]->getDifTilde()[1*_num_groups + group]) 
	    * _lengths_x[x] * flux[(cell+_cells_x)*_num_groups+group];
	}

	/* RIGHT SURFACE */
	leakage -= (_materials[cell]->getDifHat()[2*_num_groups + group] 
		    - _materials[cell]->getDifTilde()[2*_num_groups + group]) 
	  * _lengths_y[y] * flux[cell*_num_groups+group];

	if (x != _cells_x-1){
	  leakage += (_materials[cell]->getDifHat()[2*_num_groups + group] 
		      - _materials[cell]->getDifTilde()[2*_num_groups + group]) 
	    * _lengths_y[y] * flux[(cell+1)*_num_groups+group];	  
	}

	/* TOP SURFACE */
	leakage -= (_materials[cell]->getDifHat()[3*_num_groups + group] 
		    + _materials[cell]->getDifTilde()[3*_num_groups + group]) 
	  * _lengths_x[x] * flux[cell*_num_groups+group];

	if (y != 0){
	  leakage += (_materials[cell]->getDifHat()[3*_num_groups + group] 
		      + _materials[cell]->getDifTilde()[3*_num_groups + group]) 
	    * _lengths_x[x] * flux[(cell-_cells_x)*_num_groups+group];
	}
      }
    }
  }

  return leakage;
}


void Mesh::dumpFlux(materialState state){

  for (int i = 0; i < _cells_x*_cells_y; i++){
    for (int e = 0; e < _num_groups; e++)
      log_printf(NORMAL, "cell: %i, group: %i, flux: %f", i, e, _fluxes.at(state)[i*_num_groups+e]);
  }

}


void Mesh::dumpXS(){

  /* initialize variables */
  
  for (int i = 0; i < _cells_x * _cells_y; i++){
    
    log_printf(NORMAL, "-----------------------------------");
    
    if (_materials[i]->getType() == FUNCTIONAL)
      log_printf(NORMAL, "Cell %i (%i,%i), MATERIAL ID: %i, TEMP: %f", i, i % _cells_x, i / _cells_x, _materials[i]->getId(), static_cast<FunctionalMaterial*>(_materials[i])->getTemperature(CURRENT));
    else
      log_printf(NORMAL, "Cell (%i,%i), MATERIAL ID: %i", i % _cells_x, i / _cells_x, _materials[i]->getId());
    
    for (int e = 0; e < _num_groups; e++){
      log_printf(NORMAL, "GROUP        : %i", e);
      log_printf(NORMAL, "sigma_f      : %f", _materials[i]->getSigmaF()[e]);
      log_printf(NORMAL, "nu_sigma_f   : %f", _materials[i]->getNuSigmaF()[e]);
      log_printf(NORMAL, "sigma_t      : %f", _materials[i]->getSigmaT()[e]);
      log_printf(NORMAL, "sigma_a      : %f", _materials[i]->getSigmaA()[e]);
      log_printf(NORMAL, "chi          : %f", _materials[i]->getChi()[e]);
      log_printf(NORMAL, "Diff         : %f", _materials[i]->getDifCoef()[e]);
      log_printf(NORMAL, "Flux         : %f", _fluxes.at(CURRENT)[i*_num_groups+e]);
      for (int g = 0; g < _num_groups; g++)
	log_printf(NORMAL, "sigma_s %i->%i: %f", e, g, _materials[i]->getSigmaS()[g*_num_groups+e]);
      
      for (int s = 0; s < 4; s++){
	log_printf(NORMAL, "Surface      : %i", s);
	log_printf(NORMAL, "Diff Hat     : %f", _materials[i]->getDifHat()[s*_num_groups+e]);
	log_printf(NORMAL, "Diff Tilde   : %f", _materials[i]->getDifTilde()[s*_num_groups+e]);
	log_printf(NORMAL, "Current      : %f", _currents[i*_num_groups*8 + s*_num_groups + e]);
      }
    }
  }
}
