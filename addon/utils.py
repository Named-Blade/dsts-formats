import mathutils
import math

def unflop(matrix):
    rot = mathutils.Matrix.Rotation(math.radians(90), 4, 'X')
    return rot @ matrix

def flop(matrix):
    rot = mathutils.Matrix.Rotation(math.radians(-90), 4, 'X')
    return rot @ matrix