# Tripod nose jack (to-do 364 A), our own model; the builder and its sources are in ../tripod_jack.py.
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tripod_jack import build
build('NoseJack')
