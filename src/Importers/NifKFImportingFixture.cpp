#include "include/Importers/NifKFImportingFixture.h"


NifKFImportingFixture::NifKFImportingFixture() {

}

NifKFImportingFixture::NifKFImportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils, NifKFAnimationImporterRef animationImporter ) {
		this->animationImporter = animationImporter;
		this->translatorOptions = translatorOptions;
		this->translatorData = translatorData;
		this->translatorUtils = translatorUtils;
}

MStatus NifKFImportingFixture::ReadNodes( const MFileObject& file ) {
	NifInfo* nifInfo = new NifInfo();
	Ref<NiObject> root = ReadNifTree(file.fullName().asChar(), nifInfo);

	NiControllerSequenceRef controllerSequence = DynamicCast<NiControllerSequence>(root);

	if(controllerSequence == NULL) {
		return MStatus(MStatus::kFailure);
	}

	vector<ControllerLink> controllerLinks = controllerSequence->GetControllerData();


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
			continue;
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
	}

	int export_order = 0;

	for(int i = 0;i < controllerLinks.size(); i++) {
		NiInterpolatorRef interpolator = controllerLinks[i].interpolator;

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
		this->animationImporter->ImportAnimation(interpolator, target_object);

		MObject object = this->animationImporter->GetObjectByName(target_object);
		if(!object.isNull()) {
			MFnDagNode node(object);

			MPlug plug = node.findPlug("exportIndex");
			MString node_name = node.name();
			MString mel_command;

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




