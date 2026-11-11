# The signs over the three entrances, on the inside of the main hall, read as the way out (owner, 2026-11-10):
# EXIT WEST, MAIN EXIT, EXIT EAST. The Babylon model's sign pictures (the inner doors' tex_3828, tex_3854, tex_3883, stored upside down
# as the model reads them) replaced by the new ones made from the same style (Tools/make_exit_signs: Arial bold 64 px,
# #F2F4F7 on #14202E, 1024 x 104). The Android edition draws them the same way (src/sections/terminal/doors.ts).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_exit_signs.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(ROOT, 'Art', 'Signs')
DIR = '/Game/Babylon/Terminal/terminal/Textures'
tools = unreal.AssetToolsHelpers.get_asset_tools()
# inside (the inner doors' signs, facing the hall): tex_3828, 3854, 3883 say Exit; the outer doors' (tex_3815, 3841,
# 3870, facing the kerb) keep their Entrance pictures, put back as they were
for n in ('3828', '3854', '3883', '3815', '3841', '3870'):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, 'sign_%s.png' % n); t.destination_path = DIR; t.destination_name = 'tex_' + n
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    unreal.log('ADP_SIGNS tex_%s replaced' % n)
unreal.SystemLibrary.quit_editor()
