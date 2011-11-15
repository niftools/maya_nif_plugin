#include "NifTranslatorData.h"

NifTranslatorData::NifTranslatorData() {

}

NifTranslatorData::NifTranslatorData( const NifTranslatorData& copy ) 
	: importedNodes(copy.importedNodes), importedTextures(copy.importedTextures), importedMeshes(copy.importedMeshes), importedMaterials(copy.importedMaterials), existingNodes(copy.existingNodes) {

}

NifTranslatorData::~NifTranslatorData() {

}

void NifTranslatorData::Reset() {
	this->existingNodes.clear();
	this->importedNodes.clear();
	this->importedMaterials.clear();
	this->importedTextures.clear();
	this->importedMeshes.clear();
	this->importFile.setFullName("");
	this->sceneRoot = NULL;
	this->meshes.clear();
	this->nodes.clear(); 
	this->meshClusters.clear();
	this->shaders.clear();
}
