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
		NiSpecularPropertyRef specular_propery = DynamicCast<NiSpecularProperty>(geometry->GetPropertyByType(NiSpecularProperty::TYPE));
		BSLightingShaderPropertyRef shader_property = DynamicCast<BSLightingShaderProperty>(geometry->GetPropertyByType(BSLightingShaderProperty::TYPE));

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


