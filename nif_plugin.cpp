#include "nif_plugin.h"

//#define DEBUG
#ifdef DEBUG
ofstream out( "C:\\Maya NIF Plug-in Log.txt", ofstream::binary );
#else
stringstream out;
#endif

const char PLUGIN_VERSION [] = "0.5";	
const char TRANSLATOR_NAME [] = "NetImmerse Format";

//--Globals--//
string texture_path; // Path to textures gotten from option string
unsigned int export_version = VER_4_0_0_2; //Version of NIF file to export
bool import_bind_pose = false; //Determines whether or not the bind pose should be searched for

bool import_normals= false; //Determines whether normals are imported
bool import_no_ambient = false; //Determines whether ambient color is imported
bool export_white_ambient = false; //Determines whether ambient color is automatically set to white if a texture is present
bool import_comb_skel = false; //Determines whether the importer tries to combine new skins with skeletons that exist in the scene
string joint_match = ""; //String to match in the name of nodes as another way to cause themm to import as IK joints
bool use_name_mangling = false;  //Determines whether to replace characters that are invalid for Maya (along with _ so that spaces can use that character) with hex representations

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

//--NifTranslator::reader--//

//This routine is called by Maya when it is necessary to load a file of a type supported by this translator.
//Responsible for reading the contents of the given file, and creating Maya objects via API or MEL calls to reflect the data in the file.
MStatus NifTranslator::reader (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	out << "Begining Read..." << endl;
	try {
		//Get user preferences
		ParseOptionString( optionsString );

		out << "Reading NIF File..." << endl;
		//Read NIF file
		NiObjectRef root = ReadNifTree( file.fullName().asChar() );

		//Check if the user wants us to try to combine new skins with
		//an existing skeleton
		if ( import_comb_skel ) {
			//Enumerate existing nodes by name
			existingNodes.clear();
			MItDag dagIt( MItDag::kDepthFirst);

			for ( ; !dagIt.isDone(); dagIt.next() ) {
				MFnTransform transFn( dagIt.item() );
				out << "Adding " << transFn.name().asChar() << " to list of existing nodes" << endl;
				existingNodes[ transFn.name().asChar() ] = dagIt.item();
			}
		}
		
		out << "Importing Nodes..." << endl;
		//Import Nodes, starting at each child of the root
		map< NiAVObjectRef, MDagPath > objs;
		NiNodeRef root_node = DynamicCast<NiNode>(root);
		if ( root_node != NULL ) {
			//Root is a NiNode and may have children

			//Check if the user wants us to try to find the bind pose
			if ( import_bind_pose ) {
				SendNifTreeToBindPos( root_node );
			}

			//Check if the root node has a non-identity transform
			if ( root_node->GetLocalTransform() == Matrix44::IDENTITY ) {
				//Root has no transform, so treat it as the scene root
				vector<NiAVObjectRef> root_children = root_node->GetChildren();
				
				for ( unsigned int i = 0; i < root_children.size(); ++i ) {
					ImportNodes( root_children[i], objs );
				}
			} else {
				//Root has a transform, so it's probably part of the scene
				ImportNodes( StaticCast<NiAVObject>(root_node), objs );
			}
		} else {
			NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
			if ( rootAVObj != NULL ) {
				//Root is importable, but has no children
				ImportNodes( rootAVObj, objs );
			} else {
				//Root cannot be imported
				MGlobal::displayError( "The root of this NIF file is not derived from the NiAVObject class.  It cannot be imported." );
				return MStatus::kFailure;
			}
		}
		

		//Report total number of blocks in memory
		out << "Blocks in memory:  " << NiObject::NumObjectsInMemory() << endl;
		
		//--Import Data--//
		out << "Importing Data..." << endl;
		//maps to hold information about what has already been imported
		map< pair<NiMaterialPropertyRef, NiTexturingPropertyRef>, MObject > materials;
		map< NiSourceTextureRef, MObject > textures;

		out << "Itterating through all nodes that were imported" << endl;
		//Iterate through all nodes that were imported
		map< NiAVObjectRef, MDagPath >::iterator it;
		for ( it = objs.begin(); it != objs.end(); ++it ) {
			//Import the data based on the type of node - NiTriShape only so far
			if ( it->first->IsDerivedType(NiTriBasedGeom::TypeConst()) ) {
				out << "Found NiTriBasedGeom:  " << it->first << endl;

				//Cast to NiTriBasedGeom
				NiTriBasedGeomRef geom = DynamicCast<NiTriBasedGeom>(it->first);

				if ( geom == NULL ) {
					MGlobal::displayError( "Failed to cast to NiTriBasedGeom." );
					return MStatus::kFailure;
				}

				out << "Importing mesh..." << endl;
				//Import Mesh
				MDagPath meshPath = ImportMesh( geom, it->second.node() );

				//--Look for Materials--//
				MObject grpOb;
				MObject matOb;
				MObject txOb;


				out << "Looking for material and texturing properties" << endl;
				//Get Material and Texturing properties, if any
				NiMaterialPropertyRef niMatProp = NULL;
				NiTexturingPropertyRef niTexProp = NULL;
				NiSpecularPropertyRef niSpecProp = NULL;
				NiPropertyRef niProp = geom->GetPropertyByType( NiMaterialProperty::TypeConst() );
				if ( niProp != NULL ) {
					niMatProp = DynamicCast<NiMaterialProperty>( niProp );
				}
				niProp = geom->GetPropertyByType( NiTexturingProperty::TypeConst());
				if ( niProp != NULL ) {
					niTexProp = DynamicCast<NiTexturingProperty>( niProp );
				}
				niProp = geom->GetPropertyByType( NiSpecularProperty::TypeConst() );
				if ( niProp != NULL ) {
					niSpecProp = DynamicCast<NiSpecularProperty>( niProp );
				}
							
				out << "Processing material property..." << endl;
				//Process Material Property
				if ( niMatProp != NULL ) {
					//Check to see if material has already been found
					if ( materials.find( pair<NiMaterialPropertyRef, NiTexturingPropertyRef>( niMatProp, niTexProp) ) == materials.end() ) {
						//New material/texture combo  - import and add to the list
						matOb = ImportMaterial( niMatProp, niSpecProp );
						materials[ pair<NiMaterialPropertyRef, NiTexturingPropertyRef>( niMatProp, niTexProp ) ] = matOb;
					} else {
						// Use the existing material
						matOb = materials[ pair<NiMaterialPropertyRef, NiTexturingPropertyRef>( niMatProp, niTexProp ) ];
					}

					//--Create a Shading Group--//

					//Make a selection list and add the mesh to it
					MSelectionList sel_list;
					sel_list.add( meshPath );

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
					
					out << "Looking for textures..." << endl;
					//--Look for textures--//
					if ( niTexProp != NULL ) {
						MFnDependencyNode nodeFn;
						NiSourceTextureRef niSrcTex;
						TexDesc tx;

						//Get TexturingProperty Interface						
						//Cycle through for each type of texture
						int uv_set = 0;
						for (int i = 0; i < 8; ++i) {
							if ( niTexProp->HasTexture( i ) ) {
								tx = niTexProp->GetTexture( i );
								niSrcTex = tx.source;

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

								if ( niSrcTex != NULL ) {
									//Check if texture has already been used
									if ( textures.find( niSrcTex ) == textures.end() ) {
										//New texture - import it and add it to the list
										txOb = ImportTexture( niSrcTex );
										textures[niSrcTex] = txOb;
									} else {
										//Already found, use existing one
										txOb = textures[niSrcTex];
									}
								
									nodeFn.setObject(txOb);
									MPlug tx_outColor = nodeFn.findPlug( MString("outColor") );
									switch(i) {
										case BASE_MAP:
											//Base Texture
											dgModifier.connect( tx_outColor, phongFn.findPlug("color") );
											//Check if Alpha needs to be used
											{
												NiAlphaPropertyRef niAlphaProp;
												niProp = geom->GetPropertyByType(NiAlphaProperty::TypeConst() );
												if ( niProp != NULL ) {
													niAlphaProp = DynamicCast<NiAlphaProperty>(niProp);
												}
												if ( niAlphaProp != NULL && ( niAlphaProp->GetFlags() & 1) == true ) {
													//Alpha is used, connect it
													dgModifier.connect( nodeFn.findPlug("outTransparency"), phongFn.findPlug("transparency") );
												}
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
									if ( tx.clampMode == CLAMP_S_CLAMP_T ) {
										wrap_u = false;
										wrap_v = false;
									} else if ( tx.clampMode == CLAMP_S_WRAP_T ) {
										wrap_u = false;
									} else if ( tx.clampMode == WRAP_S_CLAMP_T ) {
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
								uv_set++;
							}
						}
					}
					
					out << "Invoking dgModifier..." << endl;
					dgModifier.doIt();

					//--Bind Skin if any--//
					NiSkinInstanceRef niSkinInst = geom->GetSkinInstance();
					NiSkinDataRef niSkinData;
					if ( niSkinInst != NULL ) {
						//Get the NIF skin data interface
						niSkinData = niSkinInst->GetSkinData();
					}

					if ( niSkinData != NULL ) {
						//Build up the MEL command string
						string cmd = "skinCluster -tsb ";
						
						//Add list of bones to the command
						vector<NiNodeRef> bone_nodes = niSkinInst->GetBones();

						for (unsigned int i = 0; i < bone_nodes.size(); ++i) {
							cmd.append( objs[ StaticCast<NiAVObject>(bone_nodes[i]) ].partialPathName().asChar() );
							cmd.append( " " );
						}

						//Add mesh path to the command
						cmd.append( meshPath.partialPathName().asChar() );

						//Execute command to create skin cluster bound to specific bones
						MStringArray result;
						MGlobal::executeCommand( cmd.c_str(), result );
						MFnSkinCluster clusterFn;

						MSelectionList selList;
						selList.add(result[0]);
						MObject skinOb;
						selList.getDependNode( 0, skinOb );
						clusterFn.setObject(skinOb);

						//Get a list of all verticies in this mesh
						MFnSingleIndexedComponent compFn;
						MObject vertices = compFn.create( MFn::kMeshVertComponent );
						MItGeometry gIt(meshPath);
						MIntArray vertex_indices( gIt.count() );
						for ( int j = 0; j < gIt.count(); ++j ) {
							vertex_indices[j] = j;
						}
						compFn.addElements(vertex_indices);

						//Get weight data from NIF
						vector< vector<float> > nif_weights( bone_nodes.size() );

						//Set skin weights & bind pose for each bone
						for (unsigned int i = 0; i < bone_nodes.size(); ++i) {
							nif_weights[i].resize( gIt.count() );
							//Init all values to zero
							for ( int j = 0; j < gIt.count(); ++j ) {
								nif_weights[i][j] = 0.0f;
							}

							//Get weight data
							vector<SkinWeight> weights = niSkinData->GetBoneWeights(i);
							
							//Put data in proper slots in the 2D array
							for (unsigned int j = 0; j < weights.size(); ++j ) {
								nif_weights[i][weights[j].index] = weights[j].weight;
							}
						}

						//Build Maya influence list
						MIntArray influence_list( bone_nodes.size() );
						for ( unsigned int i = 0; i < bone_nodes.size(); ++i ) {
							influence_list[i] = i;
						}

						//Build Maya weight list
						MFloatArray weight_list(  gIt.count() * int(bone_nodes.size()) );
						int k = 0;
						for ( int i = 0; i < gIt.count(); ++i ) {
							for ( int j = 0; j < int(bone_nodes.size()); ++j ) {
								weight_list[k] = nif_weights[j][i];
								++k;
							}
						}

						//Send the weights to Maya
						clusterFn.setWeights( meshPath, vertices, influence_list, weight_list, true );
					}			
				}
			}
		}
		out << "Done searching imported scene graph." << endl;



		//TODO: Restore skinning support in Niflib.  This may not be necessary anyway.
		//--Reposition Nodes--//
		//for ( it = objs.begin(); it != objs.end(); ++it ) {
		//	MFnTransform transFn( it->second );
		//	Matrix44 transform;
		//	INode * node = (INode*)it->first->QueryInterface(ID_NODE);
		//	transform = node->GetLocalTransform();
		//	float trans_arr[4][4];
		//	transform.AsFloatArr( trans_arr );

		//	transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
		//}

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

	//Report total number of blocks in memory (hopfully zero)
	out << "Blocks in memory:  " << NiObject::NumObjectsInMemory() << endl;

	return MStatus::kSuccess;
}

// Recursive function to crawl down tree looking for children and translate those that are understood
void NifTranslator::ImportNodes( NiAVObjectRef niAVObj, map< NiAVObjectRef, MDagPath > & objs, MObject parent )  {
	MObject obj;

	out << "Importing " << niAVObj << endl;
	out << "Blocks in memory:  " << NiObject::NumObjectsInMemory() << endl;

	////Stop at a non-node
	//if ( node == NULL )
	//	return;

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

		transform = niAVObj->GetLocalTransform();

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

	//Call this function for any children
	if ( niNode != NULL ) {
		vector<NiAVObjectRef> children = niNode->GetChildren();
		for ( unsigned int i = 0; i < children.size(); ++i ) {
			ImportNodes( children[i], objs, obj );
		}
	}
}

MObject NifTranslator::ImportTexture( NiSourceTextureRef niSrcTex ) {
	MObject obj;

	//block = block->GetLink("Base Texture");
	
	//If the texture is stored externally, use it
	if ( niSrcTex->IsTextureExternal() == true ) {
		//An external texture is used, create a texture node
		string file_name = niSrcTex->GetExternalFileName();
		MFnDependencyNode nodeFn;
		//obj = nodeFn.create( MFn::kFileTexture, MString( ts.fileName.c_str() ) );
		obj = nodeFn.create( MString("file"), MString( file_name.c_str() ) );

		string file_path = texture_path + file_name;

		nodeFn.findPlug( MString("ftn") ).setValue( MString( file_path.c_str() ) );

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
	else {
		throw runtime_error("This file uses an internal texture.  This is currently unsupported.");
	}
}

MObject NifTranslator::ImportMaterial( NiMaterialPropertyRef niMatProp, NiSpecularPropertyRef niSpecProp ) {
	//Create the material but don't connect it to parent yet
	MFnPhongShader phongFn;
	MObject obj = phongFn.create();
	Color3 color;

	//See if the user wants the ambient color imported
	if ( !import_no_ambient ) {
		color = niMatProp->GetAmbientColor();
		phongFn.setAmbientColor( MColor(color.r, color.g, color.b) );
	}

	color = niMatProp->GetDiffuseColor();
	phongFn.setColor( MColor(color.r, color.g, color.b) );

	
	//Set Specular color to 0 unless the mesh has a NiSpecularProperty
	phongFn.setSpecularColor( MColor( 0.0f, 0.0f, 0.0f) );

	if ( niSpecProp != NULL && (niSpecProp->GetFlags() & 1) == true ) {
		//This mesh uses specular color - load it
		color = niMatProp->GetSpecularColor();
		phongFn.setSpecularColor( MColor(color.r, color.g, color.b) );
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

MDagPath NifTranslator::ImportMesh( NiTriBasedGeomRef niGeom, MObject parent ) {
	out << "ImportMesh() begin" << endl;
	//Get NiTriBAsedGeomData
	NiTriBasedGeomDataRef niGeomData = niGeom->GetData();

	if ( niGeomData == NULL ) {
		MGlobal::displayError( "This mesh has no polygon data." );
		return MDagPath();
	}

	int NumVertices = niGeomData->GetVertexCount();

	vector<Vector3> nif_verts;
	
	//If this is a skin influenced mesh, get vertices from niGeom
	if ( niGeom->GetSkinInstance() != NULL ) {
		nif_verts = niGeom->GetSkinInfluencedVertices();
	} else {
		nif_verts = niGeomData->GetVertices();
	}
	MPointArray maya_verts(NumVertices);

	for (int i = 0; i < NumVertices; ++i) {
		maya_verts[i] = MPoint(nif_verts[i].x, nif_verts[i].y, nif_verts[i].z, 0.0f);
	}

	out << "Getting polygons..." << endl;
	//Get Polygons
	int NumPolygons = 0;
	vector<Triangle> triangles;
	triangles = niGeomData->GetTriangles();

	NumPolygons = triangles.size();

	//Nifs only have triangles, so all polys have 3 verticies
	//Create an array to tell Maya this
	int * poly_counts = new int[NumPolygons];
	for (int i = 0; i < NumPolygons; ++i) {
		poly_counts[i] = 3;
	}
	MIntArray maya_poly_counts( poly_counts, NumPolygons );

	//There are 3 verticies to list per triangle
	vector<int> connects( NumPolygons * 3 );// = new int[NumPolygons * 3];

	MIntArray maya_connects;
	for (int i = 0; i < NumPolygons; ++i) {
		maya_connects.append(triangles[i].v1);
		maya_connects.append(triangles[i].v2);
		maya_connects.append(triangles[i].v3);
	}

	//MIntArray maya_connects( connects, NumPolygons * 3 );

	//Create Mesh with empy default UV set at first
	MDagPath meshPath;
	MFnMesh meshFn;
	
	meshFn.create( NumVertices, NumPolygons, maya_verts, maya_poly_counts, maya_connects, parent );
	meshFn.getPath( meshPath );

	out << "Importing vertex colors..." << endl;
	//Import Vertex Colors
	vector<Color4> nif_colors = niGeomData->GetColors();
	if ( nif_colors.size() > 0 ) {
		//Create vertex list
		MIntArray vert_list(NumVertices);
		MColorArray maya_colors((unsigned int)nif_colors.size(), MColor(0.5f, 0.5f, 0.5f, 1.0f) );
		for (unsigned int i = 0; i < nif_colors.size(); ++i ) {
			maya_colors[i] = MColor( nif_colors[i].r, nif_colors[i].g, nif_colors[i].b, nif_colors[i].a );
			vert_list[i] = i;
		}
		meshFn.setVertexColors( maya_colors, vert_list );
	}

	out << "Examining properties..." << endl;
	//--Examine properties--//
	vector<MString> uv_set_list;

	//Make invisible if bit flag 1 is set
	if ( niGeom->GetVisibility() == false ) {
		MPlug vis = meshFn.findPlug( MString("visibility") );
		vis.setValue(false);
	}

	out << "Creating a list of the UV sets..." << endl;
	//Create a list of the UV sets used by the texturing property if there is one
	NiPropertyRef niProp = niGeom->GetPropertyByType( NiTexturingProperty::TypeConst() );
	NiTexturingPropertyRef niTexProp;
	if ( niProp != NULL ) {
		niTexProp = DynamicCast<NiTexturingProperty>(niProp);
	}
	if ( niTexProp != NULL ) {

		for ( int i = 0; i < 8; ++i ) {
			if ( niTexProp->HasTexture(i) == true ) {
				switch(i) {
					case BASE_MAP:
						uv_set_list.push_back( MString("map1") );
						break;
					case DARK_MAP:
						uv_set_list.push_back( MString("dark") );
						break;
					case DETAIL_MAP:
						uv_set_list.push_back( MString("detail") );
						break;
					case GLOSS_MAP:
						uv_set_list.push_back( MString("gloss") );
						break;
					case GLOW_MAP:
						uv_set_list.push_back( MString("glow") );
						break;
					case BUMP_MAP:
						uv_set_list.push_back( MString("bump") );
						break;
					case DECAL_0_MAP:
						uv_set_list.push_back( MString("decal0") );
						break;
					case DECAL_1_MAP:
						uv_set_list.push_back( MString("decal1") );
						break;
				}
			}
		}
	}

	out << "UV Set List:  "  << endl;
	for ( unsigned int i = 0; i < uv_set_list.size(); ++i ) {
		out << uv_set_list[i].asChar() << endl;
	}

	out << "Getting default UV set..." << endl;
	// Get default (first) UV Set if there is one		
	if ( niGeomData->GetUVSetCount() > 0 ) {
		meshFn.clearUVs();
		vector<TexCoord> uv_set;
		//Arrays for maya
		MFloatArray u_arr(NumVertices), v_arr(NumVertices);

		out << "Loop through " << niGeomData->GetUVSetCount() << " UV sets..." << endl;
		for (int i = 0; i < niGeomData->GetUVSetCount(); ++i) {
			uv_set = niGeomData->GetUVSet(i);

			out << "uv_set_list.size():  " << uv_set_list.size() << endl;
			out << "i:  " << i << endl;


			for (int j = 0; j < NumVertices; ++j) {
				u_arr[j] = uv_set[j].u;
				v_arr[j] = 1.0f - uv_set[j].v;
			}

			//out << "Original UV Array:" << endl;
			//for ( int j = 0; j < NumVertices; ++j ) {
			//	out << "U:  " << uv_set[j].u << "  V:  " << uv_set[j].v << endl;
			//}
			//out << "Maya UV Array:" << endl;
			//for ( int j = 0; j < NumVertices; ++j ) {
			//	out << "U:  " << u_arr[j] << "  V:  " << v_arr[j]<< endl;
			//}
			
			//Assign the UVs to the object
			MString uv_set_name("map1");
			if ( i < int(uv_set_list.size()) ) {
				out << "Entered if statement." << endl;
				uv_set_name = uv_set_list[i];
			}

			out << "Check if a UV set needs to be created..." << endl;
			if ( uv_set_name != "map1" ) {
				meshFn.createUVSet( uv_set_name );
				out << "Creating UV Set:  " << uv_set_name.asChar() << endl;
			}
	
			out << "Set UVs...  u_arr:  " << u_arr.length() << " v_arr:  " << v_arr.length() << endl;
			MStatus stat = meshFn.setUVs( u_arr, v_arr, &uv_set_name );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to set UVs.");
			}

			out << "Assign UVs..." << endl;
			stat = meshFn.assignUVs( maya_poly_counts, maya_connects, &uv_set_name );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to assign UVs.");
			}
			////Delete default "map1" UV set
			//meshFn.deleteUVSet( MString("map1") );
		}
	}

	//See if the user wants us to import the normals
	if ( import_normals ) {
		out << "Getting Normals..." << endl;
		// Load Normals
		vector<Vector3> nif_normals = niGeomData->GetNormals();
		if ( nif_normals.size() != 0 ) {
			if ( nif_normals.size() != NumVertices ) {
				throw runtime_error ("Normal size does not equal Num Vertices");
			}

			MVectorArray maya_normals(NumVertices);
			MIntArray vert_list(NumVertices);
			for (int norm = 0; norm < NumVertices; ++norm) {
				maya_normals[norm] = MVector(nif_normals[norm].x, nif_normals[norm].y, nif_normals[norm].z );
				vert_list[norm] = norm;
			}

			MStatus stat = meshFn.setVertexNormals( maya_normals, vert_list );
			if ( stat != MS::kSuccess ) {
				out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to assign UVs.");
			}
		}
	}



	out << "Deleting poly_counts..." << endl;
	//Delete everything that was used to load verticies
	delete [] poly_counts;

	out << "ImportMesh() end" << endl;
	return meshPath;
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


		out << "Exporting nodes..." << endl;
		nodes.clear();
		meshes.clear();
		//Export nodes
		ExportDAGNodes();

		out << "Enumerating skin clusters..." << endl;
		meshClusters.clear();
		EnumerateSkinClusters();

		out << "Exporting meshes..." << endl;
		for ( list<MObject>::iterator mesh = meshes.begin(); mesh != meshes.end(); ++mesh ) {
			ExportMesh( *mesh );
		}

		//--Write finished NIF file--//

		out << "Writing Finished NIF file..." << endl;
		WriteNifTree( file.fullName().asChar(), StaticCast<NiObject>(sceneRoot), export_version );

		out << "Export Complete." << endl;
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

	vector<ComplexShape::WeightedVertex> nif_vts( vts.length() );
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

	vector<ComplexShape::TexCoordSet> nif_uvs;

	//Record assotiation between name and uv set index for later
	map<string,int> uvSetNums;

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
			uvSetNums[set_name] = i;

			//Get the UVs
			meshFn.getUVs( myUCoords, myVCoords, &uvSetNames[i] );

			//Store the data
			ComplexShape::TexCoordSet tcs;
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
		}
	}

	cs.SetTexCoordSets( nif_uvs );

	out << "===Exported UV Data===" << endl;

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

	vector<ComplexShape::ComplexFace> nif_faces;


	//Add shaders to propGroup array
	vector< vector<NiPropertyRef> > propGroups;
	for ( unsigned int shader_num = 0; shader_num < Shaders.length(); ++shader_num ) {
		
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

		ComplexShape::ComplexFace cf;

		//Assume all faces use material 0 for now
		cf.propGroupIndex = 0;
		
		for( int i = 0; i < poly_vert_count; ++i ) {
			ComplexShape::ComplexPoint cp;

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

			for ( unsigned int j = 0; j < uvSetNames.length(); ++j ) {
				ComplexShape::TexCoordIndex tci;
				tci.texCoordSetIndex  = uvSetNums[ uvSetNames[j].asChar() ];
				int uv_index;
				itPoly.getUVIndex(i, uv_index, &uvSetNames[j] );
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

	out << "===Exported Face Data===" << endl;
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
					if ( nodes.find( myBones[0].fullPathName().asChar() ) == nodes.end() ) {
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
				out << "Number of weights:  " << myWeights.length() << endl;
				out << "Number of bones:  " << myBones.length() << endl;
				out << "Number of Maya vertices:  " << gIt.count() << endl;
				out << "Number of NIF vertices:  " << int(nif_vts.size()) << endl;
				unsigned int weight_index = 0;
				ComplexShape::SkinInfluence sk;
				for ( unsigned int vert_index = 0; vert_index < nif_vts.size(); ++vert_index ) {
					for ( unsigned int bone_index = 0; bone_index < myBones.length(); ++bone_index ) {
						out << "vert_index:  " << vert_index << "  bone_index:  " << bone_index << "  weight_index:  " << weight_index << endl;	
						// Only bother with weights that are significant
						if ( myWeights[weight_index] > 0.0 ) {
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

	out << "Split ComplexShape" <<endl;
	NiAVObjectRef avObj = cs.Split( parNode );

	out << "Get the NiAVObject portion of the root of the split" <<endl;
	//Get the NiAVObject portion of the root of the split
	ExportAV( avObj, dagNode );

	//If polygon mesh is hidden, hide tri_shape
	MPlug vis = visibleMeshFn.findPlug( MString("visibility") );
	bool value;
	vis.getValue(value);
	if ( value == false ) {
		out << "Visibility of " << visibleMeshFn.name().asChar() << " is " << value << endl;
		avObj->SetVisibility(false);
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
				bool intermediate = false;
				MObject matching_child;

				//Check to see what kind of node we should create
				for( int i = 0; i != nodeFn.childCount(); ++i ) {
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

					}
				}

				if ( !intermediate ) {
	
					if ( tri_shape == true ) {
						out << "Adding Mesh to list to be exported later..." << endl;
						meshes.push_back( it.item() );
						//NiTriShape
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

		//We will at least need a NiMaterialProperty
		NiMaterialPropertyRef niMatProp = new NiMaterialProperty;
		//Only create these if necessary
		NiTexturingPropertyRef niTexProp = NULL;  
		NiAlphaPropertyRef niAlphaProp = NULL;
		NiSpecularPropertyRef niSpecProp = NULL;

		MColor color;
		MObject texture;

		out << "Testing for MFnLambertShader function set." << endl;
		if ( itDep.item().hasFn( MFn::kLambert ) ) {
			out << "Attaching MFnLambertShader function set." << endl;
			//All shaders inherit from lambert
			MFnLambertShader lambertFn( itDep.item() );

			//Set Name
			niMatProp->SetName( MakeNifName( lambertFn.name() ) );

			//--Get info from Color slots--//
			out << "Getting base color" << endl;
			GetColor( lambertFn, "color", color, texture );
			out << "Checking whether base texture is used" << endl;
			//If a texture is used, the color is white and the texture is used
			if ( texture.isNull() == true ) {
				//No texture
				niMatProp->SetDiffuseColor( Color3( color.r, color.g, color.b ) );
			} else {
				niMatProp->SetDiffuseColor( Color3(1.0f, 1.0f, 1.0f) ); // white
				//See if the user wants us to export white ambient if there is a texture
				if ( export_white_ambient ) {
					niMatProp->SetAmbientColor( Color3(1.0f, 1.0f, 1.0f) ); // white
				}

				out << "Base texture is used.  Create NiTexturingProperty." << endl;
				//Base texture is used.  Create NiTexturingProperty.
				if ( niTexProp == NULL ) {
					niTexProp = new NiTexturingProperty;
				}
				//Find texture with the same maya name
				if ( texture.hasFn( MFn::kDependencyNode ) ) {
					MFnDependencyNode depFn(texture);
					string texname = depFn.name().asChar();
					out << "Texture found:  " << texname << endl;
					if ( textures.find( texname ) != textures.end() ) {
						TexDesc td;
						td.source = textures[texname];
						niTexProp->SetTexture( BASE_MAP, td );
						//Check if texture has alpha
						MPlug fha = depFn.findPlug("fileHasAlpha");
						bool value;
						fha.getValue( value );
						if ( value == true ) {
							if ( niAlphaProp == NULL ) {
								out << "Base texture has alpha, so create a NiAlphaProperty" << endl;
								//Base texture has alpha, so create a NiAlphaProperty
								niAlphaProp = new NiAlphaProperty;
								niAlphaProp->SetFlags(237);
							}
						}
					}
				}
			}

			//Make sure white ambient wasn't already exported
			if ( !(texture.isNull() == false && export_white_ambient) ) {
				out << "Getting ambient color" << endl;
				GetColor( lambertFn,"ambientColor", color, texture );
				//Textures are not supported
				if ( texture.isNull() == true ) {
					//No texture
					niMatProp->SetAmbientColor( Color3( color.r, color.g, color.b ) );
				} else {
					niMatProp->SetAmbientColor( Color3(1.0f, 1.0f, 1.0f) ); // white
					MGlobal::displayWarning("Ambient textures are not supported by the NIF format.  Ignored.");
				}
			}

			out << "Getting incandescence color" << endl;
			GetColor( lambertFn,"incandescence", color, texture );
			//Textures are not supported
			if ( texture.isNull() == true ) {
				//No texture
				//This color is reversed
				niMatProp->SetEmissiveColor( Color3( color.r, color.g, color.b ) );
			} else {
				niMatProp->SetEmissiveColor( Color3(1.0f, 1.0f, 1.0f) ); // white
				
				out << "Glow texture is used.  Create NiTexturingProperty." << endl;
				//Glow texture is used.  Create NiTexturingProperty.
				if ( niTexProp == NULL ) {
					niTexProp = new NiTexturingProperty;
				}
				//Find texture with the same maya name
				if ( texture.hasFn( MFn::kDependencyNode ) ) {
					MFnDependencyNode depFn(texture);
					string texname = depFn.name().asChar();
					out << "Texture found:  " << texname << endl;
					if ( textures.find( texname ) != textures.end() ) {
						TexDesc td;
						td.source = textures[texname];
						niTexProp->SetTexture( GLOW_MAP, td );
					}
				}			
			}

			out << "Getting transparency color" << endl;
			GetColor( lambertFn,"transparency", color, texture );
			//Textures are probably set because the base texture has
			//alpha which is connected to this node.  So just set the
			//transparency and don't worry about whethere there's a
			//texture connected or not.
			float trans = (color.r + color.g + color.b) / 3.0f;
			if ( trans != color.r ) {
				MGlobal::displayWarning("Colored transparency is not supported by the NIF format.  An average of the color channels will be used.");
			}
			//Maya trans is reverse of NIF trans
			trans = 1.0f - trans;
			niMatProp->SetTransparency( trans );

			if ( trans < 1.0f ) {
				if ( niAlphaProp == NULL ) {
					out << "Transparency is used, so create a NiAlphaProperty" << endl;
					//Transparency is used, so create a NiAlphaProperty
					niAlphaProp = new NiAlphaProperty;
					niAlphaProp->SetFlags(237);
				}
			}

			//TODO: Support bump maps, environment maps, gloss maps, and multi-texturing (detail, dark, and decal?)
		}

		out << "Testing for MFnReflectShader function set" << endl;
		//Shader may also have specular color
		if ( itDep.item().hasFn( MFn::kReflect ) ) {
			out << "Attaching MFnReflectShader function set" << endl;
			MFnReflectShader reflectFn( itDep.item() );

			out << "Getting specular color" << endl;
			GetColor( reflectFn, "specularColor", color, texture );
			//Textures are not supported
			if ( texture.isNull() == true ) {
				//No texture
				niMatProp->SetSpecularColor( Color3( color.r, color.g, color.b ) );

				//Only build a NiSpecularProperty if specular color is not black

				if ( color.r != 0.0f || color.g != 0.0f || color.b != 0.0f ) {
					//Specularity is used, so create a NiSpecularProperty and put it in the list
					if ( niSpecProp == NULL ) {
						niSpecProp = new NiSpecularProperty;
						niSpecProp->SetFlags(1);
					}
				}

			} else {
				niMatProp->SetSpecularColor( Color3(1.0f, 1.0f, 1.0f) ); // white
				MGlobal::displayWarning("Gloss textures are not supported yet.  Ignored.");
			}

			//Handle cosine power/glossiness
			out << "Testing for MFnPhongShader function set" << endl;
			if ( itDep.item().hasFn( MFn::kPhong ) ) {
				out << "Attaching MFnReflectShader function set" << endl;
				MFnPhongShader phongFn( itDep.item() );

				out << "Getting cosine power" << endl;
				MPlug p = phongFn.findPlug("cosinePower");

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
						texture = plugs[i].node();

						// stop looping
						break;
					}
				}

				//Textures are not supported
				if ( texture.isNull() == true ) {
					//No texture
					niMatProp->SetGlossiness( phongFn.cosPower() );
				} else {
					niMatProp->SetGlossiness( 20.0f );
					MGlobal::displayWarning("Gloss textures are not yet supported.  Ignored.");
				}
			} else {
				//Not a phong texture so not currently supported
				MGlobal::displayWarning("Only Shaders with Cosine Power, such as Phong, can currently be used to set exported glossiness.  Default value of 20.0 used.");
				niMatProp->SetGlossiness( 20.0f );
			}
		} else {
			//No reflecting shader used, so set some defautls
			niMatProp->SetSpecularColor( Color3( 0.0f, 0.0f, 0.0f) );
			niMatProp->SetGlossiness( 20.0f );
		}

		

		//TODO:  Figure out how to guestimate glossiness from Blin shaders.


		out << "Putting created properties into a vector" << endl;
		//Put properties into a vector
		vector<NiPropertyRef> niProps;

		//We will at least need a NiMaterialProperty
		if ( niMatProp != NULL ) {
			niProps.push_back( StaticCast<NiProperty>(niMatProp) );
		}
		if ( niTexProp != NULL ) {
			niProps.push_back( StaticCast<NiProperty>(niTexProp) );
		}
		if ( niAlphaProp != NULL ) {
			out << "Adding " << niAlphaProp << " to vector" << endl;
			niProps.push_back( StaticCast<NiProperty>(niAlphaProp) );
		}
		if ( niSpecProp != NULL ) {
			niProps.push_back( StaticCast<NiProperty>(niSpecProp) );
		}

		out << "Associating proprety vector with Maya shader name: ";
		//Associate property vector with Maya shader name
		MFnDependencyNode depFn( itDep.item() );
		out << depFn.name().asChar() << endl;
		shaders[ depFn.name().asChar() ] = niProps;
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

		// get the attribute for the full texture path
		MPlug ftn = fn.findPlug("fileTextureName");

		// get the filename from the attribute
		MString filename;
		ftn.getValue(filename);

		//Create the NiSourceTexture block
		NiSourceTextureRef ni_tex = new NiSourceTexture;

		//Get Node Name
		ni_tex->SetName( MakeNifName( fn.name() ) );

		MString fname;
		ftn.getValue(fname);
		string fileName = fname.asChar();

		//Figure fixed file name
		unsigned int len = unsigned int( texture_path.size() );

		out << "Full Texture Path:  " << fname.asChar() << endl;
		if ( fname.length() > len && fname.substring( 0, len - 1 ) == texture_path.c_str() ) {
			fileName = fname.substring( len, fname.length() - 1 ).asChar();
			out << "File Name:  " << fileName << endl;
		}

		ni_tex->SetExternalTexture( fileName, NULL );

		//Associate NIF object with fileTexture DagPath
		string path = fn.name().asChar();
		textures[path] = ni_tex;

		//TEMP: Write the block so we can see it worked
		out << ni_tex->asString();

		// get next fileTexture
		it.next();
	} 
}

void NifTranslator::ParseOptionString( const MString & optionsString ) {
	//Parse Options String
	MStringArray options;
	out << "optionsString:  " << optionsString.asChar() << endl;
	optionsString.split( ';', options );
	for (unsigned int i = 0; i < options.length(); ++i) {
		MStringArray tokens;
		options[i].split( '=', tokens );
		out << "tokens[0]:  " << tokens[0].asChar() << endl;
		out << "tokens[1]:  " << tokens[1].asChar() << endl;
		if ( tokens[0] == "texturePath" ) {
			texture_path = tokens[1].asChar();
			if ( texture_path[ texture_path.size() - 1 ] != '/' ) {
				texture_path.append("/");
			}
			out << "Texture Path:  " << texture_path << endl;
			
		}
		if ( tokens[0] == "exportVersion" ) {
			MStringArray versionParts;
			tokens[1].split( '.', versionParts );

			if ( versionParts.length() != 4 ) {
				MGlobal::displayWarning( "Invalid export version specified.  Using default of 4.0.0.2." );
				export_version = VER_4_0_0_2;
			} else {
				unsigned char verBits[4];
				for ( int i = 0; i < 4; ++i ) {
					verBits[3-i] = unsigned char( atoi( versionParts[i].asChar() ) );
				}
				export_version = *((unsigned int *)verBits);
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
		if ( tokens[0] == "importSkelComb" ) {
			if ( tokens[1] == "1" ) {
				import_comb_skel = true;
			} else {
				import_comb_skel = false;
			}
			out << "Combine with Existing Skeleton:  " << import_comb_skel << endl;
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
	map<string,MObject>::iterator it = existingNodes.find( name );
	if ( it == existingNodes.end() ) {
		out << "A joint named " << name << " did not exist in list." << endl;
		return MObject::kNullObj;
	}

	//It's in the list
	MObject jointObj = it->second;

	//Check if it is an Ik joint
	if ( jointObj.hasFn( MFn::kJoint ) ) {
		out << "Matching joint is already an IK joint." << endl;
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
	out << "New Maya name:  " << newName.str() << endl;
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
				out << "Sub string:  " << sub << endl;
				if ( sub == "_0x" ) {
					sub = str.substr( i+3, 2);
					out << "Sub sub string:  " << sub << endl;
					temp.clear();
					temp << sub; // should be the char number
					int ch_num;
					temp >> ch_num;
					out << "The int returned from the string stream extraction is:  " << ch_num << endl;
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