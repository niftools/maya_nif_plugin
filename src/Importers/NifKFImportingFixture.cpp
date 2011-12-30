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

	for(int i = 0;i < controllerLinks.size(); i++) {
		NiInterpolatorRef interpolator = controllerLinks[i].interpolator;
		string pallete_string = controllerLinks[i].stringPalette->GetPaletteString();
		string target = "";
		int offset = controllerLinks[i].nodeNameOffset;

		while(pallete_string[offset] != 0 && offset < pallete_string.length()) {
			target.append(1,pallete_string[offset]);
			offset++;
		}

		MString target_object = this->translatorUtils->MakeMayaName(target);
		this->animationImporter->ImportAnimation(interpolator, target_object);
	}


	return MStatus(MStatus::kFailure);
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




