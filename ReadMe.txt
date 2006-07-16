Maya 6.5 NIF File Translator

Currently supports importing static (non-animated) NIF files.  Export is *not* supported.  Pre-compiled versions are availiable for Maya 6.5 and 7.0 for Windows.  To use on other versions of Maya or other operating systems, you will need to compile from source.

WARNING:  

This importer is very incomplete, and will not import all data in the NIF file.  This includes animation, sound data, normals, particle systems, collision information, bounding boxes, and more!  It should be seen only as a fun thing to mess around with for now.  I can't promise that files you import today and save as mb files will be able to be re-exported flawlessly when I write the export functionality later.

KNOWN ISSUES:

* When meshes have multiple UV sets, the textures are not always attached to the correct UV set.
* Export resutls can be unpredictable if UVs go outside the 0.0-1.0 range.  Suggest staying within the picture in the UV editor.
* Niflib has endian issues that are not taken care of, so a PowerPC compile will be impossible.

INSTALLATION:

place nifTranslator.mll in your plug-in folder.  Probably something like:

C:\Program Files\Alias\Maya6.5\bin\plug-ins

Place the nifTranslatorOpts.mel file in your scripts folder. Probably something like:

C:\Documents and Settings\*YourUserName*\My Documents\Maya\scripts

From within Maya:

From the menu, choose:  Window -> Settings/Preferences -> Plug-in Manager...

Look for nifTranslator.mll in the list of available plugins and check the two boxes next to it "load" and "auto load"

Now choose File->Open Scene, and click the Options... button.

In the File Type drop down box, select "NetImmerse Format"

Now you should see Texture Source Directory under File Type Specific Options.  Press the button to browse for the location of your textures.

You should now be able to open and import NIF files like any other scene file that Maya understands.

COMPILING FROM SOURCE:

This release uses Niflib 0.5.4, availiable separately.  You can compile both Niflib and the Maya plug-in together or separately by first compiling Niflib in static library form and then linking it with the Maya plug-in.

This code SHOULD be compatible with multiple versions of Maya.  If you successfully compile this code for a version of Maya other than 6.5 or 7.0, please send it to me so others can use it! =)

If you can program in C++, have Maya, and want to help make this plugin better, come join our project at http://niftools.sourceforge.net!

IMPORT:

The following are imported:
 * NiNodes are imported as either transforms or ikJoints.
 * NiTriShapes and NiTriStrips are imported as polygon meshes.
 * NiAlphaProperty and NiSpecularProperty are honored.
 * NiMaterialProperties and NiTexturingProperties are imported as shaders.
 * Meshes with a NiSkinInstance are bound to the skeleton.
 
The following are NOT imported:
 * Animation of any kind
 * Oblivion Havok data or any other collision information.
 * Special nodes like Billboards lose their special properties.
 * Cameras
 * Lights
 * Evironment Maps
 * Anything else not listed

EXPORT

The following are exported:
 * Polygon meshes (triangulation is not necessary).  Vertex positions,
   normals, and one set of UV coordinates are exported.
 * DAG nodes other than meshes are exported as NiNodes and include
   transform information.  This includes things you will want to remove
   with NifSkope like default cameras.
 * One shader per mesh.  Multiple shaders on a single mesh will cause the
   export to fail.  Attributes exported are color, transparency (averaged), 
   ambient color, incandecence, and specular color.  Color can be a texture,
   but no other textures are exported.  Paths are absolute and will need to
   be fixed in NifSkope.
   
The following are NOT exported:
 * Animation of any kind
 * Skin bindings
 * Oblivion Havok data or any other collision information.
 * Cameras
 * Lights
 * Evironment Maps
 * Anything else not listed
 