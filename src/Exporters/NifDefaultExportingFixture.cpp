#include "include/Exporters/NifDefaultExportingFixture.h"

NifDefaultExportingFixture::NifDefaultExportingFixture() {

}

NifDefaultExportingFixture::NifDefaultExportingFixture( NifTranslatorDataRef translatorData, NifTranslatorOptionsRef translatorOptions, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->nodeExporter = new NifNodeExporter(translatorOptions, translatorData, translatorUtils);
	this->meshExporter = new NifMeshExporter(this->nodeExporter, translatorOptions, translatorData, translatorUtils);
	this->materialExporter = new NifMaterialExporter(translatorOptions, translatorData, translatorUtils);
	this->animationExporter = new NifAnimationExporter(translatorOptions, translatorData, translatorUtils);
}

MStatus NifDefaultExportingFixture::WriteNodes( const MFileObject& file ) {
	try {
	//out << "Creating root node...";
	//Create new root node
	this->translatorData->exportedSceneRoot = new NiNode;
	this->translatorData->exportedSceneRoot->SetName( "Scene Root" );
	//out << sceneRoot << endl;


	//out << "Exporting shaders..." << endl;
	this->translatorData->shaders.clear();
	//Export shaders and textures
	this->materialExporter->ExportMaterials();

	//out << "Clearing Nodes" << endl;
	this->translatorData->nodes.clear();
	//out << "Clearing Meshes" << endl;
	this->translatorData->meshes.clear();
	//Export nodes

	//out << "Exporting nodes..." << endl;
	this->nodeExporter->ExportDAGNodes();

	//out << "Enumerating skin clusters..." << endl;
	this->translatorData->meshClusters.clear();
	this->meshExporter->EnumerateSkinClusters();

	//out << "Exporting meshes..." << endl;
	for ( list<MObject>::iterator mesh = this->translatorData->meshes.begin(); mesh != this->translatorData->meshes.end(); ++mesh ) {
		this->meshExporter->ExportMesh( *mesh );
	}

	//out << "Applying Skin offsets..." << endl;
	this->meshExporter->ApplyAllSkinOffsets( StaticCast<NiAVObject>(this->translatorData->exportedSceneRoot) );



	//--Write finished NIF file--//

	//out << "Writing Finished NIF file..." << endl;
	NifInfo nif_info(this->translatorOptions->exportVersion, this->translatorOptions->exportUserVersion);
	nif_info.endian = ENDIAN_LITTLE; //Intel endian format
	nif_info.exportInfo1 = "NifTools Maya NIF Plug-in " + string(PLUGIN_VERSION);
	nif_info.userVersion2 = this->translatorOptions->exportUserVersion2;
	WriteNifTree( file.fullName().asChar(), StaticCast<NiObject>(this->translatorData->exportedSceneRoot), nif_info );

	//out << "Export Complete." << endl;

	//Clear temporary data
	//out << "Clearing temporary data" << endl;
	this->translatorData->Reset();
	}
	catch( exception & e ) {
		stringstream out;
		out << "Error:  " << e.what() << endl;
		MGlobal::displayError( out.str().c_str() );
		return MStatus::kFailure;
	}
	catch( ... ) {
		MGlobal::displayError( "Error:  Unknown Exception." );
		return MStatus::kFailure;
	}

	//#ifndef _DEBUG
	////Clear the stringstream so it doesn't waste a bunch of RAM
	//out.clear();
	//#endif

	return MS::kSuccess;
}

string NifDefaultExportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifExportingFixture::asString(verbose)<<endl;
	out<<this->nodeExporter->asString(verbose)<<endl;
	out<<this->meshExporter->asString(verbose)<<endl;
	out<<this->materialExporter->asString(verbose)<<endl;

	return out.str();
}

const Type& NifDefaultExportingFixture::getType() const {
	return TYPE;
}


const Type NifDefaultExportingFixture::TYPE("NifDefaultExportingFixture",&NifExportingFixture::TYPE);
