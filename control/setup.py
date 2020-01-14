from setuptools import setup, find_packages
from setuptools.command.test import test as TestCommand
import versioneer


class EigerTest(TestCommand):
    """
    Class to test the package using nose correctly.

    Based on description at http://bit.ly/2IRKDE6
    """

    def finalize_options(self):
        """
        Finalize command options.
        """

        TestCommand.finalize_options(self)
        self.test_args = []
        self.test_suite = True

    def run_tests(self):
        """
        Run the test suite using nose.

        """
        import nose
        nose.run_exit(argv=['nosetests'])


# Build the command class for setup, merging in versioneer and test commands
merged_cmdclass = versioneer.get_cmdclass()

merged_cmdclass.update({'test': EigerTest})

# Define the required odin-control package version
required_odin_version = "0.9.0"

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
    "mock",
    "configparser"
]

setup(
    name="eiger-control",
    cmdclass=merged_cmdclass,
    version=versioneer.get_version(),
    description="Eiger plugins for the Odin control framework",
    url="https://github.com/dls-controls/eiger-detector",
    author="Gary Yendell",
    author_email="gary.yendell@diamond.ac.uk",
    packages=find_packages(),
    install_requires=["odin-control", "odin-data", "configparser"],
    tests_require=tests_require,
    extras_require={"test": ["nose", "coverage", "mock"]},
    entry_points={
        "console_scripts": [
            "eiger_odin  = odin.server:main",
            "eiger_simulator = eiger.eiger_simulator:main"
        ]
    },
)
