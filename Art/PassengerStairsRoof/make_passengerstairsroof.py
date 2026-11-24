# The roofed passenger stairs (to-do 212): exactly the open ones (Art/PassengerStairs/make_passengerstairs.py: same parts, points, materials and textures) with the
# roof of roof_parts.py over both flights and the platform.   blender -b --python Art/PassengerStairsRoof/make_passengerstairsroof.py [-- --pic DIR]
# After Tools/import_model.py: Tools/share_model_materials.py PassengerStairsRoof PassengerStairs (its meshes use the open one's materials; the copies go).
import os
_p = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'PassengerStairs', 'make_passengerstairs.py')
exec(compile(open(_p, encoding='utf-8').read(), _p, 'exec'), {'__name__': '__main__', '__file__': _p, 'ROOF': True})
