"""Setup script for eiger_detector python package."""
from pkg_resources import require
require('dls_dependency_tree')

from dls_dependency_tree import tree as parser

import os
import sys
import ConfigParser
from setuptools import setup, find_packages

def get_dependency(module_root, dependecy):
  return parser.dependency_tree(module_path=module_root).macros[dependecy]

def get_requirements():

  # Create the config file for this module and the odin-data module
  file_path = os.path.abspath(__file__)
  this_root = file_path.split("data/tools/python")[0]
  odin_data_root = get_dependency(this_root, "ODINDATA")

  config = ConfigParser.ConfigParser()
  config.add_section('Plugin Paths')
  config.set("Plugin Paths", "odin-data_path", os.path.join(odin_data_root, "prefix/lib"))
  config.set("Plugin Paths", "eiger-detector_path", os.path.join(this_root, "prefix/lib"))
  config_file_path = os.path.join(this_root, "data/tools/python/odindataclient")
  config_file_path = os.path.join(config_file_path, "frameprocessorclient.cfg")
  with open(config_file_path, 'wb') as configfile:
    config.write(configfile)

  # Get the requirements
  with open('requirements.txt') as f:
    required = f.read().splitlines()

  return required

setup(name='eiger-data',
      version="1.3",
      description='Eiger Meta Data Filewriter',
      author='Matt Taylor',
      author_email='matthew.taylor@diamond.ac.uk',
      packages=find_packages(),
      install_requires=get_requirements(),
      entry_points={
        'console_scripts': [
            'eigerMetaListener = metalistener.eigerMetaListener:main',
         ]
      },
      zip_safe=False,
      package_data={'': ['frameprocessorclient.cfg']}
)
