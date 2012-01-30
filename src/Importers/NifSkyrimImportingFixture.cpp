#include "include/Importers/NifSkyrimImportingFixture.h"


NifSkyrimImportingFixture::NifSkyrimImportingFixture() {

}

NifSkyrimImportingFixture::NifSkyrimImportingFixture( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
	this->nodeImporter = new NifNodeImporter(translatorOptions, translatorData, translatorUtils);
	this->meshImporter = new NifMeshImporter(translatorOptions, translatorData, translatorUtils);
	this->materialImporter = new NifMaterialImporterSkyrim(translatorOptions, translatorData, translatorUtils);
	this->animationImporter = new NifAnimationImporter(translatorOptions, translatorData, translatorUtils);
}

string NifSkyrimImportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifDefaultImportingFixture::asString(verbose)<<endl;
	out<<"NifSkyrimImportingFixture";

	return out.str();
}

const Type& NifSkyrimImportingFixture::GetType() const {
	return TYPE;
}

const Type NifSkyrimImportingFixture::TYPE("NifSkyrimImportingFixture", &NifDefaultImportingFixture::TYPE);
