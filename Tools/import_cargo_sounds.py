# The cargo terminal's sounds into the game (Art/Sounds/CargoTerminal, CC0 from Freesound - SOURCES.md there):
# /Game/CargoTerminal/Sounds/Cg* (the Cg*Hum / Vent / Rain ones loop; CargoSounds.cpp plays them).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_cargo_sounds.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
SD = '/Game/CargoTerminal/Sounds'
def log(*a): unreal.log('ADP_CGSND ' + ' '.join(str(x) for x in a))
for _f in glob.glob(os.path.join(ROOT, 'Content', 'CargoTerminal', 'Sounds', '*.uasset')): os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)
if not EAL.does_directory_exist(SD): EAL.make_directory(SD)
for n, looping in (('CgRainRoofOut', True), ('CgRainRoofIn', True), ('CgHallHum', True), ('CgPlantHum', True), ('CgOfficeVent', True), ('CgPrinter', False)):
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'CargoTerminal', n + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + n)
    if w: w.set_editor_property('looping', looping); EAL.save_loaded_asset(w); log('sound', n, 'ok', w.get_editor_property('duration'))
    else: log('sound', n, 'MISSING')
log('done')
unreal.SystemLibrary.quit_editor()
