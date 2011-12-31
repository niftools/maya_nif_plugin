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

	int targetType = -1;
	int export_order = 0;

	for(int i = 0;i < controllerLinks.size(); i++) {
		NiInterpolatorRef interpolator = controllerLinks[i].interpolator;

		string target = "";

		if(controllerLinks[i].targetName.size() > 0) {
			target = controllerLinks[i].targetName;
			targetType = 1;
		} else if (controllerLinks[i].nodeName.size() > 0) {
			target = controllerLinks[i].nodeName;
			targetType = 2;
		} else {
			string pallete_string = controllerLinks[i].stringPalette->GetPaletteString();
			int offset = controllerLinks[i].nodeNameOffset;

			while(pallete_string[offset] != 0 && offset < pallete_string.length()) {
				target.append(1,pallete_string[offset]);
				offset++;
			}
			targetType = 3;
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




