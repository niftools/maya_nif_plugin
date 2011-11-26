#include "include/Exporters/NifExportingFixture.h"

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
