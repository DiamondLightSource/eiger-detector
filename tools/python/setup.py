"""Setup script for eiger_detector python package."""

import sys
from setuptools import setup, find_packages

with open('requirements.txt') as f:
    required = f.read().splitlines()

setup(name='eiger_detector',
      version="1.0.0",
      description='Eiger Meta Data Filewriter',
      author='Matt Taylor',
      author_email='matthew.taylor@diamond.ac.uk',
      packages=find_packages(),
      install_requires=required,
      entry_points={
        'console_scripts': [
            'eigerMetaListener = metalistener.eigerMetaListener:main',
         ]
      },
      zip_safe=False,
)
