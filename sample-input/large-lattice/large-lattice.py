from openmoc import *
import openmoc.log as log
import openmoc.plotter as plotter
import openmoc.materialize as materialize
import numpy as np
import matplotlib.pyplot as mpl


###############################################################################
#######################   Main Simulation Parameters   ########################
###############################################################################

num_threads = options.num_omp_threads
track_spacing = options.track_spacing
num_azim = options.num_azim
tolerance = options.tolerance
max_iters = options.max_iters

log.setLogLevel('NORMAL')


###############################################################################
###########################   Creating Materials   ############################
###############################################################################

log.py_printf('NORMAL', 'Importing materials data from HDF5...')

materials = materialize.materialize('../c5g7-materials.hdf5')

uo2_id = materials['UO2'].getId()
water_id = materials['Water'].getId()


###############################################################################
###########################   Creating Surfaces   #############################
###############################################################################

log.py_printf('NORMAL', 'Creating surfaces...')

circles = []
planes = []
planes.append(XPlane(x=-2.0))
planes.append(XPlane(x=2.0))
planes.append(YPlane(y=-2.0))
planes.append(YPlane(y=2.0))
circles.append(Circle(x=0.0, y=0.0, radius=0.4))
circles.append(Circle(x=0.0, y=0.0, radius=0.3))
circles.append(Circle(x=0.0, y=0.0, radius=0.2))
for plane in planes: plane.setBoundaryType(REFLECTIVE)


###############################################################################
#############################   Creating Cells   ##############################
###############################################################################

log.py_printf('NORMAL', 'Creating cells...')

cells = []
cells.append(CellBasic(universe=1, material=uo2_id))
cells.append(CellBasic(universe=1, material=water_id))
cells.append(CellBasic(universe=2, material=uo2_id))
cells.append(CellBasic(universe=2, material=water_id))
cells.append(CellBasic(universe=3, material=uo2_id))
cells.append(CellBasic(universe=3, material=water_id))
cells.append(CellFill(universe=5, universe_fill=4))
cells.append(CellFill(universe=0, universe_fill=6))

cells[0].addSurface(halfspace=-1, surface=circles[0])
cells[1].addSurface(halfspace=+1, surface=circles[0])
cells[2].addSurface(halfspace=-1, surface=circles[1])
cells[3].addSurface(halfspace=+1, surface=circles[1])
cells[4].addSurface(halfspace=-1, surface=circles[2])
cells[5].addSurface(halfspace=+1, surface=circles[2])

cells[7].addSurface(halfspace=+1, surface=planes[0])
cells[7].addSurface(halfspace=-1, surface=planes[1])
cells[7].addSurface(halfspace=+1, surface=planes[2])
cells[7].addSurface(halfspace=-1, surface=planes[3])


###############################################################################
###########################   Creating Lattices   #############################
###############################################################################

log.py_printf('NORMAL', 'Creating 16 x 16 lattice...')

# 2x2 assembly
assembly = Lattice(id=4, width_x=1.0, width_y=1.0)
assembly.setLatticeCells([[1, 2], [1, 3]])

# 2x2 core
core = Lattice(id=6, width_x=2.0, width_y=2.0)
core.setLatticeCells([[5, 5], [5, 5]])


###############################################################################
##########################   Creating the Geometry   ##########################
###############################################################################

log.py_printf('NORMAL', 'Creating geometry...')

geometry = Geometry()
for material in materials.values(): geometry.addMaterial(material)
for cell in cells: geometry.addCell(cell)
geometry.addLattice(assembly)
geometry.addLattice(core)

geometry.initializeFlatSourceRegions()


###############################################################################
########################   Creating the TrackGenerator   ######################
###############################################################################

log.py_printf('NORMAL', 'Initializing the track generator...')

track_generator = TrackGenerator(geometry, num_azim, track_spacing)
track_generator.generateTracks()


###############################################################################
###########################   Running a Simulation   ##########################
###############################################################################

# nthr_arr = [1,2,3,4,5,6,7,8]
nthr_arr = [1,2,3,4,5,6,7,8]
# nthr_arr = [1,2]
# num_threads = 8;
ev_arr = ['PAPI_TOT_CYC',
          'PAPI_L1_DCM',
          'PAPI_TOT_INS']

thr_counts = np.zeros((len(nthr_arr), len(nthr_arr), len(ev_arr)), dtype=np.int64)
thr_counts_accum = np.zeros((len(nthr_arr), len(ev_arr)),  dtype=np.int64)

solver = CPUSolver(geometry, track_generator)
for event in ev_arr:
	solver.addPapiEvent(event)
# max_iters = 5

for num_threads in nthr_arr:
	solver.setNumThreads(num_threads)
	solver.setSourceConvergenceThreshold(tolerance)
	solver.convergeSource(max_iters)
	solver.printTimerReport()

	for tid in xrange(thr_counts.shape[1]):
		for event in xrange(thr_counts.shape[2]):
			if tid < num_threads:
				value = solver.getThreadEventCount(ev_arr[event],tid)
				thr_counts_accum[num_threads-1][event] += value

	solver.resetPapiThreadCounts()

fig = mpl.figure()
# ax = fig.add_subplot(111)
event0 = 0
event1 = 1
event2 = 2

mpl.subplot(311)
l = mpl.plot(nthr_arr, np.divide(thr_counts_accum[:,event0],nthr_arr), '-b')
mpl.grid(True)
mpl.title('PAPI counts')
mpl.ylabel(ev_arr[event0])

mpl.subplot(312)
mpl.plot(nthr_arr, np.divide(thr_counts_accum[:,event1],nthr_arr), 'r')
mpl.grid(True)
mpl.ylabel(ev_arr[event1])

mpl.subplot(313)
mpl.plot(nthr_arr, np.divide(thr_counts_accum[:,event2],nthr_arr), 'r')
mpl.grid(True)
mpl.xlabel('Number of threads')
mpl.ylabel(ev_arr[event2])

mpl.show()

# ax.set_title(ev_arr[event])
# ax.set_xlabel('Number of threads')
# ax.set_ylabel('Event count')
# mpl.plot( nthr_arr, thr_counts_accum[:,0], '--r' )
# mpl.show()

###############################################################################
############################   Generating Plots   #############################
###############################################################################

# log.py_printf('NORMAL', 'Plotting data...')

#plotter.plotTracks(track_generator)
#plotter.plotSegments(track_generator)
# plotter.plotMaterials(geometry, gridsize=250)
#plotter.plotCells(geometry, gridsize=500)
#plotter.plotFlatSourceRegions(geometry, gridsize=500)
#plotter.plotFluxes(geometry, solver, energy_groups=[1,2,3,4,5,6,7])

log.py_printf('TITLE', 'Finished')
