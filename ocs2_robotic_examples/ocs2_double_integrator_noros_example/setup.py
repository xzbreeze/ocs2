#!/usr/bin/env python

from distutils.core import setup
from catkin_pkg.python_setup import generate_distutils_setup

setup_args = generate_distutils_setup(
     packages=['ocs2_double_integrator_noros_example'],
     package_dir={'': 'src'}
)

setup(**setup_args)
