#include "include/Common/NifTranslatorDataWrapper.h"

NifTranslatorDataWrapper::~NifTranslatorDataWrapper() {

}

const Type NifTranslatorDataWrapper::TYPE("NifTranslatorDataWrapper", &NifTranslatorRefObject::TYPE);

std::string NifTranslatorDataWrapper::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorRefObject::asString(verbose);
	out<<"NifTranslator custom data"<<endl;

	return out.str();
}

const Type& NifTranslatorDataWrapper::GetType() const {
	return TYPE;
}
