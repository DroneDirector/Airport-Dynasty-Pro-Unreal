# Terminal part 2a (owner approved 2026-11-23, blueprint v2 sheets 2-4 and 9c): the Floor 2 concourse's numbers, one place for
# make_concourse.py (the models), make_terminal_east.py (the lining of the gate walkway's wall), Art/TerminalAnnex/
# make_pier_extension.py (that wall, x 242..551), Tools/terminal_concourse_edit.py (the layout). Plan metres; y from the ground.
F2 = 6.01
TOP = 10.2                                             # the partitions' tops (4.2 m)
# the walkway's south wall (z 52) opened: x0, x1, head (y)
BIG = [(231.5, 239.3), (262.0, 286.0), (344.0, 396.0), (474.0, 518.0)]
LANES = [(228.3, 230.7), (290.1, 291.9), (398.1, 399.9), (470.1, 471.9), (520.1, 521.9)]    # the transfers' way out; the lanes to toilets, lounge, shops
HB, HL, HD = 10.21, 8.91, 8.36                         # heads: big openings, lanes, escape doors
ESC_N = [298.5, 318.0, 331.0, 412.0, 436.0, 460.0, 536.0]                                   # escape doors to the walkway (sheet 9c), closed
ESC_S = [234.5, 244.0, 252.0, 266.0, 282.0, 314.0, 333.0, 436.0, 460.0]                      # ... to the lane / gallery (352, 388, 480, 512 stand in open ways)
DW = 1.0                                               # half an escape door's opening (the door's frame stands in it)
def holes():
    return sorted([(a, b, HB) for a, b in BIG] + [(a, b, HL) for a, b in LANES] + [(x - DW, x + DW, HD) for x in ESC_N])
