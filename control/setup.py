from setuptools import setup, find_packages
import versioneer

# Define the required odin-control package version
required_odin_version = "0.3.1"

# Define installation requirements based on odin version 
install_requires = [
    "odin=={:s}".format(required_odin_version)
]

# Define installation requirements based on odin version 
dependency_links = [
    "https://github.com/odin-detector/odin-control/zipball/{0}#egg=odin-{0}".format(
        required_odin_version
    )
]

tests_require = [
    "nose",
    "coverage",
    "mock"
]

setup(
    name="eiger-control",
    version=1.3,
    description="Eiger plugins for the Odin control framework",
    url="https://github.com/dls-controls/eiger-detector",
    author="Gary Yendell",
    author_email="gary.yendell@diamond.ac.uk",
    packages=find_packages(),
    install_requires=["odin-control", "odin-data", "configparser"],
    extras_require={
      "test": ["nose", "coverage", "mock"],
    },
    entry_points={
        "console_scripts": [
            "eiger_odin  = odin.server:main",
        ]
    },
)