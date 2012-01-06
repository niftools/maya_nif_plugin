#ifndef _NIFTRANSLATOROPTIONS_H
#define _NIFTRANSLATOROPTIONS_H

#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MFStream.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h> 
#include <maya/MItDag.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPointArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MVector.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MAnimUtil.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>

#include <string> 
#include <vector>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <iostream>

#include <ComplexShape.h>
#include <MatTexCollection.h>
#include <niflib.h>
#include <obj/NiAlphaProperty.h>
#include <obj/NiMaterialProperty.h>
#include <obj/NiNode.h>
#include <obj/NiObject.h>
#include <obj/NiProperty.h>
#include <obj/NiSkinData.h>
#include <obj/NiSkinInstance.h>
#include <obj/NiSourceTexture.h>
#include <obj/NiSpecularProperty.h>
#include <obj/NiTexturingProperty.h>
#include <obj/NiTriBasedGeom.h>
#include <obj/NiTriBasedGeomData.h>
#include <obj/NiTriShape.h>
#include <obj/NiTriShapeData.h>
#include <obj/NiTriStripsData.h>
#include <obj/NiTimeController.h>
#include <obj/NiKeyframeController.h>
#include <obj/NiKeyframeData.h>
#include <obj/NiTextureProperty.h>
#include <obj/NiImage.h>

#include "NifTranslatorRefObject.h"

using namespace Niflib;
using namespace std;

class NifTranslatorOptions;

typedef Ref<NifTranslatorOptions> NifTranslatorOptionsRef;


const char PLUGIN_VERSION [] = "0.6.1";	
const char TRANSLATOR_NAME [] = "NetImmerse Format";

enum TexPathMode {
	PATH_MODE_AUTO, //Uses search paths to strip intelligently
	PATH_MODE_FULL, //Uses full path
	PATH_MODE_NAME, //Uses only file name
	PATH_MODE_PREFIX //Uses user-defined prefix
};

class NifTranslatorOptions : public NifTranslatorRefObject {
public:
	//--Globals--//
	string texture_path; // Path to textures gotten from option string
	unsigned int export_version; //Version of NIF file to export
	unsigned int export_user_version; //Game-specific user version of NIF file to export
	bool import_bind_pose; //Determines whether or not the bind pose should be searched for

	vector<string> export_shapes;
	vector<string> export_joints;
	bool import_normals; //Determines whether normals are imported
	bool import_no_ambient; //Determines whether ambient color is imported
	bool export_white_ambient; //Determines whether ambient color is automatically set to white if a texture is present
	bool import_comb_skel; //Determines whether the importer tries to combine new skins with skeletons that exist in the scene
	string joint_match; //String to match in the name of nodes as another way to cause themm to import as IK joints
	bool use_name_mangling;  //Determines whether to replace characters that are invalid for Maya (along with _ so that spaces can use that character) with hex representations
	bool export_tri_strips;  //Determines whether to export NiTriShape objects or NiTriStrip objects
	int export_part_bones; //Determines the maximum number of bones per skin partition.
	bool export_tan_space; //Determines whether Oblivion tangent space data is generated for meshes on export
	bool export_mor_rename; //Determines whether NiTriShapes tagged with materials that have Morrowind body parts are renamed to match those body parts
	
	TexPathMode tex_path_mode;  //Determines the way textures paths are exported
	string tex_path_prefix; //Optional prefix to add to texture paths.

	//this tells the exporter what to export from the scene: geometry or animation
	string exportType;

	int spline_animation_npoints; //the amounts of points for spline animation curves per time unit
	int spline_animation_degree; //the degree of the polynom for the spline animation curves

	string animation_target; // the name of the target of the kf file or controller sequence
	string animation_name; //the name of the animation stored in the kf file or the controlelr sequence


	//can't work with virtual functions like this, it's not safe to call reset from the base con
	NifTranslatorOptions();

	virtual void Reset();

	virtual void ParseOptionsString(const MString & optionsString);

	virtual ~NifTranslatorOptions();

	virtual string asString( bool verbose = false ) const;

	virtual const Type& GetType() const;

	const static Type TYPE;

};

#endif