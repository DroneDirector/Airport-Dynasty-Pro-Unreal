# Nanite copies of Unreal's basic shapes (owner, 2026-11-20, to-do 149: the hangars and the control tower are built from
# thousands of engine cubes and cylinders, which have no Nanite data - their shadows went through the slow non-Nanite path):
# /Game/Shapes/SM_Cube and SM_Cylinder, the same shape, size and UVs, Nanite on. The code uses them (ADPShapes::Cube()).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_nanite_shapes.py" -unattended -nullrhi
import os, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EL = unreal.EditorAssetLibrary
for src, dst in (('/Engine/BasicShapes/Cube', '/Game/Shapes/SM_Cube'), ('/Engine/BasicShapes/Cylinder', '/Game/Shapes/SM_Cylinder')):
    f = os.path.join(ROOT, 'Content', dst[len('/Game/'):] + '.uasset')
    if os.path.exists(f):
        os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
        EL.delete_asset(dst)
    m = EL.duplicate_asset(src, dst)
    ns = m.get_editor_property('nanite_settings')
    ns.set_editor_property('enabled', True)
    m.set_editor_property('nanite_settings', ns)
    EL.save_loaded_asset(m)
    unreal.log('ADP_SHAPES %s -> %s Nanite %s' % (src, dst, m.get_editor_property('nanite_settings').get_editor_property('enabled')))
