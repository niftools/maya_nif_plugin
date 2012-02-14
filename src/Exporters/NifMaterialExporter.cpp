#include "include/Exporters/NifMaterialExporter.h"

NifMaterialExporter::NifMaterialExporter() {

}

NifMaterialExporter::NifMaterialExporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {

}


void NifMaterialExporter::ExportShaders()
{
	//out << "NifTranslator::ExportShaders {" << endl;
	// iterate through the shaders in the scene
	MItDependencyNodes itDep(MFn::kLambert);

	//out << "Iterating through all shaders in scehe..." << endl;
	// we have to keep iterating until we get through
	// all of the nodes in the scene
	//
	for ( ; !itDep.isDone(); itDep.next() ) {

		MColor diffuse, ambient, emissive, transparency, specular(0.0, 0.0, 0.0, 0.0);
		string mat_name;
		float glossiness = 20.0f;
		bool use_alpha = false;
		bool use_spec = false;
		bool base_texture = false;
		bool multi_texture = false;
		MObject diff_tex, ambi_tex, emis_tex, glos_tex, spec_tex, tran_tex;

		//out << "Testing for MFnLambertShader function set." << endl;
		if ( itDep.item().hasFn( MFn::kLambert ) ) {
			//out << "Attaching MFnLambertShader function set." << endl;
			//All shaders inherit from lambert
			MFnLambertShader lambertFn( itDep.item() );

			//Get name
			mat_name = this->translatorUtils->MakeNifName(lambertFn.name());

			//check if the material is a skin
			int is_skin = mat_name.find("skin");
			if(is_skin == 0) {
				mat_name = "skin";
			}

			//--Get info from Color slots--//
			//out << "Getting color slot info..." << endl;
			GetColor( lambertFn, "color", diffuse, diff_tex );
			GetColor( lambertFn,"ambientColor", ambient, ambi_tex );
			GetColor( lambertFn,"incandescence", emissive, emis_tex );
			GetColor( lambertFn,"transparency", transparency, tran_tex );

			//Shader may also have specular color
			if ( itDep.item().hasFn( MFn::kReflect ) ) {
				//out << "Attaching MFnReflectShader function set" << endl;
				MFnReflectShader reflectFn( itDep.item() );

				//out << "Getting specular color" << endl;
				GetColor( reflectFn, "specularColor", specular, spec_tex );

				if ( specular.r != 0.0 && specular.g != 0.0 && specular.b != 0.0 ) {
					use_spec = true;
				}

				//Handle cosine power/glossiness
				if ( itDep.item().hasFn( MFn::kPhong ) ) {
					//out << "Attaching MFnReflectShader function set" << endl;
					MFnPhongShader phongFn( itDep.item() );

					//out << "Getting cosine power" << endl;
					MPlug p = phongFn.findPlug("cosinePower");
					glossiness = phongFn.cosPower();

					//out << "Get plugs connected to cosinePower attribute" << endl;
					// get plugs connected to cosinePower attribute
					MPlugArray plugs;
					p.connectedTo(plugs,true,false);

					//out << "See if any file textures are present" << endl;
					// see if any file textures are present
					for( int i = 0; i != plugs.length(); ++i ) {

						// if file texture found
						if( plugs[i].node().apiType() == MFn::kFileTexture ) {
							//out << "File texture found" << endl;
							glos_tex = plugs[i].node();

							// stop looping
							break;
						}
					}

				} else {
					//Not a phong texture so not currently supported
					MGlobal::displayWarning("Only Shaders with Cosine Power, such as Phong, can currently be used to set exported glossiness.  Default value of 20.0 used.");
					//TODO:  Figure out how to guestimate glossiness from Blin shaders.
				}
			} else {
				//No reflecting shader used, so set some defautls
			}
		}

		//out << "Checking whether base texture is used and if it has alpha" << endl;
		//Check whether base texture is used and if it has has alpha
		if ( diff_tex.isNull() == false ) {
			base_texture = true;

			if ( diff_tex.hasFn( MFn::kDependencyNode ) ) {
				MFnDependencyNode depFn(diff_tex);
				MPlug fha = depFn.findPlug("fileHasAlpha");
				bool value;
				fha.getValue( value );
				if ( value == true ) {
					use_alpha = true;
				}
			}
		}

		//out << "Checking whether any other supported texture types are being used" << endl;
		//Check whether any other supported texture types are being used
		if ( emis_tex.isNull() == false ) {
			multi_texture = true;
		}

		//Create a NIF Material Wrapper and put the collected values into it
		unsigned int mat_index = this->materialCollection.CreateMaterial( true, base_texture, multi_texture, use_spec, use_alpha, this->translatorOptions->exportVersion );
		MaterialWrapper mw = this->materialCollection.GetMaterial( mat_index );

		NiMaterialPropertyRef niMatProp = mw.GetColorInfo();

		if ( use_alpha ) {
			NiAlphaPropertyRef niAlphaProp = mw.GetTranslucencyInfo();
			niAlphaProp->SetFlags(237);
		}

		if ( use_spec ) {
			NiSpecularPropertyRef niSpecProp = mw.GetSpecularInfo();
			niSpecProp->SetSpecularState(true);
		}

		//Set Name
		niMatProp->SetName( mat_name );

		//--Diffuse Color--//

		if ( diff_tex.isNull() == true ) {
			//No Texture
			niMatProp->SetDiffuseColor( Color3( diffuse.r, diffuse.g, diffuse.b ) );
		} else {
			//Has Texture, so set diffuse color to white
			niMatProp->SetDiffuseColor( Color3(1.0f, 1.0f, 1.0f) );

			//Attach texture
			if ( diff_tex.hasFn( MFn::kDependencyNode ) ) {
				MFnDependencyNode depFn(diff_tex);
				string texname = depFn.name().asChar();
				//out << "Base texture found:  " << texname << endl;

				if ( this->textures.find( texname ) != this->textures.end() ) {
					mw.SetTextureIndex( BASE_MAP, this->textures[texname] );
				}
			}

		}

		//--Ambient Color--//	

		if ( ambi_tex.isNull() == true ) {
			//No Texture.  Check if we should export white ambient
			if ( diff_tex.isNull() == false && this->translatorOptions->exportWhiteAmbient == true ) {
				niMatProp->SetAmbientColor( Color3(1.0f, 1.0f, 1.0f) ); // white
			} else {
				niMatProp->SetAmbientColor( Color3( ambient.r, ambient.g, ambient.b ) );
			}
		} else {
			//Ambient texture found.  This is not supported so set ambient to white and warn user.
			niMatProp->SetAmbientColor( Color3(1.0f, 1.0f, 1.0f) ); // white
			MGlobal::displayWarning("Ambient textures are not supported by the NIF format.  Ignored.");
		}

		//--Emmissive Color--//

		if ( emis_tex.isNull() == true ) {
			//No Texture
			niMatProp->SetEmissiveColor( Color3( emissive.r, emissive.g, emissive.b ) );
		} else {
			//Has texture, so set emissive color to white
			niMatProp->SetEmissiveColor( Color3(1.0f, 1.0f, 1.0f) ); //white

			//Attach texture
			if ( emis_tex.hasFn( MFn::kDependencyNode ) ) {
				MFnDependencyNode depFn(emis_tex);
				string texname = depFn.name().asChar();
				//out << "Glow texture found:  " << texname << endl;

				if ( this->textures.find( texname ) != this->textures.end() ) {
					mw.SetTextureIndex( GLOW_MAP, this->textures[texname] );
				}
			}
		}

		//--Transparency--//

		//Any alpha textures are probably set because the base texture has
		//alpha which is connected to this node.  So just set the
		//transparency and don't worry about whethere there's a
		//texture connected or not.
		float trans = (transparency.r + transparency.g + transparency.b) / 3.0f;
		if ( trans != transparency.r ) {
			MGlobal::displayWarning("Colored transparency is not supported by the NIF format.  An average of the color channels will be used.");
		}
		//Maya trans is reverse of NIF trans
		trans = 1.0f - trans;
		niMatProp->SetTransparency( trans );

		//--Specular Color--//

		if ( spec_tex.isNull() == true ) {
			//No Texture.  Check if we should expot black specular
			if ( use_spec == false ) {
				niMatProp->SetSpecularColor( Color3(0.0f, 0.0f, 0.0f) ); // black
			} else {
				niMatProp->SetSpecularColor( Color3( specular.r, specular.g, specular.b ) );
			}
		} else {
			//Specular texture found.  This is not supported so set specular to black and warn user.
			niMatProp->SetSpecularColor( Color3(0.0f, 0.0f, 0.0f) ); // black
			MGlobal::displayWarning("Gloss textures are not supported yet.  Ignored.");
		}

		//--Glossiness--//

		niMatProp->SetGlossiness( glossiness );

		if ( glos_tex.isNull() == false ) {
			//Cosine Power texture found.  This is not supported so warn user.
			MGlobal::displayWarning("Gloss textures are not supported yet.  Ignored.");
		}

		//TODO: Support bump maps, environment maps, gloss maps, and multi-texturing (detail, dark, and decal?)

		//out << "Associating material index with Maya shader name";
		//Associate material index with Maya shader name
		MFnDependencyNode depFn( itDep.item() );
		//out << depFn.name().asChar() << endl;
		this->translatorData->shaders[ depFn.name().asChar() ] = mw.GetProperties();
	}

	//out << "}" << endl;
}


void NifMaterialExporter::GetColor( MFnDependencyNode& fn, MString name, MColor & color, MObject & texture ) {
	//out << "NifTranslator::GetColor( " << fn.name().asChar() << ", " << name.asChar() << ", (" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << "), " << texture.apiType() << " ) {" << endl;
	MPlug p;
	texture = MObject();
	color = MColor();

	//out << "Get a plug to the attribute" << endl;
	// get a plug to the attribute
	p = fn.findPlug( name + "R" );
	p.getValue(color.r);
	p = fn.findPlug( name + "G" );
	p.getValue(color.g);
	p = fn.findPlug( name + "B" );
	p.getValue(color.b);
	p = fn.findPlug( name + "A" );
	p.getValue(color.a);
	p = fn.findPlug(name);

	//out << "Get plugs connected to color attribute" << endl;
	// get plugs connected to color attribute
	MPlugArray plugs;
	p.connectedTo(plugs,true,false);

	//out << "See if any file textures are present" << endl;
	// see if any file textures are present
	for( int i = 0; i != plugs.length(); ++i ) {

		// if file texture found
		if( plugs[i].node().apiType() == MFn::kFileTexture ) {
			//out << "File texture found" << endl;
			texture = plugs[i].node();

			// stop looping
			break;
		}
	}

	//out << "Special processing for base color?" << endl;
	if( name == "color" && color.r < 0.01 && color.g < 0.01 && color.b < 0.01) {
		color.r = color.g = color.b = 0.6f;
	}

	// output the name, color and texture ID
	//out << "\t" << name.asChar() << ":  ("
	//	<< color.r << ", "
	//	<< color.g << ", "
	//	<< color.b << ", "
	//	<< color.a << ")" << endl;

	//out << "}" << endl;
}

void NifMaterialExporter::ExportTextures()
{
	// create an iterator to go through all file textures
	MItDependencyNodes it(MFn::kFileTexture);

	//iterate through all textures
	while(!it.isDone())
	{
		// attach a dependency node to the file node
		MFnDependencyNode fn(it.item());

		// get the attribute for the full texture path and size
		MPlug ftn = fn.findPlug("fileTextureName");
		MPlug osx = fn.findPlug("outSizeX");
		MPlug osy = fn.findPlug("outSizeY");

		// get the filename from the attribute
		MString filename;
		ftn.getValue(filename);

		//Get image size
		float x, y;
		osx.getValue(x);
		osy.getValue(y);

		//Determine whether the dimentions of the texture are powers of two
		if ( ((int(x) & (int(x) - 1)) != 0 ) || ((int(y) & (int(y) - 1)) != 0 ) ) {

			//Print the value for now:
			stringstream ss;

			ss << "File texture " << filename.asChar() << " has dimentions that are not powers of 2.  Non-power of 2 dimentions will not work in most games.";
			MGlobal::displayWarning( ss.str().c_str() );
		}

		//Create a NIF texture wrapper
		unsigned int tex_index = this->materialCollection.CreateTexture( this->translatorOptions->exportVersion );
		TextureWrapper tw = this->materialCollection.GetTexture( tex_index );

		//Get Node Name
		tw.SetObjectName( this->translatorUtils->MakeNifName( fn.name() ) );

		MString fname;
		ftn.getValue(fname);
		string fileName = fname.asChar();

		//Replace back slash with forward slash to match with paths
		for ( unsigned i = 0; i < fileName.size(); ++i ) {
			if ( fileName[i] == '\\' ) {
				fileName[i] = '/';
			}
		}

		//out << "Full Texture Path:  " << fname.asChar() << endl;

		//Figure fixed file name
		string::size_type index;
		MStringArray paths;
		MString(this->translatorOptions->texturePath.c_str()).split( '|', paths );
		switch( this->translatorOptions->texturePathMode ) {
		case PATH_MODE_AUTO:
			for ( unsigned p = 0; p < paths.length(); ++p ) {
				unsigned len = paths[p].length();
				if ( len >= fileName.size() ) {
					continue;
				}
				if ( fileName.substr( 0, len ) == paths[p].asChar() ) {
					//Found a matching path, cut matching part out
					fileName = fileName.substr( len + 1 );
					break;
				}
			}
			break;
		case PATH_MODE_PREFIX:
		case PATH_MODE_NAME:
			index = fileName.rfind( "/" );
			if ( index != string::npos ) { 
				//We don't want the slash itself
				if ( index + 1 < fileName.size() ) {
					fileName = fileName.substr( index + 1 );
				}
			}
			if ( this->translatorOptions->texturePathMode == PATH_MODE_NAME ) {
				break;
			}
			//Now we're doing the prefix case
			fileName = this->translatorOptions->texturePathPrefix + fileName;
			break;

			//Do nothing for full path since the full path is already
			//set in file name
		case PATH_MODE_FULL: break;
		}

		//Now make all slashes back slashes since some games require this
		for ( unsigned i = 0; i < fileName.size(); ++i ) {
			if ( fileName[i] == '/' ) {
				fileName[i] = '\\';
			}
		}

		//out << "File Name:  " << fileName << endl;

		tw.SetExternalTexture( fileName );

		//Associate NIF texture index with fileTexture DagPath
		string path = fn.name().asChar();
		this->textures[path] = tex_index;

		// get next fileTexture
		it.next();
	}
}

string NifMaterialExporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifMaterialExporter"<<endl;

	return out.str();
}

const Type& NifMaterialExporter::GetType() const {
	return TYPE;
}

const Type NifMaterialExporter::TYPE("NifMaterialExporter",&NifTranslatorFixtureItem::TYPE);
