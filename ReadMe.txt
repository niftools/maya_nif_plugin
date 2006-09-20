Maya 6.5 NIF File Translator

Currently supports importing static (non-animated) NIF files.  Export is *not* supported.  Pre-compiled versions are available for Maya 6.5 and 7.0 for Windows.  To use on other versions of Maya or other operating systems, you will need to compile from source.

WARNING:  

This importer is very incomplete, and will not import all data in the NIF file.  This includes animation, sound data, normals, particle systems, collision information, bounding boxes, and more!  It should be seen only as a fun thing to mess around with for now.  I can't promise that files you import today and save as mb files will be able to be re-exported flawlessly when I write the export functionality later.

KNOWN ISSUES:

* Niflib has endian issues that are not taken care of, so a PowerPC compile will be impossible.
* Importing Normals does not work on skinned meshes
* Meshes affected by multiple skin clusters cannot be exported yet.  Suggest deleting history and re-binding to the skeleton.

INSTALLATION:

place nifTranslator.mll in your plug-in folder.  Probably something like:

C:\Program Files\Alias\Maya6.5\bin\plug-ins

Place the nifTranslatorOpts.mel file in your scripts folder. Probably something like:

C:\Documents and Settings\*YourUserName*\My Documents\Maya\scripts

From within Maya:

From the menu, choose:  Window -> Settings/Preferences -> Plug-in Manager...

Look for nifTranslator.mll in the list of available plug-ins and check the two boxes next to it "load" and "auto load"

Now choose File->Open Scene, and click the Options... button.

In the File Type drop down box, select "NetImmerse Format"

Now you should see Texture Source Directory under File Type Specific Options.  Press the button to browse for the location of your textures.

You should now be able to open and import NIF files like any other scene file that Maya understands.

COMPILING FROM SOURCE:

This release uses Niflib 0.5.6, available separately.  You can compile both Niflib and the Maya plug-in together or separately by first compiling Niflib in static library form and then linking it with the Maya plug-in.

This code SHOULD be compatible with multiple versions of Maya.  If you successfully compile this code for a version of Maya other than 6.5 or 7.0, please send it to me so others can use it! =)

If you can program in C++, have Maya, and want to help make this plug-in better, come join our project at http://niftools.sourceforge.net!

OPTIONS:

You can access the options by choosing the option box (square) next to either
"File->Open Scene" or "File->Export All" rather than clicking the words.  Then choose
"NetImmerse Format" from the File Type drop down box.  The options should
appear.

=Texture Source Directory=
This is the directory that the plug-in will look for textures in.  It will
prepend this to any texture path read from the NIF file when creating the
FileTexture node.  This also has the effect of stripping this added information
back out of the NIF file if the beginning of the path matches the text entered
here.

=Use Name Mangling to Preserve Original Names=
This will cause characters which are invalid for Maya names to be replaced
with hex equivalents on import, and translated back to the original character
on export.  Disabling it will cause all invalid characters to be changed to
underscores on import and names will be exported with no changes.

=Attempt to Find Original Bind Pose=
NIF files do not store their original bind pose, but sometimes a reasonable
substitute can be calculated by trying to line up the bones with the pre-
deformed skins.  Checking this activates this option.  Clearing it will
cause the NIF file to import in whatever position it is actually in.

=Try to Use Original Normals=
Usually Maya will automatically calculate the normals of a polygon, but it
optionally supports the setting of "user normals."  This type of fixed normal
does not seem to work well with skins, but may allow you to retain the
original normal information from the NIF file exactly if successful.

=Ignore Ambient Color=
Most NIF files with textures have their ambient color set to white.  This
causes there to be no shading on the model when rendered with Maya.  Enable
this if you plan to render a NIF with textures.  NIF files without textures,
however, may lose important lighting information if this is enabled.

=Combine New Skins with Existing Skeleton=
This is useful when using the Import, rather than Open command.  If you have
already opened or created an IK joint hierarchy, this option will cause the
import command to search for names in the NIF file which match the names in
the joint hierarchy, and use the existing joints instead of the ones in the
original file.  NOTE:  This will only work if the existing joints are in exactly
the same position that those in the file are in.

=Import nodes with the following in their name as joints=
Usually only NiNodes flagged as skin influences will be imported as IK joints,
but this will cause any with the given sub string somewhere in their name to
be imported as IK joints as well.

=NIF Version=
This determines what version of NIF files is created by the export process.
If an invalid version string is specified, the default of 4.0.0.2 is used; the
lowest NIF version that Niflib supports.

=Export White Ambient if Texture is Present=
Every NIF file from a game I've seen has a white ambient component if there is
a texture present, but Maya's default is black.  You can get some
interesting effects by setting the ambient color to something other than
white, but they will look very different from all the other objects in the
game.

=Arrange Triangles in Strips=
This will option arranges the triangles within meshes into efficient strips.
This may cause problems in some games, but generally increases performance.

=Generate Oblivion Tangent Space Data=
Oblivion NIF files have some game-specific data packed into the file.  This
command causes this extra data to be calculated and included in the exported
NIF file.  Has no affect for other games.

=Maximum Number of Bones Per Skin Partition=
Skin partitions hold skin information optimized for hardware-assisted skinning.
Depending on the shader version, there are different limits on the number of bones
that can be in each partition.  4 should always be safe, but a limit similar to the
other files in the game should provide the best performance.  All official Oblivion
files seems to use a max of 20.  On the other hand, Civ4 provides two versions of
each file, one with 4 bones per partition for backwards compatibility, and one
with up to 16 bones per partition for more recent video cards.


IMPORT:

The following are imported:
 * NiNodes are imported as either transforms or ikJoints.
 * NiTriShapes and NiTriStrips are imported as polygon meshes.
 * NiAlphaProperty and NiSpecularProperty are honored.
 * NiMaterialProperties and NiTexturingProperties are imported as Phong shaders.
 * Meshes with a NiSkinInstance are bound to the skeleton.
 * Simple collision boxes used in some games are imported as implicitBox nodes.
 
The following are NOT imported:
 * Animation of any kind
 * Oblivion Havok collision information.
 * Special nodes like Billboards lose their special properties.
 * Cameras
 * Lights
 * Environment Maps
 * Anything else not listed

EXPORT

The following are exported:
 * Polygon meshes (triangulation is not necessary).  Components exported are
   vertex positions, normals, vertex colors and UV coordinates for supported
   texture types.  They can either be in triangle list or triangle strip format.
 * DAG nodes other than meshes are exported as NiNodes and include
   transform information.  Maya's default DAG nodes like the default
   cameras are ignored.  Visibility is exported by flagging the NiNode as
   hidden.  History items are not exported.
 * Any shaders connected to a mesh.   Meshes are automatically split into
   multiple NiTriShapes by material.  Attributes exported are color,
   transparency (averaged), ambient color, incandescence, and specular color.
   Color and incandescence can be textures.
 * Any textures connected to a mesh through a material's color or
   incandescence attributes.  Paths are fixed to remove anything in the
   texture folder option.
 * Skin bindings are exported so long as only one skin cluster affects
   a mesh.
 * Implicit box nodes with their transform parented to another transform node
   are treated as simple collision boxes used in some games.  To create one,
   execute the following command in the command line at the bottom of the screen:
   createNode implicitBox;
   You can then parent this box to the node that will hold the simple collision
   box.
   
The following are NOT exported:
 * Animation of any kind
 * Oblivion Havok collision information.
 * Cameras
 * Lights
 * Environment Maps
 * Anything else not listed
