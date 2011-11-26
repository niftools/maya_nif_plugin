#include "include/Importers/NifImportingFixture.h"

string NifImportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose);
	out<<"NifTranslatorImporterFixture"<<endl;

	return out.str();
}

const Type& NifImportingFixture::GetType() const {
	return TYPE;
}

const Type NifImportingFixture::TYPE("NifTranslatorImporterFixture",&NifTranslatorRefObject::TYPE);
