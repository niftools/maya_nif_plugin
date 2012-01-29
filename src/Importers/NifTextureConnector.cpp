#include "include/Importers/NifTextureConnector.h"


NifTextureConnector::NifTextureConnector() {

}

NifTextureConnector::NifTextureConnector(MFnDependencyNode texture_placement, int uv_set) {
	this->texturePlacement.setObject(texture_placement.object());
	this->uvSet = uv_set;
}

void NifTextureConnector::ConnectTexture( MDagPath mesh_path ) {
	MDGModifier dgModifier;
	MFnDependencyNode chooserFn;
	chooserFn.create( "uvChooser", "uvChooser" );

	//Connection between the mesh and the uvChooser
	MFnMesh meshFn;
	meshFn.setObject(mesh_path);
	dgModifier.connect( meshFn.findPlug("uvSet")[uvSet].child(0), chooserFn.findPlug("uvSets").elementByLogicalIndex(0) );

	//Connections between the uvChooser and the place2dTexture
	dgModifier.connect( chooserFn.findPlug("outUv"), texturePlacement.findPlug("uvCoord") );
	dgModifier.connect( chooserFn.findPlug("outVertexCameraOne"), texturePlacement.findPlug("vertexCameraOne") );
	dgModifier.connect( chooserFn.findPlug("outVertexUvOne"), texturePlacement.findPlug("vertexUvOne") );
	dgModifier.connect( chooserFn.findPlug("outVertexUvTwo"), texturePlacement.findPlug("vertexUvTwo") );
	dgModifier.connect( chooserFn.findPlug("outVertexUvThree"), texturePlacement.findPlug("vertexUvThree") );
	dgModifier.doIt();
}

string NifTextureConnector::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorRefObject::asString(verbose)<<endl;
	out<<texturePlacement.name()<<endl;
	out<<"UV set index: "<<uvSet<<endl;

	return out.str();
}

const Type& NifTextureConnector::GetType() const {
	return TYPE;
}

const Type NifTextureConnector::TYPE("NifTextureConnector", &NifTranslatorRefObject::TYPE);
