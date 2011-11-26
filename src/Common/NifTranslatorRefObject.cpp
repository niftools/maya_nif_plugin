#include "include/Common/NifTranslatorRefObject.h"

string NifTranslatorRefObject::asString( bool verbose /*= false */ ) const {
	stringstream out;
	out<<"NifTranslator object"<<endl;
	return out.str();
}

const Type& NifTranslatorRefObject::GetType() const {
	return TYPE;
}

bool NifTranslatorRefObject::IsSameType( const Type & compare_to ) const {
	return GetType().IsSameType( compare_to );
}

bool NifTranslatorRefObject::IsSameType( const RefObject * object ) const {
	return GetType().IsSameType( object->GetType() );
}

bool NifTranslatorRefObject::IsDerivedType( const Type & compare_to ) const {
	return GetType().IsDerivedType( compare_to );
}

bool NifTranslatorRefObject::IsDerivedType( const RefObject * object ) const {
	return GetType().IsDerivedType( object->GetType() );
}

unsigned int NifTranslatorRefObject::NumObjectsInMemory() {
	return objectsInMemory;
}

void NifTranslatorRefObject::AddRef() const {
	++_ref_count;
}

void NifTranslatorRefObject::SubtractRef() const {
	_ref_count--;
	if ( _ref_count < 1 ) {
		delete this;
	}
}

NifTranslatorRefObject::NifTranslatorRefObject() {
		this->_ref_count = 0;
		objectsInMemory++;
}

NifTranslatorRefObject::~NifTranslatorRefObject() {
	objectsInMemory--;
}

const Type NifTranslatorRefObject::TYPE("NifTranslatorRefObject",NULL);

unsigned int NifTranslatorRefObject::objectsInMemory = 0;
