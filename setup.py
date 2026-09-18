"""Nearly all packaging config lives in pyproject.toml - this file exists
only to mark the wheel as platform/ABI-specific.

chiaki_py.lib's compiled extension (built separately via CMake, see
scripts/build_wheel.ps1) is bundled as package-data rather than declared as
an ext_modules Extension, since setuptools doesn't build it - CMake does.
Without has_ext_modules() overridden below, setuptools has no signal that
this isn't a pure-Python package, and bdist_wheel would tag the output
py3-none-any: installable, and silently broken, on any platform/Python
version other than the one it was actually built for.
"""

from setuptools import setup
from setuptools.dist import Distribution


class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        return True


setup(distclass=BinaryDistribution)
