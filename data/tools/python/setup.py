"""Setup script for eiger_detector python package."""
from pkg_resources import require

import os
import sys
import ConfigParser
import versioneer
from setuptools import setup, find_packages

def get_requirements():
  with open('requirements.txt') as f:
    required = f.read().splitlines()

  return required

setup(name='eiger-data',
      version=versioneer.get_version(),
      cmdclass=versioneer.get_cmdclass(),
      description='Eiger Data',
      author='Matt Taylor',
      author_email='matthew.taylor@diamond.ac.uk',
      packages=find_packages(),
      install_requires=get_requirements(),
      zip_safe=False,
      entry_points={
          'console_scripts': [
               'eiger_meta_writer  = odin_data.meta_writer.meta_writer_app:main'
          ]
     },
)
