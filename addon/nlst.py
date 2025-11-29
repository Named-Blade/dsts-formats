def write_nlst(geom):
    lines = []
    
    for bone in geom.skeleton.bones:
        if bone.name != "mvglDummy0000":
            parent = (f"{bone.parent.name_hash:08X}" if bone.parent else "null").lower()
            hash = (f"{bone.name_hash:08X}").lower()
            type = "geometry" if bone.is_geometry else "joint"
            lines.append(f"{bone.name}, {hash}, {parent}, {type}")
        
    for mesh in geom.meshes:
        hash = (f"{mesh.name_hash:08X}").lower()
        lines.append(f"{mesh.name}, {hash}, null, mesh")
    
    return "\r\n".join(lines) + "\r\n"