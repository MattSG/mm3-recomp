# MM3 distance fog GPU regression

Build this directory with Visual Studio CMake, SDL3_DIR and XBOXRECOMP_DXC,
then run `ctest --test-dir <build> -C Release --output-on-failure`.

The three cases exercise production MM3 fog over actual D3D11, D3D11 MSAA
and SDL GPU renderers. They retain near/mid/far depth, destination-alpha
preservation, sky fill, world-up horizon fade, invalid range rejection and
subsequent UI drawing checks from the former runtime binding suite.

The test files include the generic binding fixtures for device setup, triangles
and readback, then add MM3-owned shader/curve assertions. The fork builds its
own suite without any MM3 source, shaders or cases. Its host_draw checks cover
borrowed handles, ordered rendering, rejected callbacks and guest state recovery.
