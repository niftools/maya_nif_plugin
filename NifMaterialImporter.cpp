#include "NifMaterialImporter.h"

NifMaterialImporter::NifMaterialImporter() {

}

NifMaterialImporter::NifMaterialImporter( NifTranslatorOptions& translatorOptions, NifTranslatorData& translatorData, NifTranslatorUtils& translatorUtils )
	: NifTranslatorFixtureItem(translatorOptions,translatorData,translatorUtils) {

}

MObject NifMaterialImporter::ImportTexture( TextureWrapper & tw )
{
	MObject obj;

	string file_name = tw.GetTextureFileName();

	//Warn the user that internal textures are not supported
	if ( tw.IsTextureExternal() == false ) {
		MGlobal::displayWarning( "This NIF file contains an internaly stored texture.  This is not currently supported." );
		return MObject::kNullObj;
	}

	//create a texture node
	MFnDependencyNode nodeFn;
	obj = nodeFn.create( MString("file"), MString( file_name.c_str() ) );

	//--Search for the texture file--//

	//Replace back slash with forward slash
	unsigned last_slash = 0;
	for ( unsigned i = 0; i < file_name.size(); ++i ) {
		if ( file_name[i] == '\\' ) {
			file_name[i] = '/';
		}
	}

	//MString fName = file_name.c_str();

	MFileObject mFile;
	mFile.setName( MString(file_name.c_str()) );

	MString found_file = "";

	//Check if the file exists in the current working directory
	mFile.setRawPath( this->translatorData.importFile.rawPath() );
	//out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
	if ( mFile.exists() ) {
		found_file =  mFile.fullName();
	} else {
		//out << "File Not Found." << endl;
	}

	if ( found_file == "" ) {
		//Check if the file exists in any of the given search paths
		MStringArray paths;
		MString(this->translatorOptions.texture_path.c_str()).split( '|', paths );

		for ( unsigned i = 0; i < paths.length(); ++i ) {
			if ( paths[i].substring(0,0) == "." ) {
				//Relative path
				mFile.setRawPath( this->translatorData.importFile.rawPath() + paths[i] );
			} else {
				//Absolute path
				mFile.setRawPath( paths[i] );

			}

			//out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
			if ( mFile.exists() ) {
				//File exists at path entry i
				found_file = mFile.fullName();
				break;
			} else {
				//out << "File Not Found." << endl;
			}

			////Maybe it's a relative path
			//mFile.setRawPath( importFile.rawPath() + paths[i] );
			//				out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
			//if ( mFile.exists() ) {
			//	//File exists at path entry i
			//	found_file = mFile.fullName();
			//	break;
			//} else {
			//	out << "File Not Found." << endl;
			//}
		}
	}

	if ( found_file == "" ) {
		//None of the searches found the file... just use the original value
		//from the NIF
		found_file = file_name.c_str();
	}

	nodeFn.findPlug( MString("ftn") ).setValue(found_file);

	//Get global texture list
	MItDependencyNodes nodeIt( MFn::kTextureList );
	MObject texture_list = nodeIt.item();
	MFnDependencyNode slFn;
	slFn.setObject( texture_list );

	MPlug textures = slFn.findPlug( MString("textures") );

	// Loop until an available connection is found
	MPlug element;
	int next = 0;
	while( true )
	{
		element = textures.elementByLogicalIndex( next );

		// If this plug isn't connected, break and use it below.
		if ( element.isConnected() == false )
			break;

		next++;
	}

	MPlug message = nodeFn.findPlug( MString("message") );

	// Connect '.message' plug of render node to "shaders"/"textures" plug of default*List
	MDGModifier dgModifier;
	dgModifier.connect( message, element );

	dgModifier.doIt();

	return obj;
}

void NifMaterialImporter::ImportMaterialsAndTextures( NiAVObjectRef & root )
{
	//Gather all materials and textures from the file
	this->translatorData.mtCollection.GatherMaterials( root );

	//out << mtCollection.GetNumTextures() << " textures and " << mtCollection.GetNumMaterials() << " found." << endl;

	//Cycle through each texture that was found, creating a Maya fileTexture for it
	for ( size_t i = 0; i < this->translatorData.mtCollection.GetNumTextures(); ++i ) {
		this->translatorData.importedTextures[i] = this->ImportTexture( this->translatorData.mtCollection.GetTexture(i) );
	}

	//Cycle through each material that was found, creating a Maya phong shader for it
	for ( size_t i = 0; i < this->translatorData.mtCollection.GetNumMaterials(); ++i ) {
		this->translatorData.importedMaterials[i] = this->ImportMaterial( this->translatorData.mtCollection.GetMaterial(i) );
	}
}

MObject NifMaterialImporter::ImportMaterial( MaterialWrapper & mw )
{
	//Create the material but don't connect it to parent yet
	MFnPhongShader phongFn;
	MObject obj = phongFn.create();
	Color3 color;

	NiMaterialPropertyRef niMatProp = mw.GetColorInfo();

	//See if the user wants the ambient color imported
	if ( !this->translatorOptions.import_no_ambient ) {
		color = niMatProp->GetAmbientColor();
		phongFn.setAmbientColor( MColor(color.r, color.g, color.b) );
	}

	color = niMatProp->GetDiffuseColor();
	phongFn.setColor( MColor(color.r, color.g, color.b) );


	//Set Specular color to 0 unless the mesh has a NiSpecularProperty
	NiSpecularPropertyRef niSpecProp = mw.GetSpecularInfo();

	if ( niSpecProp != NULL && (niSpecProp->GetFlags() & 1) == true ) {
		//This mesh uses specular color - load it
		color = niMatProp->GetSpecularColor();
		phongFn.setSpecularColor( MColor(color.r, color.g, color.b) );
	} else {
		phongFn.setSpecularColor( MColor( 0.0f, 0.0f, 0.0f) );
	}

	color = niMatProp->GetEmissiveColor();
	phongFn.setIncandescence( MColor(color.r, color.g, color.b) );

	if ( color.r > 0.0 || color.g > 0.0 || color.b > 0.0) {
		phongFn.setGlowIntensity( 0.25 );
	}

	float glossiness = niMatProp->GetGlossiness();
	phongFn.setCosPower( glossiness );

	float alpha = niMatProp->GetTransparency();
	//Maya's concept of alpha is the reverse of the NIF's concept
	alpha = 1.0f - alpha;
	phongFn.setTransparency( MColor( alpha, alpha, alpha, alpha) );

	MString name = this->translatorUtils.MakeMayaName( niMatProp->GetName() );
	phongFn.setName( name );

	return obj;
}

NifMaterialImporter::~NifMaterialImporter() {

}
