"""Setup script for eiger_daq python package."""

import sys
from setuptools import setup, find_packages

with open('requirements.txt') as f:
    required = f.read().splitlines()

setup(name='eiger_daq',
      version="0.3.1",
      description='Eiger Meta Data Filewriter',
      url='https//github.com/odin-detector/',
      author='Matt Taylor',
      author_email='matthew.taylor@diamond.ac.uk',
      packages=find_packages(),
      install_requires=required,
      entry_points={
        'console_scripts': [
            'eigerMetaListener = eigerMetaListener:main',
         ]
      },
      zip_safe=False,
)
