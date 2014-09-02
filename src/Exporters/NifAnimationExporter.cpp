#include "Exporters/NifAnimationExporter.h"

NifAnimationExporter::NifAnimationExporter(){

}

NifAnimationExporter::NifAnimationExporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions,translatorData,translatorUtils) {

}

string NifAnimationExporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifAnimationExporter"<<endl;

	return out.str();
}

const Type& NifAnimationExporter::GetType() const {
	return TYPE;
}

const Type NifAnimationExporter::TYPE("NifAnimationExporter",&NifTranslatorFixtureItem::TYPE);


