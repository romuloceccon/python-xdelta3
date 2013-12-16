from distutils.core import setup, Extension
import os

xdelta_dir = os.path.join(os.environ['HOME'], 'xdelta-src')
xdelta3_c_src = os.path.join(xdelta_dir, 'xdelta3.c')

setup(name='xdelta3',
      version='0.1',
      ext_modules=[Extension('xdelta3',
                             ['src/python_xdelta3.c', xdelta3_c_src],
                             include_dirs=[xdelta_dir],
                             libraries=['lzma'],
                             define_macros=[('HAVE_CONFIG_H', None),
                                            ('NOT_MAIN', None),
                                            ('XD3_POSIX', '1'),
                                            ('XD3_USE_LARGEFILE64', '1')])],
      )