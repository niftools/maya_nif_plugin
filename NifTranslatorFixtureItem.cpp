#include "NifTranslatorFixtureItem.h"

NifTranslatorFixtureItem::NifTranslatorFixtureItem() 
	: translatorOptions(NifTranslatorOptions()), translatorData(NifTranslatorData()), translatorUtils(NifTranslatorUtils()){

}

NifTranslatorFixtureItem::NifTranslatorFixtureItem( NifTranslatorOptions& translatorOptions, NifTranslatorData& translatorData, NifTranslatorUtils& translatorUtils ) 
	: translatorOptions(translatorOptions), translatorData(translatorData), translatorUtils(translatorUtils) {

}

NifTranslatorFixtureItem::~NifTranslatorFixtureItem() {

}
