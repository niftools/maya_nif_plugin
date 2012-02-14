#include "include/Exporters/NifMaterialExporterSkyrim.h"

NifMaterialExporterSkyrim::NifMaterialExporterSkyrim() {

}

NifMaterialExporterSkyrim::NifMaterialExporterSkyrim( NifTranslatorOptionsRef translator_options, NifTranslatorDataRef translator_data, NifTranslatorUtilsRef translator_utils ) {
	this->translatorOptions = translator_options;
	this->translatorData = translator_data;
	this->translatorUtils = translator_utils;
}

void NifMaterialExporterSkyrim::ExportShaders() {

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
