#include "Common/NifTranslatorData.h"

NifTranslatorData::NifTranslatorData() {

}

NifTranslatorData::NifTranslatorData( const NifTranslatorData& copy ) 
	: importedNodes(copy.importedNodes), importedMeshes(copy.importedMeshes), existingNodes(copy.existingNodes) {

}

NifTranslatorData::~NifTranslatorData() {

}

void NifTranslatorData::Reset() {
	this->existingNodes.clear();
	this->importedNodes.clear();
	this->importedMeshes.clear();
	this->importFile.setFullName("");
	this->exportedSceneRoot = NULL;
	this->meshes.clear();
	this->nodes.clear(); 
	this->meshClusters.clear();
	this->shaders.clear();
}

std::string NifTranslatorData::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorRefObject::asString();
	
	if(verbose == false) {
		out<<"Existing nodes count:   "<<this->existingNodes.size();
		out<<"Imported nodes count:   "<<this->importedNodes.size();
		out<<"Imported meshes count:   "<<this->importedMeshes.size();
		out<<"Imported file name:   "<<this->importFile.name();
		out<<"Scene root:  "<<endl<<this->exportedSceneRoot->asString()<<endl;
		out<<"Meshes:   "<<this->meshes.size()<<endl;

	} else {
		out<<"Existing nodes count:   "<<this->existingNodes.size();
		for(map<string,MDagPath>::const_iterator it = this->existingNodes.begin();it != this->existingNodes.end(); it++) {
			out<<"Nif node - maya node association"<<it->first<<"   -   "<<it->second.partialPathName()<<endl;
		}

		out<<"Imported nodes count:   "<<this->importedNodes.size();
		for(map<NiAVObjectRef,MDagPath>::const_iterator it = this->importedNodes.begin();it != this->importedNodes.end(); it++) {
			out<<"Imported nif nodes -  MDagPath:  "<<it->first->asString(false)<<"  -  "<<it->second.partialPathName()<<endl;
		}
		out<<"Imported meshes count:   "<<this->importedMeshes.size();
		out<<"Imported file name:   "<<this->importFile.name();
		out<<"Scene root:  "<<endl<<this->exportedSceneRoot->asString()<<endl;
		out<<"Meshes:   "<<this->meshes.size()<<endl;
	}

	return out.str();
}

const Type& NifTranslatorData::GetType() const {
	return TYPE;
}

const Type NifTranslatorData::TYPE("NifTranslatorData",&NifTranslatorRefObject::TYPE);
