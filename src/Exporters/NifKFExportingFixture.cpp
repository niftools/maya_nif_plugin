#include "include/Exporters/NifKFExportingFixture.h"

NifKFExportingFixture::NifKFExportingFixture() {

}

NifKFExportingFixture::NifKFExportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils, NifKFAnimationExporterRef animationExporter ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->animationExporter = animationExporter;
}

MStatus NifKFExportingFixture::WriteNodes( const MFileObject& file ) {
	MItDag iterator(MItDag::kDepthFirst);

	for(; !iterator.isDone(); iterator.next()) {
		MFnDagNode node(iterator.currentItem());
		if(node.isIntermediateObject()) {
			continue;
		}
		if(MAnimUtil::isAnimated(iterator.currentItem())) {
			this->translatorData->animatedObjects.push_back(iterator.currentItem());
			MFnDependencyNode node(iterator.currentItem());
		}
	}

	NiControllerSequenceRef controllerSequence = DynamicCast<NiControllerSequence>(NiControllerSequence::Create());
	controllerSequence->SetStartTime(0);
	controllerSequence->SetStopTime(0);

	vector<MFnDependencyNode> objectsWithExportIndexes;
	vector<MFnDependencyNode> objectsWithoutExportIndexes;

	for(int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MFnDependencyNode node(this->translatorData->animatedObjects.at(i));
		MPlug plug = node.findPlug("exportIndex");
		string name = node.name().asChar();

		if(plug.isNull()) {
			objectsWithoutExportIndexes.push_back(node);
		} else {
			objectsWithExportIndexes.push_back(node);
		}
	}

	this->translatorData->animatedObjects.clear();

	for(int i = 0;i < (int)(objectsWithExportIndexes.size()) - 1; i++) {
		for(int j = i + 1; j < objectsWithExportIndexes.size(); j++) {
			MPlug plug_i = objectsWithExportIndexes.at(i).findPlug("exportIndex");
			MPlug plug_j = objectsWithExportIndexes.at(j).findPlug("exportIndex");

			if(plug_i.asInt() > plug_j.asInt()) {
				MObject aux = objectsWithExportIndexes.at(i).object();
				objectsWithExportIndexes.at(i).setObject(objectsWithExportIndexes.at(j).object());
				objectsWithExportIndexes.at(j).setObject(aux);
			}
		}
	}

	for(int i = 0; i < objectsWithExportIndexes.size(); i++) {
		this->translatorData->animatedObjects.push_back(objectsWithExportIndexes.at(i).object());
	}

	for(int i = 0; i < objectsWithoutExportIndexes.size(); i++) {
		this->translatorData->animatedObjects.push_back(objectsWithoutExportIndexes.at(i).object());
	}

	for(int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		
	}

	for(int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MFnDependencyNode node(this->translatorData->animatedObjects.at(i));
		string name = node.name().asChar();
		this->animationExporter->ExportAnimation(controllerSequence, this->translatorData->animatedObjects.at(i));
	}

	//out << "Writing Finished NIF file..." << endl;
	NifInfo nif_info(this->translatorOptions->export_version, this->translatorOptions->export_user_version);
	nif_info.endian = ENDIAN_LITTLE; //Intel endian format
	nif_info.exportInfo1 = "NifTools Maya NIF Plug-in " + string(PLUGIN_VERSION);

	Niflib::WriteNifTree(file.fullName().asChar(), controllerSequence, nif_info);

	return MStatus::kSuccess;
}

string NifKFExportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifExportingFixture::asString(verbose)<<endl;
	out<<"NifKFExportingFixture"<<endl;

	return out.str();
}

const Type& NifKFExportingFixture::getType() const {
	return TYPE;
}

const Type NifKFExportingFixture::TYPE("NifKFExportingFixture",&NifExportingFixture::TYPE);


