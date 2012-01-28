#include "include/Importers/NifMaterialImporterSkyrim.h"


NifMaterialImporterSkyrim::NifMaterialImporterSkyrim() {

}


NifMaterialImporterSkyrim::NifMaterialImporterSkyrim( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
}


void NifMaterialImporterSkyrim::ImportMaterialsAndTextures( NiAVObjectRef & root ) {
	
}


void NifMaterialImporterSkyrim::GatherMaterialsAndTextures( NiAVObjectRef & root )
{
	if(root->GetType().IsSameType(NiGeometry::TYPE)) {
		NiGeometryRef geometry = DynamicCast<NiGeometry>(root);
		NiAlphaPropertyRef alpha_property = DynamicCast<NiAlphaProperty>(geometry->GetPropertyByType(NiAlphaProperty::TYPE));
		NiSpecularPropertyRef specular_property = DynamicCast<NiSpecularProperty>(geometry->GetPropertyByType(NiSpecularProperty::TYPE));
		BSLightingShaderPropertyRef shader_property = DynamicCast<BSLightingShaderProperty>(geometry->GetPropertyByType(BSLightingShaderProperty::TYPE));

		int valid_properties = 0;

		if(alpha_property != NULL) {
			valid_properties++;
		}
		if(specular_property != NULL) {
			valid_properties++;
		}
		if(shader_property != NULL) {
			valid_properties++;
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
				if(specular_property != NULL && property_group[i]->GetType().IsSameType(NiSpecularProperty::TYPE)) {
					NiSpecularPropertyRef current_specular_property = DynamicCast<NiSpecularProperty>(property_group[i]);
					if(current_specular_property == specular_property) {
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

			if(similarities == valid_properties) {
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
				new_shader.setReflectivity(reflective_strength);
				new_shader.setIncandescence(incadescence_color);
				new_shader.setCosPower(cosine_power);
				new_shader.setTransparency(transparency);
			}
			if(alpha_property != NULL) {

			}
			if(specular_property != NULL) {

			}

			vector<NiPropertyRef> property_group;

			property_group.push_back(DynamicCast<NiProperty>(shader_property));
			property_group.push_back(DynamicCast<NiProperty>(alpha_property));
			property_group.push_back(DynamicCast<NiProperty>(specular_property));

			this->imported_materials.push_back(found_material);
			this->property_groups.push_back(property_group);
		}

		this->translatorData->importedMaterialsSkyrim.push_back(pair<NiGeometryRef, MObject>(geometry, found_material));

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


