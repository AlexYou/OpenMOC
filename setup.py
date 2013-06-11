import config
import os
from setuptools import setup, find_packages
from distutils.extension import Extension
from distutils.command.build_ext import build_ext
import numpy


# Obtain the numpy include directory
try:
    numpy_include = numpy.get_include()
except AttributeError:
    numpy_include = numpy.get_numpy_include()


# If the user selected 'all' compilers, enumerate them
if config.cpp_compilers == ['all']:
    config.cpp_compilers = ['gcc', 'icpc']

# If the user selected 'all' floating point precision levels, enumerate them
if config.fp_precision == ['all']:
    config.fp_precision = ['double', 'single']

# If the user wishes to compile using debug mode, append the debugging flag
if config.debug_mode:
    for k in config.compiler_flags:
        config.compiler_flags[k].append('-g')

# If the user wishes to compile the CUDA package, append nvcc to list of compilers
if config.with_cuda:
    config.cpp_compilers.append('nvcc')


# Create list of extensions for Python modules within the openmoc Python package
extensions = []

# The main extension will be openmoc compiled with gcc and double precision
extensions.append(Extension(name = '_openmoc', 
                    sources = config.sources['c++'], 
                    library_dirs = config.library_directories['gcc'], 
                    libraries = config.shared_libraries['gcc'],
                    extra_link_args = config.linker_flags['gcc'], 
                    include_dirs = config.include_directories['gcc'],
                    define_macros = config.macros['gcc'][config.default_fp],
                    swig_opts = config.swig_flags))

config.sources['c++'].remove('openmoc/openmoc.i')


# A CUDA extension if the user requested it
if config.with_cuda:
    extensions.append(Extension(name = '_openmoc_cuda', 
                        sources = config.sources['cuda'], 
                        library_dirs = config.library_directories['nvcc'], 
                        libraries = config.shared_libraries['nvcc'],
                        extra_link_args = config.linker_flags['nvcc'], 
                        include_dirs = config.include_directories['nvcc'],
                        define_macros = config.macros['nvcc'][config.default_fp],
                        swig_opts = config.swig_flags,
                        export_symbols = ['init_openmoc']))
                      
    config.sources['cuda'].remove('openmoc/cuda/openmoc_cuda.i')


# A MIC extension if the user requested it
if config.with_mic:
#    config.linker_flags['gcc'] = config.linker_flags.append('-Wl,-soname,_openmoc.so')
    extensions.append(Extension(name = '_openmoc_mic', 
                        sources = config.sources['mic'], 
                        library_dirs = config.library_directories['icpc'], 
                        libraries = config.shared_libraries['icpc'],
                        extra_link_args = config.linker_flags['icpc'], 
                        include_dirs = config.include_directories['icpc'],
                        define_macros = config.macros['icpc'][config.default_fp],
                        swig_opts = config.swig_flags,
                        export_symbols = ['init_openmoc']))
                      
    config.sources['mic'].remove('openmoc/mic/openmoc_mic.i')
#    config.linker_flags[' = config.linker_flags.remove('-Wl,-soname,_openmoc.so')


# Loop over the compilers and floating point precision levels to create
# extension modules for each (ie, openmoc.icpc.double, openmoc.cuda.single, etc)
for fp in config.fp_precision:
    for cc in config.cpp_compilers:

        if cc == 'nvcc':
            ext_name = '_openmoc_cuda_' + fp
            swig_interface_file = 'openmoc/cuda/' + fp
            swig_interface_file += '/openmoc_cuda_' + fp + '.i'
            sources = config.sources['cuda']
            sources.append(swig_interface_file)

        elif cc == 'gcc':
            ext_name = '_openmoc_gnu_' + fp
            swig_interface_file = 'openmoc/gnu/' + fp
            swig_interface_file += '/openmoc_gnu_' + fp + '.i'
            sources = config.sources['c++']
            sources.append(swig_interface_file)

        elif cc == 'icpc':
            ext_name = '_openmoc_intel_' + fp
            swig_interface_file = 'openmoc/intel/' + fp
            swig_interface_file += '/openmoc_intel_' + fp + '.i'
            sources = config.sources['c++']
            sources.append(swig_interface_file)

        else:
            raise NameError('Compiler ' + str(cc) + ' is not supported')

        # Create the extension module
        extensions.append(Extension(name = ext_name, 
                            sources = sources, 
                            library_dirs = config.library_directories[cc], 
                            libraries = config.shared_libraries[cc],
                            extra_link_args = config.linker_flags[cc], 
                            include_dirs = config.include_directories[cc],
                            define_macros = config.macros[cc][fp],
                            swig_opts = config.swig_flags))


def customize_compiler(self):
    """Inject redefined _compile method into distutils

    Adapted from Robert McGibbon's CUDA distutils setup provide in open source 
    form here: https://github.com/rmcgibbo/npcuda-example
    """
    
    # Inform the compiler it can processes .cu CUDA source files
    self.src_extensions.append('.cu')

    # Save reference to the default _compile method
    super_compile = self._compile

    # Redefine the _compile method. This gets executed for each
    # object but distutils doesn't have the ability to change compilers
    # based on source extension, so we add that functionality here
    def _compile(obj, src, ext, cc_args, extra_postargs, pp_opts):

        # If GNU is a defined macro and the source is C++, use gcc
        if '-DGNU' in pp_opts and os.path.splitext(src)[1] == '.cpp':
            if config.with_ccache:
                self.set_executable('compiler_so', 'ccache gcc')
            else:
                self.set_executable('compiler_so', 'gcc')
            postargs = config.compiler_flags['gcc']

        # If INTEL is a defined macro and the source is C++, use icpc
        elif '-DINTEL' in pp_opts and os.path.splitext(src)[1] == '.cpp':
            if config.with_ccache:
                self.set_executable('compiler_so', 'ccache icpc')
            else:
                self.set_executable('compiler_so', 'icpc')
            postargs = config.compiler_flags['icpc']

        # If CUDA is a defined macro and the source is C++, compile SWIG-wrapped
        # CUDA code with gcc
        elif '-DCUDA' in pp_opts and os.path.splitext(src)[1] == '.cpp':
            if config.with_ccache:
                self.set_executable('compiler_so', 'ccache gcc')
            else:
                self.set_executable('compiler_so', 'gcc')
            postargs = config.compiler_flags['gcc']

        # If CUDA is a defined macro and the source is CUDA, use nvcc
        elif '-DCUDA' in pp_opts and os.path.splitext(src)[1] == '.cu':
            self.set_executable('compiler_so', 'ccache nvcc')
            postargs = config.compiler_flags['nvcc']

        # If we cannot determine how to compile this file, throw exception
        else:
            raise EnvironmentError('Unable to compile ' + str(src))

        # Now call distutils-defined _compile method
        super_compile(obj, src, ext, cc_args, postargs, pp_opts)

    # Inject our redefined _compile method into the class
    self._compile = _compile


def customize_linker(self):
    """Inject redefined link method into distutils

    Adapted from Robert McGibbon's CUDA distutils setup provide in open source 
    form here: https://github.com/rmcgibbo/npcuda-example
    """

    # save references to the default link method
    super_link = self.link

    # Redefine the link method. This gets executed to link each extension
    # module. We add the functionality to choose the compiler for linking
    # based on the name of the extension module
    def link(target_desc, objects, output_filename, 
             output_dir=None, libraries=None,
             library_dirs=None, runtime_library_dirs=None,
             export_symbols=None, debug=0, extra_preargs=None,
             extra_postargs=None, build_temp=None, target_lang=None):

        # If compiling different extensions of openmoc using different compilers
        # and/or floating point precision levels, we must remove autogenerated
        # files from distutils for each subsequent extension
        for obj in objects[:]:
            if '_intel' in obj and '_gnu' in output_filename:
                objects.remove(obj)
            elif '_gnu' in obj and '_intel' in output_filename:
                objects.remove(obj)
            elif '_single' in obj and '_double' in output_filename:
                objects.remove(obj)
            elif '_double' in obj and '_single' in output_filename:
                objects.remove(obj)

        # If the filename for the extension contains intel, use icpc to link
        if 'intel' in output_filename or 'mic' in output_filename:
            self.set_executable('linker_so', 'icpc')
            self.set_executable('linker_exe', 'icpc')

        # If the filename for the extension contains cuda, use g++ to link
        elif 'cuda' in output_filename:
            self.set_executable('linker_so', 'g++')
            self.set_executable('linker_exe', 'g++')

        # Otherwise, use g++ to link by default
        else:
            self.set_executable('linker_so', 'g++')
            self.set_executable('linker_exe', 'g++')
        
        # Now call distutils-defined link method
        super_link(target_desc, objects,
                   output_filename, output_dir, libraries,
                   library_dirs, runtime_library_dirs,
                   export_symbols, debug, extra_preargs,
                   extra_postargs, build_temp)

    # Inject our redefined link method into the class
    self.link = link


# Run the customize_compiler to inject redefined and customized _compile and 
# link methods into distutils
class custom_build_ext(build_ext):

    def build_extensions(self):
        customize_compiler(self.compiler)
        customize_linker(self.compiler)
        build_ext.build_extensions(self)


# Run setuptools/distutils setup method for the complete build
setup(name = config.package_name,
      version = '0.1',
      description = 'An open source method of characteristics code for ' + \
                    'solving the 2D neutron distribution in nuclear reactors]',
      author = 'Will Boyd',
      author_email = 'wboyd@mit.edu',
      url = 'https://github.com/mit-crpg/OpenMOC',
      ext_modules = extensions,
      packages = find_packages(),

      # Inject our custom compiler and linker triggers
      cmdclass={'build_ext': custom_build_ext},

      # Since the package has C++ code, the egg cannot be zipped
      zip_safe=False)
