import bpy
import mathutils
import sys
import statistics
import math


def import_skeleton(skeleton, coordinate_remap=None):
    """
    Imports a Skeleton object into Blender as an armature.
    Bone transforms are parent-relative (bind pose).

    Leaf bones (no children) will get a tail offset equal to the median bone length
    to keep proportions consistent.
    """

    armature_data = bpy.data.armatures.new("Skeleton")
    armature_obj = bpy.data.objects.new("Geom", armature_data)
    bpy.context.collection.objects.link(armature_obj)
    bpy.context.view_layer.objects.active = armature_obj
    bpy.ops.object.mode_set(mode='EDIT')

    bone_map = {}

    # ---------------------------------------------------------
    # Coordinate remapping helper
    # ---------------------------------------------------------
    def apply_remap_matrix(matrix):
        if coordinate_remap is None:
            return matrix
        if callable(coordinate_remap):
            return coordinate_remap(matrix)
        elif isinstance(coordinate_remap, mathutils.Matrix):
            return coordinate_remap @ matrix
        return matrix

    # ---------------------------------------------------------
    # Build global (bind) transforms
    # ---------------------------------------------------------
    def local_matrix(bone):
        q = bone.transform.quaternion
        p = bone.transform.position
        s = bone.transform.scale

        pos = mathutils.Vector(p[:3])
        scl = mathutils.Vector(s[:3])
        quat = mathutils.Quaternion((q[3], q[0], q[1], q[2]))  # (w,x,y,z)

        mat_loc = mathutils.Matrix.Translation(pos)
        mat_rot = quat.to_matrix().to_4x4()
        mat_scl = mathutils.Matrix.Diagonal((*scl, 1.0))
        return mat_loc @ mat_rot @ mat_scl

    global_mats = {}

    def compute_global(bone):
        if bone.name in global_mats:
            return global_mats[bone.name]
        if bone.parent:
            parent_mat = compute_global(bone.parent)
            global_mats[bone.name] = parent_mat @ local_matrix(bone)
        else:
            global_mats[bone.name] = local_matrix(bone)
        return global_mats[bone.name]

    for bone in skeleton.bones:
        compute_global(bone)

    # ---------------------------------------------------------
    # Create edit bones
    # ---------------------------------------------------------
    for bone in skeleton.bones:
        edit_bone = armature_data.edit_bones.new(bone.name)
        bone_map[edit_bone.name] = edit_bone
        bone.name = edit_bone.name

    # ---------------------------------------------------------
    # Compute head positions and children
    # ---------------------------------------------------------
    head_positions = {}
    children_map = {}
    for bone in skeleton.bones:
        gmat = apply_remap_matrix(global_mats[bone.name])
        head_positions[bone.name] = gmat.to_translation()
        if bone.parent:
            children_map.setdefault(bone.parent.name, []).append(bone.name)

    # Compute lengths of bones with children
    bone_lengths = []
    for bone_name, edit_bone in bone_map.items():
        child_names = children_map.get(bone_name, [])
        if child_names:
            # average vector to children
            avg = mathutils.Vector((0, 0, 0))
            for cname in child_names:
                avg += head_positions[cname]
            avg /= len(child_names)
            length = (avg - head_positions[bone_name]).length
            if length > 0:
                bone_lengths.append(length)

    # Use median length for leaf bones
    median_length = statistics.median(bone_lengths) if bone_lengths else 0.1

    # ---------------------------------------------------------
    # Assign heads, tails, parents
    # ---------------------------------------------------------
    for bone in skeleton.bones:
        edit_bone = bone_map[bone.name]
        edit_bone.head = head_positions[bone.name]

        child_names = children_map.get(bone.name, [])
        if child_names:
            # average child head positions
            avg = mathutils.Vector((0, 0, 0))
            for cname in child_names:
                avg += head_positions[cname]
            avg /= len(child_names)
            tail = avg
        else:
            # Leaf bone: point along global Y axis (Blender up)
            tail = head_positions[bone.name] + mathutils.Vector((0, 0, median_length))

        # Avoid degenerate bones
        if (tail - edit_bone.head).length < 1e-5:
            tail = edit_bone.head + mathutils.Vector((0, 0, median_length))

        edit_bone.tail = tail
        edit_bone.roll = 0.0

        if bone.parent:
            edit_bone.parent = bone_map[bone.parent.name]

    bpy.ops.object.mode_set(mode='OBJECT')
    return armature_obj