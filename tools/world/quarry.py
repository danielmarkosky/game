"""The Quarry, under the Hollows: where the stone for the camp and the Lantern was cut, and fired.
Down through a trapdoor of planks in the Hollows' floor, a long drop into the Rigging, a shaft with
nothing to climb or swing from; on through the Kiln, where the fires breathe; to the Cutting, at
the end of it, where the riggers kept their knot. Back up the Rigging, the knot is the only way:
tie the rope off and climb it. (Or sit on the bench in the Cutting, and travel.)"""
from world.build import region, room, ground, roof, pit

region('quarry')


def slick(r, x0, x1, y0, y1):
    return r.fill(x0, x1, y0, y1, 'o')


# ----------------------------------------------------------------------------------------------
# The Rigging: a tall shaft, its walls and roof slick stone, and ledges up it, each with slick
# faces and undersides and hook blocks at one corner. You come down the middle, past them all, to
# the floor. Back up: hook the next ledge's corner, and reeling in only hangs you under it, too low
# to get onto it; tie the rope off on the ledge you're on, and climb the line. Up the last one, off
# the top ledge and up through the trapdoor. A gem on the third ledge.
# ----------------------------------------------------------------------------------------------

rig = room('rigging', 'The Rigging', 'cave', 40, 42)
ground(rig, 3)
slick(rig, 0, 0, 4, 41).fill(39, 39, 4, 41, 'o').fill(0, 39, 41, 41, 'o')   # nothing to hook up here
rig.fill(28, 38, 4, 10, '#')                    # A, on the floor on the right
slick(rig, 28, 28, 4, 8).fill(28, 28, 9, 10, '%')
rig.fill(17, 23, 15, 17, '#')                   # B, out in the middle
slick(rig, 17, 23, 15, 15).fill(17, 17, 16, 17, 'o').fill(23, 23, 16, 17, '%')
rig.fill(28, 35, 22, 24, '#')                   # C, on the right again
slick(rig, 28, 35, 22, 22).fill(35, 35, 23, 24, 'o').fill(28, 28, 23, 24, '%')
rig.fill(17, 23, 29, 31, '#')                   # D, in the middle
slick(rig, 17, 23, 29, 29).fill(17, 17, 30, 31, 'o').fill(23, 23, 30, 31, '%')
rig.fill(7, 12, 35, 37, '#')                    # and the top one, by the trapdoor
slick(rig, 7, 12, 35, 35).fill(7, 7, 36, 37, 'o').fill(12, 12, 36, 36, 'o').put(12, 37, '%')
rig.door('T', 13, 3).door('L', 4)
rig.put(14, 4, 'S')
rig.put(32, 25, '*')                            # on C

rig.props((5, 4, 'mine_frame'), (24, 4, 'rubble'), (33, 11, 'planks'), (20, 18, 'rubble'), (31, 25, 'wheelbarrow'),
          (19, 32, 'bones'), (9, 38, 'chains'), (26, 40, 'hanging_lantern'), (36, 40, 'chains'), (21, 14, 'hanging_lantern'))

# ----------------------------------------------------------------------------------------------
# The Kiln: a long low hall where the stone was fired, and the fires still breathe: flues, columns
# of fire floor to roof, burning a while, then cold a while, flickering just before they light.
# In on the right past one; then two pits of spikes, a flue standing in each (two in the second),
# and hook blocks in the roof between: swing across while they're cold, or the rope burns through,
# or you do. A gem over the second pit, low: swing low for it.
# ----------------------------------------------------------------------------------------------

kiln = room('kiln', 'The Kiln', 'ember', 44, 24)
ground(kiln, 3)
roof(kiln, 10)
kiln.put(37, 4, 'F', period='3', phase='0', flue='1')       # one to walk past
pit(kiln, 26, 34, bottom=1, top=3)
kiln.fill(30, 31, 9, 9, '%')                    # hook blocks over the first pit
kiln.put(28, 1, 'F', period='4', phase='0.5', flue='1')     # and a flue in it, on the far side
pit(kiln, 9, 18, bottom=1, top=3)
kiln.fill(13, 14, 9, 9, '%')                    # hook blocks over the second
kiln.put(11, 1, 'F', period='4', phase='0.5', flue='1')     # flues either side, together
kiln.put(16, 1, 'F', period='4', phase='0.5', flue='1')
kiln.door('R', 4).door('L', 4)
kiln.put(40, 4, 'S')
kiln.put(13, 4, '*')                            # over the second pit, low

kiln.props((41, 4, 'cold_fire'), (22, 4, 'anvil'), (3, 4, 'cold_fire'), (6, 9, 'chains'), (23, 9, 'chains'), (35, 9, 'chains'))

# ----------------------------------------------------------------------------------------------
# The Cutting: the end of the quarry. Up on a ledge too high to jump to, slick-faced, under a slick
# roof, the riggers' knot, and a bench. Below it, a flue, and beyond that a block of stone: hook it
# from under the ledge while the flue's cold and reel it in (it comes to your feet; the rope burns
# if the flue lights first); then it's a step up.
# ----------------------------------------------------------------------------------------------

cut = room('cutting', 'The Cutting', 'cave', 40, 24)
ground(cut, 3)
roof(cut, 18)
slick(cut, 1, 14, 18, 18)                       # no hooking over the ledge
cut.fill(1, 8, 4, 8, '#')                       # the ledge
slick(cut, 8, 8, 4, 8)
cut.put(3, 9, 'P', ability='tie_off')
cut.put(5, 9, 'B')
cut.put(13, 4, 'F', period='4', phase='0.75', flue='1')
cut.put(17, 4, 'O')                             # the stone
cut.door('R', 4)
cut.put(36, 4, 'S')
cut.put(30, 13, '*')                            # up under the roof on the right: the long rope

cut.fill(28, 32, 17, 17, '%')                   # hook blocks for it
cut.props((2, 9, 'lantern_post'), (8, 9, 'signpost'), (22, 4, 'rubble'), (26, 4, 'mine_frame'), (33, 4, 'wheelbarrow'),
          (37, 4, 'planks'), (20, 17, 'stalactites'), (35, 17, 'stalactites'))
