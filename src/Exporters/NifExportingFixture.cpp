#include "Exporters/NifExportingFixture.h"


NifExportingFixture::NifExportingFixture() {

}

NifExportingFixture::NifExportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
}


string NifExportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose);
	out<<"NifTranslatorExportingFixture"<<endl;

	return out.str();
}

const Type& NifExportingFixture::getType() const {
	return TYPE;
}

const Type NifExportingFixture::TYPE("NifTranslatorExportingFixture",&NifTranslatorRefObject::TYPE);
