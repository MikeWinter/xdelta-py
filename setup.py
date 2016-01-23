from setuptools import Extension, setup

setup(name='DjangoDelta',
      version='1.0.1',
      description='XDelta3 Delta Encoding Tools for Django',
      author='Michael Winter',
      author_email='mail@michael-winter.me.uk',
      license='GPLv2+',
      py_modules=['xdelta'],
      ext_modules=[Extension('_xdelta', ['deltamodule.c', 'xdelta3.c', 'buffer.c', 'lru_cache.c'],
                             define_macros=[('HAVE_CONFIG_H', '1')])],
      test_suite='tests')
