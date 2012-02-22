#include "include/Exporters/NifMaterialExporterSkyrim.h"

NifMaterialExporterSkyrim::NifMaterialExporterSkyrim() {

}

NifMaterialExporterSkyrim::NifMaterialExporterSkyrim( NifTranslatorOptionsRef translator_options, NifTranslatorDataRef translator_data, NifTranslatorUtilsRef translator_utils ) {
	this->translatorOptions = translator_options;
	this->translatorData = translator_data;
	this->translatorUtils = translator_utils;
}

void NifMaterialExporterSkyrim::ExportMaterials() {
	MFnDependencyNode shader_node;
	MItDependencyNodes it_nodes( MFn::kLambert);

	while(!it_nodes.isDone()) {
		if(it_nodes.thisNode().hasFn(MFn::kLambert)) {
			shader_node.setObject(it_nodes.thisNode());

			NiAlphaPropertyRef alpha_property = NULL;
			BSShaderTextureSetRef shader_textures = new BSShaderTextureSet();
			BSLightingShaderPropertyRef shader_property = new BSLightingShaderProperty();

			string texture = "";
			string normal_map = "";

			unsigned int shader_type = 0;
			unsigned int shader_flags1 = 0;
			unsigned int shader_flags2 = 0;

			Color3 emissive_color(0.0, 0.0, 0.0);
			Color3 specular_color(0.0, 0.0, 0.0);

			float alpha = 0;
			float glossiness = 12.5;
			float specular_strength = 1;
			float lightning_effect1 = 0.3;
			float lightning_effect2 = 2.0;
			float emissive_saturation = 1;

			TexCoord texture_translation1(0, 0);
			TexCoord texture_repeat(1, 1);

			MColor color;
			MPlugArray connections;
			MObject current_texture;

			for(int i = 0; i < 9; i++) {
				shader_textures->setTexture(i, "");
			}

			MPlug shader_flags_plug = shader_node.findPlug("skyrimShaderFlags1");
			if(!shader_flags_plug.isNull()) {
				shader_flags1 = this->stringToSkyrimShaderFlags1(shader_flags_plug.asString());
			}
			shader_flags_plug = shader_node.findPlug("skyrimShaderFlags2");
			if(!shader_flags_plug.isNull()) {
				shader_flags2 = this->stringToSkyrimShaderFlags2(shader_flags_plug.asString());
			}
			MPlug transparency_plug = shader_node.findPlug("transparency");
			if(!transparency_plug.isNull()) {
				alpha = 1 - transparency_plug.asFloat();
			}

			this->GetColor(shader_node, "color", color, current_texture);
			if(!current_texture.isNull()) {
				string file_name = this->ExportTexture(current_texture);
				shader_textures->setTexture(0, file_name);
				MFnDependencyNode texture_node(current_texture);
				texture_node.findPlug("outAlpha").connectedTo(connections, false, true);
				if(connections.length() > 0 && connections[0].node() == shader_node.object()) {
					alpha_property = new NiAlphaProperty();
					alpha_property->SetFlags(237);
				}
			}

			connections.clear();
			MPlug in_normal = shader_node.findPlug("normalCamera");
			in_normal.connectedTo(connections, true, false);
			if(connections.length() > 0) {
				MPlug bump_plug = connections[0];
				MFnDependencyNode bump_node(bump_plug.node());
				MPlug bump_value_plug = bump_node.findPlug("bumpValue");
				connections.clear();
				bump_value_plug.connectedTo(connections, true, false);
				if(connections.length() > 0) {
					MPlug alpha_plug = connections[0];
					current_texture = alpha_plug.node();
					string file_name = this->ExportTexture(current_texture);
					shader_textures->setTexture(1, file_name);
				}
			}

			if(shader_node.object().hasFn(MFn::Type::kPhong)) {
				MFnPhongShader phong_node(shader_node.object());
				glossiness = phong_node.findPlug("cosinePower").asFloat();

				MColor incadescence_color = phong_node.incandescence();
				emissive_color.b = incadescence_color.b;
				emissive_color.g = incadescence_color.g;
				emissive_color.r = incadescence_color.r;

				MColor specular_color_m = phong_node.specularColor();
				specular_color.b = specular_color_m.b;
				specular_color.g = specular_color_m.g;
				specular_color.r = specular_color_m.r;

				specular_strength = phong_node.reflectivity() * 1000;
				emissive_saturation = phong_node.glowIntensity() * 1000;
			} else {
				MGlobal::displayWarning("Warning! Shader types besides phong aren't supported too well");
			}

			shader_property->setTextureSet(shader_textures);
			shader_property->setGlossiness(glossiness);
			shader_property->setEmissiveColor(emissive_color);
			shader_property->setSpecularColor(specular_color);
			shader_property->setTextureTranslation1(texture_translation1);
			shader_property->setTextureRepeat(texture_repeat);
			shader_property->setEmissiveSaturation(emissive_saturation);
			shader_property->setShaderFlags1(shader_flags1);
			shader_property->setShaderFlags2(shader_flags2);
			shader_property->setUnknownInt7(3);
			shader_property->setAlpha(alpha);
			

			vector<NiPropertyRef> properties;
			properties.push_back(DynamicCast<NiProperty>(shader_property));
			if(alpha_property != NULL) {
				properties.push_back(DynamicCast<NiProperty>(alpha_property));
			}

			this->translatorData->shaders[shader_node.name().asChar()] = properties;
		} else {
			MGlobal::displayWarning("Warning! Unsupported shader type found");
		}

		it_nodes.next();
	}
}

string NifMaterialExporterSkyrim::ExportTexture( MObject texture ) {
	MFnDependencyNode texture_node;
	MString texture_name;
	MPlug current_plug;
	MPlug size_x_plug;
	MPlug size_y_plug;

	texture_node.setObject(texture);
	size_x_plug = texture_node.findPlug("outSizeX");
	size_y_plug = texture_node.findPlug("outSizeY");

	float osx = size_x_plug.asFloat();
	float osy = size_y_plug.asFloat();

	current_plug = texture_node.findPlug("fileTextureName");
	texture_name = current_plug.asString();

	if( ((int(osx) & (int(osx) - 1)) != 0 ) || ((int(osy) & (int(osy) - 1)) != 0 )) {
		//Print the value for now:
		stringstream ss;

		ss << "File texture " << texture_name.asChar() << " has dimensions that are not powers of 2.  Non-power of 2 dimentions will not work in most games.";
		MGlobal::displayWarning( ss.str().c_str() );
	}

	string file_name = texture_name.asChar();
	//Figure fixed file name
	string::size_type index;
	MStringArray paths;
	MString(this->translatorOptions->texturePath.c_str()).split( '|', paths );
	switch( this->translatorOptions->texturePathMode ) {
	case PATH_MODE_AUTO:
		for ( unsigned p = 0; p < paths.length(); ++p ) {
			unsigned len = paths[p].length();
			if ( len >= file_name.size() ) {
				continue;
			}
			if ( file_name.substr( 0, len ) == paths[p].asChar() ) {
				//Found a matching path, cut matching part out
				file_name = file_name.substr( len + 1 );
				break;
			}
		}
		break;
	case PATH_MODE_PREFIX:
	case PATH_MODE_NAME:
		index = file_name.rfind( "/" );
		if ( index != string::npos ) { 
			//We don't want the slash itself
			if ( index + 1 < file_name.size() ) {
				file_name = file_name.substr( index + 1 );
			}
		}
		if ( this->translatorOptions->texturePathMode == PATH_MODE_NAME ) {
			break;
		}
		//Now we're doing the prefix case
		file_name = this->translatorOptions->texturePathPrefix + file_name;
		break;

		//Do nothing for full path since the full path is already
		//set in file name
	case PATH_MODE_FULL: break;
	}

	//Now make all slashes back slashes since some games require this
	for ( unsigned i = 0; i < file_name.size(); ++i ) {
		if ( file_name[i] == '/' ) {
			file_name[i] = '\\';
		}
	}

	return file_name;
}

unsigned int NifMaterialExporterSkyrim::stringToSkyrimShaderFlags1( MString flags_string ) {
	unsigned int shader_flags = 0;
	
	MStringArray flags;
	flags_string.split('|', flags);
	for(int i = 0; i < flags.length(); i++) {
		MString flag = flags[i];
		if (flag == "SLSF1_Specular") {
			shader_flags = (shader_flags | 1);
		} else if (flag == "SLSF1_Skinned") {
			shader_flags = (shader_flags | 2);
		} else if (flag == "SLSF1_Temp_Refraction") {
			shader_flags = (shader_flags | 4);
		} else if (flag == "SLSF1_Vertex_Alpha") {
			shader_flags = (shader_flags | 8);
		} else if (flag == "SLSF1_Greyscale_To_PaletteColor") {
			shader_flags = (shader_flags | 16);
		} else if (flag == "SLSF1_Greyscale_To_PaletteAlpha") {
			shader_flags = (shader_flags | 32);
		} else if (flag == "SLSF1_Use_Falloff") {
			shader_flags = (shader_flags | 64);
		} else if (flag == "SLSF1_Environment_Mapping") {
			shader_flags = (shader_flags | 128);
		} else if (flag == "SLSF1_Recieve_Shadows") {
			shader_flags = (shader_flags | 256);
		} else if (flag == "SLSF1_Cast_Shadows") {
			shader_flags = (shader_flags | 512);
		} else if (flag == "SLSF1_Facegen_Detail_Map") {
			shader_flags = (shader_flags | 1024);
		} else if (flag == "SLSF1_Parallax") {
			shader_flags = (shader_flags | 2048);
		} else if (flag == "SLSF1_Model_Space_Normals") {
			shader_flags = (shader_flags | 4096);
		} else if (flag == "SLSF1_Non-Projective_Shadows") {
			shader_flags = (shader_flags | 8192);
		} else if (flag == "SLSF1_Landscape") {
			shader_flags = (shader_flags | 16384);
		} else if (flag == "SLSF1_Refraction") {
			shader_flags = (shader_flags | 32768);
		} else if (flag == "SLSF1_Fire_Refraction") {
			shader_flags = (shader_flags | 65536);
		} else if (flag == "SLSF1_Eye_Environment_Mapping") {
			shader_flags = (shader_flags | 131072);
		} else if (flag == "SLSF1_Hair_Soft_Lighting") {
			shader_flags = (shader_flags | 262144);
		} else if (flag == "SLSF1_Screendoor_Alpha_Fade") {
			shader_flags = (shader_flags | 524288);
		} else if (flag == "SLSF1_Localmap_Hide_Secret") {
			shader_flags = (shader_flags | 1048576);
		} else if (flag == "SLSF1_FaceGen_RGB_Tint") {
			shader_flags = (shader_flags | 2097152);
		} else if (flag == "SLSF1_Own_Emit") {
			shader_flags = (shader_flags | 4194304);
		} else if (flag == "SLSF1_Projected_UV") {
			shader_flags = (shader_flags | 8388608);
		} else if (flag == "SLSF1_Multiple_Textures") {
			shader_flags = (shader_flags | 16777216);
		} else if (flag == "SLSF1_Remappable_Textures") {
			shader_flags = (shader_flags | 33554432);
		} else if (flag == "SLSF1_Decal") {
			shader_flags = (shader_flags | 67108864);
		} else if (flag == "SLSF1_Dynamic_Decal") {
			shader_flags = (shader_flags | 134217728);
		} else if (flag == "SLSF1_Parallax_Occlusion") {
			shader_flags = (shader_flags | 268435456);
		} else if (flag == "SLSF1_External_Emittance") {
			shader_flags = (shader_flags | 536870912);
		} else if (flag == "SLSF1_Soft_Effect") {
			shader_flags = (shader_flags | 1073741824);
		} else if (flag == "SLSF1_ZBuffer_Test") {
			shader_flags = (shader_flags | 2147483648);
		}
	}

	return shader_flags;
}

unsigned int NifMaterialExporterSkyrim::stringToSkyrimShaderFlags2( MString flags_string ) {
	unsigned int shader_flags = 0;

	MStringArray flags;
	flags_string.split('|', flags);
	for(int i = 0; i < flags.length(); i++) {
		MString flag = flags[i];
		if (flag == "SLSF2_ZBuffer_Write") {
			shader_flags = (shader_flags | 1);
		} else if (flag == "SLSF2_LOD_Landscape") {
			shader_flags = (shader_flags | 2);
		} else if (flag == "SLSF2_LOD_Objects") {
			shader_flags = (shader_flags | 4);
		} else if (flag == "SLSF2_No_Fade") {
			shader_flags = (shader_flags | 8);
		} else if (flag == "SLSF2_Double_Sided") {
			shader_flags = (shader_flags | 16);
		} else if (flag == "SLSF2_Vertex_Colors") {
			shader_flags = (shader_flags | 32);
		} else if (flag == "SLSF2_Glow_Map") {
			shader_flags = (shader_flags | 64);
		} else if (flag == "SLSF2_Assume_Shadowmask") {
			shader_flags = (shader_flags | 128);
		} else if (flag == "SLSF2_Packed_Tangent") {
			shader_flags = (shader_flags | 256);
		} else if (flag == "SLSF2_Multi_Index_Snow") {
			shader_flags = (shader_flags | 512);
		} else if (flag == "SLSF2_Vertex_Lighting") {
			shader_flags = (shader_flags | 1024);
		} else if (flag == "SLSF2_Uniform_Scale") {
			shader_flags = (shader_flags | 2048);
		} else if (flag == "SLSF2_Fit_Slope") {
			shader_flags = (shader_flags | 4096);
		} else if (flag == "SLSF2_Billboard") {
			shader_flags = (shader_flags | 8192);
		} else if (flag == "SLSF2_No_LOD_Land_Blend") {
			shader_flags = (shader_flags | 16384);
		} else if (flag == "SLSF2_EnvMap_Light_Fade") {
			shader_flags = (shader_flags | 32768);
		} else if (flag == "SLSF2_Wireframe") {
			shader_flags = (shader_flags | 65536);
		} else if (flag == "SLSF2_Weapon_Blood") {
			shader_flags = (shader_flags | 131072);
		} else if (flag == "SLSF2_Hide_On_Local_Map") {
			shader_flags = (shader_flags | 262144);
		} else if (flag == "SLSF2_Premult_Alpha") {
			shader_flags = (shader_flags | 524288);
		} else if (flag == "SLSF2_Cloud_LOD") {
			shader_flags = (shader_flags | 1048576);
		} else if (flag == "SLSF2_Anisotropic_Lighting") {
			shader_flags = (shader_flags | 2097152);
		} else if (flag == "SLSF2_No_Transparency_Multisampling") {
			shader_flags = (shader_flags | 4194304);
		} else if (flag == "SLSF2_Unused01") {
			shader_flags = (shader_flags | 8388608);
		} else if (flag == "SLSF2_Multi_Layer_Parallax") {
			shader_flags = (shader_flags | 16777216);
		} else if (flag == "SLSF2_Soft_Lighting") {
			shader_flags = (shader_flags | 33554432);
		} else if (flag == "SLSF2_Rim_Lighting") {
			shader_flags = (shader_flags | 67108864);
		} else if (flag == "SLSF2_Back_Lighting") {
			shader_flags = (shader_flags | 134217728);
		} else if (flag == "SLSF2_Unused02") {
			shader_flags = (shader_flags | 268435456);
		} else if (flag == "SLSF2_Tree_Anim") {
			shader_flags = (shader_flags | 536870912);
		} else if (flag == "SLSF2_Effect_Lighting") {
			shader_flags = (shader_flags | 1073741824);
		} else if (flag == "SLSF2_HD_LOD_Objects") {
			shader_flags = (shader_flags | 2147483648);
		}
	}

	return shader_flags;
}

string NifMaterialExporterSkyrim::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifMaterialExporter::asString(verbose)<<endl;
	out<<"NifMaterialExporterSkyrim"<<endl;

	return out.str();
}

const Type& NifMaterialExporterSkyrim::GetType() const {
	return TYPE;
}

const Type NifMaterialExporterSkyrim::TYPE("NifMaterialExporterSkyrim", &NifMaterialExporter::TYPE);
