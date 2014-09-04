#ifndef _NIFMATERIALIMPORTERSKYRIM_H
#define _NIFMATERIALIMPORTERSKYRIM_H

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
#include <maya/MProgressWindow.h>

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
#include <obj/NiAVObject.h>
#include <obj/NiTriBasedGeom.h>
#include <obj/BSLightingShaderProperty.h>
#include <obj/BSShaderTextureSet.h>

#include "Common/NifTranslatorRefObject.h"
#include "Common/NifTranslatorOptions.h"
#include "Common/NifTranslatorData.h"
#include "Common/NifTranslatorUtils.h"
#include "Common/NifTranslatorFixtureItem.h"
#include "Importers/NifMaterialImporter.h"

class NifMaterialImporterSkyrim;

typedef Ref<NifMaterialImporterSkyrim> NifMaterialImporterSkyrimRef;

class NifMaterialImporterSkyrim : public NifMaterialImporter {
private:

	//these 2 vectors are actually interconnected meaning that
	//an MOject in the imported materials corresponds to a set of properties in the property_groups
	//at the exact same index. A maya material is created for a set of properties
	vector<vector<NiPropertyRef>> property_groups;

	vector<MObject> imported_materials;

public:

	NifMaterialImporterSkyrim();

	NifMaterialImporterSkyrim(NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils);

	virtual void ImportMaterialsAndTextures( NiAVObjectRef & root );

	void GatherMaterialsAndTextures( NiAVObjectRef & root);

	MString skyrimShaderFlags1ToString( unsigned int shader_flags);

	MString skyrimShaderFlags2ToString( unsigned int shader_flags);

	MString skyrimShaderTypeToString(unsigned int shader_type);

	virtual string asString( bool verbose = false ) const;

	virtual const Type& GetType() const;

	const static Type TYPE;
};

#endif