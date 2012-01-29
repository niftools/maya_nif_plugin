#include "include/Importers/NifMaterialImporter.h"

NifMaterialImporter::NifMaterialImporter() {

}

NifMaterialImporter::NifMaterialImporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils )
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
	mFile.setRawPath( this->translatorData->importFile.rawPath() );
	//out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
	if ( mFile.exists() ) {
		found_file =  mFile.fullName();
	} else {
		//out << "File Not Found." << endl;
	}

	if ( found_file == "" ) {
		//Check if the file exists in any of the given search paths
		MStringArray paths;
		MString(this->translatorOptions->texturePath.c_str()).split( '|', paths );

		for ( unsigned i = 0; i < paths.length(); ++i ) {
			if ( paths[i].substring(0,0) == "." ) {
				//Relative path
				mFile.setRawPath( this->translatorData->importFile.rawPath() + paths[i] );
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
	this->materialCollection.GatherMaterials( root );

	//out << mtCollection.GetNumTextures() << " textures and " << mtCollection.GetNumMaterials() << " found." << endl;

	bool reserved = MProgressWindow::reserve();

	if(reserved == true) {

		MProgressWindow::setProgressMin(0);
		MProgressWindow::setProgressMax(this->materialCollection.GetNumTextures() - 1);
		MProgressWindow::setTitle("Importing textures");
		MProgressWindow::startProgress();

		//Cycle through each texture that was found, creating a Maya fileTexture for it
		for ( size_t i = 0; i < this->materialCollection.GetNumTextures(); ++i ) {
			this->importedTextures[i] = this->ImportTexture( this->materialCollection.GetTexture(i) );
			MProgressWindow::advanceProgress(1);
		}

		MProgressWindow::endProgress();
	} else {
		for ( size_t i = 0; i < this->materialCollection.GetNumTextures(); ++i ) {
			this->importedTextures[i] = this->ImportTexture( this->materialCollection.GetTexture(i) );
		}
	}

	reserved = MProgressWindow::reserve();

	if(reserved == true) {

		MProgressWindow::setProgressMin(0);
		MProgressWindow::setProgressMax(this->materialCollection.GetNumMaterials() - 1);
		MProgressWindow::setTitle("Importing materials");
		MProgressWindow::startProgress();

		//Cycle through each material that was found, creating a Maya phong shader for it
		for ( size_t i = 0; i < this->materialCollection.GetNumMaterials(); ++i ) {
			this->importedMaterials[i] = this->ImportMaterial( this->materialCollection.GetMaterial(i) );

			MProgressWindow::advanceProgress(1);
		}

		MProgressWindow::endProgress();
	} else {
		//Cycle through each material that was found, creating a Maya phong shader for it
		for ( size_t i = 0; i < this->materialCollection.GetNumMaterials(); ++i ) {
			this->importedMaterials[i] = this->ImportMaterial( this->materialCollection.GetMaterial(i) );
		}
	}

	MDGModifier dgModifier;

	for(int material_index = 0; material_index < this->materialCollection.GetNumMaterials(); material_index++) {
		MaterialWrapper mw = this->materialCollection.GetMaterial(material_index);
		vector<NifTextureConnectorRef> texture_connectors;
		vector<NiPropertyRef> property_group = mw.GetProperties();

		MObject material_object = this->importedMaterials[material_index];
		this->translatorData->importedMaterials.push_back(pair<vector<NiPropertyRef>, MObject>(property_group, material_object));

		MFnPhongShader phongFn(material_object);

		//Cycle through for each type of texture
		for (int i = 0; i < 8; ++i) {
			//Texture type is supported, get Texture
			unsigned int tex_index = mw.GetTextureIndex( TexType(i) );

			//If there is no matching texture for this slot, continue to the next one.
			if ( tex_index == NO_TEXTURE ) {
				continue;
			}

			//Skip this texture slot if it's an un-supported type.
			switch(i) {
			case DARK_MAP:
				//Temporary until/if Dark Textures are supported
				MGlobal::displayWarning( "Dark Textures are not yet supported." );
				continue;
			case DETAIL_MAP:
				//Temporary until/if Detail Textures are supported
				MGlobal::displayWarning( "Detail Textures are not yet supported." );
				continue;
			case GLOSS_MAP:
				//Temporary until/if Detail Textures are supported
				MGlobal::displayWarning( "Gloss Textures are not yet supported." );
				continue;
			case BUMP_MAP:
				//Temporary until/if Bump Map Textures are supported
				MGlobal::displayWarning( "Bump Map Textures are not yet supported." );
				continue;
			case DECAL_0_MAP:
			case DECAL_1_MAP:
				//Temporary until/if Decal Textures are supported
				MGlobal::displayWarning( "Decal Textures are not yet supported." );
				continue;
			};


			//Look up Maya fileTexture
			if ( this->importedTextures.find( tex_index ) == this->importedTextures.end() ) {
				//There was no match in the previously imported textures.
				//This may be caused by the NIF textures being stored internally, so just continue to the next slot.
				continue;
			}

			MObject texture_object = this->importedTextures[tex_index];

			//out << "Connecting a texture..." << endl;
			//Connect the texture
			MFnDependencyNode texture_node;
			NiAlphaPropertyRef alpha_property = mw.GetTranslucencyInfo();
			if ( texture_object != MObject::kNullObj ) {
				texture_node.setObject(texture_object);
				MPlug texture_color = texture_node.findPlug( MString("outColor") );
				switch(i) {
				case BASE_MAP:
					//Base Texture
					dgModifier.connect( texture_color, phongFn.findPlug("color") );
					//Check if Alpha needs to be used

					if ( alpha_property != NULL && ( alpha_property->GetBlendState() == true || alpha_property->GetTestState() == true ) ) {
						//Alpha is used, connect it
						dgModifier.connect( texture_node.findPlug("outTransparency"), phongFn.findPlug("transparency") );
					}
					break;
				case GLOW_MAP:
					//Glow Texture
					dgModifier.connect( texture_node.findPlug("outAlpha"), phongFn.findPlug("glowIntensity") );
					texture_node.findPlug("alphaGain").setValue(0.25);
					texture_node.findPlug("defaultColorR").setValue( 0.0 );
					texture_node.findPlug("defaultColorG").setValue( 0.0 );
					texture_node.findPlug("defaultColorB").setValue( 0.0 );
					dgModifier.connect( texture_color, phongFn.findPlug("incandescence") );
					break;
				}

				//Check for clamp mode
				bool wrap_u = true, wrap_v = true;
				TexClampMode clamp_mode = mw.GetTexClampMode( TexType(i) );
				if ( clamp_mode == CLAMP_S_CLAMP_T ) {
					wrap_u = false;
					wrap_v = false;
				} else if ( clamp_mode == CLAMP_S_WRAP_T ) {
					wrap_u = false;
				} else if ( clamp_mode == WRAP_S_CLAMP_T ) {
					wrap_v = false;
				}

				//Create 2D Texture Placement
				MFnDependencyNode texture_placement;
				texture_placement.create( "place2dTexture", "place2dTexture" );
				texture_placement.findPlug("wrapU").setValue(wrap_u);
				texture_placement.findPlug("wrapV").setValue(wrap_v);

				//Connect all the 18 things
				dgModifier.connect( texture_placement.findPlug("coverage"), texture_node.findPlug("coverage") );
				dgModifier.connect( texture_placement.findPlug("mirrorU"), texture_node.findPlug("mirrorU") );
				dgModifier.connect( texture_placement.findPlug("mirrorV"), texture_node.findPlug("mirrorV") );
				dgModifier.connect( texture_placement.findPlug("noiseUV"), texture_node.findPlug("noiseUV") );
				dgModifier.connect( texture_placement.findPlug("offset"), texture_node.findPlug("offset") );
				dgModifier.connect( texture_placement.findPlug("outUV"), texture_node.findPlug("uvCoord") );
				dgModifier.connect( texture_placement.findPlug("outUvFilterSize"), texture_node.findPlug("uvFilterSize") );
				dgModifier.connect( texture_placement.findPlug("repeatUV"), texture_node.findPlug("repeatUV") );
				dgModifier.connect( texture_placement.findPlug("rotateFrame"), texture_node.findPlug("rotateFrame") );
				dgModifier.connect( texture_placement.findPlug("rotateUV"), texture_node.findPlug("rotateUV") );
				dgModifier.connect( texture_placement.findPlug("stagger"), texture_node.findPlug("stagger") );
				dgModifier.connect( texture_placement.findPlug("translateFrame"), texture_node.findPlug("translateFrame") );
				dgModifier.connect( texture_placement.findPlug("vertexCameraOne"), texture_node.findPlug("vertexCameraOne") );
				dgModifier.connect( texture_placement.findPlug("vertexUvOne"), texture_node.findPlug("vertexUvOne") );
				dgModifier.connect( texture_placement.findPlug("vertexUvTwo"), texture_node.findPlug("vertexUvTwo") );
				dgModifier.connect( texture_placement.findPlug("vertexUvThree"), texture_node.findPlug("vertexUvThree") );
				dgModifier.connect( texture_placement.findPlug("wrapU"), texture_node.findPlug("wrapU") );
				dgModifier.connect( texture_placement.findPlug("wrapV"), texture_node.findPlug("wrapV") );
				//(Whew!)

				//Create uvChooser if necessary
				unsigned int uv_set = mw.GetTexUVSetIndex( TexType(i) );
				if ( uv_set > 0 ) {
					NifTextureConnectorRef texture_connector = new NifTextureConnector(texture_placement, uv_set);
					texture_connectors.push_back(texture_connector);
				}
			}
		}
		this->translatorData->importedTextureConnectors.push_back(pair<vector<NiPropertyRef>, vector<NifTextureConnectorRef>>(property_group, texture_connectors));
	}

	dgModifier.doIt();
}

MObject NifMaterialImporter::ImportMaterial( MaterialWrapper & mw )
{
	//Create the material but don't connect it to parent yet
	MFnPhongShader phongFn;
	MObject obj = phongFn.create();
	Color3 color;

	NiMaterialPropertyRef niMatProp = mw.GetColorInfo();

	//See if the user wants the ambient color imported
	if ( !this->translatorOptions->importNoAmbient ) {
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

	MString name = this->translatorUtils->MakeMayaName( niMatProp->GetName() );
	phongFn.setName( name );

	return obj;
}

std::string NifMaterialImporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose);
	out<<"NifMaterialImporter"<<endl;

	return out.str();
}

const Type& NifMaterialImporter::GetType() const {
	return TYPE;
}

const Type NifMaterialImporter::TYPE("NifMaterialImporter",&NifTranslatorFixtureItem::TYPE);
