#include "include/Common/NifTranslatorUtils.h"

NifTranslatorUtils::NifTranslatorUtils() {

}

NifTranslatorUtils::NifTranslatorUtils( NifTranslatorDataRef translatorData,NifTranslatorOptionsRef translatorOptions ) {
	this->translatorData = translatorData;
	this->translatorOptions = translatorOptions;
}

NifTranslatorUtils::~NifTranslatorUtils() {

}

MObject NifTranslatorUtils::GetExistingJoint( const string & name )
{
	//If it's not in the list, return a null object
	map<string,MDagPath>::iterator it = this->translatorData->existingNodes.find( name );
	if ( it == this->translatorData->existingNodes.end() ) {
		//out << "A joint named " << name << " did not exist in list." << endl;
		return MObject::kNullObj;
	}

	//It's in the list
	MFnDagNode nodeFn( it->second );
	MObject jointObj = nodeFn.object();

	//Check if it is an Ik joint
	if ( jointObj.hasFn( MFn::kJoint ) ) {
		//out << "Matching joint is already an IK joint." << endl;
		//It's a joint already, return it
		return jointObj;
	} else {
		return MObject::kNullObj;
	}
}

MString NifTranslatorUtils::MakeMayaName( const string & nifName ) {
	stringstream newName;
	newName.fill('0');

	for ( unsigned int i = 0; i < nifName.size(); ++i ) {
		if ( this->translatorOptions->useNameMangling ) {
			if ( nifName[i] == ' ' ) {
				newName << "_";
			} else if ( 
				(nifName[i] >= '0' && nifName[i] <= '9') ||
				(nifName[i] >= 'A' && nifName[i] <= 'Z') ||
				(nifName[i] >= 'a' && nifName[i] <= 'z')
				) {
					newName << nifName[i];
			} else {
				newName << "_0x" << setw(2) << hex << int(nifName[i]);
			}
		} else {
			if ( 
				(nifName[i] >= '0' && nifName[i] <= '9') ||
				(nifName[i] >= 'A' && nifName[i] <= 'Z') ||
				(nifName[i] >= 'a' && nifName[i] <= 'z')
				) {
					newName << nifName[i];
			} else {
				newName << "_";
			}
		}
	}
	//out << "New Maya name:  " << newName.str() << endl;
	return MString( newName.str().c_str() );
}

MString NifTranslatorUtils::MakeMayaName( const string& nif_name, int duplicate_count )
{
	stringstream newName;
	newName.fill('0');

	for ( unsigned int i = 0; i < nif_name.size(); ++i ) {
		if ( this->translatorOptions->useNameMangling ) {
			if ( nif_name[i] == ' ' ) {
				newName << "_";
			} else if ( 
				(nif_name[i] >= '0' && nif_name[i] <= '9') ||
				(nif_name[i] >= 'A' && nif_name[i] <= 'Z') ||
				(nif_name[i] >= 'a' && nif_name[i] <= 'z')
				) {
					newName << nif_name[i];
			} else {
				newName << "_0x" << setw(2) << hex << int(nif_name[i]);
			}
		} else {
			if ( 
				(nif_name[i] >= '0' && nif_name[i] <= '9') ||
				(nif_name[i] >= 'A' && nif_name[i] <= 'Z') ||
				(nif_name[i] >= 'a' && nif_name[i] <= 'z')
				) {
					newName << nif_name[i];
			} else {
				newName << "_";
			}
		}
	}

	if (duplicate_count >= 0) {
		newName << "_00DUP" << duplicate_count;
	}

	//out << "New Maya name:  " << newName.str() << endl;
	return MString( newName.str().c_str() );
}

string NifTranslatorUtils::MakeNifName( const MString & mayaName )
{
	stringstream newName;
	stringstream temp;
	temp.setf ( ios_base::hex, ios_base::basefield );  // set hex as the basefield

	string str = mayaName.asChar();

	if ( this->translatorOptions->useNameMangling ) {
		for ( unsigned int i = 0; i < str.size(); ++i ) {
			if ( i + 5 < str.size() ) {
				string sub = str.substr( i, 3);
				//out << "Sub string:  " << sub << endl;
				if ( sub == "_0x" ) {
					sub = str.substr( i+3, 2);
					//out << "Sub sub string:  " << sub << endl;
					temp.clear();
					temp << sub; // should be the char number
					int ch_num;
					temp >> ch_num;
					//out << "The int returned from the string stream extraction is:  " << ch_num << endl;
					if ( temp.fail() == false ) {
						newName << char(ch_num);
						i += 4;
					}
					continue;
				}
			}

			if(i + 6 < str.size()) {
				string sub = str.substr( i, 6);
				if(sub == "_00DUP") {
					break;
				}
			}

			if ( str[i] == '_' ) {
				newName << " ";
			} else {
				newName << str[i];
			}
		}

		return newName.str();
	} else {
		return str;
	}
}

void NifTranslatorUtils::ConnectShader( MObject material_object, vector<NifTextureConnectorRef> texture_connectors, MDagPath mesh_path, MSelectionList selection_list )
{
	if ( material_object == MObject::kNullObj ) {
		//No material to connect
		return;
	}

	//Connect the shader
	//out << "Connecting shader..." << endl;

	//--Create a Shading Group--//

	//Create the shading group from the list
	MFnSet setFn;
	setFn.create( selection_list, MFnSet::kRenderableOnly, false );
	setFn.setName("shadingGroup");

	//--Connect the mesh to the shading group--//

	//Set material to a phong function set
	MFnPhongShader phongFn;
	phongFn.setObject( material_object );

	//Break the default connection that is created
	MPlugArray plug_array;
	MPlug surfaceShader = setFn.findPlug("surfaceShader");
	surfaceShader.connectedTo( plug_array, true, true );
	MDGModifier dgModifier;
	dgModifier.disconnect( plug_array[0], surfaceShader );

	//Connect outColor to surfaceShader
	dgModifier.connect( phongFn.findPlug("outColor"), surfaceShader );

	//out << "Invoking dgModifier..." << endl;
	dgModifier.doIt();

	for(int i = 0; i < texture_connectors.size(); i++) {
		texture_connectors[i]->ConnectTexture(mesh_path);
	}
}

void NifTranslatorUtils::AdjustSkeleton( NiAVObjectRef & root )
{
	NiNodeRef niNode = DynamicCast<NiNode>(root);

	if ( niNode != NULL ) {
		string name = MakeMayaName( root->GetName() ).asChar();
		if ( this->translatorData->existingNodes.find( name ) != this->translatorData->existingNodes.end() ) {
			//out << "Copying current scene transform of " << name << " over the top of " << root << endl;
			//Found a match
			MDagPath existing = this->translatorData->existingNodes[name];

			//Get World Matrix of Dag Path
			MMatrix my_trans = existing.inclusiveMatrix();
			MTransformationMatrix myTrans(my_trans);

			//Warn user about any scaling problems
			double myScale[3];
			myTrans.getScale( myScale, MSpace::kPreTransform );
			if ( abs(myScale[0] - 1.0) > 0.0001 || abs(myScale[1] - 1.0) > 0.0001 || abs(myScale[2] - 1.0) > 0.0001 ) {
				MGlobal::displayWarning("Some games such as the Elder Scrolls do not react well when scale is not 1.0.  Consider freezing scale transforms on all nodes before export.");
			}
			if ( abs(myScale[0] - myScale[1]) > 0.0001 || abs(myScale[0] - myScale[2]) > 0.001 || abs(myScale[1] - myScale[2]) > 0.001 ) {
				MGlobal::displayWarning("The NIF format does not support separate scales for X, Y, and Z.  An average will be used.  Consider freezing scale transforms on all nodes before export.");
			}

			//Copy Maya matrix to Niflib matrix
			Matrix44 ni_trans( 
				(float)my_trans[0][0], (float)my_trans[0][1], (float)my_trans[0][2], (float)my_trans[0][3],
				(float)my_trans[1][0], (float)my_trans[1][1], (float)my_trans[1][2], (float)my_trans[1][3],
				(float)my_trans[2][0], (float)my_trans[2][1], (float)my_trans[2][2], (float)my_trans[2][3],
				(float)my_trans[3][0], (float)my_trans[3][1], (float)my_trans[3][2], (float)my_trans[3][3]
			);

			//Check if root has a parent
			Matrix44 par_world;
			NiNodeRef par = root->GetParent();
			if ( par != NULL ) {
				par_world = par->GetWorldTransform();
			}

			//Move bone to world position of maya transform * inverse of current
			//parent world transform
			root->SetLocalTransform( ni_trans * par_world.Inverse() );

			//ExportAV( root, existingNodes[name] );
		}

		//Call this function on all children of this node
		vector<NiAVObjectRef> children = niNode->GetChildren();
		for ( unsigned i = 0; i < children.size(); ++i ) {
			AdjustSkeleton( children[i] );
		}
	}
}

MMatrix NifTranslatorUtils::MatrixN2M( const Matrix44 & n )
{
	//Copy Niflib matrix to Maya matrix

	float myMat[4][4] = { 
		n[0][0], n[0][1], n[0][2], n[0][3],
		n[1][0], n[1][1], n[1][2], n[1][3],
		n[2][0], n[2][1], n[2][2], n[2][3],
		n[3][0], n[3][1], n[3][2], n[3][3]
	};

	return MMatrix(myMat);
}

Niflib::Matrix44 NifTranslatorUtils::MatrixM2N( const MMatrix & n )
{
	//Copy Maya matrix to Niflib matrix
	return Matrix44( 
		(float)n[0][0], (float)n[0][1], (float)n[0][2], (float)n[0][3],
		(float)n[1][0], (float)n[1][1], (float)n[1][2], (float)n[1][3],
		(float)n[2][0], (float)n[2][1], (float)n[2][2], (float)n[2][3],
		(float)n[3][0], (float)n[3][1], (float)n[3][2], (float)n[3][3]
	);
}

const Type NifTranslatorUtils::TYPE("NifTranslatorUtils",&NifTranslatorRefObject::TYPE);

std::string NifTranslatorUtils::asString( bool verbose /*= false */ ) const {
	stringstream out;

	
	out<<NifTranslatorRefObject::asString(verbose);
	out<<"NifTranslatorOptions:"<<endl;
	out<<this->translatorOptions->asString(verbose);
	out<<"NifTranslatorData:   "<<endl;
	out<<this->translatorData->asString(verbose);

	return out.str();
}

const Type& NifTranslatorUtils::GetType() const {
	return TYPE;
}

bool NifTranslatorUtils::isExportedShape( const MString& name )
{
	if(!this->isValidObject(name)) {
		return false;
	}

	for(int i = 0;i < this->translatorOptions->exportedShapes.size(); i++) {
		if(name.asChar() == this->translatorOptions->exportedShapes[i])
			return true;
	}

	return false;
}

bool NifTranslatorUtils::isExportedJoint( const MString& name )
{
	if(!this->isValidObject(name)) {
		return false;
	}
	
	for(int i = 0;i < this->translatorOptions->exportedJoints.size(); i++) {
		if(name.asChar() == this->translatorOptions->exportedJoints[i])
			return true;
	}

	return false;
}


bool NifTranslatorUtils::isExportedMisc( const MString& name )
{
	if(!this->isValidObject(name)) {
		return false;
	}
	
	for(int i = 0;i < this->translatorOptions->exportedMisc.size(); i++) {
		if(name.asChar() == this->translatorOptions->exportedMisc[i])
			return true;
	}

	return false;
}


bool NifTranslatorUtils::isValidObject( const MString& name )
{
	if(name == "front" || name == "side" || name == "top" || name == "persp" ) return false;
	if(name == "Manipulator1" || name == "ViewCompass" ||  name == "CubeCompass" || name == "groundPlane_transform") return false;

	return true;
}

NiNodeRef NifTranslatorUtils::GetDAGParent( MObject dagNode )
{
	// attach a function set for a dag node to the object.
	MFnDagNode nodeFn(dagNode);

	//out << "Looping through all Maya parents." << endl;
	for( unsigned int i = 0; i < nodeFn.parentCount(); ++i ) {
		//out << "Get the MObject for the i'th parent and attach a fnction set to it." << endl;
		// get the MObject for the i'th parent
		MObject parent = nodeFn.parent(i);

		// attach a function set to it
		MFnDagNode parentFn(parent);

		//out << "Get Parent Path." << endl;
		string par_path = parentFn.fullPathName().asChar();

		//out << "Check if parent exists in the map we've built." << endl;

		//Check if parent exists in map we've built
		if ( this->translatorData->nodes.find( par_path ) != this->translatorData->nodes.end() ) {
			//out << "Parent found." << endl;
			//Object found
			return this->translatorData->nodes[par_path];
		} else {
			//Block was created, parent to scene root
			return this->translatorData->exportedSceneRoot;
		}
	}

	//Block was created, parent to scene root
	return this->translatorData->exportedSceneRoot;
}



