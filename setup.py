from distutils.core import setup, Extension
import numpy as np

hmp_module = Extension('hmp',
	sources = ['hmp_python_wrapper.c'],
	include_dirs=[np.get_include(), './hmp/', './ffmpeg/include/'],
	extra_compile_args=['-O3'],
	extra_link_args=['-lhmp', '-L./hmp/', '-lavutil', '-lavcodec', '-lavformat', '-lswscale', '-L./ffmpeg/lib/']
)

setup ( name = 'hmp',
	version = '1.0',
	description = 'A module for extracting a HMP representation from a MPEG-2 raw video.',
	ext_modules = [ hmp_module ]
)
