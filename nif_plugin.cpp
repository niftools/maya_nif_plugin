#include "nif_plugin.h"

//#define _DEBUG
ofstream out;

const char PLUGIN_VERSION [] = "0.6.1";	
const char TRANSLATOR_NAME [] = "NetImmerse Format";

//--Globals--//
string texture_path; // Path to textures gotten from option string
unsigned int export_version = VER_4_0_0_2; //Version of NIF file to export
unsigned int export_user_version = 0; //Game-specific user version of NIF file to export
bool import_bind_pose = false; //Determines whether or not the bind pose should be searched for

bool import_normals= false; //Determines whether normals are imported
bool import_no_ambient = false; //Determines whether ambient color is imported
bool export_white_ambient = false; //Determines whether ambient color is automatically set to white if a texture is present
bool import_comb_skel = false; //Determines whether the importer tries to combine new skins with skeletons that exist in the scene
string joint_match = ""; //String to match in the name of nodes as another way to cause themm to import as IK joints
bool use_name_mangling = false;  //Determines whether to replace characters that are invalid for Maya (along with _ so that spaces can use that character) with hex representations
bool export_tri_strips = false;  //Determiens whether to export NiTriShape objects or NiTriStrip objects
int export_part_bones = 0; //Determines the maximum number of bones per skin partition.
bool export_tan_space = false; //Determines whether Oblivion tangent space data is generated for meshes on export
bool export_mor_rename = false; //Determines whether NiTriShapes tagged with materials that have Morrowind body parts are renamed to match those body parts
enum TexPathMode {
	PATH_MODE_AUTO, //Uses search paths to strip intelligently
	PATH_MODE_FULL, //Uses full path
	PATH_MODE_NAME, //Uses only file name
	PATH_MODE_PREFIX //Uses user-defined prefix
};
TexPathMode tex_path_mode = PATH_MODE_AUTO;  //Determines the way textures paths are exported
string tex_path_prefix; //Optional prefix to add to texture paths.
//--Function Definitions--//

//--NifTranslator::identifyFile--//

// This routine must use passed data to determine if the file is of a type supported by this translator

// Code adapted from animImportExport example that comes with Maya 6.5

MPxFileTranslator::MFileKind NifTranslator::identifyFile (const MFileObject& fileName, const char* buffer, short size) const {
	//out << "Checking File Type..." << endl;
	const char *name = fileName.name().asChar();
	int nameLength = (int)strlen(name);

	if ( name[nameLength-4] != '.' ) {
		return kNotMyFileType;
	}
	if ( name[nameLength-3] != 'n' && name[nameLength-3] != 'N' ) {
		return kNotMyFileType;
	}
	if ( name[nameLength-2] != 'i' && name[nameLength-2] != 'I' ) {
		return kNotMyFileType;
	}
	if ( name[nameLength-1] != 'f' && name[nameLength-1] != 'F' ) {
		return kNotMyFileType;
	}

	return kIsMyFileType;
}

//--Plug-in Load/Unload--//

//--initializePlugin--//

// Code adapted from lepTranslator example that comes with Maya 6.5
MStatus initializePlugin( MObject obj ) {
	#ifdef _DEBUG
	out.open( "C:\\Maya NIF Plug-in Log.txt", ofstream::binary );
	#endif

	//out << "Initializing Plugin..." << endl;
	MStatus   status;
	MFnPlugin plugin( obj, "NifTools", PLUGIN_VERSION );

	// Register the translator with the system
	status =  plugin.registerFileTranslator( TRANSLATOR_NAME,  //File Translator Name
										"nifTranslator.rgb", //Icon
										NifTranslator::creator, //Factory Function
										"nifTranslatorOpts", //MEL Script for options dialog
										NULL, //Default Options
										false ); //Requires MEL support
	if (!status) {
		status.perror("registerFileTranslator");
		return status;
	}

	//out << "Done Initializing." << endl;
	return status;
}

//--uninitializePlugin--//

// Code adapted from lepTranslator example that comes with Maya 6.5
MStatus uninitializePlugin( MObject obj )
{
	MStatus   status;
	MFnPlugin plugin( obj );

	status =  plugin.deregisterFileTranslator( TRANSLATOR_NAME );
	if (!status) {
		status.perror("deregisterFileTranslator");
		return status;
	}

	return status;
}

//Adjust NiNodes in the original file that match names
//in the maya scene to have the same transforms before
//importing the new mesh over the top of the old one
void NifTranslator::AdjustSkeleton( NiAVObjectRef & root ) {
	NiNodeRef niNode = DynamicCast<NiNode>(root);

	if ( niNode != NULL ) {
		string name = MakeMayaName( root->GetName() ).asChar();
		if ( existingNodes.find( name ) != existingNodes.end() ) {
			out << "Copying current scene transform of " << name << " over the top of " << root << endl;
			//Found a match
			MDagPath existing = existingNodes[name];

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

//--NifTranslator::reader--//

//This routine is called by Maya when it is necessary to load a file of a type supported by this translator.
//Responsible for reading the contents of the given file, and creating Maya objects via API or MEL calls to reflect the data in the file.
MStatus NifTranslator::reader (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	out << "Begining Read..." << endl;
	try {
		//Get user preferences
		ParseOptionString( optionsString );

		//Clear arrays
		importedNodes.clear();
		importedTextures.clear();
		importedMaterials.clear();
		mtCollection.Clear();
		importedMeshes.clear();

		//Save file
		importFile = file;

		out << "Reading NIF File..." << endl;
		//Read NIF file
		NiObjectRef root = ReadNifTree( file.fullName().asChar() );
		
		out << "Importing Nodes..." << endl;
		//Import Nodes, starting at each child of the root
		NiNodeRef root_node = DynamicCast<NiNode>(root);
		if ( root_node != NULL ) {
			//Root is a NiNode and may have children

			//Check if the user wants us to try to find the bind pose
			if ( import_bind_pose ) {
				SendNifTreeToBindPos( root_node );
			}

			//Check if the user wants us to try to combine new skins with
			//an existing skeleton
			if ( import_comb_skel ) {
				//Enumerate existing nodes by name
				existingNodes.clear();
				MItDag dagIt( MItDag::kDepthFirst);

				for ( ; !dagIt.isDone(); dagIt.next() ) {
					MFnTransform transFn( dagIt.item() );
					out << "Adding " << transFn.name().asChar() << " to list of existing nodes" << endl;
					MDagPath nodePath;
					dagIt.getPath( nodePath );
					existingNodes[ transFn.name().asChar() ] = nodePath;
				}

				//Adjust NiNodes in the original file that match names
				//in the maya scene to have the same transforms before
				//importing the new mesh over the top of the old one
				NiAVObjectRef rootAV = DynamicCast<NiAVObject>(root);
				if ( rootAV != NULL ) {
					AdjustSkeleton( rootAV );
				}
			}

			//Check if the root node has a non-identity transform
			if ( root_node->GetLocalTransform() == Matrix44::IDENTITY ) {
				//Root has no transform, so treat it as the scene root
				vector<NiAVObjectRef> root_children = root_node->GetChildren();
				
				for ( unsigned int i = 0; i < root_children.size(); ++i ) {
					ImportNodes( root_children[i], importedNodes );
				}
			} else {
				//Root has a transform, so it's probably part of the scene
				ImportNodes( StaticCast<NiAVObject>(root_node), importedNodes );
			}
		} else {
			NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
			if ( rootAVObj != NULL ) {
				//Root is importable, but has no children
				ImportNodes( rootAVObj, importedNodes );
			} else {
				//Root cannot be imported
				MGlobal::displayError( "The root of this NIF file is not derived from the NiAVObject class.  It cannot be imported." );
				return MStatus::kFailure;
			}
		}

		//--Import Materials--//
		out << "Importing Materials..." << endl;
		
		NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
		if ( rootAVObj != NULL ) {
			//Root is importable
			ImportMaterialsAndTextures( rootAVObj );
		}
		
		
		//--Import Meshes--//
		out << "Importing Meshes..." << endl;

		//Iterate through all meshes that were imported.
		//This had to be deffered because all bones must exist
		//when attaching skin
		for ( unsigned i = 0; i < importedMeshes.size(); ++i ) {
			out << "Importing mesh..." << endl;
			//Import Mesh
			MDagPath meshPath = ImportMesh( importedMeshes[i].first, importedMeshes[i].second);
		}
		out << "Done importing meshes." << endl;

		//--Import Animation--//
		out << "Importing Animation keyframes..." << endl;

		//Iterate through all imported nodes, looking for any with animation keys

		for ( map<NiAVObjectRef,MDagPath>::iterator it = importedNodes.begin(); it != importedNodes.end(); ++it ) {
			//Check to see if this node has any animation controllers
			if ( it->first->IsAnimated() ) {
				ImportControllers( it->first, it->second );
			}
		}

		out << "Deselecting anything that was selected by MEL commands" << endl;
		MGlobal::clearSelectionList();

		//Clear temporary data
		out << "Clearing temporary data" << endl;
		existingNodes.clear();
		importedNodes.clear();
		importedMaterials.clear();
		importedTextures.clear();
		importedMeshes.clear();
		importFile.setFullName("");
		sceneRoot = NULL;
		meshes.clear();
		nodes.clear(); 
		meshClusters.clear();
		shaders.clear();
	}
	catch( exception & e ) {
		MGlobal::displayError( e.what() );
		return MStatus::kFailure;
	}
	catch( ... ) {
		MGlobal::displayError( "Error:  Unknown Exception." );
		return MStatus::kFailure;
	}

	
	
	out << "Finished Read" << endl;

#ifndef _DEBUG
	//Clear the stringstream so it doesn't waste a bunch of RAM
	out.clear();
#endif

	return MStatus::kSuccess;
}

//Function that will import animation controllers from the NIF file
void NifTranslator::ImportControllers( NiAVObjectRef niAVObj, MDagPath & path ) {
	list<NiTimeControllerRef> controllers = niAVObj->GetControllers();

	//Iterate over the controllers, reacting properly to each type
	for ( list<NiTimeControllerRef>::iterator it = controllers.begin(); it != controllers.end(); ++it ) {
		if ( (*it)->IsDerivedType( NiKeyframeController::TYPE ) ) {
			//--NiKeyframeController--//
			NiKeyframeControllerRef niKeyCont = DynamicCast<NiKeyframeController>(*it);

			NiKeyframeDataRef niKeyData = niKeyCont->GetData();

			MFnTransform transFn( path.node() );

			MFnAnimCurve traXFn;
			MFnAnimCurve traYFn;
			MFnAnimCurve traZFn;

			MObject traXcurve = traXFn.create( transFn.findPlug("translateX") );
			MObject traYcurve = traYFn.create( transFn.findPlug("translateY") );
			MObject traZcurve = traZFn.create( transFn.findPlug("translateZ") );

			MTimeArray traTimes;
			MDoubleArray traXValues;
			MDoubleArray traYValues;
			MDoubleArray traZValues;

			vector<Key<Vector3> > tra_keys = niKeyData->GetTranslateKeys();

			for ( size_t i = 0; i < tra_keys.size(); ++i) {
				traTimes.append( MTime( tra_keys[i].time, MTime::kSeconds ) );
				traXValues.append( tra_keys[i].data.x );
				traYValues.append( tra_keys[i].data.y );
				traZValues.append( tra_keys[i].data.z );

				//traXFn.addKeyframe( tra_keys[i].time * 24.0, tra_keys[i].data.x );
				//traYFn.addKeyframe( tra_keys[i].time * 24.0, tra_keys[i].data.y );
				//traZFn.addKeyframe( tra_keys[i].time * 24.0, tra_keys[i].data.z );
			}

			traXFn.addKeys( &traTimes, &traXValues );
			traYFn.addKeys( &traTimes, &traYValues );
			traZFn.addKeys( &traTimes, &traZValues );

			KeyType kt = niKeyData->GetRotateType();

			if ( kt != XYZ_ROTATION_KEY ) {
				vector<Key<Quaternion> > rot_keys = niKeyData->GetQuatRotateKeys();

				MFnAnimCurve rotXFn;
				MFnAnimCurve rotYFn;
				MFnAnimCurve rotZFn;

				MObject rotXcurve = rotXFn.create( transFn.findPlug("rotateX") );
				//rotXFn.findPlug("rotationInterpolation").setValue(3);
				MObject rotYcurve = rotYFn.create( transFn.findPlug("rotateY") );
				//rotYFn.findPlug("rotationInterpolation").setValue(3);
				MObject rotZcurve = rotZFn.create( transFn.findPlug("rotateZ") );
				//rotZFn.findPlug("rotationInterpolation").setValue(3);

				MTimeArray rotTimes;
				MDoubleArray rotXValues;
				MDoubleArray rotYValues;
				MDoubleArray rotZValues;

				MEulerRotation mPrevRot;
				for( size_t i = 0; i < rot_keys.size(); ++i ) {
					Quaternion niQuat = rot_keys[i].data;

					MQuaternion mQuat( niQuat.x, niQuat.y, niQuat.z, niQuat.w );
					MEulerRotation mRot = mQuat.asEulerRotation();
					MEulerRotation mAlt;
					mAlt[0] = PI + mRot[0];
					mAlt[1] = PI - mRot[1];
					mAlt[2] = PI + mRot[2];

					for ( size_t j = 0; j < 3; ++j ) {
						double prev_diff = abs(mRot[j] - mPrevRot[j]);
						//Try adding and subtracting multiples of 2 pi radians to get
						//closer to the previous angle
						while (true) {
							double new_angle = mRot[j] - (PI * 2);
							double diff = abs( new_angle - mPrevRot[j] );
							if ( diff < prev_diff ) {
								mRot[j] = new_angle;
								prev_diff = diff;
							} else {
								break;
							}
						}
						while (true) {
							double new_angle = mRot[j] + (PI * 2);
							double diff = abs( new_angle - mPrevRot[j] );
							if ( diff < prev_diff ) {
								mRot[j] = new_angle;
								prev_diff = diff;
							} else {
								break;
							}
						}
					}

					for ( size_t j = 0; j < 3; ++j ) {
						double prev_diff = abs(mAlt[j] - mPrevRot[j]);
						//Try adding and subtracting multiples of 2 pi radians to get
						//closer to the previous angle
						while (true) {
							double new_angle = mAlt[j] - (PI * 2);
							double diff = abs( new_angle - mPrevRot[j] );
							if ( diff < prev_diff ) {
								mAlt[j] = new_angle;
								prev_diff = diff;
							} else {
								break;
							}
						}
						while (true) {
							double new_angle = mAlt[j] + (PI * 2);
							double diff = abs( new_angle - mPrevRot[j] );
							if ( diff < prev_diff ) {
								mAlt[j] = new_angle;
								prev_diff = diff;
							} else {
								break;
							}
						}
					}

					
					//Try taking advantage of the fact that:
					//Rotate(x,y,z) = Rotate(pi + x, pi - y, pi +z)
					double rot_diff = ( (abs(mRot[0] - mPrevRot[0]) + abs(mRot[1] - mPrevRot[1]) + abs(mRot[2] - mPrevRot[2]) ) / 3.0 );
					double alt_diff = ( (abs(mAlt[0] - mPrevRot[0]) + abs(mAlt[1] - mPrevRot[1]) + abs(mAlt[2] - mPrevRot[2]) ) / 3.0 );

					if ( alt_diff < rot_diff ) {
						mRot = mAlt;
					}

					mPrevRot = mRot;

					rotTimes.append( MTime(rot_keys[i].time, MTime::kSeconds) );
					rotXValues.append( mRot[0] );
					rotYValues.append( mRot[1] );
					rotZValues.append( mRot[2] );
				}

				
				rotXFn.addKeys( &rotTimes, &rotXValues );
				rotYFn.addKeys( &rotTimes, &rotYValues );
				rotZFn.addKeys( &rotTimes, &rotZValues );
			}
		}
	}
}

// Recursive function to crawl down tree looking for children and translate those that are understood
void NifTranslator::ImportNodes( NiAVObjectRef niAVObj, map< NiAVObjectRef, MDagPath > & objs, MObject parent )  {
	MObject obj;

	out << "Importing " << niAVObj << endl;

	////Stop at a non-node
	//if ( node == NULL )
	//	return;

	//Check if this node is a skinned NiTriBasedGeom.  If so, parent it to the scene instead
	//of whereever the NIF file has it.

	bool is_skin = false;

	vector<NiAVObjectRef> nodesToTest;

	if ( niAVObj->IsDerivedType( NiNode::TYPE ) ) {
		NiNodeRef nnr = DynamicCast<NiNode>(niAVObj);
		if ( nnr->IsSplitMeshProxy() ) {
			//Test all children
			nodesToTest = nnr->GetChildren();
		}
	} else if ( niAVObj->IsDerivedType(NiTriBasedGeom::TYPE ) ) {
		//This is a shape, so test it.
		nodesToTest.push_back(niAVObj);
	}

	for ( size_t i = 0; i < nodesToTest.size(); ++i ) {
		NiTriBasedGeomRef niTriGeom = DynamicCast<NiTriBasedGeom>( nodesToTest[i] );
		if ( niTriGeom && niTriGeom->IsSkin() ) {
			is_skin = true;
			break;
		}
	}

	if ( is_skin ) {
		parent = MObject::kNullObj;
	}

	//This must be a node, so process its basic attributes	
	MFnTransform transFn;
	MString name = MakeMayaName( niAVObj->GetName() );
	string strName = name.asChar();

	int flags = niAVObj->GetFlags();


	NiNodeRef niNode = DynamicCast<NiNode>(niAVObj);

	//Determine whether this node is an IK joint
	bool is_joint = false;
	if ( niNode != NULL) {
		if ( joint_match != "" && strName.find(joint_match) != string::npos ) {
			is_joint = true;
		} else if ( niNode->IsSkinInfluence() == true ) {
			is_joint = true;
		}
	}

	//Check if the user wants us to try to combine new skins with an
	//existing skeleton
	if ( import_comb_skel ) {
		//Check if joint already exits in scene.  If so, use it.
		if ( is_joint ) {
			obj = GetExistingJoint( name.asChar() );
		}
	}

	if ( obj.isNull() == false ) {
		//A matching object was found
		transFn.setObject( obj );
	} else {
		if ( is_joint ) {
			// This is a bone, create an IK joint parented to the given parent
			MFnIkJoint IKjointFn;
			obj = IKjointFn.create( parent );

			//Set Transform Fn to work on the new IK joint
			transFn.setObject( obj );
		}
		else {
			//Not a bone, create a transform node parented to the given parent
			obj = transFn.create( parent );
		}

		//--Set the Transform Matrix--//
		Matrix44 transform;

		if ( is_skin ) {
			transform = niAVObj->GetWorldTransform();
		} else {
			transform = niAVObj->GetLocalTransform();
		}

		//put matrix into a float array
		float trans_arr[4][4];
		transform.AsFloatArr( trans_arr );

		transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
		transFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

		//Set name
		MFnDependencyNode dnFn;
		dnFn.setObject(obj);
		dnFn.setName( name );	
	}

	//Add the newly created object to the objects list
	MDagPath path;
	transFn.getPath( path );
	objs[niAVObj] = path;

	//Check if this object has a bounding box
	if ( niAVObj->HasBoundingBox() ) {
		//Get bounding box info
		BoundingBox bb = niAVObj->GetBoundingBox();		
		
		//Create a transform node to move the bounding box around
		MFnTransform tranFn;
		tranFn.create( obj );
		tranFn.setName("BoundingBox");
		Matrix44 bbTrans( bb.translation, bb.rotation, 1.0f);
		out << "bbTrans:" << endl << bbTrans << endl;
		out << "Translation:  " << bb.translation << endl;
		out << "Rotation:  " << bb.rotation << endl;

		//put matrix into a float array
		float bb_arr[4][4];
		bbTrans.AsFloatArr( bb_arr );
		MStatus stat = tranFn.set( MTransformationMatrix(MMatrix(bb_arr)) );
		if ( stat != MS::kSuccess ) {
			out << stat.errorString().asChar() << endl;
			throw runtime_error("Failed to set bounding box transforms.");
		}
		tranFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

		//Create an implicit box parented to the node we just created
		MFnDagNode dagFn;
		dagFn.create( "implicitBox", tranFn.object() );
		dagFn.findPlug("sizeX").setValue( bb.radius.x );
		dagFn.findPlug("sizeY").setValue( bb.radius.y );
		dagFn.findPlug("sizeZ").setValue( bb.radius.z );
		
	}

	//Check to see if this is a mesh
	if ( niAVObj->IsDerivedType( NiTriBasedGeom::TYPE ) ) {
		//This is a mesh, so add it to the mesh list
		importedMeshes.push_back( pair<NiAVObjectRef,MObject>(niAVObj,obj) );
	}

	if ( niNode != NULL ) {

		//Check to see if this is a mesh proxy
		if ( niNode->IsSplitMeshProxy() ) {
			out << niNode << " is a split mesh proxy." << endl;
			//Since this is a mesh proxy, treat it like a mesh and do not
			//call this function on any children
			importedMeshes.push_back( pair<NiAVObjectRef,MObject>(niAVObj,obj) );
		} else {
			//Call this function for any children
			vector<NiAVObjectRef> children = niNode->GetChildren();
			for ( unsigned int i = 0; i < children.size(); ++i ) {
				ImportNodes( children[i], objs, obj );
			}
		}
	}
}

MDagPath NifTranslator::ImportMesh( NiAVObjectRef root, MObject parent ) {
	out << "ImportMesh() begin" << endl;

	//Make invisible if this or any children are invisible
	bool visible = true;
	if ( root->GetVisibility() == false ) {
		visible = false;
	} else {
		NiNodeRef rootNode = DynamicCast<NiNode>(root);
		if ( rootNode != NULL ) {
			vector<NiAVObjectRef> children = rootNode->GetChildren();
			for ( unsigned i = 0; i < children.size(); ++i ) {
				if ( children[i]->GetVisibility() == false ) {
					visible = false;
					break;
				}
			}
		}
		
	}

	if ( visible == false ) {
		MFnDagNode parFn(parent);
		MPlug vis = parFn.findPlug( MString("visibility") );
		vis.setValue(false);
	}

	out << "Try out ComplexShape::Merge on" << root << endl;
	ComplexShape cs;

	cs.Merge( root );

	vector<WeightedVertex> nif_verts = cs.GetVertices();
	unsigned NumVertices = unsigned(nif_verts.size());
	out << "Num Vertices:  " << NumVertices << endl;
	
	MPointArray maya_verts(NumVertices);

	for (unsigned i = 0; i < NumVertices; ++i) {
		maya_verts[i] = MPoint(nif_verts[i].position.x, nif_verts[i].position.y, nif_verts[i].position.z, 0.0f);
	}

	out << "Getting polygons..." << endl;
	//Get Polygons
	int NumPolygons = 0;
	vector<ComplexFace> niRawFaces = cs.GetFaces();

	MIntArray maya_poly_counts;
	MIntArray maya_connects;
	vector<ComplexFace> niFaces;
	for (unsigned i = 0; i < niRawFaces.size(); ++i) {

		//Only append valid triangles
		if ( niRawFaces[i].points.size() != 3 ) {
			//not a triangle
			continue;
		}

		const ComplexFace & f = niRawFaces[i];

		unsigned p0 = f.points[0].vertexIndex;
		unsigned p1 = f.points[1].vertexIndex;
		unsigned p2 = f.points[2].vertexIndex;
		
		if ( p0 == p1 || p0 == p2 || p1 == p2 ) {
			//Invalid triangle
			continue;
		}

		maya_connects.append( p0 );
		maya_connects.append( p1 );
		maya_connects.append( p2 );
		maya_poly_counts.append(3);
		niFaces.push_back( f );
	}
	niRawFaces.clear();

	//NumPolygons = triangles.size();
	NumPolygons = niFaces.size();
	out << "Num Polygons:  " << NumPolygons << endl;

	//MIntArray maya_connects( connects, NumPolygons * 3 );

	//Create Mesh with empy default UV set at first
	MDagPath meshPath;
	MFnMesh meshFn;
	
	MStatus stat;
	meshFn.create( NumVertices, maya_poly_counts.length(), maya_verts, maya_poly_counts, maya_connects, parent, &stat );
	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to create mesh.");
	}
	
	meshFn.getPath( meshPath );

	out << "Importing vertex colors..." << endl;
	//Import Vertex Colors
	vector<Color4> nif_colors = cs.GetColors();
	if ( nif_colors.size() > 0 ) {
		//Create vertex list
		MIntArray face_list;
		MIntArray vert_list;
		MColorArray maya_colors;
		
		for ( unsigned f = 0; f < niFaces.size(); ++f ) {
			//Make sure all of the points in this face have color
			if (
				niFaces[f].points[0].colorIndex == CS_NO_INDEX ||
				niFaces[f].points[1].colorIndex == CS_NO_INDEX ||
				niFaces[f].points[2].colorIndex == CS_NO_INDEX
			) { 
				continue; 
			}

			face_list.append(f);
			face_list.append(f);
			face_list.append(f);

			vert_list.append( niFaces[f].points[0].vertexIndex );
			vert_list.append( niFaces[f].points[1].vertexIndex );
			vert_list.append( niFaces[f].points[2].vertexIndex );

			Color4 color1 = nif_colors[ niFaces[f].points[0].colorIndex ];
			Color4 color2 = nif_colors[ niFaces[f].points[1].colorIndex ];
			Color4 color3 = nif_colors[ niFaces[f].points[2].colorIndex ];

			maya_colors.append( MColor( color1.r, color1.g, color1.b, color1.a ) );
			maya_colors.append( MColor( color2.r, color2.g, color2.b, color2.a ) );
			maya_colors.append( MColor( color3.r, color3.g, color3.b, color3.a ) );
		}

		MStatus stat = meshFn.setFaceVertexColors( maya_colors, face_list, vert_list );
		if ( stat != MS::kSuccess ) {
			out << stat.errorString().asChar() << endl;
			throw runtime_error("Failed to set Colors.");
		}
	}

	out << "Creating a list of the UV sets..." << endl;
	//Create a list of the UV sets used by the complex shape if any

	vector<TexCoordSet> niUVSets = cs.GetTexCoordSets();
	vector<MString> uv_set_list( niUVSets.size() );

	for ( unsigned i = 0; i < niUVSets.size(); ++i ) {
		switch( niUVSets[i].texType ) {
			case BASE_MAP:
				uv_set_list[i] = MString("map1");
				break;
			case DARK_MAP:
				uv_set_list[i] = MString("dark");
				break;
			case DETAIL_MAP:
				uv_set_list[i] = MString("detail");
				break;
			case GLOSS_MAP:
				uv_set_list[i] = MString("gloss");
				break;
			case GLOW_MAP:
				uv_set_list[i] = MString("glow");
				break;
			case BUMP_MAP:
				uv_set_list[i] = MString("bump");
				break;
			case DECAL_0_MAP:
				uv_set_list[i] = MString("decal0");
				break;
			case DECAL_1_MAP:
				uv_set_list[i] = MString("decal1");
				break;
		}
	}

	out << "UV Set List:  "  << endl;
	for ( unsigned int i = 0; i < uv_set_list.size(); ++i ) {
		out << uv_set_list[i].asChar() << endl;
	}

	out << "Getting default UV set..." << endl;
	// Get default (first) UV Set if there is one		
	if ( niUVSets.size() > 0 ) {
		meshFn.clearUVs();
		vector<TexCoord> uv_set;

		out << "Loop through " << niUVSets.size() << " UV sets..." << endl;
		for ( unsigned i = 0; i < niUVSets.size(); ++i ) {
			uv_set = niUVSets[i].texCoords;

			//Arrays for maya
			MFloatArray u_arr(uv_set.size()), v_arr(uv_set.size());

			//out << "uv_set_list.size():  " << uv_set_list.size() << endl;
			//out << "i:  " << i << endl;


			for ( unsigned j = 0; j < uv_set.size(); ++j) {
				u_arr[j] = uv_set[j].u;
				v_arr[j] = 1.0f - uv_set[j].v;
			}

			//out << "Original UV Array:" << endl;
			//for ( unsigned j = 0; j < uv_set.size(); ++j ) {
			//	out << "U:  " << uv_set[j].u << "  V:  " << uv_set[j].v << endl;
			//}
			//out << "Maya UV Array:" << endl;
			//for ( unsigned j = 0; j < uv_set.size(); ++j ) {
			//	out << "U:  " << u_arr[j] << "  V:  " << v_arr[j]<< endl;
			//}
			
			//Assign the UVs to the object
			MString uv_set_name("map1");
			if ( i < int(uv_set_list.size()) ) {
				//out << "Entered if statement." << endl;
				uv_set_name = uv_set_list[i];
			}

			out << "Check if a UV set needs to be created..." << endl;
			if ( uv_set_name != "map1" ) {
				meshFn.createUVSet( uv_set_name );
				out << "Creating UV Set:  " << uv_set_name.asChar() << endl;
			} else {
				//Clear out the current UV set
				//meshFn.clearUVs( &uv_set_name );
			}
	
			out << "Set UVs...  u_arr:  " << u_arr.length() << " v_arr:  " << v_arr.length() << " uv_set_name " << uv_set_name.asChar() << endl;
			MStatus stat = meshFn.setUVs( u_arr, v_arr, &uv_set_name );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to set UVs.");
			}

			out << "Create list of which UV to assign to each polygon vertex";
			maya_poly_counts.clear();
			maya_connects.clear();
			//Create list of which UV to assign to each polygon vertex
			for ( unsigned f = 0; f < niFaces.size(); ++f ) {
				if (
					niFaces[f].points[0].vertexIndex == niFaces[f].points[1].vertexIndex ||
					niFaces[f].points[0].vertexIndex == niFaces[f].points[2].vertexIndex ||
					niFaces[f].points[1].vertexIndex == niFaces[f].points[2].vertexIndex
				) {
					//Invalid triangle
					continue;
				}

				//Make sure all of the points in this face have color
				unsigned match[3];
				int matches_found = 0;
				for ( unsigned p = 0; p < 3; ++p ) {
					//Figure out which index we're using, if any
					for ( unsigned t = 0; t < niFaces[f].points[p].texCoordIndices.size(); ++t ) {
						//out << "niFaces[" << f << "].points[" << p << "].texCoordIndices[" << t << "].texCoordSetIndex:  " << niFaces[f].points[p].texCoordIndices[t].texCoordSetIndex << endl;
						if ( niFaces[f].points[p].texCoordIndices[t].texCoordSetIndex == i ) {
							//out << "Match found at " << t << endl;
							++matches_found;
							match[p] = t;
							break;
						}
					}
				}

				//out << " Matches found:  " << matches_found << endl;
				if ( matches_found != 3 ) {
					//This face is not mapped, 0 points
					maya_poly_counts.append(0);
					continue;
				}

				unsigned tcIndices[3] = {
					niFaces[f].points[0].texCoordIndices[ match[0] ].texCoordIndex,
					niFaces[f].points[1].texCoordIndices[ match[1] ].texCoordIndex,
					niFaces[f].points[2].texCoordIndices[ match[2] ].texCoordIndex
				};


				if ( tcIndices[0] == CS_NO_INDEX || tcIndices[0] == CS_NO_INDEX || tcIndices[0] == CS_NO_INDEX ) { 
					//This face is not mapped, 0 points
					maya_poly_counts.append(0);
					continue; 
				}

				maya_poly_counts.append(3); //3 points in this face

				//out << "Texture Coord indices for set " << i << " face " << f << endl;

				maya_connects.append(tcIndices[0]);
				maya_connects.append(tcIndices[1]);
				maya_connects.append(tcIndices[2]);
				
			}

			//unsigned sum = 0;
			//for ( unsigned i = 0; i < maya_poly_counts.length(); ++i ) {
			//	sum += maya_poly_counts[i];
			//}

			//out << "Poly Count Length:  " << maya_poly_counts.length() << endl;
			//out << "Poly Count Sum:  " << sum << endl;
			//out << "Connects Length:  " << maya_connects.length() << endl;

			stat = meshFn.assignUVs( maya_poly_counts, maya_connects, &uv_set_name );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to assign UVs.");
			}
		}
	}

	//See if the user wants us to import the normals
	if ( import_normals ) {
		out << "Getting Normals..." << endl;
		// Load Normals
		vector<Vector3> nif_normals = cs.GetNormals();
		//Create vertex list
		MIntArray face_list;
		MIntArray vert_list;
		MVectorArray maya_normals;
		if ( nif_normals.size() != 0 ) {
			for ( unsigned f = 0; f < niFaces.size(); ++f ) {
				//Make sure all of the points in this face have a normal
				if (
					niFaces[f].points[0].normalIndex == CS_NO_INDEX ||
					niFaces[f].points[1].normalIndex == CS_NO_INDEX ||
					niFaces[f].points[2].normalIndex == CS_NO_INDEX
				) { 
					continue; 
				}

				face_list.append(f);
				face_list.append(f);
				face_list.append(f);

				vert_list.append( niFaces[f].points[0].vertexIndex );
				vert_list.append( niFaces[f].points[1].vertexIndex );
				vert_list.append( niFaces[f].points[2].vertexIndex );

				Vector3 norm1 = nif_normals[ niFaces[f].points[0].normalIndex ];
				Vector3 norm2 = nif_normals[ niFaces[f].points[1].normalIndex ];
				Vector3 norm3 = nif_normals[ niFaces[f].points[2].normalIndex ];

				maya_normals.append( MVector( norm1.x, norm1.y, norm1.z ) );
				maya_normals.append( MVector( norm2.x, norm2.y, norm2.z ) );
				maya_normals.append( MVector( norm3.x, norm3.y, norm3.z ) );
			}

			MStatus stat = meshFn.setFaceVertexNormals( maya_normals, face_list, vert_list );

			//MStatus stat = meshFn.setVertexNormals( maya_normals, vert_list );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to set Normals.");
			}
		}
	}

	out << "Importing Materials and textures" << endl;
	vector< vector<NiPropertyRef> > propGroups = cs.GetPropGroups();

	//Create a selection list for each prop group
	vector<MSelectionList> sel_lists(propGroups.size());
	//sel_list.add( meshPath );

	//If there is only one property group, use it on the whole mesh
	if ( propGroups.size() == 1 ) {
		sel_lists[0].add( meshPath );
	} else {
		//Add the faces affected by each property group to the corresponding
		//selection list
		MItMeshPolygon gIt(meshPath);
		for ( ; !gIt.isDone(); gIt.next() ) {
			//out << "gIt.index():  " << gIt.index() << endl;
			unsigned prop_index = niFaces[ gIt.index() ].propGroupIndex;
			if ( prop_index != CS_NO_INDEX ) {
				//out << "Appending face " << gIt.index() << " to selection list " << prop_index << endl;
				sel_lists[prop_index].add( meshPath, gIt.polygon() );
			}
		}
	}

	for ( unsigned i = 0; i < propGroups.size(); ++i ) {
		ConnectShader( propGroups[i], meshPath, sel_lists[i] );
	}

	out << "Bind skin if any" << endl;
	//--Bind Skin if any--//

	vector<NiNodeRef> bone_nodes = cs.GetSkinInfluences();
	if ( bone_nodes.size() != 0 ) {
		//Build up the MEL command string
		string cmd = "skinCluster -tsb ";
		
		//out << "Add list of bones to the command" << endl;
		//Add list of bones to the command
		for (unsigned int i = 0; i < bone_nodes.size(); ++i) {
			cmd.append( importedNodes[ StaticCast<NiAVObject>(bone_nodes[i]) ].partialPathName().asChar() );
			cmd.append( " " );
		}

		//out << "Add mesh path to the command" << endl;
		//Add mesh path to the command
		cmd.append( meshPath.partialPathName().asChar() );

		//out << "Execute command" << endl;
		//Execute command to create skin cluster bound to specific bones
		MStringArray result;
		MGlobal::executeCommand( cmd.c_str(), result );
		MFnSkinCluster clusterFn;

		MSelectionList selList;
		selList.add(result[0]);
		MObject skinOb;
		selList.getDependNode( 0, skinOb );
		clusterFn.setObject(skinOb);

		//out << "Get a list of all vertices in this mesh" << endl;
		//Get a list of all verticies in this mesh
		MFnSingleIndexedComponent compFn;
		MObject vertices = compFn.create( MFn::kMeshVertComponent );
		MItGeometry gIt(meshPath);
		MIntArray vertex_indices( gIt.count() );
		for ( int j = 0; j < gIt.count(); ++j ) {
			vertex_indices[j] = j;
		}
		compFn.addElements(vertex_indices);

		//out << "Get weight data from NIF" << endl;
		//Get weight data from NIF
		vector< vector<float> > nif_weights( bone_nodes.size() );

		//out << "Set skin weights & bind pose for each bone" << endl;
		//initialize 2D array to zeros
		for (unsigned int i = 0; i < bone_nodes.size(); ++i) {
			nif_weights[i].resize( nif_verts.size() );
			for ( unsigned j = 0; j < nif_verts.size(); ++j ) {
				nif_weights[i][j] = 0.0f;
			} 
		}
			

		//out << "Put data in proper slots in the 2D array" << endl;
		//Put data in proper slots in the 2D array
		for ( unsigned v = 0; v < nif_verts.size(); ++v ) {
			for ( unsigned w = 0; w < nif_verts[v].weights.size(); ++w ) {
				SkinInfluence & si = nif_verts[v].weights[w];

				nif_weights[si.influenceIndex][v] = si.weight;
			}
		}

		//out << "Build Maya influence list" << endl;
		//Build Maya influence list
		MIntArray influence_list( bone_nodes.size() );
		for ( unsigned int i = 0; i < bone_nodes.size(); ++i ) {
			influence_list[i] = i;
		}

		//out << "Build Maya weight list" << endl;
		//Build Maya weight list
		MFloatArray weight_list(  gIt.count() * int(bone_nodes.size()) );
		int k = 0;
		for ( int i = 0; i < gIt.count(); ++i ) {
			for ( int j = 0; j < int(bone_nodes.size()); ++j ) {
				//out << "Bone:  " << bone_nodes[j] << "\tWeight:  " << nif_weights[j][i] << endl;
				weight_list[k] = nif_weights[j][i];
				++k;
			}
		}

		//out << "Send the weights to Maya" << endl;
		//Send the weights to Maya
		clusterFn.setWeights( meshPath, vertices, influence_list, weight_list, true );
	}			

	out << "ImportMesh() end" << endl;
	return meshPath;
}

void ApplyAllSkinOffsets( NiAVObjectRef & root ) {
	NiGeometryRef niGeom = DynamicCast<NiGeometry>(root);
	if ( niGeom != NULL && niGeom->IsSkin() == true ) {
		niGeom->ApplySkinOffset();
	}

	NiNodeRef niNode = DynamicCast<NiNode>(root);
	if ( niNode != NULL ) {
		//Call this function on all children
		vector<NiAVObjectRef> children = niNode->GetChildren();

		for ( unsigned i = 0; i < children.size(); ++i ) {
			ApplyAllSkinOffsets( children[i] );
		}
	}
}

//--NifTranslator::writer--//

//This routine is called by Maya when it is necessary to save a file of a type supported by this translator.
//Responsible for traversing all objects in the current Maya scene, and writing a representation to the given
//file in the supported format.
MStatus NifTranslator::writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	
	try {
		//Get user preferences
		ParseOptionString( optionsString );

		out << "Creating root node...";
		//Create new root node
		sceneRoot = new NiNode;
		sceneRoot->SetName( "Scene Root" );
		out << sceneRoot << endl;

		out << "Exporting file textures..." << endl;
		textures.clear();
		//Export file textures and get back a map of DAG path to Nif block
		ExportFileTextures();

		
		out << "Exporting shaders..." << endl;
		shaders.clear();
		//Export shaders
		ExportShaders();


		out << "Clearing Nodes" << endl;
		nodes.clear();
		out << "Clearing Meshes" << endl;
		meshes.clear();
		//Export nodes

		out << "Exporting nodes..." << endl;
		ExportDAGNodes();

		out << "Enumerating skin clusters..." << endl;
		meshClusters.clear();
		EnumerateSkinClusters();

		out << "Exporting meshes..." << endl;
		for ( list<MObject>::iterator mesh = meshes.begin(); mesh != meshes.end(); ++mesh ) {
			ExportMesh( *mesh );
		}

		out << "Applying Skin offsets..." << endl;
		ApplyAllSkinOffsets( StaticCast<NiAVObject>(sceneRoot) );



		//--Write finished NIF file--//

		out << "Writing Finished NIF file..." << endl;
		NifInfo nif_info(export_version, export_user_version);
		nif_info.endian = ENDIAN_LITTLE; //Intel endian format
		nif_info.exportInfo1 = "NifTools Maya NIF Plug-in " + string(PLUGIN_VERSION);
		WriteNifTree( file.fullName().asChar(), StaticCast<NiObject>(sceneRoot), nif_info );

		out << "Export Complete." << endl;

		//Clear temporary data
		out << "Clearing temporary data" << endl;
		existingNodes.clear();
		importedNodes.clear();
		importedMaterials.clear();
		importedTextures.clear();
		importedMeshes.clear();
		importFile.setFullName("");
		sceneRoot = NULL;
		meshes.clear();
		nodes.clear(); 
		meshClusters.clear();
		shaders.clear();
	}
	catch( exception & e ) {
		stringstream out;
		out << "Error:  " << e.what() << endl;
		MGlobal::displayError( out.str().c_str() );
		return MStatus::kFailure;
	}
	catch( ... ) {
		MGlobal::displayError( "Error:  Unknown Exception." );
		return MStatus::kFailure;
	}

#ifndef _DEBUG
	//Clear the stringstream so it doesn't waste a bunch of RAM
	out.clear();
#endif

	return MS::kSuccess;
}

void NifTranslator::ExportMesh( MObject dagNode ) {
	out << "NifTranslator::ExportMesh {";
	ComplexShape cs;
	MStatus stat;
	MObject mesh;

	//Find Mesh child of given transform objet
	MFnDagNode nodeFn(dagNode);

	cs.SetName( MakeNifName(nodeFn.name()) );

	for( int i = 0; i != nodeFn.childCount(); ++i ) {
		// get a handle to the child
		if ( nodeFn.child(i).hasFn(MFn::kMesh) ) {
			MFnMesh tempFn( nodeFn.child(i) );
			//No history items
			if ( !tempFn.isIntermediateObject() ) {
				out << "Found a mesh child." << endl;
				mesh = nodeFn.child(i);
				break;
			}
		}
	}

	MFnMesh visibleMeshFn(mesh, &stat);
	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to create visibleMeshFn.");
	}
	
	out << visibleMeshFn.name().asChar() << ") {" << endl;
	MFnMesh meshFn;
	MObject dataObj;
	MPlugArray inMeshPlugArray;
	MPlug childPlug;
	MPlug geomPlug;
	MPlug inputPlug;

	// this will hold the returned vertex positions
	MPointArray vts;

	//For now always use the visible mesh
	meshFn.setObject(mesh);



	//			// get the mesh coming into the clusterFn.  This
	//			// is the mesh before being deformed but after
	//			// being edited/tweaked/etc.

	//			out << "Get input plug" << endl;
	//			inputPlug = clusterFn.findPlug("input", &stat);
	//			if ( stat != MS::kSuccess ) {
	//				out << stat.errorString().asChar() << endl;
	//				throw runtime_error("Unable to find input plug");
	//			}

	//			unsigned int meshIndex = clusterFn.indexForOutputShape( visibleMeshFn.object(),&stat );
	//			if ( stat != MS::kSuccess ) {
	//				out << stat.errorString().asChar() << endl;
	//				throw runtime_error("Failed to get index for output shape");
	//			}

	//			childPlug = inputPlug.elementByLogicalIndex( meshIndex, &stat ); 
	//			if ( stat != MS::kSuccess ) {
	//				out << stat.errorString().asChar() << endl;
	//				throw runtime_error("Failed to get element by logical index");
	//			}
	//			geomPlug = childPlug.child(0,&stat); 
	//			if ( stat != MS::kSuccess ) {
	//				out << stat.errorString().asChar() << endl;
	//				throw runtime_error("failed to get geomPlug");
	//			}

	//			stat = geomPlug.getValue(dataObj);
	//			if ( stat != MS::kSuccess ) {
	//				out << stat.errorString().asChar() << endl;
	//				throw runtime_error("Failed to get value from geomPlug.");
	//			}

	//			out << "Use input mesh instead of the visible one." << endl;
	//			// let use this mesh instead of the visible one
	//			if ( dataObj.hasFn( MFn::kMesh ) == true ) {
	//				out << "Data has MeshFN function set." << endl;
	//				//stat = meshFn.setObject(dataObj);
	//				if ( stat != MS::kSuccess ) {
	//					out << stat.errorString().asChar() << endl;
	//					throw runtime_error("Failed to set new object");
	//				}
	//				
	//				//use_visible_mesh = false;
	//			} else {
	//				out << "Data does not have meshFn function set" << endl;
	//			}
	//		}
	//	//}
	//	out << "Loop Complete" << endl;
	////}

	out << "Use the function set to get the points" << endl;
	// use the function set to get the points
	stat = meshFn.getPoints(vts);
	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to get points.");
	}

	//Maya won't store any information about objects with no vertices.  Just skip it.
	if ( vts.length() == 0 ) {
		MGlobal::displayWarning("An object in this scene has no vertices.  Nothing will be exported.");
		return;
	}

	vector<WeightedVertex> nif_vts( vts.length() );
	for( int i=0; i != vts.length(); ++i ) {
		nif_vts[i].position.x = float(vts[i].x);
		nif_vts[i].position.y = float(vts[i].y);
		nif_vts[i].position.z = float(vts[i].z);
	}

	//Set vertex info later since it includes skin weights
	//cs.SetVertices( nif_vts );

	out << "Use the function set to get the colors" << endl;
	MColorArray myColors;
	meshFn.getFaceVertexColors( myColors );

	out << "Prepare NIF color vector" << endl;
	vector<Color4> niColors( myColors.length() );
	for( unsigned int i = 0; i < myColors.length(); ++i ) {
		niColors[i] = Color4( myColors[i].r, myColors[i].g, myColors[i].b, myColors[i].a );
	}
	cs.SetColors( niColors );


	// this will hold the returned vertex positions
	MFloatVectorArray nmls;

	out << "Use the function set to get the normals" << endl;
	// use the function set to get the normals
	stat = meshFn.getNormals( nmls, MSpace::kTransform );
	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to get normals");
	}
	
	out << "Prepare NIF normal vector" << endl;
	vector<Vector3> nif_nmls( nmls.length() );
	for( int i=0; i != nmls.length(); ++i ) {
		nif_nmls[i].x = float(nmls[i].x);
		nif_nmls[i].y = float(nmls[i].y);
		nif_nmls[i].z = float(nmls[i].z);
	}
	cs.SetNormals( nif_nmls );

	out << "Use the function set to get the UV set names" << endl;
	MStringArray uvSetNames;
	MString baseUVSet;
	MFloatArray myUCoords;
	MFloatArray myVCoords;
	bool has_uvs = false;

	// get the names of the uv sets on the mesh
	meshFn.getUVSetNames(uvSetNames);

	vector<TexCoordSet> nif_uvs;

	//Record assotiation between name and uv set index for later
	map<string,int> uvSetNums;

	int set_num = 0;
	for ( unsigned int i = 0; i < uvSetNames.length(); ++i ) {
		if ( meshFn.numUVs( uvSetNames[i] ) > 0 ) {
			TexType tt;
			string set_name = uvSetNames[i].asChar();
			if ( set_name == "base" || set_name == "map1" ) {
				tt = BASE_MAP;
			} else if ( set_name == "dark" ) {
				tt = DARK_MAP;
			} else if ( set_name == "detail" ) {
				tt = DETAIL_MAP;
			} else if ( set_name == "gloss" ) {
				tt = GLOSS_MAP;
			} else if ( set_name == "glow" ) {
				tt = GLOW_MAP;
			} else if ( set_name == "bump" ) {
				tt = BUMP_MAP;
			} else if ( set_name == "decal0" ) {
				tt = DECAL_0_MAP;
			} else if ( set_name == "decal1" ) {
				tt = DECAL_1_MAP;
			} else {
				tt = BASE_MAP;
			}

			//Record the assotiation
			uvSetNums[set_name] = set_num;

			//Get the UVs
			meshFn.getUVs( myUCoords, myVCoords, &uvSetNames[i] );

			//Make sure this set actually has some UVs in it.  Maya sometimes returns empty UV sets.
			if ( myUCoords.length() == 0 ) {
				continue;
			}

			//Store the data
			TexCoordSet tcs;
			tcs.texType = tt;
			tcs.texCoords.resize( myUCoords.length() );
			for ( unsigned int j = 0; j < myUCoords.length(); ++j ) {
				tcs.texCoords[j].u = myUCoords[j];
				//Flip the V coords
				tcs.texCoords[j].v = 1.0f - myVCoords[j];
			}
			nif_uvs.push_back(tcs);

			baseUVSet = uvSetNames[i];
			has_uvs = true;

			set_num++;
		}
	}

	cs.SetTexCoordSets( nif_uvs );

	//out << "===Exported UV Data===" << endl;

	//for ( unsigned int i = 0; i < nif_uvs.size(); ++i ) {
	//	out << "   UV Set:  " << nif_uvs[i].texType << endl;
	//	for ( unsigned int j = 0; j < nif_uvs[i].texCoords.size(); ++j ) {
	//		out << "      (" << nif_uvs[i].texCoords[j].u << ", " << nif_uvs[i].texCoords[j].u << ")" << endl;
	//	}
	//}

	//vector<Triangle> nif_tris;

	// this will hold references to the shaders used on the meshes
	MObjectArray Shaders;

	// this is used to hold indices to the materials returned in the object array
	MIntArray    FaceIndices;

	out << "Get the connected shaders" << endl;
	// get the shaders used by the i'th mesh instance
	// Assume this is not instanced for now
	// TODO support instanceing properly
	stat = visibleMeshFn.getConnectedShaders(0,Shaders,FaceIndices);

	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to get connected shader list.");
		
	}

	vector<ComplexFace> nif_faces;


	//Add shaders to propGroup array
	vector< vector<NiPropertyRef> > propGroups;
	for ( unsigned int shader_num = 0; shader_num < Shaders.length(); ++shader_num ) {

		//Maya sometimes lists shaders that are not actually attached to any face.  Disregard them.
		bool shader_is_used = false;
		for ( size_t f = 0; f < FaceIndices.length(); ++f ) {
			if ( FaceIndices[f] == shader_num ) {
				shader_is_used = true;
				break;
			}
		}

		if ( shader_is_used == false ) {
			//Shader isn't actually used, so continue to the next one.
			continue;
		}
		
		out << "Found attached shader:  ";
		//Attach all properties previously associated with this shader to
		//this NiTriShape
		MFnDependencyNode fnDep(Shaders[shader_num]);

		//Find the shader that this shading group connects to
		MPlug p = fnDep.findPlug("surfaceShader");
		MPlugArray plugs;
		p.connectedTo( plugs, true, false );
		for ( unsigned int i = 0; i < plugs.length(); ++i ) {
			if ( plugs[i].node().hasFn( MFn::kLambert ) ) {
				fnDep.setObject( plugs[i].node() );
				break;
			}
		}

		out << fnDep.name().asChar() << endl;
		vector<NiPropertyRef> niProps = shaders[ fnDep.name().asChar() ];

		propGroups.push_back( niProps );
	}
	cs.SetPropGroups( propGroups );
	
	out << "Export vertex and normal data" << endl;
	// attach an iterator to the mesh
	MItMeshPolygon itPoly(mesh, &stat);
	if ( stat != MS::kSuccess ) {
		throw runtime_error("Failed to create polygon iterator.");
	}

	// Create a list of faces with vertex IDs, and duplicate normals so they have the same ID
	for ( ; !itPoly.isDone() ; itPoly.next() ) {
		int poly_vert_count = itPoly.polygonVertexCount(&stat);

		if ( stat != MS::kSuccess ) {
			throw runtime_error("Failed to get vertex count.");
		}

		//Ignore polygons with less than 3 vertices
		if ( poly_vert_count < 3 ) {
			continue;
		}

		ComplexFace cf;

		//Assume all faces use material 0 for now
		cf.propGroupIndex = 0;
		
		for( int i = 0; i < poly_vert_count; ++i ) {
			ComplexPoint cp;

			cp.vertexIndex = itPoly.vertexIndex(i);
			cp.normalIndex = itPoly.normalIndex(i);
			if ( niColors.size() > 0 ) {
				int color_index;
				stat = meshFn.getFaceVertexColorIndex( itPoly.index(), i, color_index );
				if ( stat != MS::kSuccess ) {
					out << stat.errorString().asChar() << endl;
					throw runtime_error("Failed to get vertex color.");
				}
				cp.colorIndex = color_index;
			}

//#if MAYA_API_VERSION > 700
//			int color_index;
//			itPoly.getColorIndex(i, color_index );
//			
//#endif

			//Get the UV set names used by this particular vertex
			MStringArray vertUvSetNames;
			itPoly.getUVSetNames( vertUvSetNames );
			for ( unsigned int j = 0; j < vertUvSetNames.length(); ++j ) {
				TexCoordIndex tci;
				tci.texCoordSetIndex  = uvSetNums[ vertUvSetNames[j].asChar() ];
				int uv_index;
				itPoly.getUVIndex(i, uv_index, &vertUvSetNames[j] );
				tci.texCoordIndex  = uv_index;
				cp.texCoordIndices.push_back( tci );
			}
			cf.points.push_back(cp);
		}
		nif_faces.push_back( cf );
	}

	//Set shader/face association
	if ( nif_faces.size() != FaceIndices.length() ) {
		throw runtime_error("Num faces found do not match num faces reported.");
	}
	for ( unsigned int face_index = 0; face_index < nif_faces.size(); ++face_index ) {
		nif_faces[face_index].propGroupIndex = FaceIndices[face_index];
	}

	cs.SetFaces( nif_faces );

	//out << "===Exported Face Data===" << endl;
	//for ( unsigned int i = 0; i < nif_faces.size(); ++i ) {
	//	out << "Face " << i << endl
	//		<< "   propGroupIndex:  " << nif_faces[i].propGroupIndex << endl
	//		<< "   Points:" << endl;
	//	for ( unsigned int j = 0; j < nif_faces[i].points.size(); ++j ) {
	//		out << "      colorIndex:  "  << nif_faces[i].points[j].colorIndex << endl
	//			<< "      normalIndex:  "  << nif_faces[i].points[j].normalIndex << endl
	//			<< "      vertexIndex:  "  << nif_faces[i].points[j].vertexIndex << endl
	//			<< "      texCoordIndices:  "  << endl;
	//		for ( unsigned int k = 0; k < nif_faces[i].points[j].texCoordIndices.size(); ++k ) {
	//			out << "         texCoordIndex:  " <<  nif_faces[i].points[j].texCoordIndices[k].texCoordIndex << endl
	//				<< "         texCoordSetIndex:  " <<  nif_faces[i].points[j].texCoordIndices[k].texCoordSetIndex << endl;
	//		}
	//	}
	//}

	//--Skin Processing--//

	//Look up any skin clusters
	if ( meshClusters.find( visibleMeshFn.fullPathName().asChar() ) != meshClusters.end() ) {
		const vector<MObject> & clusters = meshClusters[ visibleMeshFn.fullPathName().asChar() ];
		//for ( vector<MObject>::const_iterator cluster = clusters.begin(); cluster != clusters.end(); ++cluster ) {
			//TODO:  Support shapes with more than one skin cluster affecting them
			if ( clusters.size() > 1 ) {
				throw runtime_error("Objects with multiple skin clusters affecting them are not currently supported.  Try deleting the history and re-binding them.");
			}

			vector<MObject>::const_iterator cluster = clusters.begin();
			if ( cluster->isNull() != true ) {	
				MFnSkinCluster clusterFn(*cluster);
			

				out << "Processing skin..." << endl;
				//Get path to visible mesh
				MDagPath meshPath;
				visibleMeshFn.getPath( meshPath );

				out << "Getting a list of all verticies in this mesh" << endl;
				//Get a list of all verticies in this mesh
				MFnSingleIndexedComponent compFn;
				MObject vertices = compFn.create( MFn::kMeshVertComponent );
				MItGeometry gIt(meshPath);
				MIntArray vertex_indices( gIt.count() );
				for ( int vert_index = 0; vert_index < gIt.count(); ++vert_index ) {
					vertex_indices[vert_index] = vert_index;
				}
				compFn.addElements(vertex_indices);

				out << "Getting Influences" << endl;
				//Get influences
				MDagPathArray myBones;
				clusterFn.influenceObjects( myBones, &stat );
				
				out << "Creating a list of NiNodeRefs of influences." << endl;
				//Create list of NiNodeRefs of influences
				vector<NiNodeRef> niBones( myBones.length() );
				for ( unsigned int bone_index = 0; bone_index < niBones.size(); ++bone_index ) {
					if ( nodes.find( myBones[bone_index].fullPathName().asChar() ) == nodes.end() ) {
						//There is a problem; one of the joints was not exported.  Abort.
						throw runtime_error("One of the joints necessary to export a bound skin was not exported.");
					}
					niBones[bone_index] = nodes[ myBones[bone_index].fullPathName().asChar() ];
				}

				out << "Getting weights from Maya" << endl;
				//Get weights from Maya
				MDoubleArray myWeights;
				unsigned int bone_count = myBones.length();
				stat = clusterFn.getWeights( meshPath, vertices, myWeights, bone_count );
				if ( stat != MS::kSuccess ) {
					out << stat.errorString().asChar() << endl;
					throw runtime_error("Failed to get vertex weights.");
				}

				out << "Setting skin influence list in ComplexShape" << endl;
				//Set skin information in ComplexShape
				cs.SetSkinInfluences( niBones );
				
				out << "Adding weights to ComplexShape vertices" << endl;
				//out << "Number of weights:  " << myWeights.length() << endl;
				//out << "Number of bones:  " << myBones.length() << endl;
				//out << "Number of Maya vertices:  " << gIt.count() << endl;
				//out << "Number of NIF vertices:  " << int(nif_vts.size()) << endl;
				unsigned int weight_index = 0;
				SkinInfluence sk;
				for ( unsigned int vert_index = 0; vert_index < nif_vts.size(); ++vert_index ) {
					for ( unsigned int bone_index = 0; bone_index < myBones.length(); ++bone_index ) {
						//out << "vert_index:  " << vert_index << "  bone_index:  " << bone_index << "  weight_index:  " << weight_index << endl;	
						// Only bother with weights that are significant
						if ( myWeights[weight_index] > 0.0f ) {
							sk.influenceIndex = bone_index;
							sk.weight = float(myWeights[weight_index]);
							
							nif_vts[vert_index].weights.push_back(sk);
						}
						++weight_index;
					}
				}
			}
	//}
}

	out << "Setting vertex info" << endl;
	//Set vertex info now that any skins have been processed
	cs.SetVertices( nif_vts );

	//ComplexShape is now complete, so split it

	//Get parent
	NiNodeRef parNode = GetDAGParent( dagNode );
	Matrix44 transform = Matrix44::IDENTITY;
	vector<NiNodeRef> influences = cs.GetSkinInfluences();
	if ( influences.size() > 0 ) {
		//This is a skin, so we use the common ancestor of all influences
		//as the parent
		vector<NiAVObjectRef> objects;
		for ( size_t i = 0; i < influences.size(); ++i ) {
			objects.push_back( StaticCast<NiAVObject>(influences[i]) );
		}

		//Get world transform of existing parent
		Matrix44 oldParWorld = parNode->GetWorldTransform();

		//Set new parent node
		parNode = FindCommonAncestor( objects );

		transform = oldParWorld * parNode->GetWorldTransform().Inverse();
	}

	//Get transform using temporary NiAVObject
	NiAVObjectRef tempAV = new NiAVObject;
	ExportAV(tempAV, dagNode );

	out << "Split ComplexShape from " << meshFn.name().asChar() << endl;
	NiAVObjectRef avObj = cs.Split( parNode, tempAV->GetLocalTransform() * transform, export_part_bones, export_tri_strips, export_tan_space );

	out << "Get the NiAVObject portion of the root of the split" <<endl;
	//Get the NiAVObject portion of the root of the split
	avObj->SetName( tempAV->GetName() );
	avObj->SetVisibility( tempAV->GetVisibility() );
	avObj->SetFlags( tempAV->GetFlags() );

	//If polygon mesh is hidden, hide tri_shape
	MPlug vis = visibleMeshFn.findPlug( MString("visibility") );
	bool visibility;
	vis.getValue(visibility);


	NiNodeRef splitRoot = DynamicCast<NiNode>(avObj);
	if ( splitRoot != NULL ) {
		//Root is a NiNode with NiTriBasedGeom children.
		vector<NiAVObjectRef> children = splitRoot->GetChildren();
		for ( unsigned c = 0; c < children.size(); ++c ) {
			//Set the default collision propogation flag to "use triangles"
			children[c]->SetFlags(2);
			// Make the mesh invisible if necessary
			if ( visibility == false ) {
				children[c]->SetVisibility(false);
			}

			//Search for Morrowind-Specific body part names in materials, if requested
			if( export_mor_rename ) {
				NiMaterialPropertyRef niMatProp = DynamicCast<NiMaterialProperty>( children[c]->GetPropertyByType(NiMaterialProperty::TYPE) );
				if ( niMatProp != NULL ) {
					string mat_name = niMatProp->GetName();
					if ( mat_name.find( "Neck" ) != string::npos ) {
						children[c]->SetName( "Tri Neck" );
					}
					if ( mat_name.find( "Chest" ) != string::npos ) {
						children[c]->SetName( "Tri Chest" );
					}
					if ( mat_name.find( "Groin" ) != string::npos ) {
						children[c]->SetName( "Tri Groin" );
					}
					if ( mat_name.find( "Tail" ) != string::npos ) {
						children[c]->SetName( "Tri Tail" );
					}
					if ( mat_name.find( "Right Upper Arm" ) != string::npos ) {
						children[c]->SetName( "Tri Right Upper Arm" );
					}
					if ( mat_name.find( "Left Upper Arm" ) != string::npos ) {
						children[c]->SetName( "Tri Left Upper Arm" );
					}
					if ( mat_name.find( "Right Forearm" ) != string::npos ) {
						children[c]->SetName( "Tri Right Forearm" );
					}
					if ( mat_name.find( "Left Forearm" ) != string::npos ) {
						children[c]->SetName( "Tri Left Forearm" );
					}
					if ( mat_name.find( "Right Wrist" ) != string::npos ) {
						children[c]->SetName( "Tri Right Wrist" );
					}
					if ( mat_name.find( "Left Wrist" ) != string::npos ) {
						children[c]->SetName( "Tri Left Wrist" );
					}
					if ( mat_name.find( "Right Hand" ) != string::npos ) {
						children[c]->SetName( "Tri Right Hand" );
					}
					if ( mat_name.find( "Left Hand" ) != string::npos ) {
						children[c]->SetName( "Tri Left Hand" );
					}
					if ( mat_name.find( "Right Upper Leg" ) != string::npos ) {
						children[c]->SetName( "Tri Right Upper Leg" );
					}
					if ( mat_name.find( "Left Upper Leg" ) != string::npos ) {
						children[c]->SetName( "Tri Left Upper Leg" );
					}
					if ( mat_name.find( "Right Knee" ) != string::npos ) {
						children[c]->SetName( "Tri Right Knee" );
					}
					if ( mat_name.find( "Left Knee" ) != string::npos ) {
						children[c]->SetName( "Tri Left Knee" );
					}
					if ( mat_name.find( "Right Ankle" ) != string::npos ) {
						children[c]->SetName( "Tri Right Ankle" );
					}
					if ( mat_name.find( "Left Ankle" ) != string::npos ) {
						children[c]->SetName( "Tri Left Ankle" );
					}
					if ( mat_name.find( "Right Foot" ) != string::npos ) {
						children[c]->SetName( "Tri Right Foot" );
					}
					if ( mat_name.find( "Left Foot" ) != string::npos ) {
						children[c]->SetName( "Tri Left Foot" );
					}
				}
			}
		} 

	} else {
		//Root must be a NiTriBasedGeom.  Make it invisible if necessary
		if ( visibility == false ) {
			avObj->SetVisibility(false);
		}
	}


	out << "}" << endl;
}

//--Itterate through all of the Maya DAG nodes, adding them to the tree--//
void NifTranslator::ExportDAGNodes() {

	out << "NifTranslator::ExportDAGNodes {" << endl
		<< "Creating DAG iterator..." << endl;
	//Create iterator to go through all DAG nodes depth first
	MItDag it(MItDag::kDepthFirst);

	out << "Looping through all DAG nodes..." << endl;
	for( ; !it.isDone(); it.next() ) {
		out << "Attaching function set for DAG node to the object." << endl;

		// attach a function set for a dag node to the
		// object. Rather than access data directly,
		// we access it via the function set.
		MFnDagNode nodeFn(it.item());

		out << "Object name is:  " << nodeFn.name().asChar() << endl;

		//Skip over Maya's default objects by name
		if ( 
			nodeFn.name().substring(0, 13) == "UniversalManip" ||
			nodeFn.name() == "groundPlane_transform" ||
			nodeFn.name() == "ViewCompass" ||
			nodeFn.name() == "Manipulator1" ||
			nodeFn.name() == "persp" ||
			nodeFn.name() == "top" ||
			nodeFn.name() == "front" ||
			nodeFn.name() == "side"
		) {
			continue;
		}

		// only want non-history items
		if( !nodeFn.isIntermediateObject() ) {
			out << "Object is not a history item" << endl;

			//Check if this is a transform node
			if ( it.item().hasFn(MFn::kTransform) ) {
				//This is a transform node, check if it is an IK joint or a shape
				out << "Object is a transform node." << endl;

				NiAVObjectRef avObj;

				bool tri_shape = false;
				bool bounding_box = false;
				bool intermediate = false;
				MObject matching_child;

				//Check to see what kind of node we should create
				for( int i = 0; i != nodeFn.childCount(); ++i ) {

					//out << "API Type:  " << nodeFn.child(i).apiTypeStr() << endl;
					// get a handle to the child
					if ( nodeFn.child(i).hasFn(MFn::kMesh) ) {
						MFnMesh meshFn( nodeFn.child(i) );
						//history items don't count
						if ( !meshFn.isIntermediateObject() ) {
							out << "Object is a mesh." << endl;
							tri_shape = true;
							matching_child = nodeFn.child(i);
							break;
						} else {
							//This has an intermediate mesh under it.  Don't export it at all.
							intermediate = true;
							break;
						}

					} else if ( nodeFn.child(i).hasFn( MFn::kBox ) ) {
						out << "Found Bounding Box" << endl;

						//Set bounding box info
						BoundingBox bb;

						Matrix44 niMat= MatrixM2N( nodeFn.transformationMatrix() );
		
						bb.translation = niMat.GetTranslation();
						bb.rotation = niMat.GetRotation();
						bb.unknownInt = 1;
						
						//Get size of box
						MFnDagNode dagFn( nodeFn.child(i) );
						dagFn.findPlug("sizeX").getValue( bb.radius.x );
						dagFn.findPlug("sizeY").getValue( bb.radius.y );
						dagFn.findPlug("sizeZ").getValue( bb.radius.z );

						//Find the parent NiNode, if any
						if ( nodeFn.parentCount() > 0 ) {
							MFnDagNode parFn( nodeFn.parent(0) );
							if ( nodes.find( parFn.fullPathName().asChar() ) != nodes.end() ) {
								NiNodeRef parNode = nodes[parFn.fullPathName().asChar()];
								parNode->SetBoundingBox(bb);
							}
						}

						bounding_box = true;
					}
				}

				if ( !intermediate ) {
	
					if ( tri_shape == true ) {
						out << "Adding Mesh to list to be exported later..." << endl;
						meshes.push_back( it.item() );
						//NiTriShape
					} else if ( bounding_box == true ) {
						//Do nothing
					} else {
						out << "Creating a NiNode..." << endl;
						//NiNode
						NiNodeRef niNode = new NiNode;
						ExportAV( StaticCast<NiAVObject>(niNode), it.item() );
						
						out << "Associating NiNode with node DagPath..." << endl;
						//Associate NIF object with node DagPath
						string path = nodeFn.fullPathName().asChar();
						nodes[path] = niNode;

						//Parent should have already been created since we used a
						//depth first iterator in ExportDAGNodes
						NiNodeRef parNode = GetDAGParent( it.item() );
						parNode->AddChild( StaticCast<NiAVObject>(niNode) );
					}
				}
			}
		}
	}
	out << "Loop complete" << endl;


						////Is this a joint and therefore has bind pose info?
					//if ( it.item().hasFn(MFn::kJoint) ) {
					//}
				/*}*/
		//}

		//// get the name of the node
		//MString name = meshFn.name();

		//// write the node type found
		//out << "node: " << name.asChar() << endl;

		//// write the info about the children
		//out <<"num_kids " << meshFn.childCount() << endl;

		//for(int i=0;i<meshFn.childCount();++i) {
//			// get the MObject for the i'th child
		//MObject child = meshFn.child(i);

		//// attach a function set to it
		//MFnDagNode fnChild(child);

		//// write the child name
		//out << "\t" << fnChild.name().asChar();
		//out << endl;


	out << "}" << endl;
}

void NifTranslator::ExportAV( NiAVObjectRef avObj, MObject dagNode ) {
	// attach a function set for a dag node to the object.
	MFnDagNode nodeFn(dagNode);

	out << "Fixing name from " << nodeFn.name().asChar() << " to ";

	//Fix name
	string name = MakeNifName( nodeFn.name() );
	avObj->SetName( name );
	out << name << endl;

	MMatrix my_trans = nodeFn.transformationMatrix();
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

	//Set default collision propogation type of "Use triangles"
	avObj->SetFlags(10);

	//Set visibility
	MPlug vis = nodeFn.findPlug( MString("visibility") );
	bool value;
	vis.getValue(value);
	out << "Visibility of " << nodeFn.name().asChar() << " is " << value << endl;
	if ( value == false ) {
		avObj->SetVisibility(false);
	}


	out << "Copying Maya matrix to Niflib matrix" << endl;
	//Copy Maya matrix to Niflib matrix
	Matrix44 ni_trans( 
		(float)my_trans[0][0], (float)my_trans[0][1], (float)my_trans[0][2], (float)my_trans[0][3],
		(float)my_trans[1][0], (float)my_trans[1][1], (float)my_trans[1][2], (float)my_trans[1][3],
		(float)my_trans[2][0], (float)my_trans[2][1], (float)my_trans[2][2], (float)my_trans[2][3],
		(float)my_trans[3][0], (float)my_trans[3][1], (float)my_trans[3][2], (float)my_trans[3][3]
	);

	out << "Storing local transform values..." << endl;
	//Store Transform Values
	avObj->SetLocalTransform( ni_trans );
}

NiNodeRef NifTranslator::GetDAGParent( MObject dagNode ) {
	// attach a function set for a dag node to the object.
	MFnDagNode nodeFn(dagNode);

	out << "Looping through all Maya parents." << endl;
	for( unsigned int i = 0; i < nodeFn.parentCount(); ++i ) {
		out << "Get the MObject for the i'th parent and attach a fnction set to it." << endl;
		// get the MObject for the i'th parent
		MObject parent = nodeFn.parent(i);

		// attach a function set to it
		MFnDagNode parentFn(parent);

		out << "Get Parent Path." << endl;
		string par_path = parentFn.fullPathName().asChar();

		out << "Check if parent exists in the map we've built." << endl;

		//Check if parent exists in map we've built
		if ( nodes.find( par_path ) != nodes.end() ) {
			out << "Parent found." << endl;
			//Object found
			return nodes[par_path];
		} else {
			//Block was created, parent to scene root
			return sceneRoot;
		}
	}

	//Block was created, parent to scene root
	return sceneRoot;
}

void NifTranslator::GetColor( MFnDependencyNode& fn, MString name, MColor & color, MObject & texture ) {
	out << "NifTranslator::GetColor( " << fn.name().asChar() << ", " << name.asChar() << ", (" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << "), " << texture.apiType() << " ) {" << endl;
	MPlug p;
	texture = MObject();
	color = MColor();

	out << "Get a plug to the attribute" << endl;
	// get a plug to the attribute
	p = fn.findPlug( name + "R" );
	p.getValue(color.r);
	p = fn.findPlug( name + "G" );
	p.getValue(color.g);
	p = fn.findPlug( name + "B" );
	p.getValue(color.b);
	p = fn.findPlug( name + "A" );
	p.getValue(color.a);
	p = fn.findPlug(name);

	out << "Get plugs connected to color attribute" << endl;
	// get plugs connected to color attribute
	MPlugArray plugs;
	p.connectedTo(plugs,true,false);

	out << "See if any file textures are present" << endl;
	// see if any file textures are present
	for( int i = 0; i != plugs.length(); ++i ) {

  		// if file texture found
		if( plugs[i].node().apiType() == MFn::kFileTexture ) {
			out << "File texture found" << endl;
			texture = plugs[i].node();

			// stop looping
			break;
		}
	}

	out << "Special processing for base color?" << endl;
	if( name == "color" && color.r < 0.01 && color.g < 0.01 && color.b < 0.01) {
  		color.r = color.g = color.b = 0.6f;
	}

	// output the name, color and texture ID
	out << "\t" << name.asChar() << ":  ("
		<< color.r << ", "
		<< color.g << ", "
		<< color.b << ", "
		<< color.a << ")" << endl;

	out << "}" << endl;
} 

//--Iterate through all shaders, creating property sets for each one--//
void NifTranslator::ExportShaders() {
	out << "NifTranslator::ExportShaders {" << endl;
	// iterate through the shaders in the scene
	MItDependencyNodes itDep(MFn::kLambert);

	out << "Iterating through all shaders in scehe..." << endl;
	// we have to keep iterating until we get through
	// all of the nodes in the scene
	//
	for ( ; !itDep.isDone(); itDep.next() ) {

		MColor diffuse, ambient, emissive, transparency, specular(0.0, 0.0, 0.0, 0.0);
		string mat_name;
		float glossiness = 20.0f;
		bool use_alpha = false;
		bool use_spec = false;
		bool base_texture = false;
		bool multi_texture = false;
		MObject diff_tex, ambi_tex, emis_tex, glos_tex, spec_tex, tran_tex;

		out << "Testing for MFnLambertShader function set." << endl;
		if ( itDep.item().hasFn( MFn::kLambert ) ) {
			out << "Attaching MFnLambertShader function set." << endl;
			//All shaders inherit from lambert
			MFnLambertShader lambertFn( itDep.item() );

			//Get name
			mat_name = MakeNifName( lambertFn.name() );

			//--Get info from Color slots--//
			out << "Getting color slot info..." << endl;
			GetColor( lambertFn, "color", diffuse, diff_tex );
			GetColor( lambertFn,"ambientColor", ambient, ambi_tex );
			GetColor( lambertFn,"incandescence", emissive, emis_tex );
			GetColor( lambertFn,"transparency", transparency, tran_tex );

			//Shader may also have specular color
			if ( itDep.item().hasFn( MFn::kReflect ) ) {
				out << "Attaching MFnReflectShader function set" << endl;
				MFnReflectShader reflectFn( itDep.item() );

				out << "Getting specular color" << endl;
				GetColor( reflectFn, "specularColor", specular, spec_tex );

				if ( specular.r != 0.0 && specular.g != 0.0 && specular.b != 0.0 ) {
					use_spec = true;
				}

				//Handle cosine power/glossiness
				if ( itDep.item().hasFn( MFn::kPhong ) ) {
					out << "Attaching MFnReflectShader function set" << endl;
					MFnPhongShader phongFn( itDep.item() );

					out << "Getting cosine power" << endl;
					MPlug p = phongFn.findPlug("cosinePower");
					glossiness = phongFn.cosPower();

					out << "Get plugs connected to cosinePower attribute" << endl;
					// get plugs connected to cosinePower attribute
					MPlugArray plugs;
					p.connectedTo(plugs,true,false);

					out << "See if any file textures are present" << endl;
					// see if any file textures are present
					for( int i = 0; i != plugs.length(); ++i ) {

  						// if file texture found
						if( plugs[i].node().apiType() == MFn::kFileTexture ) {
							out << "File texture found" << endl;
							glos_tex = plugs[i].node();

							// stop looping
							break;
						}
					}

				} else {
					//Not a phong texture so not currently supported
					MGlobal::displayWarning("Only Shaders with Cosine Power, such as Phong, can currently be used to set exported glossiness.  Default value of 20.0 used.");
					//TODO:  Figure out how to guestimate glossiness from Blin shaders.
				}
			} else {
				//No reflecting shader used, so set some defautls
			}
		}

		out << "Checking whether base texture is used and if it has alpha" << endl;
		//Check whether base texture is used and if it has has alpha
		if ( diff_tex.isNull() == false ) {
			base_texture = true;

			if ( diff_tex.hasFn( MFn::kDependencyNode ) ) {
				MFnDependencyNode depFn(diff_tex);
				MPlug fha = depFn.findPlug("fileHasAlpha");
				bool value;
				fha.getValue( value );
				if ( value == true ) {
					use_alpha = true;
				}
			}
		}

		out << "Checking whether any other supported texture types are being used" << endl;
		//Check whether any other supported texture types are being used
		if ( emis_tex.isNull() == false ) {
			multi_texture = true;
		}

		//Create a NIF Material Wrapper and put the collected values into it
		unsigned int mat_index = mtCollection.CreateMaterial( true, base_texture, multi_texture, use_spec, use_alpha, export_version );
		MaterialWrapper mw = mtCollection.GetMaterial( mat_index );

		NiMaterialPropertyRef niMatProp = mw.GetColorInfo();
		
		if ( use_alpha ) {
			NiAlphaPropertyRef niAlphaProp = mw.GetTranslucencyInfo();
			niAlphaProp->SetFlags(237);
		}

		if ( use_spec ) {
			NiSpecularPropertyRef niSpecProp = mw.GetSpecularInfo();
			niSpecProp->SetSpecularState(true);
		}
		
		//Set Name
		niMatProp->SetName( mat_name );

		//--Diffuse Color--//

		if ( diff_tex.isNull() == true ) {
			//No Texture
			niMatProp->SetDiffuseColor( Color3( diffuse.r, diffuse.g, diffuse.b ) );
		} else {
			//Has Texture, so set diffuse color to white
			niMatProp->SetDiffuseColor( Color3(1.0f, 1.0f, 1.0f) );

			//Attach texture
			if ( diff_tex.hasFn( MFn::kDependencyNode ) ) {
				MFnDependencyNode depFn(diff_tex);
				string texname = depFn.name().asChar();
				out << "Base texture found:  " << texname << endl;
				
				if ( textures.find( texname ) != textures.end() ) {
					mw.SetTextureIndex( BASE_MAP, textures[texname] );
				}
			}

		}

		//--Ambient Color--//	

		if ( ambi_tex.isNull() == true ) {
			//No Texture.  Check if we should expot white ambient
			if ( diff_tex.isNull() == false && export_white_ambient == true ) {
				niMatProp->SetAmbientColor( Color3(1.0f, 1.0f, 1.0f) ); // white
			} else {
				niMatProp->SetAmbientColor( Color3( ambient.r, ambient.g, ambient.b ) );
			}
		} else {
			//Ambient texture found.  This is not supported so set ambient to white and warn user.
			niMatProp->SetAmbientColor( Color3(1.0f, 1.0f, 1.0f) ); // white
			MGlobal::displayWarning("Ambient textures are not supported by the NIF format.  Ignored.");
		}

		//--Emmissive Color--//

		if ( emis_tex.isNull() == true ) {
			//No Texture
			niMatProp->SetEmissiveColor( Color3( emissive.r, emissive.g, emissive.b ) );
		} else {
			//Has texture, so set emissive color to white
			niMatProp->SetEmissiveColor( Color3(1.0f, 1.0f, 1.0f) ); //white

			//Attach texture
			if ( emis_tex.hasFn( MFn::kDependencyNode ) ) {
				MFnDependencyNode depFn(emis_tex);
				string texname = depFn.name().asChar();
				out << "Glow texture found:  " << texname << endl;
				
				if ( textures.find( texname ) != textures.end() ) {
					mw.SetTextureIndex( GLOW_MAP, textures[texname] );
				}
			}
		}

		//--Transparency--//

		//Any alpha textures are probably set because the base texture has
		//alpha which is connected to this node.  So just set the
		//transparency and don't worry about whethere there's a
		//texture connected or not.
		float trans = (transparency.r + transparency.g + transparency.b) / 3.0f;
		if ( trans != transparency.r ) {
			MGlobal::displayWarning("Colored transparency is not supported by the NIF format.  An average of the color channels will be used.");
		}
		//Maya trans is reverse of NIF trans
		trans = 1.0f - trans;
		niMatProp->SetTransparency( trans );

		//--Specular Color--//

		if ( spec_tex.isNull() == true ) {
			//No Texture.  Check if we should expot black specular
			if ( use_spec == false ) {
				niMatProp->SetSpecularColor( Color3(0.0f, 0.0f, 0.0f) ); // black
			} else {
				niMatProp->SetSpecularColor( Color3( specular.r, specular.g, specular.b ) );
			}
		} else {
			//Specular texture found.  This is not supported so set specular to black and warn user.
			niMatProp->SetSpecularColor( Color3(0.0f, 0.0f, 0.0f) ); // black
			MGlobal::displayWarning("Gloss textures are not supported yet.  Ignored.");
		}

		//--Glossiness--//

		niMatProp->SetGlossiness( glossiness );

		if ( glos_tex.isNull() == false ) {
			//Cosine Power texture found.  This is not supported so warn user.
			MGlobal::displayWarning("Gloss textures are not supported yet.  Ignored.");
		}

		//TODO: Support bump maps, environment maps, gloss maps, and multi-texturing (detail, dark, and decal?)

		out << "Associating material index with Maya shader name";
		//Associate material index with Maya shader name
		MFnDependencyNode depFn( itDep.item() );
		out << depFn.name().asChar() << endl;
		shaders[ depFn.name().asChar() ] = mw.GetProperties();
	}

	out << "}" << endl;
}

//--Iterate through all file textures, creating NiSourceTexture blocks for each one--//
void NifTranslator::ExportFileTextures() {
	// create an iterator to go through all file textures
	MItDependencyNodes it(MFn::kFileTexture);

	//iterate through all textures
	while(!it.isDone())
	{
		// attach a dependency node to the file node
		MFnDependencyNode fn(it.item());

		// get the attribute for the full texture path and size
		MPlug ftn = fn.findPlug("fileTextureName");
		MPlug osx = fn.findPlug("outSizeX");
		MPlug osy = fn.findPlug("outSizeY");

		// get the filename from the attribute
		MString filename;
		ftn.getValue(filename);
		
		//Get image size
		float x, y;
		osx.getValue(x);
		osy.getValue(y);

		//Determine whether the dimentions of the texture are powers of two
		if ( ((int(x) & (int(x) - 1)) != 0 ) || ((int(y) & (int(y) - 1)) != 0 ) ) {

			//Print the value for now:
			stringstream ss;

			ss << "File texture " << filename.asChar() << " has dimentions that are not powers of 2.  Non-power of 2 dimentions will not work in most games.";
			MGlobal::displayWarning( ss.str().c_str() );
		}

		//Create a NIF texture wrapper
		unsigned int tex_index = mtCollection.CreateTexture( export_version );
		TextureWrapper tw = mtCollection.GetTexture( tex_index );

		//Get Node Name
		tw.SetObjectName( MakeNifName( fn.name() ) );

		MString fname;
		ftn.getValue(fname);
		string fileName = fname.asChar();

		//Replace back slash with forward slash to match with paths
		for ( unsigned i = 0; i < fileName.size(); ++i ) {
			if ( fileName[i] == '\\' ) {
				fileName[i] = '/';
			}
		}

		out << "Full Texture Path:  " << fname.asChar() << endl;

		//Figure fixed file name
		string::size_type index;
		MStringArray paths;
		MString(texture_path.c_str()).split( '|', paths );
		switch( tex_path_mode ) {
			case PATH_MODE_AUTO:
				for ( unsigned p = 0; p < paths.length(); ++p ) {
					unsigned len = paths[p].length();
					if ( len >= fileName.size() ) {
						continue;
					}
					if ( fileName.substr( 0, len ) == paths[p].asChar() ) {
						//Found a matching path, cut matching part out
						fileName = fileName.substr( len + 1 );
						break;
					}
				}
				break;
			case PATH_MODE_PREFIX:
			case PATH_MODE_NAME:
				index = fileName.rfind( "/" );
				if ( index != string::npos ) { 
					//We don't want the slash itself
					if ( index + 1 < fileName.size() ) {
						fileName = fileName.substr( index + 1 );
					}
				}
				if ( tex_path_mode == PATH_MODE_NAME ) {
					break;
				}
				//Now we're doing the prefix case
				fileName = tex_path_prefix + fileName;
				break;
				
			//Do nothing for full path since the full path is already
			//set in file name
			case PATH_MODE_FULL: break;
		}

		//Now make all slashes back slashes since some games require this
		for ( unsigned i = 0; i < fileName.size(); ++i ) {
			if ( fileName[i] == '/' ) {
				fileName[i] = '\\';
			}
		}

		out << "File Name:  " << fileName << endl;

		tw.SetExternalTexture( fileName );

		//Associate NIF texture index with fileTexture DagPath
		string path = fn.name().asChar();
		textures[path] = tex_index;

		// get next fileTexture
		it.next();
	} 
}

void NifTranslator::ParseOptionString( const MString & optionsString ) {
	//Parse Options String
	MStringArray options;
	//out << "optionsString:" << endl;
	//out << optionsString.asChar() << endl;
	optionsString.split( ';', options );
	for (unsigned int i = 0; i < options.length(); ++i) {
		MStringArray tokens;
		options[i].split( '=', tokens );
		//out << "tokens[0]:  " << tokens[0].asChar() << endl;
		//out << "tokens[1]:  " << tokens[1].asChar() << endl;
		if ( tokens[0] == "texturePath" ) {
			texture_path = tokens[1].asChar();

			//Replace back slash with forward slash
			for ( unsigned i = 0; i < texture_path.size(); ++i ) {
				if ( texture_path[i] == '\\' ) {
					texture_path[i] = '/';
				}
			}

			out << "Texture Path:  " << texture_path << endl;
			
		}
		if ( tokens[0] == "exportVersion" ) {
			export_version = ParseVersionString( tokens[1].asChar() );

			if ( export_version == VER_INVALID ) {
				MGlobal::displayWarning( "Invalid export version specified.  Using default of 4.0.0.2." );
				export_version = VER_4_0_0_2;
			}
			out << "Export Version:  0x" << hex << export_version << dec << endl;
		}
		if ( tokens[0] == "importBindPose" ) {
			if ( tokens[1] == "1" ) {
				import_bind_pose = true;
			} else {
				import_bind_pose = false;
			}
			out << "Import Bind Pose:  " << import_bind_pose << endl;
		}
		if ( tokens[0] == "importNormals" ) {
			if ( tokens[1] == "1" ) {
				import_normals = true;
			} else {
				import_normals = false;
			}
			out << "Import Normals:  " << import_normals << endl;
		}
		if ( tokens[0] == "importNoAmbient" ) {
			if ( tokens[1] == "1" ) {
				import_no_ambient = true;
			} else {
				import_no_ambient = false;
			}
			out << "Import No Ambient:  " << import_no_ambient << endl;
		}
		if ( tokens[0] == "exportWhiteAmbient" ) {
			if ( tokens[1] == "1" ) {
				export_white_ambient = true;
			} else {
				export_white_ambient = false;
			}
			out << "Export White Ambient:  " << export_white_ambient << endl;
		}
		if ( tokens[0] == "exportTriStrips" ) {
			if ( tokens[1] == "1" ) {
				export_tri_strips = true;
			} else {
				export_tri_strips = false;
			}
			out << "Export Triangle Strips:  " << export_tri_strips << endl;
		}
		if ( tokens[0] == "exportTanSpace" ) {
			if ( tokens[1] == "1" ) {
				export_tan_space = true;
			} else {
				export_tan_space = false;
			}
			out << "Export Tangent Space:  " << export_tan_space << endl;
		}
		if ( tokens[0] == "exportMorRename" ) {
			if ( tokens[1] == "1" ) {
				export_mor_rename = true;
			} else {
				export_mor_rename = false;
			}
			out << "Export Morrowind Rename:  " << export_mor_rename << endl;
		}
		if ( tokens[0] == "importSkelComb" ) {
			if ( tokens[1] == "1" ) {
				import_comb_skel = true;
			} else {
				import_comb_skel = false;
			}
			out << "Combine with Existing Skeleton:  " << import_comb_skel << endl;
		}
		if ( tokens[0] == "exportPartBones" ) {
			export_part_bones = atoi( tokens[1].asChar() );
			out << "Max Bones per Skin Partition:  " << export_part_bones << endl;
		}
		if ( tokens[0] == "exportUserVersion" ) {
			export_user_version = atoi( tokens[1].asChar() );
			out << "User Version:  " << export_user_version << endl;
		}
		if ( tokens[0] == "importJointMatch" ) {
			joint_match = tokens[1].asChar();
			out << "Import Joint Match:  " << joint_match << endl;
		}
		if ( tokens[0] == "useNameMangling" ) {
			if ( tokens[1] == "1" ) {
				use_name_mangling = true;
			} else {
				use_name_mangling = false;
			}
			out << "Use Name Mangling:  " << use_name_mangling << endl;
		}
		if ( tokens[0] == "exportPathMode" ) {
			out << " Export Texture Path Mode:  ";
			if ( tokens[1] == "Name" ) {
				tex_path_mode = PATH_MODE_NAME;
				out << "Name" << endl;
			} else if ( tokens[1] == "Full" ) {
				tex_path_mode = PATH_MODE_FULL;
				out << "Full" << endl;
			} else if ( tokens[1] == "Prefix" ) {
				tex_path_mode = PATH_MODE_PREFIX;
				out << "Prefix" << endl;
			} else {
				//Auto is default
				tex_path_mode = PATH_MODE_AUTO;
				out << "Auto" << endl;
			}
		}

		if ( tokens[0] == "exportPathPrefix" ) {
			tex_path_prefix = tokens[1].asChar();
			out << "Export Texture Path Prefix:  " << tex_path_prefix << endl;
		}
	}
}

void NifTranslator::EnumerateSkinClusters() {

	//Iterate through all skin clusters in the scene
	MItDependencyNodes clusterIt( MFn::kSkinClusterFilter );
	for ( ; !clusterIt.isDone(); clusterIt.next() ) {
		MObject clusterObj = clusterIt.item();

		//Attach function set
		MFnSkinCluster clusterFn(clusterObj);

		//Add an assotiation for each mesh that this skin cluster is
		//connected to
		MObjectArray shapes;
		clusterFn.getOutputGeometry( shapes );

		for ( unsigned int i = 0; i < shapes.length(); i++ ) {
			if ( shapes[i].hasFn( MFn::kMesh ) ) {
				MFnMesh meshFn( shapes[i] );

				meshClusters[ meshFn.fullPathName().asChar() ].push_back( clusterObj );

				out << "Cluster:  "  << clusterFn.name().asChar() << "  Mesh:  " << meshFn.fullPathName().asChar() <<endl;
			}
		}
	} 
}

MObject NifTranslator::GetExistingJoint( const string & name ) {
	//If it's not in the list, return a null object
	map<string,MDagPath>::iterator it = existingNodes.find( name );
	if ( it == existingNodes.end() ) {
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

MObject NifTranslator::MakeJoint( MObject & jointObj ) {

	MStatus stat;

	//Check if this is an Ik joint
	if ( jointObj.hasFn( MFn::kJoint ) ) {
		out << "Matching joint is already an IK joint." << endl;
		//It's a joint already, return it
		return jointObj;
	}

	out << "Matching joint object must be changed to an IK joint." << endl;
	//The object is not a joint, so make it into one.
	MFnTransform transFn(jointObj);
	
	//Temporary data
	MTransformationMatrix transMat;
	MObjectArray children;
	MObjectArray parents;

	//Copy name
	MString name = transFn.name();

	//Copy transform
	transMat = transFn.transformation();

	//Copy children
	for ( unsigned int i = 0; i < transFn.childCount(); ++i ) {
		children.append( transFn.child(i) );
	}

	//Copy additional parents
	for ( unsigned int i = 0; i < transFn.parentCount(); ++i ) {
		parents.append( transFn.parent(i) );

	}



	//Create new object
	MFnIkJoint jointFn;
	jointFn.create( MObject::kNullObj, &stat );
	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to create joint.");
	}



	//Set transform
	stat = jointFn.set( transMat );
	if ( stat != MS::kSuccess ) {
			out << stat.errorString().asChar() << endl;
			throw runtime_error("Failed to set transform.");
		}

	//Set children
	for ( unsigned int i = 0; i < children.length(); ++i ) {
		MFnDagNode nodeFn( children[i] );
		out << "Attaching " << nodeFn.name().asChar() << " as child of " << name.asChar() << endl;
		stat = jointFn.addChild( children[i], MFnDagNode::kNextPos, true );
		if ( stat != MS::kSuccess ) {
			out << stat.errorString().asChar() << endl;
			throw runtime_error("Failed to attach child.");
		}
	}

	//Set additional parents
	for ( unsigned int i = 0; i < parents.length(); ++i ) {
		if ( parents[i].hasFn( MFn::kDagNode ) ) {
			MFnDagNode nodeFn( parents[i] );
			out << "Attaching " << name.asChar() << " to " << nodeFn.name().asChar() << endl;
			stat = nodeFn.addChild( jointFn.object() );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to attach to parent.");
			}
		}
	}

			//Destroy original object
	MString command = "delete " + transFn.fullPathName();
	MGlobal::executeCommand( command );

	//Set the name of the new object
	jointFn.setName( name, &stat );
	if ( stat != MS::kSuccess ) {
		out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to set name.");
	}

	//Return new joint
	return jointFn.object();
}

MString NifTranslator::MakeMayaName( const string & nifName ) {
	stringstream newName;
	newName.fill('0');

	for ( unsigned int i = 0; i < nifName.size(); ++i ) {
		if ( use_name_mangling ) {
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

string NifTranslator::MakeNifName( const MString & mayaName ) {
	stringstream newName;
	stringstream temp;
	temp.setf ( ios_base::hex, ios_base::basefield );  // set hex as the basefield

	string str = mayaName.asChar();

	if ( use_name_mangling ) {
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

MMatrix MatrixN2M( const Matrix44 & n ) {
	//Copy Niflib matrix to Maya matrix

	float myMat[4][4] = { 
		n[0][0], n[0][1], n[0][2], n[0][3],
		n[1][0], n[1][1], n[1][2], n[1][3],
		n[2][0], n[2][1], n[2][2], n[2][3],
		n[3][0], n[3][1], n[3][2], n[3][3]
	};

	return MMatrix(myMat);
}

Matrix44 MatrixM2N( const MMatrix & n ) {
	//Copy Maya matrix to Niflib matrix
	return Matrix44( 
		(float)n[0][0], (float)n[0][1], (float)n[0][2], (float)n[0][3],
		(float)n[1][0], (float)n[1][1], (float)n[1][2], (float)n[1][3],
		(float)n[2][0], (float)n[2][1], (float)n[2][2], (float)n[2][3],
		(float)n[3][0], (float)n[3][1], (float)n[3][2], (float)n[3][3]
	);
}

MObject NifTranslator::ImportMaterial( MaterialWrapper & mw ) {

	//Create the material but don't connect it to parent yet
	MFnPhongShader phongFn;
	MObject obj = phongFn.create();
	Color3 color;

	NiMaterialPropertyRef niMatProp = mw.GetColorInfo();

	//See if the user wants the ambient color imported
	if ( !import_no_ambient ) {
		color = niMatProp->GetAmbientColor();
		phongFn.setAmbientColor( MColor(color.r, color.g, color.b) );
	}

	color = niMatProp->GetDiffuseColor();
	phongFn.setColor( MColor(color.r, color.g, color.b) );

	
	//Set Specular color to 0 unless the mesh has a NiSpecularProperty
	NiSpecularPropertyRef niSpecProp = mw.GetSpecularInfo();

	if ( niSpecProp != NULL && (niSpecProp->GetFlags() & 1) == true ) {
		//This mesh uses specular color - load it
		color = niMatProp->GetSpecularColor();
		phongFn.setSpecularColor( MColor(color.r, color.g, color.b) );
	} else {
		phongFn.setSpecularColor( MColor( 0.0f, 0.0f, 0.0f) );
	}

	color = niMatProp->GetEmissiveColor();
	phongFn.setIncandescence( MColor(color.r, color.g, color.b) );

	if ( color.r > 0.0 || color.g > 0.0 || color.b > 0.0) {
		phongFn.setGlowIntensity( 0.25 );
	}

	float glossiness = niMatProp->GetGlossiness();
	phongFn.setCosPower( glossiness );

	float alpha = niMatProp->GetTransparency();
	//Maya's concept of alpha is the reverse of the NIF's concept
	alpha = 1.0f - alpha;
	phongFn.setTransparency( MColor( alpha, alpha, alpha, alpha) );

	MString name = MakeMayaName( niMatProp->GetName() );
	phongFn.setName( name );

	return obj;
}

MObject NifTranslator::ImportTexture( TextureWrapper & tw ) {
	MObject obj;

	string file_name = tw.GetTextureFileName();

	//Warn the user that internal textures are not supported
	if ( tw.IsTextureExternal() == false ) {
		MGlobal::displayWarning( "This NIF file contains an internaly stored texture.  This is not currently supported." );
		return MObject::kNullObj;
	}

	//create a texture node
	MFnDependencyNode nodeFn;
	obj = nodeFn.create( MString("file"), MString( file_name.c_str() ) );

	//--Search for the texture file--//
	
	//Replace back slash with forward slash
	unsigned last_slash = 0;
	for ( unsigned i = 0; i < file_name.size(); ++i ) {
		if ( file_name[i] == '\\' ) {
			file_name[i] = '/';
		}
	}

	//MString fName = file_name.c_str();

	MFileObject mFile;
	mFile.setName( MString(file_name.c_str()) );

	MString found_file = "";

	//Check if the file exists in the current working directory
	mFile.setRawPath( importFile.rawPath() );
	out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
	if ( mFile.exists() ) {
		found_file =  mFile.fullName();
	} else {
		out << "File Not Found." << endl;
	}

	if ( found_file == "" ) {
		//Check if the file exists in any of the given search paths
		MStringArray paths;
		MString(texture_path.c_str()).split( '|', paths );

		for ( unsigned i = 0; i < paths.length(); ++i ) {
			if ( paths[i].substring(0,0) == "." ) {
				//Relative path
				mFile.setRawPath( importFile.rawPath() + paths[i] );
			} else {
				//Absolute path
				mFile.setRawPath( paths[i] );

			}
			
			out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
			if ( mFile.exists() ) {
				//File exists at path entry i
				found_file = mFile.fullName();
				break;
			} else {
				out << "File Not Found." << endl;
			}

			////Maybe it's a relative path
			//mFile.setRawPath( importFile.rawPath() + paths[i] );
			//				out << "Looking for file:  " << mFile.rawPath().asChar() << " + " << mFile.name().asChar() << endl;
			//if ( mFile.exists() ) {
			//	//File exists at path entry i
			//	found_file = mFile.fullName();
			//	break;
			//} else {
			//	out << "File Not Found." << endl;
			//}
		}
	}

	if ( found_file == "" ) {
		//None of the searches found the file... just use the original value
		//from the NIF
		found_file = file_name.c_str();
	}

	nodeFn.findPlug( MString("ftn") ).setValue(found_file);

	//Get global texture list
	MItDependencyNodes nodeIt( MFn::kTextureList );
	MObject texture_list = nodeIt.item();
	MFnDependencyNode slFn;
	slFn.setObject( texture_list );

	MPlug textures = slFn.findPlug( MString("textures") );

	// Loop until an available connection is found
	MPlug element;
	int next = 0;
	while( true )
	{
		element = textures.elementByLogicalIndex( next );

		// If this plug isn't connected, break and use it below.
		if ( element.isConnected() == false )
			break;

		next++;
	}

	MPlug message = nodeFn.findPlug( MString("message") );

	// Connect '.message' plug of render node to "shaders"/"textures" plug of default*List
	MDGModifier dgModifier;
	dgModifier.connect( message, element );

	dgModifier.doIt();

	return obj;
}

void NifTranslator::ConnectShader( const vector<NiPropertyRef> & properties, MDagPath meshPath, MSelectionList sel_list ) {
	//--Look for Materials--//
	MObject grpOb;
	
	out << "Looking for previously imported shaders..." << endl;

	unsigned int mat_index = mtCollection.GetMaterialIndex( properties );

	if ( mat_index == NO_MATERIAL ) {
		//No material to connect
		return;
	}

	//Look up Maya Shader
	if ( importedMaterials.find( mat_index ) == importedMaterials.end() ) {
		throw runtime_error("The material was previously imported, but does not appear in the list.  This should not happen.");
	}

	MObject matOb = importedMaterials[mat_index];

	if ( matOb == MObject::kNullObj ) {
		//No material to connect
		return;
	}

	//Connect the shader
	out << "Connecting shader..." << endl;

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
		
	out << "Looking for previously imported textures..." << endl;

	MaterialWrapper mw = mtCollection.GetMaterial( mat_index );

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
		if ( importedTextures.find( tex_index ) == importedTextures.end() ) {
			//There was no match in the previously imported textures.
			//This may be caused by the NIF textures being stored internally, so just continue to the next slot.
			continue;
		}

		MObject txOb = importedTextures[tex_index];

		out << "Connecting a texture..." << endl;
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

	out << "Invoking dgModifier..." << endl;
	dgModifier.doIt();
}

void NifTranslator::ImportMaterialsAndTextures( NiAVObjectRef & root ) {
	//Gather all materials and textures from the file
	mtCollection.GatherMaterials( root );

	out << mtCollection.GetNumTextures() << " textures and " << mtCollection.GetNumMaterials() << " found." << endl;

	//Cycle through each texture that was found, creating a Maya fileTexture for it
	for ( size_t i = 0; i < mtCollection.GetNumTextures(); ++i ) {
		importedTextures[i] = ImportTexture( mtCollection.GetTexture(i) );
	}

	//Cycle through each material that was found, creating a Maya phong shader for it
	for ( size_t i = 0; i < mtCollection.GetNumMaterials(); ++i ) {
		importedMaterials[i] = ImportMaterial( mtCollection.GetMaterial(i) );
	}
}