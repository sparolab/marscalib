from setuptools import find_packages
from setuptools import setup

setup(
    name='sphere_calibration',
    version='0.0.0',
    packages=find_packages(
        include=('sphere_calibration', 'sphere_calibration.*')),
)
