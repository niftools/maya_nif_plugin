#include "Common/NifTranslatorFixtureItem.h"

NifTranslatorFixtureItem::NifTranslatorFixtureItem() {

}

NifTranslatorFixtureItem::NifTranslatorFixtureItem( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorOptions = translatorOptions;
	this->translatorData = translatorData;
	this->translatorUtils = translatorUtils;
}

NifTranslatorFixtureItem::~NifTranslatorFixtureItem() {

}

std::string NifTranslatorFixtureItem::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorRefObject::asString(verbose);
	out<<"NifTranslatorOptions:"<<endl;
	out<<this->translatorOptions->asString(verbose)<<endl;
	out<<"NifTranslatorData:"<<endl;
	out<<this->translatorData->asString(verbose)<<endl;
	out<<"NifTranslatorUtils:"<<endl;
	out<<this->translatorUtils->asString(verbose)<<endl;

	return out.str();
}

const Type& NifTranslatorFixtureItem::GetType() const {
	return TYPE;
}

int NifTranslatorFixtureItem::countOperations( NiAVObjectRef target ) {
	return 0;
}

const Type NifTranslatorFixtureItem::TYPE("NifTranslatorFixtureItem" ,&NifTranslatorRefObject::TYPE);
