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
				float reflective_strength = shader_property->getSpecularStrength() / 1000;
				float cosine_power = shader_property->getGlossiness();
				float transparency = 1.0f - shader_property->getAlpha();

				new_shader.setReflectedColor(reflective_color);
				new_shader.setReflectivity(0.750f);
				new_shader.setIncandescence(incadescence_color);
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

	if(root->GetType().IsSameType(NiNode::TYPE)) {
		NiNodeRef node = DynamicCast<NiNode>(root);
		vector<NiAVObjectRef> children = node->GetChildren();

		for(int i = 0; i < children.size(); i++) {
			this->GatherMaterialsAndTextures(children[i]);
		}
	}
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


