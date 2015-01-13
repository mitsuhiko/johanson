import os
import sys
import subprocess

import pytest
from cffi import FFI


here = os.path.abspath(os.path.dirname(__file__))
base = os.path.normpath(os.path.join(here, os.path.pardir, os.path.pardir))
ffi = FFI()

include = os.path.join(base, 'include')
header = os.path.join(include, 'johanson.h')

if sys.platform == 'darwin':
    debug_builds = os.path.join(base, 'build/native')
    lib_name = 'libjohanson-d.dylib'
    ffi.cdef(subprocess.Popen([
        'cc', '-E', '-DJHN_API=', '-DJHN_NOINCLUDE',
        header], stdout=subprocess.PIPE).communicate()[0])
elif sys.platform == 'win32':
    debug_builds = os.path.join(base, 'build/native')
    lib_name = 'johanson-d.dll'

    ffi.cdef(subprocess.Popen([
        'cl', '/EP', '/DJHN_API=', '/DJHN_NOINCLUDE',
        header], stdout=subprocess.PIPE).communicate()[0].replace('\r', ''))
else:
    raise NotImplementedError()


lib = ffi.dlopen(os.path.join(debug_builds, lib_name))


@pytest.fixture(scope='function')
def jhn(request):
    return lib
