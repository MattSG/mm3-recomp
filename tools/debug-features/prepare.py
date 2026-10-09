"""Install probes into this worktree's private HD archive, keeping its original.

Run once, while this experiment's game is stopped. The primary checkout is
never read or written. Request a probe by writing its number to the file named
by MM3_DEBUG_FEATURE_REQUEST while a car is active.
"""
from pathlib import Path
import hashlib
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PROBES = {
    0: 'gameLog("FEATURE baseline\\n")',
    1: 'toggleDebugHud(1)',
    2: 'menuCreate("debugmenu")',
    3: 'cameraExtendedModes()',
    4: 'renderFacadeSetMaxFloors(3)',
    5: 'gameRunScript("engineprofiler.lua")',
    6: 'aiDebugSetPedestrianCount(1)',
    7: 'gameRunScript("E3settings.lua")',
    8: 'menuDestroy("debugmenu")\ntoggleDebugHud(0)\nrenderFacadeSetMaxFloors(2)\ncameraBindToPlayer()',
    9: 'cameraBindToInput()',
}

def prepare():
    archive = ROOT / 'game_files/Data/Data_hd.zip'
    backup = archive.with_suffix('.zip.before-features')
    if not backup.exists():
        shutil.copy2(archive, backup)
        assert hashlib.sha256(backup.read_bytes()).digest() == hashlib.sha256(archive.read_bytes()).digest()
    temporary = archive.with_suffix('.zip.new')
    # Preserve the title's original local headers, offsets and alignment.
    shutil.copy2(backup, temporary)
    with zipfile.ZipFile(temporary, 'a') as target:
        for number, body in PROBES.items():
            target.writestr(f'Scripts/feature_probe_{number}.lua',
                            'function onInit()\n' + body + '\nend\n')
    with zipfile.ZipFile(temporary) as check:
        assert check.testzip() is None
        assert all(f'Scripts/feature_probe_{n}.lua' in check.namelist() for n in PROBES)
        with zipfile.ZipFile(backup) as original:
            assert all(check.getinfo(e.filename).header_offset == e.header_offset
                       for e in original.infolist())
    temporary.replace(archive)
    print(f'Installed {len(PROBES)} probes; original preserved at {backup}')

if __name__ == '__main__':
    prepare()
