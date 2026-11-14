# The road network's data (owner, 2026-11-14; HANDOVER 3b, to-do 55): every road from the owner's map plans
# (Art/Maps/Airport-Road-Town-Overview-v11.png, Town-Plan-v7.png, Airport-Only-v11.png) written to Data/roads.json,
# which the game builds with MetaRoad Free when it starts (Source/.../RoadNetwork.cpp). Never placed by hand: run this
# again and the whole network is rebuilt the same.
#   python Tools/make_roads.py      (then copy Data/roads.json to Content/Data)
#
# A road: its path in airport plan metres (x east along the plan, y as in Data/airfield.json), and its lanes either side
# of the path, inside out, each {type, w (m)}: Driving, Bus, Parking, Sidewalk (kerb cm), Shoulder, Median, Restricted.
# Right-hand traffic: the right lanes run along the path, the left lanes against it.
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
roads = []


def road(id, pts, left, right, **kw):
    r = {'id': id, 'pts': [list(p) for p in pts], 'lanes': {'left': left, 'right': right}}
    r.update(kw)
    roads.append(r)


def lane(t, w, **kw):
    d = {'type': t, 'w': w}
    d.update(kw)
    return d


# ---- Part 1: one test road (only built with -RoadTest): two 3.5 m lanes, 2.5 m pavements both sides, on the open
# ground west of the fire station, curving, to see MetaRoad's road on our ground ----
TWO_LANE_TOWN = dict(left=[lane('Driving', 3.5), lane('Sidewalk', 2.5, kerb=15)],
                     right=[lane('Driving', 3.5), lane('Sidewalk', 2.5, kerb=15)])
# street lights (Faroese swan-neck posts, StreetLights.h): at the airport both sides, the approach one side, none from
# about 1 km out; the town both sides, narrow one-way streets one side. Zebra crossings at the forecourt.
road('TEST-PART1', [(-1262, 70), (-1285, 110), (-1315, 150), (-1330, 200), (-1332, 260)], test=True,
     lights=dict(spacing=35, sides='both'), crossings=[dict(at=30, w=3.0)], **TWO_LANE_TOWN)

out = ROOT / 'Data' / 'roads.json'
out.write_text(json.dumps({'source': 'Tools/make_roads.py', 'roads': roads}, indent=1), encoding='utf-8')
print('%s: %d roads' % (out, len(roads)))
