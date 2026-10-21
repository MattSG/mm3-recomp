# Retired menu-quad observation

These source fragments preserve the retired renderer-quad menu observation and
CPU vertex-tint extraction from XboxRecomp commit 852a10d. MM3 now uses native
menu geometry and focus. The active runtime exposes only its presentation
rectangle for translating native menu coordinates into window coordinates.

Reference only; these fragments are not compiled.

The unused pointer-to-D-pad callback and navigation policy are preserved in
input-pointer.c.txt from commit 6eef38a; native MM3 menu input does not use it.
