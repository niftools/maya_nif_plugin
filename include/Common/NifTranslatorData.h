#ifndef _NIFTRANSLATORDATA_H
#define _NIFTRANSLATORDATA_H

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MFileObject.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnBase.h>
#include <maya/MFnComponent.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnSet.h>
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
#include <Ref.h>

#include "NifTranslatorRefObject.h"
#include "NifTranslatorDataWrapper.h"

using namespace Niflib;
using namespace std;

class NifTranslatorData;

typedef Ref<NifTranslatorData> NifTranslatorDataRef;

class NifTranslatorData : public NifTranslatorRefObject {
public:

	//Map to hold existing nodes that were found for connecting skins to
	//skeletons on import
	map<string, MDagPath> existingNodes;
	//maps to hold information about what has already been imported
	map< NiAVObjectRef, MDagPath > importedNodes;
	map< unsigned int, MObject > importedMaterials;

	MatTexCollection mtCollection;

	map< unsigned int, MObject > importedTextures;
	vector< pair<NiAVObjectRef, MObject> > importedMeshes;
	MFileObject importFile; //The file currently being imported

	map<string, unsigned int> textures;
	NiNodeRef sceneRoot;

	list< MObject > meshes;
	//A map to hold associations between DAG paths and NIF object
	map<string, NiNodeRef> nodes; 

	map< string, vector<MObject> > meshClusters;

	map< string, vector<NiPropertyRef> > shaders;

	map<string,NifTranslatorDataWrapperRef> customData;

	NifTranslatorData();
	NifTranslatorData(const NifTranslatorData& copy);

	virtual ~NifTranslatorData();

	virtual void Reset();

	virtual string asString( bool verbose = false ) const;

	virtual const Type& GetType() const;

	const static Type TYPE;
};

#endif