#include "include/Importers/NifMaterialImporterSkyrim.h"


NifMaterialImporterSkyrim::NifMaterialImporterSkyrim() {

}

NifMaterialImporterSkyrim::NifMaterialImporterSkyrim( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
}

void NifMaterialImporterSkyrim::ImportMaterialsAndTextures( NiAVObjectRef & root ) {
	this->GatherMaterialsAndTextures(root);
}

void NifMaterialImporterSkyrim::GatherMaterialsAndTextures( NiAVObjectRef & root ) {
	if(root->GetType().IsDerivedType(NiGeometry::TYPE)) {
		NiGeometryRef geometry = DynamicCast<NiGeometry>(root);
		NiAlphaPropertyRef alpha_property = NULL;
		BSLightingShaderPropertyRef shader_property = NULL;

		array<2, Ref<NiProperty>> properties = geometry->getBsProperties(); 

		for(int i = 0; i < 2; i++) {
			NiPropertyRef current_property = properties[i]; 

			if(current_property != NULL && current_property->GetType().IsSameType(BSLightingShaderProperty::TYPE)) {
				shader_property = DynamicCast<BSLightingShaderProperty>(current_property);
			}
			if(current_property != NULL && current_property->GetType().IsSameType(NiAlphaProperty::TYPE)) {
				alpha_property = DynamicCast<NiAlphaProperty>(current_property);
			}
		}

		int valid_properties = 0;

		if(alpha_property != NULL) {
			valid_properties++;
		}
		if(shader_property != NULL) {
			valid_properties++;
		}

		if(valid_properties == 0) {
			return;
		}

		MObject found_material;

		for(int i = 0; i < this->property_groups.size(); i++) {
			vector<NiPropertyRef> property_group = this->property_groups[i];
			int similarities = 0;

			for(int i = 0; i < property_group.size(); i++) {
				if(alpha_property != NULL && property_group[i]->GetType().IsSameType(NiAlphaProperty::TYPE)) {
					NiAlphaPropertyRef current_alpha_property = DynamicCast<NiAlphaProperty>(property_group[i]);
					if(current_alpha_property == alpha_property) {
						similarities++;
					}
				}
				if(shader_property != NULL && property_group[i]->GetType().IsSameType(BSLightingShaderProperty::TYPE)) {
					BSLightingShaderPropertyRef current_shader_property = DynamicCast<BSLightingShaderProperty>(property_group[i]);
					if(current_shader_property == shader_property) {
						similarities++;
					}
				}
			}

			if(valid_properties != 0 && similarities == valid_properties) {
				found_material = this->imported_materials[i];
				break;
			}
		}

		if(found_material.isNull()) {
			MFnPhongShader new_shader;
			found_material = new_shader.create();

			if(shader_property != NULL) {
				MColor reflective_color(shader_property->getSpecularColor().r, shader_property->getSpecularColor().g, shader_property->getSpecularColor().b);
				MColor incadescence_color(shader_property->getEmissiveColor().r, shader_property->getEmissiveColor().g, shader_property->getEmissiveColor().b);
				float glow_intensity = shader_property->getEmissiveSaturation() / 1000;
				float reflective_strength = shader_property->getSpecularStrength() / 1000;
				float cosine_power = shader_property->getGlossiness();
				float transparency = 1.0f - shader_property->getAlpha();

				new_shader.setReflectedColor(reflective_color);
				new_shader.findPlug("reflectivity").setFloat(reflective_strength);
				new_shader.setIncandescence(incadescence_color);
				new_shader.setGlowIntensity(glow_intensity);
				new_shader.setCosPower(cosine_power);
				new_shader.setTransparency(transparency);

				BSShaderTextureSetRef texture_set = shader_property->getTextureSet();
				if(texture_set != NULL) {
					MDGModifier dg_modifier;
					string color_texture = texture_set->getTexture(0);
					string normal_map = texture_set->getTexture(1);

					if(color_texture.length() > 0) {
						MString color_texture_file = this->GetTextureFilePath(color_texture);
						MFnDependencyNode color_texture_node;
						color_texture_node.create(MString("file"), MString(color_texture.c_str()));
						color_texture_node.findPlug("ftn").setValue(color_texture_file);
						MItDependencyNodes node_it(MFn::kTextureList);
						MFnDependencyNode texture_list(node_it.item());
						MPlug textures = texture_list.findPlug(MString("textures"));

						MPlug current_texture;
						int next = 0;
						while(true) {
							current_texture = textures.elementByLogicalIndex(next);

							if(current_texture.isConnected() == false) {
								break;
							}
							next++;
						}
						MPlug texture_message = color_texture_node.findPlug(MString("message"));
						dg_modifier.connect(texture_message, current_texture);

						MPlug color_texture_outcolor = color_texture_node.findPlug("outColor");
						MPlug new_shader_color = new_shader.findPlug("color");

						dg_modifier.connect(color_texture_outcolor, new_shader_color);

						if(alpha_property != NULL) {
							MPlug color_texture_outalpha = color_texture_node.findPlug("outTransparency");
							MPlug new_shader_transparency = new_shader.findPlug("transparency");
							dg_modifier.connect(color_texture_outalpha, new_shader_transparency);
						}

						MFnDependencyNode color_texture_placement;
						color_texture_placement.create("place2dTexture", "place2dTexture");

						dg_modifier.connect( color_texture_placement.findPlug("coverage"), color_texture_node.findPlug("coverage") );
						dg_modifier.connect( color_texture_placement.findPlug("mirrorU"), color_texture_node.findPlug("mirrorU") );
						dg_modifier.connect( color_texture_placement.findPlug("mirrorV"), color_texture_node.findPlug("mirrorV") );
						dg_modifier.connect( color_texture_placement.findPlug("noiseUV"), color_texture_node.findPlug("noiseUV") );
						dg_modifier.connect( color_texture_placement.findPlug("offset"), color_texture_node.findPlug("offset") );
						dg_modifier.connect( color_texture_placement.findPlug("outUV"), color_texture_node.findPlug("uvCoord") );
						dg_modifier.connect( color_texture_placement.findPlug("outUvFilterSize"), color_texture_node.findPlug("uvFilterSize") );
						dg_modifier.connect( color_texture_placement.findPlug("repeatUV"), color_texture_node.findPlug("repeatUV") );
						dg_modifier.connect( color_texture_placement.findPlug("rotateFrame"), color_texture_node.findPlug("rotateFrame") );
						dg_modifier.connect( color_texture_placement.findPlug("rotateUV"), color_texture_node.findPlug("rotateUV") );
						dg_modifier.connect( color_texture_placement.findPlug("stagger"), color_texture_node.findPlug("stagger") );
						dg_modifier.connect( color_texture_placement.findPlug("translateFrame"), color_texture_node.findPlug("translateFrame") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexCameraOne"), color_texture_node.findPlug("vertexCameraOne") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexUvOne"), color_texture_node.findPlug("vertexUvOne") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexUvTwo"), color_texture_node.findPlug("vertexUvTwo") );
						dg_modifier.connect( color_texture_placement.findPlug("vertexUvThree"), color_texture_node.findPlug("vertexUvThree") );
						dg_modifier.connect( color_texture_placement.findPlug("wrapU"), color_texture_node.findPlug("wrapU") );
						dg_modifier.connect( color_texture_placement.findPlug("wrapV"), color_texture_node.findPlug("wrapV") );

					}

					if(normal_map.length() > 0) {
						MString normal_map_file = this->GetTextureFilePath(normal_map);
						MFnDependencyNode normal_map_node;
						normal_map_node.create(MString("file"), MString(normal_map.c_str()));
						normal_map_node.findPlug("ftn").setValue(normal_map_file);
						MItDependencyNodes node_it(MFn::kTextureList);
						MFnDependencyNode texture_list(node_it.item());
						MPlug textures = texture_list.findPlug(MString("textures"));

						MPlug current_texture;
						int next = 0;
						while(true) {
							current_texture = textures.elementByLogicalIndex(next);

							if(current_texture.isConnected() == false) {
								break;
							}
							next++;
						}

						MPlug texture_message = normal_map_node.findPlug("message");
						dg_modifier.connect(texture_message, current_texture);

						MFnDependencyNode bump2d_node;
						bump2d_node.create(MString("bump2d"),MString("bump2d"));
						MPlug bump_interp = bump2d_node.findPlug("bumpInterp");
						bump_interp.setInt(1);
						MString str = bump_interp.info();

						MPlug normal_map_outAlpha = normal_map_node.findPlug("outAlpha");
						MPlug bump2d_bumpvalue = bump2d_node.findPlug("bumpValue");
						dg_modifier.connect(normal_map_outAlpha, bump2d_bumpvalue);

						MPlug bump2d_outnormal = bump2d_node.findPlug("outNormal");
						MPlug new_shader_normalcamera = new_shader.findPlug("normalCamera");
						dg_modifier.connect(bump2d_outnormal, new_shader_normalcamera);

						MFnDependencyNode normal_map_placement;
						normal_map_placement.create("place2dTexture", "place2dTexture");

						dg_modifier.connect( normal_map_placement.findPlug("coverage"), normal_map_node.findPlug("coverage") );
						dg_modifier.connect( normal_map_placement.findPlug("mirrorU"), normal_map_node.findPlug("mirrorU") );
						dg_modifier.connect( normal_map_placement.findPlug("mirrorV"), normal_map_node.findPlug("mirrorV") );
						dg_modifier.connect( normal_map_placement.findPlug("noiseUV"), normal_map_node.findPlug("noiseUV") );
						dg_modifier.connect( normal_map_placement.findPlug("offset"), normal_map_node.findPlug("offset") );
						dg_modifier.connect( normal_map_placement.findPlug("outUV"), normal_map_node.findPlug("uvCoord") );
						dg_modifier.connect( normal_map_placement.findPlug("outUvFilterSize"), normal_map_node.findPlug("uvFilterSize") );
						dg_modifier.connect( normal_map_placement.findPlug("repeatUV"), normal_map_node.findPlug("repeatUV") );
						dg_modifier.connect( normal_map_placement.findPlug("rotateFrame"), normal_map_node.findPlug("rotateFrame") );
						dg_modifier.connect( normal_map_placement.findPlug("rotateUV"), normal_map_node.findPlug("rotateUV") );
						dg_modifier.connect( normal_map_placement.findPlug("stagger"), normal_map_node.findPlug("stagger") );
						dg_modifier.connect( normal_map_placement.findPlug("translateFrame"), normal_map_node.findPlug("translateFrame") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexCameraOne"), normal_map_node.findPlug("vertexCameraOne") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexUvOne"), normal_map_node.findPlug("vertexUvOne") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexUvTwo"), normal_map_node.findPlug("vertexUvTwo") );
						dg_modifier.connect( normal_map_placement.findPlug("vertexUvThree"), normal_map_node.findPlug("vertexUvThree") );
						dg_modifier.connect( normal_map_placement.findPlug("wrapU"), normal_map_node.findPlug("wrapU") );
						dg_modifier.connect( normal_map_placement.findPlug("wrapV"), normal_map_node.findPlug("wrapV") );
					}

					dg_modifier.doIt();
				}

				MString shader_Type = this->skyrimShaderTypeToString(shader_property->GetSkyrimShaderType());
				MString shader_flags1 = this->skyrimShaderFlags1ToString(shader_property->getShaderFlags1());
				MString shader_flags2 = this->skyrimShaderFlags2ToString(shader_property->getShaderFlags2());

				MString mel_command = "addAttr -dt \"string\" -shortName skyrimShaderFlags1 ";
				MGlobal::executeCommand(mel_command + new_shader.name());
				mel_command = "addAttr -dt \"string\" -shortName skyrimShaderFlags2 ";
				MGlobal::executeCommand(mel_command + new_shader.name());
				mel_command = "addAttr -dt \"string\" -shortName skyrimShaderType ";
				MGlobal::executeCommand(mel_command + new_shader.name());

				mel_command = "setAttr -type \"string\" ";
				MGlobal::executeCommand(mel_command + new_shader.name() + "\.skyrimShaderFlags1 \"" + shader_flags1 + "\"");
				MGlobal::executeCommand(mel_command + new_shader.name() + "\.skyrimShaderFlags2 \"" + shader_flags2 + "\"");
				MGlobal::executeCommand(mel_command + new_shader.name() + "\.skyrimShaderType \"" + shader_Type + "\"");

				unsigned int shader_type2 = shader_property->GetSkyrimShaderType();
				if(texture_set != NULL) {
					if(shader_type2 == 1) {
						mel_command = "addAttr -dt \"string\" -shortName cubeMapTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());
						mel_command = "addAttr -dt \"string\" -shortName evironmentMaskTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());

						mel_command = "setAttr -type \"string\" ";
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.cubeMapTexture \"" + this->GetTextureFilePath(texture_set->getTexture(4)) + "\"");
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.evironmentMaskTexture \"" + this->GetTextureFilePath(texture_set->getTexture(5)) + "\"");
					} else if(shader_type2 == 2) {
						mel_command = "addAttr -dt \"string\" -shortName glowMapTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());
						
						mel_command = "setAttr -type \"string\" ";
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.glowMapTexture \"" + this->GetTextureFilePath(texture_set->getTexture(2)) + "\"");
					} else if(shader_type2 == 5) {
						mel_command = "addAttr -dt \"string\" -shortName glowMapTexture ";
						MGlobal::executeCommand(mel_command + new_shader.name());

						mel_command = "setAttr -type \"string\" ";
						MGlobal::executeCommand(mel_command + new_shader.name() + "\.skinTexture \"" + this->GetTextureFilePath(texture_set->getTexture(2)) + "\"");

					}
				}
			}

			vector<NiPropertyRef> property_group;

			if(shader_property != NULL) {
				property_group.push_back(DynamicCast<NiProperty>(shader_property));
			}
			if(alpha_property != NULL) {
				property_group.push_back(DynamicCast<NiProperty>(alpha_property));
			}

			this->imported_materials.push_back(found_material);
			this->property_groups.push_back(property_group);
			this->translatorData->importedMaterials.push_back(pair<vector<NiPropertyRef>, MObject>(property_group, found_material));
		}
		return;
	}

	if(root->GetType().IsDerivedType(NiNode::TYPE)) {
		NiNodeRef node = DynamicCast<NiNode>(root);
		vector<NiAVObjectRef> children = node->GetChildren();

		for(int i = 0; i < children.size(); i++) {
			this->GatherMaterialsAndTextures(children[i]);
		}
	}
}

MString NifMaterialImporterSkyrim::skyrimShaderFlags1ToString( unsigned int shader_flags ) {
	MString ret;

	bool has_flags = true;
	while(has_flags == true) {
		has_flags = false;

		if ((1 & shader_flags) == 1) {
			ret += "|SLSF1_Specular";
			shader_flags = (shader_flags & ~1);
			has_flags = true;
		} else if ((2 & shader_flags) == 2) {
			ret += "|SLSF1_Skinned";
			shader_flags = (shader_flags & ~2);
			has_flags = true;
		} else if ((4 & shader_flags) == 4) {
			ret += "|SLSF1_Temp_Refraction";
			shader_flags = (shader_flags & ~4);
			has_flags = true;
		} else if ((8 & shader_flags) == 8) {
			ret += "|SLSF1_Vertex_Alpha";
			shader_flags = (shader_flags & ~8);
			has_flags = true;
		} else if ((16 & shader_flags) == 16) {
			ret += "|SLSF1_Greyscale_To_PaletteColor";
			shader_flags = (shader_flags & ~16);
			has_flags = true;
		} else if ((32 & shader_flags) == 32) {
			ret += "|SLSF1_Greyscale_To_PaletteAlpha";
			shader_flags = (shader_flags & ~32);
			has_flags = true;
		} else if ((64 & shader_flags) == 64) {
			ret += "|SLSF1_Use_Falloff";
			shader_flags = (shader_flags & ~64);
			has_flags = true;
		} else if ((128 & shader_flags) == 128) {
			ret += "|SLSF1_Environment_Mapping";
			shader_flags = (shader_flags & ~128);
			has_flags = true;
		} else if ((256 & shader_flags) == 256) {
			ret += "|SLSF1_Recieve_Shadows";
			shader_flags = (shader_flags & ~256);
			has_flags = true;
		} else if ((512 & shader_flags) == 512) {
			ret += "|SLSF1_Cast_Shadows";
			shader_flags = (shader_flags & ~512);
			has_flags = true;
		} else if ((1024 & shader_flags) == 1024) {
			ret += "|SLSF1_Facegen_Detail_Map";
			shader_flags = (shader_flags & ~1024);
			has_flags = true;
		} else if ((2048 & shader_flags) == 2048) {
			ret += "|SLSF1_Parallax";
			shader_flags = (shader_flags & ~2048);
			has_flags = true;
		} else if ((4096 & shader_flags) == 4096) {
			ret += "|SLSF1_Model_Space_Normals";
			shader_flags = (shader_flags & ~4096);
			has_flags = true;
		} else if ((8192 & shader_flags) == 8192) {
			ret += "|SLSF1_Non-Projective_Shadows";
			shader_flags = (shader_flags & ~8192);
			has_flags = true;
		} else if ((16384 & shader_flags) == 16384) {
			ret += "|SLSF1_Landscape";
			shader_flags = (shader_flags & ~16384);
			has_flags = true;
		} else if ((32768 & shader_flags) == 32768) {
			ret += "|SLSF1_Refraction";
			shader_flags = (shader_flags & ~32768);
			has_flags = true;
		} else if ((65536 & shader_flags) == 65536) {
			ret += "|SLSF1_Fire_Refraction";
			shader_flags = (shader_flags & ~65536);
			has_flags = true;
		} else if ((131072 & shader_flags) == 131072) {
			ret += "|SLSF1_Eye_Environment_Mapping";
			shader_flags = (shader_flags & ~131072);
			has_flags = true;
		} else if ((262144 & shader_flags) == 262144) {
			ret += "|SLSF1_Hair_Soft_Lighting";
			shader_flags = (shader_flags & ~262144);
			has_flags = true;
		} else if ((524288 & shader_flags) == 524288) {
			ret += "|SLSF1_Screendoor_Alpha_Fade";
			shader_flags = (shader_flags & ~524288);
			has_flags = true;
		} else if ((1048576 & shader_flags) == 1048576) {
			ret += "|SLSF1_Localmap_Hide_Secret";
			shader_flags = (shader_flags & ~1048576);
			has_flags = true;
		} else if ((2097152 & shader_flags) == 2097152) {
			ret += "|SLSF1_FaceGen_RGB_Tint";
			shader_flags = (shader_flags & ~2097152);
			has_flags = true;
		} else if ((4194304 & shader_flags) == 4194304) {
			ret += "|SLSF1_Own_Emit";
			shader_flags = (shader_flags & ~4194304);
			has_flags = true;
		} else if ((8388608 & shader_flags) == 8388608) {
			ret += "|SLSF1_Projected_UV";
			shader_flags = (shader_flags & ~8388608);
			has_flags = true;
		} else if ((16777216 & shader_flags) == 16777216) {
			ret += "|SLSF1_Multiple_Textures";
			shader_flags = (shader_flags & ~16777216);
			has_flags = true;
		} else if ((33554432 & shader_flags) == 33554432) {
			ret += "|SLSF1_Remappable_Textures";
			shader_flags = (shader_flags & ~33554432);
			has_flags = true;
		} else if ((67108864 & shader_flags) == 67108864) {
			ret += "|SLSF1_Decal";
			shader_flags = (shader_flags & ~67108864);
			has_flags = true;
		} else if ((134217728 & shader_flags) == 134217728) {
			ret += "|SLSF1_Dynamic_Decal";
			shader_flags = (shader_flags & ~134217728);
			has_flags = true;
		} else if ((268435456 & shader_flags) == 268435456) {
			ret += "|SLSF1_Parallax_Occlusion";
			shader_flags = (shader_flags & ~268435456);
			has_flags = true;
		} else if ((536870912 & shader_flags) == 536870912) {
			ret += "|SLSF1_External_Emittance";
			shader_flags = (shader_flags & ~536870912);
			has_flags = true;
		} else if ((1073741824 & shader_flags) == 1073741824) {
			ret += "|SLSF1_Soft_Effect";
			shader_flags = (shader_flags & ~1073741824);
		} else if ((2147483648 & shader_flags) == 2147483648) {
			ret += "|SLSF1_ZBuffer_Test";
			shader_flags = (shader_flags & ~2147483648);
			has_flags = true;
		}
	}

	if(ret.length() > 1) {
			ret = ret.substring(1, ret.length());
	}

	return ret;
}

MString NifMaterialImporterSkyrim::skyrimShaderFlags2ToString( unsigned int shader_flags ) {
	MString ret;

	bool has_flags = true;
	while(has_flags == true) {
		has_flags = false;

		if ((1 & shader_flags) == 1) {
			ret += "|SLSF2_ZBuffer_Write";
			shader_flags = (shader_flags & ~1);
			has_flags = true;
		} else if ((2 & shader_flags) == 2) {
			ret += "|SLSF2_LOD_Landscape";
			shader_flags = (shader_flags & ~2);
			has_flags = true;
		} else if ((4 & shader_flags) == 4) {
			ret += "|SLSF2_LOD_Objects";
			shader_flags = (shader_flags & ~4);
			has_flags = true;
		} else if ((8 & shader_flags) == 8) {
			ret += "|SLSF2_No_Fade";
			shader_flags = (shader_flags & ~8);
			has_flags = true;
		} else if ((16 & shader_flags) == 16) {
			ret += "|SLSF2_Double_Sided";
			shader_flags = (shader_flags & ~16);
			has_flags = true;
		} else if ((32 & shader_flags) == 32) {
			ret += "|SLSF2_Vertex_Colors";
			shader_flags = (shader_flags & ~32);
			has_flags = true;
		} else if ((64 & shader_flags) == 64) {
			ret += "|SLSF2_Glow_Map";
			shader_flags = (shader_flags & ~64);
			has_flags = true;
		} else if ((128 & shader_flags) == 128) {
			ret += "|SLSF2_Assume_Shadowmask";
			shader_flags = (shader_flags & ~128);
			has_flags = true;
		} else if ((256 & shader_flags) == 256) {
			ret += "|SLSF2_Packed_Tangent";
			shader_flags = (shader_flags & ~256);
			has_flags = true;
		} else if ((512 & shader_flags) == 512) {
			ret += "|SLSF2_Multi_Index_Snow";
			shader_flags = (shader_flags & ~512);
			has_flags = true;
		} else if ((1024 & shader_flags) == 1024) {
			ret += "|SLSF2_Vertex_Lighting";
			shader_flags = (shader_flags & ~1024);
			has_flags = true;
		} else if ((2048 & shader_flags) == 2048) {
			ret += "|SLSF2_Uniform_Scale";
			shader_flags = (shader_flags & ~2048);
			has_flags = true;
		} else if ((4096 & shader_flags) == 4096) {
			ret += "|SLSF2_Fit_Slope";
			shader_flags = (shader_flags & ~4096);
			has_flags = true;
		} else if ((8192 & shader_flags) == 8192) {
			ret += "|SLSF2_Billboard";
			shader_flags = (shader_flags & ~8192);
			has_flags = true;
		} else if ((16384 & shader_flags) == 16384) {
			ret += "|SLSF2_No_LOD_Land_Blend";
			shader_flags = (shader_flags & ~16384);
			has_flags = true;
		} else if ((32768 & shader_flags) == 32768) {
			ret += "|SLSF2_EnvMap_Light_Fade";
			shader_flags = (shader_flags & ~32768);
			has_flags = true;
		} else if ((65536 & shader_flags) == 65536) {
			ret += "|SLSF2_Wireframe";
			shader_flags = (shader_flags & ~65536);
			has_flags = true;
		} else if ((131072 & shader_flags) == 131072) {
			ret += "|SLSF2_Weapon_Blood";
			shader_flags = (shader_flags & ~131072);
			has_flags = true;
		} else if ((262144 & shader_flags) == 262144) {
			ret += "|SLSF2_Hide_On_Local_Map";
			shader_flags = (shader_flags & ~262144);
			has_flags = true;
		} else if ((524288 & shader_flags) == 524288) {
			ret += "|SLSF2_Premult_Alpha";
			shader_flags = (shader_flags & ~524288);
			has_flags = true;
		} else if ((1048576 & shader_flags) == 1048576) {
			ret += "|SLSF2_Cloud_LOD";
			shader_flags = (shader_flags & ~1048576);
			has_flags = true;
		} else if ((2097152 & shader_flags) == 2097152) {
			ret += "|SLSF2_Anisotropic_Lighting";
			shader_flags = (shader_flags & ~2097152);
			has_flags = true;
		} else if ((4194304 & shader_flags) == 4194304) {
			ret += "|SLSF2_No_Transparency_Multisampling";
			shader_flags = (shader_flags & ~4194304);
			has_flags = true;
		} else if ((8388608 & shader_flags) == 8388608) {
			ret += "|SLSF2_Unused01";
			shader_flags = (shader_flags & ~8388608);
			has_flags = true;
		} else if ((16777216 & shader_flags) == 16777216) {
			ret += "|SLSF2_Multi_Layer_Parallax";
			shader_flags = (shader_flags & ~16777216);
			has_flags = true;
		} else if ((33554432 & shader_flags) == 33554432) {
			ret += "|SLSF2_Soft_Lighting";
			shader_flags = (shader_flags & ~33554432);
			has_flags = true;
		} else if ((67108864 & shader_flags) == 67108864) {
			ret += "|SLSF2_Rim_Lighting";
			shader_flags = (shader_flags & ~67108864);
			has_flags = true;
		} else if ((134217728 & shader_flags) == 134217728) {
			ret += "|SLSF2_Back_Lighting";
			shader_flags = (shader_flags & ~134217728);
			has_flags = true;
		} else if ((268435456 & shader_flags) == 268435456) {
			ret += "|SLSF2_Unused02";
			shader_flags = (shader_flags & ~268435456);
			has_flags = true;
		} else if ((536870912 & shader_flags) == 536870912) {
			ret += "|SLSF2_Tree_Anim";
			shader_flags = (shader_flags & ~536870912);
			has_flags = true;
		} else if ((1073741824 & shader_flags) == 1073741824) {
			ret += "|SLSF2_Effect_Lighting";
			shader_flags = (shader_flags & ~1073741824);
		} else if ((2147483648 & shader_flags) == 2147483648) {
			ret += "|SLSF2_HD_LOD_Objects";
			shader_flags = (shader_flags & ~2147483648);
			has_flags = true;
		}
	}

	if(ret.length() > 1) {
		ret = ret.substring(1, ret.length());
	}

	return ret;
}

MString NifMaterialImporterSkyrim::skyrimShaderTypeToString( unsigned int shader_type ) {
	MString ret;

	if(shader_type == 0) {
		ret = "Default";
	} else if(shader_type == 1) {
		ret = "EnvMap";
	} else if(shader_type == 5) {
		ret = "Skin";
	} else if(shader_type == 2) {
		ret = "Glow";
	} else if(shader_type == 6) {
		ret = "Hair";
	} else if(shader_type == 11) {
		ret = "Ice/Parallax";
	} else if(shader_type == 15) {
		ret = "Eye";
	}

	return ret;
}

string NifMaterialImporterSkyrim::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifMaterialImporter::asString(verbose)<<endl;
	out<<"NifMaterialExporterSkyrim"<<endl;

	return out.str();
}

const Type& NifMaterialImporterSkyrim::GetType() const {
	return TYPE;
}

const Type NifMaterialImporterSkyrim::TYPE("NifMaterialImporterSkyrim",&NifMaterialImporter::TYPE);


