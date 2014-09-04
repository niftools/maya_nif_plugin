#include "Exporters/NifKFExportingFixture.h"

NifKFExportingFixture::NifKFExportingFixture() {

}

NifKFExportingFixture::NifKFExportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->animationExporter = new NifKFAnimationExporter(translatorOptions, translatorData, translatorUtils);
}

MStatus NifKFExportingFixture::WriteNodes( const MFileObject& file ) {
	MItDag iterator(MItDag::kDepthFirst);

	for(; !iterator.isDone(); iterator.next()) {
		MFnDagNode node(iterator.currentItem());
		if(node.isIntermediateObject()) {
			continue;
		}

		if ( 
			node.name().substring(0, 13) == "UniversalManip" ||
			node.name() == "groundPlane_transform" ||
			node.name() == "ViewCompass" ||
			node.name() == "CubeCompass" ||
			node.name() == "Manipulator1" ||
			node.name() == "persp" ||
			node.name() == "top" ||
			node.name() == "front" ||
			node.name() == "side"
			) {
				continue;
		}

		if(!iterator.currentItem().hasFn(MFn::Type::kTransform)) {
			continue;
		}

		if(!(this->translatorOptions->exportType == "allanimation") && 
			(!this->translatorUtils->isExportedJoint(node.name()) && !this->translatorUtils->isExportedJoint(node.name()) && !this->translatorUtils->isExportedMisc(node.name()))) {
			continue;
		}

		this->translatorData->animatedObjects.push_back(iterator.currentItem());
	}

	NiControllerSequenceRef controller_sequence = DynamicCast<NiControllerSequence>(NiControllerSequence::Create());
	controller_sequence->SetStartTime(this->animationExporter->GetAnimationStartTime());
	controller_sequence->SetStopTime(this->animationExporter->GetAnimationEndTime());
	controller_sequence->SetName(this->translatorOptions->animationName);
	controller_sequence->SetTargetName(this->translatorOptions->animationTarget);
	controller_sequence->SetFrequency(1.0);

	vector<MFnDependencyNode*> objectsWithExportIndexes;
	vector<MFnDependencyNode*> objectsWithoutExportIndexes;

	for(unsigned int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MFnDependencyNode node(this->translatorData->animatedObjects.at(i));
		MPlug plug = node.findPlug("exportIndex");
		string name = node.name().asChar();

		// MAYA-6512
		MFnDependencyNode *anode = new MFnDagNode(node.object());
		if(plug.isNull()) {
			objectsWithoutExportIndexes.push_back(anode);
		} else {
			objectsWithExportIndexes.push_back(anode);
		}
		// FIXME probably a leak here
	}

	this->translatorData->animatedObjects.clear();

	for(unsigned int i = 0;i < objectsWithExportIndexes.size() - 1; i++) {
		for(unsigned int j = i + 1; j < objectsWithExportIndexes.size(); j++) {
			MPlug plug_i = objectsWithExportIndexes.at(i)->findPlug("exportIndex");
			MPlug plug_j = objectsWithExportIndexes.at(j)->findPlug("exportIndex");

			if(plug_i.asInt() > plug_j.asInt()) {
				MObject aux = objectsWithExportIndexes.at(i)->object();
				objectsWithExportIndexes.at(i)->setObject(objectsWithExportIndexes.at(j)->object());
				objectsWithExportIndexes.at(j)->setObject(aux);
			}
		}
	}

	for(unsigned int i = 0; i < objectsWithExportIndexes.size(); i++) {
		this->translatorData->animatedObjects.push_back(objectsWithExportIndexes.at(i)->object());
	}

	for(unsigned int i = 0; i < objectsWithoutExportIndexes.size(); i++) {
		this->translatorData->animatedObjects.push_back(objectsWithoutExportIndexes.at(i)->object());
	}

	for(unsigned int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		
	}

	for(unsigned int i = 0; i < this->translatorData->animatedObjects.size(); i++) {
		MFnDependencyNode node(this->translatorData->animatedObjects.at(i));
		string name = node.name().asChar();
		this->animationExporter->ExportAnimation(controller_sequence, this->translatorData->animatedObjects.at(i));
	}

	//out << "Writing Finished NIF file..." << endl;
	NifInfo nif_info(this->translatorOptions->exportVersion, this->translatorOptions->exportUserVersion);
	nif_info.endian = ENDIAN_LITTLE; //Intel endian format
	nif_info.exportInfo1 = "NifTools Maya NIF Plug-in " + string(PLUGIN_VERSION);
	nif_info.userVersion2 = this->translatorOptions->exportUserVersion2;

	Niflib::WriteNifTree(file.fullName().asChar(), controller_sequence, nif_info);

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


