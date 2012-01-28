#include "include/Importers/NifKFImportingFixture.h"


NifKFImportingFixture::NifKFImportingFixture() {

}

NifKFImportingFixture::NifKFImportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->animationImporter = new NifKFAnimationImporter(translatorOptions, translatorData, translatorUtils);
}

MObject NifKFImportingFixture::CreateDummyObject( MString target_object, ControllerLink controller_link )
{
	MFnTransform node_new;
	node_new.create();
	node_new.setName(target_object);

	MVector translation;
	MQuaternion rotation;
	double scale[3] = { 1, 1, 1};

	NiInterpolatorRef transform_provider = controller_link.interpolator;

	if(transform_provider->GetType().IsSameType(NiTransformInterpolator::TYPE)) {

		NiTransformInterpolatorRef transform_interpolator = DynamicCast<NiTransformInterpolator>(transform_provider);

		if(transform_interpolator->GetTranslation().x > -100000000 && transform_interpolator->GetTranslation().x < 100000000 &&
			transform_interpolator->GetTranslation().y > -100000000 && transform_interpolator->GetTranslation().y < 100000000 &&
			transform_interpolator->GetTranslation().z > -100000000 && transform_interpolator->GetTranslation().z < 100000000) {
				translation.x = transform_interpolator->GetTranslation().x;
				translation.y = transform_interpolator->GetTranslation().y;
				translation.z = transform_interpolator->GetTranslation().z;
		}

		if (transform_interpolator->GetScale() > -100000000 && transform_interpolator->GetScale() < 100000000) {
			scale[0] = transform_interpolator->GetScale();
			scale[1] = transform_interpolator->GetScale();
			scale[2] = transform_interpolator->GetScale();
		}

		if(transform_interpolator->GetRotation().w > -100000000 && transform_interpolator->GetRotation().w < 100000000 &&
			transform_interpolator->GetRotation().x > -100000000 && transform_interpolator->GetRotation().x < 100000000 &&
			transform_interpolator->GetRotation().y > -100000000 && transform_interpolator->GetRotation().y < 100000000 &&
			transform_interpolator->GetRotation().z > -100000000 && transform_interpolator->GetRotation().z < 100000000) {
				rotation.w = transform_interpolator->GetRotation().w;
				rotation.x = transform_interpolator->GetRotation().x;
				rotation.y = transform_interpolator->GetRotation().y;
				rotation.z = transform_interpolator->GetRotation().z;
		}
	}

	if(transform_provider->GetType().IsDerivedType(NiBSplineTransformInterpolator::TYPE)) {

		NiBSplineTransformInterpolatorRef transform_interpolator = DynamicCast<NiBSplineTransformInterpolator>(transform_provider);

		if(transform_interpolator->GetTranslation().x > -100000000 && transform_interpolator->GetTranslation().x < 100000000 &&
			transform_interpolator->GetTranslation().y > -100000000 && transform_interpolator->GetTranslation().y < 100000000 &&
			transform_interpolator->GetTranslation().z > -100000000 && transform_interpolator->GetTranslation().z < 100000000) {
				translation.x = transform_interpolator->GetTranslation().x;
				translation.y = transform_interpolator->GetTranslation().y;
				translation.z = transform_interpolator->GetTranslation().z;
		}

		if (transform_interpolator->GetScale() > -100000000 && transform_interpolator->GetScale() < 100000000) {
			scale[0] = transform_interpolator->GetScale();
			scale[1] = transform_interpolator->GetScale();
			scale[2] = transform_interpolator->GetScale();
		}

		if(transform_interpolator->GetRotation().w > -100000000 && transform_interpolator->GetRotation().w < 100000000 &&
			transform_interpolator->GetRotation().x > -100000000 && transform_interpolator->GetRotation().x < 100000000 &&
			transform_interpolator->GetRotation().y > -100000000 && transform_interpolator->GetRotation().y < 100000000 &&
			transform_interpolator->GetRotation().z > -100000000 && transform_interpolator->GetRotation().z < 100000000) {
				rotation.w = transform_interpolator->GetRotation().w;
				rotation.x = transform_interpolator->GetRotation().x;
				rotation.y = transform_interpolator->GetRotation().y;
				rotation.z = transform_interpolator->GetRotation().z;
		}
	}

	if(transform_provider->GetType().IsSameType(NiPoint3Interpolator::TYPE)) {

		NiPoint3InterpolatorRef transform_interpolator = DynamicCast<NiPoint3Interpolator>(transform_provider);

		if(transform_interpolator->GetPoint3Value().x > -100000000 && transform_interpolator->GetPoint3Value().x < 100000000 &&
			transform_interpolator->GetPoint3Value().y > -100000000 && transform_interpolator->GetPoint3Value().y < 100000000 &&
			transform_interpolator->GetPoint3Value().z > -100000000 && transform_interpolator->GetPoint3Value().z < 100000000) {
				translation.x = transform_interpolator->GetPoint3Value().x;
				translation.y = transform_interpolator->GetPoint3Value().y;
				translation.z = transform_interpolator->GetPoint3Value().z;
		}
	}

	if(transform_provider->GetType().IsSameType(NiFloatInterpolator::TYPE)) {
		NiFloatInterpolatorRef float_interpolator = DynamicCast<NiFloatInterpolator>(transform_provider);

		if(float_interpolator->GetFloatValue() > -100000000 && float_interpolator->GetFloatValue() < 100000000) {
			translation.x = float_interpolator->GetFloatValue();
		}
	}

	if(transform_provider->GetType().IsSameType(NiBoolInterpolator::TYPE)) {
		NiBoolInterpolatorRef bool_interpolator = DynamicCast<NiBoolInterpolator>(transform_provider);

		if(bool_interpolator->GetBoolValue() == true) {
			translation.x = 1;
		} else {
			translation.y = 0;
		}
	}

	node_new.setTranslation(translation,MSpace::kPostTransform);
	node_new.setScale(scale);
	node_new.setRotationQuaternion(rotation.x, rotation.y, rotation.z, rotation.w);

	return node_new.object();
}

MStatus NifKFImportingFixture::ReadNodes( const MFileObject& file ) {
	NifInfo* nifInfo = new NifInfo();
	Ref<NiObject> root = ReadNifTree(file.fullName().asChar(), nifInfo);

	NiControllerSequenceRef controllerSequence = DynamicCast<NiControllerSequence>(root);

	if(controllerSequence == NULL) {
		return MStatus(MStatus::kFailure);
	}

	vector<ControllerLink> controllerLinks = controllerSequence->GetControllerData();


	vector<MObject> created_objects;
	int export_order = 0;

	for(int i = 0; i < controllerLinks.size(); i++) {
		MObject object;
		string target = "";

		if(controllerLinks[i].targetName.size() > 0) {
			target = controllerLinks[i].targetName;
		} else if (controllerLinks[i].nodeName.size() > 0) {
			target = controllerLinks[i].nodeName;
		} else {
			string pallete_string = controllerLinks[i].stringPalette->GetPaletteString();
			int offset = controllerLinks[i].nodeNameOffset;

			while(pallete_string[offset] != 0 && offset < pallete_string.length()) {
				target.append(1,pallete_string[offset]);
				offset++;
			}
		}	

		MString target_object = this->translatorUtils->MakeMayaName(target);
		object = this->animationImporter->GetObjectByName(target_object);

		if(object.isNull()) {
			if(this->translatorOptions->importCreateDummyAnimationObjects == true) {
				object = this->CreateDummyObject(target_object, controllerLinks[i]);

				created_objects.push_back(object);
			} 
		} else {
			int duplicates = 0;
			MFnDependencyNode current_node;

			for(int i = 0; i < created_objects.size(); i++) {
				current_node.setObject(created_objects[i]);
				string current_name = this->translatorUtils->MakeNifName(current_node.name().asChar());
				if(current_name == target) {
					duplicates++;
				}	
			}

			if(duplicates > 0) {
				target_object = this->translatorUtils->MakeMayaName(target, duplicates);
				object = this->CreateDummyObject(target_object, controllerLinks[i]);
			}
		}

		MFnTransform transformNode(object);

		MPlug plug;
		MString mel_command;
		MString node_name = transformNode.name();

		double translation_x;
		double translation_y;
		double translation_z;

		double scale_x;
		double scale_y;
		double scale_z;

		double rotate_x;
		double rotate_y;
		double rotate_z;

		plug = transformNode.findPlug("translateX");
		translation_x = plug.asDouble();
		plug = transformNode.findPlug("translateY");
		translation_y = plug.asDouble();
		plug = transformNode.findPlug("translateZ");
		translation_z = plug.asDouble();

		plug = transformNode.findPlug("scaleX");
		scale_x = plug.asDouble();
		plug = transformNode.findPlug("scaleY");
		scale_y = plug.asDouble();
		plug = transformNode.findPlug("scaleZ");
		scale_z = plug.asDouble();

		plug = transformNode.findPlug("rotateX");
		rotate_x = plug.asDouble();
		plug = transformNode.findPlug("rotateY");
		rotate_y = plug.asDouble();
		plug = transformNode.findPlug("rotateZ");
		rotate_z = plug.asDouble();

		plug = transformNode.findPlug("translateRest");
		if(plug.isNull()) {
			mel_command = "addAttr -attributeType double3 -shortName \"translateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"translateRestX\" -attributeType double -parent \"translateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"translateRestY\" -attributeType double -parent \"translateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"translateRestZ\" -attributeType double -parent \"translateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
		}
		plug = transformNode.findPlug("translateRestX");
		plug.setDouble(translation_x);
		plug = transformNode.findPlug("translateRestY");
		plug.setDouble(translation_y);
		plug = transformNode.findPlug("translateRestZ");
		plug.setDouble(translation_z);

		plug = transformNode.findPlug("scaleRest");
		if(plug.isNull()) {
			mel_command = "addAttr -attributeType double3 -shortName \"scaleRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"scaleRestX\" -attributeType double -parent \"scaleRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"scaleRestY\" -attributeType double -parent \"scaleRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"scaleRestZ\" -attributeType double -parent \"scaleRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
		}
		plug = transformNode.findPlug("scaleRestX");
		plug.setDouble(scale_x);
		plug = transformNode.findPlug("scaleRestY");
		plug.setDouble(scale_y);
		plug = transformNode.findPlug("scaleRestZ");
		plug.setDouble(scale_z);

		plug = transformNode.findPlug("rotateRest");
		if(plug.isNull()) {
			mel_command = "addAttr -attributeType double3 -shortName \"rotateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"rotateRestX\" -attributeType double -parent \"rotateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"rotateRestY\" -attributeType double -parent \"rotateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
			mel_command = "addAttr -shortName \"rotateRestZ\" -attributeType double -parent \"rotateRest\" ";
			MGlobal::executeCommand(mel_command + node_name);
		}
		plug = transformNode.findPlug("rotateRestX");
		plug.setDouble((rotate_x / PI) * 180.0);
		plug = transformNode.findPlug("rotateRestY");
		plug.setDouble((rotate_y / PI) * 180.0);
		plug = transformNode.findPlug("rotateRestZ");
		plug.setDouble((rotate_z / PI) * 180.0);

		this->animationImporter->ImportAnimation(controllerLinks[i].interpolator, target_object);

		MFnDagNode node(object);

		plug = node.findPlug("exportIndex");
		node_name = node.name();

		if(plug.isNull()) {
			mel_command = "addAttr -at long -shortName \"exportIndex\" ";
			MGlobal::executeCommand(mel_command + node_name);
		}

		mel_command = "setAttr " + node_name + "\.exportIndex " + export_order;
		MGlobal::executeCommand(mel_command);
		export_order++;

		plug = node.findPlug("animationPriority");

		if(plug.isNull()) {
			mel_command = "addAttr -at byte -shortName \"animationPriority\" ";
			MGlobal::executeCommand(mel_command + node_name);
		}

		mel_command = "setAttr " + node_name + "\.animationPriority " + controllerLinks[i].priority;
		MGlobal::executeCommand(mel_command);
}

	return MStatus(MStatus::kSuccess);
}

MObject NifKFImportingFixture::GetObjectByName( const string& name ) {
	MItDag iterator(MItDag::kDepthFirst);
	MString mayaName = this->translatorUtils->MakeMayaName(name);

	for(;!iterator.isDone(); iterator.next()) {
		MFnDagNode node(iterator.item());
		if(mayaName == node.name()) {
			return iterator.item();
		}
	}

	return MObject();
}


string NifKFImportingFixture::asString( bool verbose /*= false */ ) {
	stringstream out;

	out<<NifImportingFixture::asString(verbose)<<endl;
	out<<"NifKFImportingFixture"<<endl;

	return out.str();
}

const Type& NifKFImportingFixture::getType() {
	return TYPE;
}

const Type NifKFImportingFixture::TYPE("NifKFImportingFixture", &NifImportingFixture::TYPE);




