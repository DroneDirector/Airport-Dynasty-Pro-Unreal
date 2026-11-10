# Builds the light mast scene (make_lightmast.py, its night look) and saves it as LightMast.blend next to this file.
#   blender -b --python save_blend.py
import os, runpy, sys, bpy
HERE = os.path.dirname(os.path.abspath(__file__))
sys.argv = [sys.argv[0], '--', HERE, '1', 'none']   # outdir, samples, no shots
runpy.run_path(os.path.join(HERE, 'make_lightmast.py'), run_name='__main__')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'LightMast.blend'))
print('SAVED', os.path.join(HERE, 'LightMast.blend'))
