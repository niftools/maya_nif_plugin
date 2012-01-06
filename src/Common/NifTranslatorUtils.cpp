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
		if ( this->translatorOptions->use_name_mangling ) {
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

void NifTranslatorUtils::ConnectShader( const vector<NiPropertyRef> & properties, MDagPath meshPath, MSelectionList sel_list )
{
	//--Look for Materials--//
	MObject grpOb;

	//out << "Looking for previously imported shaders..." << endl;

	unsigned int mat_index = this->translatorData->mtCollection.GetMaterialIndex( properties );

	if ( mat_index == NO_MATERIAL ) {
		//No material to connect
		return;
	}

	//Look up Maya Shader
	if ( this->translatorData->importedMaterials.find( mat_index ) == this->translatorData->importedMaterials.end() ) {
		throw runtime_error("The material was previously imported, but does not appear in the list.  This should not happen.");
	}

	MObject matOb = this->translatorData->importedMaterials[mat_index];

	if ( matOb == MObject::kNullObj ) {
		//No material to connect
		return;
	}

	//Connect the shader
	//out << "Connecting shader..." << endl;

	//--Create a Shading Group--//

	//Create the shading group from the list
	MFnSet setFn;
	setFn.create( sel_list, MFnSet::kRenderableOnly, false );
	setFn.setName("shadingGroup");

	//--Connect the mesh to the shading group--//

	//Set material to a phong function set
	MFnPhongShader phongFn;
	phongFn.setObject( matOb );

	//Break the default connection that is created
	MPlugArray arr;
	MPlug surfaceShader = setFn.findPlug("surfaceShader");
	surfaceShader.connectedTo( arr, true, true );
	MDGModifier dgModifier;
	dgModifier.disconnect( arr[0], surfaceShader );

	//Connect outColor to surfaceShader
	dgModifier.connect( phongFn.findPlug("outColor"), surfaceShader );

	//out << "Looking for previously imported textures..." << endl;

	MaterialWrapper mw = this->translatorData->mtCollection.GetMaterial( mat_index );

	//Cycle through for each type of texture
	for (int i = 0; i < 8; ++i) {
		//Texture type is supported, get Texture
		unsigned int tex_index = mw.GetTextureIndex( TexType(i) );

		//If there is no matching texture for this slot, continue to the next one.
		if ( tex_index == NO_TEXTURE ) {
			continue;
		}

		//Skip this texture slot if it's an un-supported type.
		switch(i) {
		case DARK_MAP:
			//Temporary until/if Dark Textures are supported
			MGlobal::displayWarning( "Dark Textures are not yet supported." );
			continue;
		case DETAIL_MAP:
			//Temporary until/if Detail Textures are supported
			MGlobal::displayWarning( "Detail Textures are not yet supported." );
			continue;
		case GLOSS_MAP:
			//Temporary until/if Detail Textures are supported
			MGlobal::displayWarning( "Gloss Textures are not yet supported." );
			continue;
		case BUMP_MAP:
			//Temporary until/if Bump Map Textures are supported
			MGlobal::displayWarning( "Bump Map Textures are not yet supported." );
			continue;
		case DECAL_0_MAP:
		case DECAL_1_MAP:
			//Temporary until/if Decal Textures are supported
			MGlobal::displayWarning( "Decal Textures are not yet supported." );
			continue;
		};


		//Look up Maya fileTexture
		if ( this->translatorData->importedTextures.find( tex_index ) == this->translatorData->importedTextures.end() ) {
			//There was no match in the previously imported textures.
			//This may be caused by the NIF textures being stored internally, so just continue to the next slot.
			continue;
		}

		MObject txOb = this->translatorData->importedTextures[tex_index];

		//out << "Connecting a texture..." << endl;
		//Connect the texture
		MFnDependencyNode nodeFn;
		NiAlphaPropertyRef niAlphaProp = mw.GetTranslucencyInfo();
		if ( txOb != MObject::kNullObj ) {
			nodeFn.setObject(txOb);
			MPlug tx_outColor = nodeFn.findPlug( MString("outColor") );
			switch(i) {
			case BASE_MAP:
				//Base Texture
				dgModifier.connect( tx_outColor, phongFn.findPlug("color") );
				//Check if Alpha needs to be used

				if ( niAlphaProp != NULL && ( niAlphaProp->GetBlendState() == true || niAlphaProp->GetTestState() == true ) ) {
					//Alpha is used, connect it
					dgModifier.connect( nodeFn.findPlug("outTransparency"), phongFn.findPlug("transparency") );
				}
				break;
			case GLOW_MAP:
				//Glow Texture
				dgModifier.connect( nodeFn.findPlug("outAlpha"), phongFn.findPlug("glowIntensity") );
				nodeFn.findPlug("alphaGain").setValue(0.25);
				nodeFn.findPlug("defaultColorR").setValue( 0.0 );
				nodeFn.findPlug("defaultColorG").setValue( 0.0 );
				nodeFn.findPlug("defaultColorB").setValue( 0.0 );
				dgModifier.connect( tx_outColor, phongFn.findPlug("incandescence") );
				break;
			}

			//Check for clamp mode
			bool wrap_u = true, wrap_v = true;
			TexClampMode clamp_mode = mw.GetTexClampMode( TexType(i) );
			if ( clamp_mode == CLAMP_S_CLAMP_T ) {
				wrap_u = false;
				wrap_v = false;
			} else if ( clamp_mode == CLAMP_S_WRAP_T ) {
				wrap_u = false;
			} else if ( clamp_mode == WRAP_S_CLAMP_T ) {
				wrap_v = false;
			}

			//Create 2D Texture Placement
			MFnDependencyNode tp2dFn;
			tp2dFn.create( "place2dTexture", "place2dTexture" );
			tp2dFn.findPlug("wrapU").setValue(wrap_u);
			tp2dFn.findPlug("wrapV").setValue(wrap_v);

			//Connect all the 18 things
			dgModifier.connect( tp2dFn.findPlug("coverage"), nodeFn.findPlug("coverage") );
			dgModifier.connect( tp2dFn.findPlug("mirrorU"), nodeFn.findPlug("mirrorU") );
			dgModifier.connect( tp2dFn.findPlug("mirrorV"), nodeFn.findPlug("mirrorV") );
			dgModifier.connect( tp2dFn.findPlug("noiseUV"), nodeFn.findPlug("noiseUV") );
			dgModifier.connect( tp2dFn.findPlug("offset"), nodeFn.findPlug("offset") );
			dgModifier.connect( tp2dFn.findPlug("outUV"), nodeFn.findPlug("uvCoord") );
			dgModifier.connect( tp2dFn.findPlug("outUvFilterSize"), nodeFn.findPlug("uvFilterSize") );
			dgModifier.connect( tp2dFn.findPlug("repeatUV"), nodeFn.findPlug("repeatUV") );
			dgModifier.connect( tp2dFn.findPlug("rotateFrame"), nodeFn.findPlug("rotateFrame") );
			dgModifier.connect( tp2dFn.findPlug("rotateUV"), nodeFn.findPlug("rotateUV") );
			dgModifier.connect( tp2dFn.findPlug("stagger"), nodeFn.findPlug("stagger") );
			dgModifier.connect( tp2dFn.findPlug("translateFrame"), nodeFn.findPlug("translateFrame") );
			dgModifier.connect( tp2dFn.findPlug("vertexCameraOne"), nodeFn.findPlug("vertexCameraOne") );
			dgModifier.connect( tp2dFn.findPlug("vertexUvOne"), nodeFn.findPlug("vertexUvOne") );
			dgModifier.connect( tp2dFn.findPlug("vertexUvTwo"), nodeFn.findPlug("vertexUvTwo") );
			dgModifier.connect( tp2dFn.findPlug("vertexUvThree"), nodeFn.findPlug("vertexUvThree") );
			dgModifier.connect( tp2dFn.findPlug("wrapU"), nodeFn.findPlug("wrapU") );
			dgModifier.connect( tp2dFn.findPlug("wrapV"), nodeFn.findPlug("wrapV") );
			//(Whew!)

			//Create uvChooser if necessary
			unsigned int uv_set = mw.GetTexUVSetIndex( TexType(i) );
			if ( uv_set > 0 ) {
				MFnDependencyNode chooserFn;
				chooserFn.create( "uvChooser", "uvChooser" );

				//Connection between the mesh and the uvChooser
				MFnMesh meshFn;
				meshFn.setObject(meshPath);
				dgModifier.connect( meshFn.findPlug("uvSet")[uv_set].child(0), chooserFn.findPlug("uvSets").elementByLogicalIndex(0) );

				//Connections between the uvChooser and the place2dTexture
				dgModifier.connect( chooserFn.findPlug("outUv"), tp2dFn.findPlug("uvCoord") );
				dgModifier.connect( chooserFn.findPlug("outVertexCameraOne"), tp2dFn.findPlug("vertexCameraOne") );
				dgModifier.connect( chooserFn.findPlug("outVertexUvOne"), tp2dFn.findPlug("vertexUvOne") );
				dgModifier.connect( chooserFn.findPlug("outVertexUvTwo"), tp2dFn.findPlug("vertexUvTwo") );
				dgModifier.connect( chooserFn.findPlug("outVertexUvThree"), tp2dFn.findPlug("vertexUvThree") );
			}
		}
	}

	//out << "Invoking dgModifier..." << endl;
	dgModifier.doIt();
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
	if(this->translatorOptions->exportType =="allgeometry" || this->translatorOptions->exportType == "allanimation") {
		return true;
	}
	for(int i = 0;i < this->translatorOptions->export_shapes.size(); i++) {
		if(name.asChar() == this->translatorOptions->export_shapes[i])
			return true;
	}

	return false;
}

bool NifTranslatorUtils::isExportedJoint( const MString& name )
{
	if(this->translatorOptions->exportType =="allgeometry" || this->translatorOptions->exportType == "allanimation") {
		return true;
	}
	for(int i = 0;i < this->translatorOptions->export_joints.size(); i++) {
		if(name.asChar() == this->translatorOptions->export_joints[i])
			return true;
	}

	return false;
}

std::string NifTranslatorUtils::MakeNifName( const MString & mayaName )
{
	stringstream newName;
	stringstream temp;
	temp.setf ( ios_base::hex, ios_base::basefield );  // set hex as the basefield

	string str = mayaName.asChar();

	if ( this->translatorOptions->use_name_mangling ) {
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
			return this->translatorData->sceneRoot;
		}
	}

	//Block was created, parent to scene root
	return this->translatorData->sceneRoot;
}



