#!BPY

"""
# Name: 'xml mesh type (.xml)...'
# Blender: 249
# Group: 'Export'
# Tooltip: 'Export to xml file format.'
"""
__author__ = "Coos Baakman"
__version__ = "2.0"
__bpydoc__ = """\
This script exports a Blender mesh to an xml formatted file
format.
"""

import Blender
from Blender import Types, Object, Material,Armature,Mesh,Modifier
from Blender.Mathutils import *
from Blender import Draw, BGL
from Blender.BGL import *
from Blender.IpoCurve import ExtendTypes, InterpTypes
import math
import xml.etree.ElementTree as ET

EXTENSION='.xml'

def callback_sel (filename):

    # A file path has been selected to export to

    if not filename.endswith (EXTENSION):
        filename += EXTENSION
    xexport = Exporter()
    xexport.exportSelected (filename)

def draw ():

    # Make the menu in Blender: a single button to press.
    glClearColor (0.55,0.6,0.6,1)
    glClear (GL_COLOR_BUFFER_BIT)
    glRasterPos2d (20,75)
    Draw.Button ("Export Selected",1,20,155,100,30,"export the selected object")

def event (evt, val):
    if evt == Draw.ESCKEY:
        Draw.Exit()

def button_event (evt):

    if evt == 1:
        fname = Blender.sys.makename (ext = EXTENSION)
        Blender.Window.FileSelector (callback_sel, "Export xml mesh", fname)
        Draw.Exit ()

# Regeister the exported menu:
Draw.Register (draw, event, button_event)

# Makes a path a basename without extension.
def stripFilename (filename):
    return os.path.basename (os.path.splitext (filename)[0])

# Removes whitespaces.
def makeAllowedNameString (name):
    return ' %s ' % name.replace (' ', '_')

# Find an armature for a given mesh object:
def findMeshArmature (mesh_obj):

    for mod in mesh_obj.modifiers:

        if mod.type == Modifier.Types.ARMATURE:

            # Return as an object:
            return mod [Modifier.Settings.OBJECT]

    return None

# Find a mesh, to which the armature belongs
def findArmatureMesh (arm_obj):

    arm_name = arm_obj.getData(True, False)

    for obj in Object.Get():

        # Skip objects that are not meshes
        if type (obj.getData(False, True)) != Types.MeshType:
            continue

        for mod in obj.modifiers:

            # Skip modifiers that are not armatures
            if mod.type != Modifier.Types.ARMATURE:
                continue

            objmodobj_name = mod [Modifier.Settings.OBJECT].getData(True, False)
            if objmodobj_name == arm_name:
            # This is my armature !

                return obj

    return None

# Tells if we need to flip face direction when applying this matrix.
def getFaceFactor (matrix):

    f = 1
    for i in range (3):
        for j in range (3):
            f *= matrix[i][j]

    if f < 0:
        return -1
    else:
        return 1

# Makes a string representation of a blender interpolation type
def interptypes2tring (i):

    if i == InterpTypes.CONST:
        return 'const' # curve remains constant from current BezTriple knot
    elif i == InterpTypes.LINEAR:
        return 'linear' # curve is linearly interpolated between adjacent knots
    elif i == InterpTypes.BEZIER:
        return 'bezier' # curve is interpolated by a Bezier curve between adjacent knots
    else:
        raise Exception ("not an interp type: %i" % i)

# Makes a string representation of a blender extension type
def extend2string (e):

    if e == ExtendTypes.CONST:
        return 'const' # curve is constant beyond first and last knots
    elif e == ExtendTypes.EXTRAP:
        return 'extrap' # curve maintains same slope beyond first and last knots
    elif e == ExtendTypes.CYCLIC:
        return 'cyclic' # curve values repeat beyond first and last knots
    elif e == ExtendTypes.CYCLIC_EXTRAP:
        return 'cyclic_extrap' # curve values repeat beyond first and last knots, but while retaining continuity
    else:
        raise Exception ("not an extend type: %i" % e)

class Exporter (object):

    # Code used in this exported is based on the following documentation:
    # http://www.blender.org/api/248PythonDoc/

    def __init__(self):

        # Switch y and z:
        self.transformVertex = Matrix ([1,0,0,0],
                                       [0,0,1,0],
                                       [0,1,0,0],
                                       [0,0,0,1])

        # Used on bone translocations:
        self.transformLoc = Matrix(self.transformVertex)
        for i in range(3):
            self.transformLoc [3][i] = 0
            self.transformLoc [i][3] = 0
        self.transformLoc [3][3] = 1

        # Used on normals and bone rotations:
        self.transformRot = Matrix(self.transformLoc)
        self.transformRot.invert()
        self.transformRot.transpose()

        self.color_channels = ['r', 'g', 'b', 'a']


    def exportSelected(self, filename):

        # Find the selected mesh (and maybe its armature)
        mesh_obj = None
        arm_obj = None

        # The selected object might actually be an armature, with a mesh attached to it.
        selected = Object.GetSelected()

        if len(selected) == 0:
            raise Exception ("No selection")

        sel_data = selected[0].getData(False,True)

        if type(sel_data) == Types.MeshType:

            mesh_obj = selected [0]
            arm_obj = findMeshArmature (mesh_obj)

        elif type(sel_data) == Types.ArmatureType:

            arm_obj = selected [0]
            mesh_obj = findArmatureMesh (arm_obj)

            if not mesh_obj:
                raise Exception ("No mesh attached to selected armature")
        else:
            raise Exception ("The selected object %s is not a mesh or armature." % str(type(selected [0])))

        root = self.toXML (mesh_obj, arm_obj)

        f = open(filename, "w")
        f.write(ET.tostring(root))
        f.close()

    def toXML (self, mesh_obj, arm_obj=None):

        mesh = self.meshToXML (mesh_obj)
        if arm_obj:
            tag_armature = self.armatureToXML(arm_obj, mesh_obj)
            mesh.append (tag_armature)

        return mesh


    def meshToXML (self, mesh_obj):

        face_factor = -getFaceFactor (self.transformRot)

        mesh = mesh_obj.getData(False, True)
        root = ET.Element ('mesh')

        # Convert the mesh's vertices to xml tags:
        tag_vertices = ET.Element ('vertices')
        root.append (tag_vertices)

        for vertex in mesh.verts:

            tag_vertex = self.vertexToXML (vertex)
            tag_vertices.append(tag_vertex)

        # Convert the mesh's faces to xml tags:
        tag_faces = ET.Element ('faces')
        root.append (tag_faces)

        # Divide the mesh in subsets, based on their materials.
        # This allows us to render different parts of the mesh with different OpenGL settings,
        # like material-specific ones.
        subset_faces = {}
        for face in mesh.faces:

            tag_face = self.faceToXML (face, face_factor)
            tag_faces.append (tag_face)

            if face.mat not in subset_faces:
                subset_faces [face.mat] = []

            subset_faces [face.mat].append (face)

        # Did we even find materials?
        if len (subset_faces) > 0:

            tag_subsets = ET.Element ('subsets')
            root.append (tag_subsets)

            for material_i in subset_faces:

                if material_i < len (mesh.materials):

                    material = mesh.materials [material_i]
                else:
                    # material index is not found in mesh
                    material = None

                tag_subset = self.subsetToXML (material, subset_faces [material_i])
                tag_subset.attrib ['id'] = str (material_i)
                tag_subsets.append (tag_subset)

        return root


    def subsetToXML (self, material, faces):

        # Take the material color settings.
        # We might need to add more in the future..
        if material:

            name = material.name
            diffuse  = material.rgbCol + [1.0]
            specular = material.specCol + [1.0]
            emission = [material.emit * diffuse [i] for i in range (4)]

        else:

            # No material associated with subset, use default values:

            name = None
            diffuse  = [1.0, 1.0, 1.0, 1.0]
            specular = [0.0, 0.0, 0.0, 0.0]
            emission = [0.0, 0.0, 0.0, 0.0]

        tag_subset = ET.Element ('subset')

        # Add name to tag, makes it easier to find back in the file.
        if name:
            tag_subset.attrib ['name'] = str(name)

        # Convert material colors to xml attributes

        c = self.color_channels # strings, r,g,b,a

        tag_diffuse = ET.Element('diffuse')
        tag_subset.append(tag_diffuse)
        tag_specular = ET.Element('specular')
        tag_subset.append(tag_specular)
        tag_emission = ET.Element('emission')
        tag_subset.append(tag_emission)

        for i in range (4):
            tag_diffuse.attrib[c[i]] = '%.4f' % diffuse [i]
            tag_specular.attrib[c[i]] = '%.4f' % specular [i]
            tag_emission.attrib[c[i]] = '%.4f' % emission [i]

        # Register all the subset's faces in xml tags:

        tag_faces = ET.Element('faces')
        tag_subset.append(tag_faces)
        for face in faces:
            if len (face.verts) == 4:
                tag_face = ET.Element('quad')
            elif len (face.verts) == 3:
                tag_face = ET.Element('triangle')

            tag_faces.append (tag_face)
            tag_face.attrib ['id'] = str(face.index)

        return tag_subset


    def faceToXML (self, face, face_factor):

        # Only faces with 3 or 4 faces are supported.
        if len(face.verts) == 3:
            tag_face = ET.Element('triangle')
        elif len(face.verts) == 4:
            tag_face = ET.Element('quad')
        else:
            raise Exception ("encountered a face with %i vertices" % len(face.v))

        tag_face.attrib['id'] = str(face.index)

        # Order determines which side is front.
        order = range(len(face.verts))
        if face_factor < 0:
            order.reverse()

        for i in order:
            tag_corner = ET.Element('corner')
            tag_face.append (tag_corner)

            # Reference to vertex:
            tag_corner.attrib['vertex_id'] = str(face.verts[i].index)

            # Texture coordinates:
            try:
                tag_corner.attrib['tex_u'] = '%.4f' % face.uv[i][0]
                tag_corner.attrib['tex_v'] = '%.4f' % face.uv[i][1]
            except:
                tag_corner.attrib['tex_u'] = '0.0'
                tag_corner.attrib['tex_v'] = '0.0'

        return tag_face


    def vertexToXML (self, vertex):

        # Register vertex properties as attributes.
        # The entire vertex will be one xml tag.
        tag_vertex = ET.Element ('vertex')

        tag_vertex.attrib ['id'] = str (vertex.index)
        co = self.transformVertex * vertex.co
        no = self.transformVertex * vertex.no

        # Vertex position in space:
        tag_pos = ET.Element ('pos')
        tag_pos.attrib['x'] = '%.4f' % co [0]
        tag_pos.attrib['y'] = '%.4f' % co [1]
        tag_pos.attrib['z'] = '%.4f' % co [2]
        tag_vertex.append (tag_pos)

        # Vertex normal, used for lighting:
        tag_pos = ET.Element ('norm')
        tag_pos.attrib['x'] = '%.4f' % no [0]
        tag_pos.attrib['y'] = '%.4f' % no [1]
        tag_pos.attrib['z'] = '%.4f' % no [2]
        tag_vertex.append (tag_pos)

        return tag_vertex


    def armatureToXML (self, arm_obj, mesh_obj):

        armature = arm_obj.getData ()
        mesh = mesh_obj.getData (False, True)
        vertex_groups = mesh.getVertGroupNames ()

        arm_matrix = arm_obj.getMatrix()

        # Any normal mesh transformation is expected to be invertible:
        try:
            mesh_inv_matrix = mesh_obj.getMatrix().invert()
        except:
            raise Exception ("mesh matrix isn't invertible")

        arm2mesh = mesh_inv_matrix * arm_matrix

        root = ET.Element('armature')
        bones_tag = ET.Element('bones')
        root.append (bones_tag)

        # Iterate over the armature's bones:
        bone_list = []
        for bone_name in armature.bones.keys():

            bone_tag = ET.Element('bone')
            bones_tag.append (bone_tag)

            # A tag for every bone:
            bone = armature.bones [bone_name]
            bone_tag.attrib['id'] = bone_name
            bone_list.append (bone)

            # Export position of bone head and tail in mesh space:
            headpos = self.transformVertex * arm2mesh * (bone.head['ARMATURESPACE'])
            tailpos = self.transformVertex * arm2mesh * (bone.tail['ARMATURESPACE'])
            bone_tag.attrib['x'] = '%.4f' % headpos[0]
            bone_tag.attrib['y'] = '%.4f' % headpos[1]
            bone_tag.attrib['z'] = '%.4f' % headpos[2]
            bone_tag.attrib['tail_x'] = '%.4f' % tailpos[0]
            bone_tag.attrib['tail_y'] = '%.4f' % tailpos[1]
            bone_tag.attrib['tail_z'] = '%.4f' % tailpos[2]

            bone_tag.attrib['weight'] = '%.4f' % bone.weight

            # Reference to parent bone:
            if bone.hasParent():
                bone_tag.attrib['parent_id'] = bone.parent.name

            if bone_name in mesh.getVertGroupNames():

                # Register references to the verices that this bone pulls at:

                vertices_tag = ET.Element('vertices')
                bone_tag.append(vertices_tag)

                for vert_i in mesh.getVertsFromGroup(bone_name, False):
                    vertex_tag = ET.Element('vertex')
                    vertices_tag.append (vertex_tag)
                    vertex_tag.attrib['id'] = str(vert_i)

        # The armature might have animations associated with it.
        animations_tag = self.armatureAnimationsToXML (arm_obj, arm2mesh, bone_list)
        root.append (animations_tag)

        return root


    def armatureAnimationsToXML (self, arm_obj, arm2mesh, bone_list):

        root = ET.Element ('animations')

        # Check every action to see if it's an animation:
        for action in Armature.NLA.GetActions ().values ():

            animation_tag = ET.Element ('animation')

            nLayers = 0

            # Check every known bone:
            for bone in bone_list:

                action.setActive (arm_obj)

                if bone.name not in action.getChannelNames ():
                    continue

                ipo = action.getChannelIpo (bone.name)

                if len (ipo.curves) <= 0:
                    # Bone doesn't move in this action
                    continue

                parent_ipo = None
                if bone.hasParent():
                    parent_ipo = action.getChannelIpo (bone.parent.name)

                # A layer for every moving bone:

                layer_tag = ET.Element('layer')
                layer_tag.attrib ['bone_id'] = bone.name
                animation_tag.append (layer_tag)

                nLayers += 1

                # Find all key frames for this bone:
                frame_numbers = []
                for curve in ipo.curves:
                    for bez_triple in curve.bezierPoints:

                        frame_num = int (bez_triple.pt [0])

                        if frame_num not in frame_numbers:
                            frame_numbers.append (frame_num)

                # Iterate over key frames
                for frame_num in frame_numbers:

                    # Get pose at specified frame number:
                    Blender.Set ("curframe", frame_num)
                    pose_bone = arm_obj.getPose().bones [bone.name]

                    m = pose_bone.localMatrix
                    if pose_bone.parent:

                        # We want the bone rotation relative to its parent.
                        # This was the only known way to do it:
                        n = pose_bone.parent.localMatrix.rotationPart().resize4x4()
                        n.invert()
                        m = m * n

                    # Register frame number, position and rotation of the bone:

                    q = m.toQuat()
                    rot = self.transformRot * arm2mesh * Vector (q.x, q.y, q.z, q.w)

                    loc = self.transformLoc * arm2mesh * pose_bone.loc

                    key_tag = ET.Element ('key')
                    layer_tag.append (key_tag)

                    key_tag.attrib ['frame'] = str (frame_num)
                    key_tag.attrib ['x'] = '%.4f' % loc.x
                    key_tag.attrib ['y'] = '%.4f' % loc.y
                    key_tag.attrib ['z'] = '%.4f' % loc.z
                    key_tag.attrib ['rot_x'] = '%.4f' % rot.x
                    key_tag.attrib ['rot_y'] = '%.4f' % rot.y
                    key_tag.attrib ['rot_z'] = '%.4f' % rot.z
                    key_tag.attrib ['rot_w'] = '%.4f' % rot.w

            if nLayers > 0: # means the action applies to this armature

                animation_tag.attrib ['name'] = action.name
                length = max (action.getFrameNumbers())

                animation_tag.attrib ['length'] = str (length)
                root.append (animation_tag)

        return root
