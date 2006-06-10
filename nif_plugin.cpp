#include "nif_plugin.h"

const char PLUGIN_VERSION [] = "pre-alpha";	
const char TRANSLATOR_NAME [] = "NetImmerse Format";

//--Globals--//
string texture_path; // Path to textures gotten from option string

//--Function Definitions--//

//--NifTranslator::identifyFile--//

// This routine must use passed data to determine if the file is of a type supported by this translator

// Code adapted from animImportExport example that comes with Maya 6.5

MPxFileTranslator::MFileKind NifTranslator::identifyFile (const MFileObject& fileName, const char* buffer, short size) const {
	//cout << "Checking File Type..." << endl;
	const char *name = fileName.name().asChar();
	int nameLength = (int)strlen(name);

	
	if ((nameLength > 4) && !_stricmp(name+nameLength-4, ".nif")) {
		return kIsMyFileType;
	}

	return	kNotMyFileType;
}

//--Plug-in Load/Unload--//

//--initializePlugin--//

// Code adapted from lepTranslator example that comes with Maya 6.5
MStatus initializePlugin( MObject obj ) {
	//cout << "Initializing Plugin..." << endl;
	MStatus   status;
	MFnPlugin plugin( obj, "NifTools", PLUGIN_VERSION, "Any");

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

	//cout << "Done Initializing." << endl;
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
	//cout << "Begining Read..." << endl;
	try {
		//Parse Options String
		MStringArray options;
		optionsString.split( ';', options );
		for (unsigned int i = 0; i < options.length(); ++i) {
			MStringArray tokens;
			options[i].split( '=', tokens );
			if ( tokens[0] == "texturePath" ) {
				texture_path = tokens[1].asChar();
				texture_path.append("/");
			}
		}

		//cout << "Reading NIF File..." << endl;
		//Read NIF file
		NiObjectRef root = ReadNifTree( file.fullName().asChar() );

		//cout << "Importing Nodes..." << endl;
		//Import Nodes, starting at each child of the root
		map< NiAVObjectRef, MDagPath > objs;

		NiNodeRef root_node = DynamicCast<NiNode>(root);
		if ( root_node != NULL ) {
			//Root is a NiNode and may have children
			vector<NiAVObjectRef> root_children = root_node->GetChildren();
			
			for ( uint i = 0; i < root_children.size(); ++i ) {
				ImportNodes( root_children[i], objs );
			}
		} else {
			NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
			if ( rootAVObj != NULL ) {
				//Root is importable, but has no children
				ImportNodes( rootAVObj, objs );
			} else {
				//Root cannot be imported
				cout << "The root of this NIF file is not derived from the NiAVObject class.  It cannot be imported.";
				MStatus::kFailure;
			}
		}
		

		//Report total number of blocks in memory
		//cout << "Blocks in memory:  " << BlocksInMemory() << endl;
		
		//--Import Data--//
		//cout << "Importing Data..." << endl;
		//maps to hold information about what has already been imported
		map< pair<NiMaterialPropertyRef, NiTexturingPropertyRef>, MObject > materials;
		map< NiSourceTextureRef, MObject > textures;

		//Iterate through all nodes that were imported
		map< NiAVObjectRef, MDagPath >::iterator it;
		for ( it = objs.begin(); it != objs.end(); ++it ) {
			
			//Create some more friendly names for the block and its type
			NiObjectRef obj = it->first;

			//Import the data based on the type of node - NiTriShape only so far
			if ( obj->IsDerivedType(NiTriBasedGeom::TYPE) ) {
				//Cast to NiTriBasedGeom
				NiTriBasedGeomRef geom = DynamicCast<NiTriBasedGeom>(obj);
				
				
				////If this is a skinned shape, turn off inherit transform
				////Some NIF files have the shapes parented to the skeleton which
				////messes up the deformation
				//TODO: Support skinning in Niflib
				//blk_ref skin_inst = geom["Skin Instance"];
				//if ( skin_inst.is_null() == false ) {
				//	MFnDagNode nodeFn( it->second.node() );
				//	nodeFn.findPlug("inheritsTransform").setValue(false);
				//}

				//Import Mesh
				MDagPath meshPath = ImportMesh( geom, it->second.node() );

				//--Look for Materials--//
				MObject grpOb;
				MObject matOb;
				MObject txOb;

				//Get Material and Texturing properties, if any
				NiMaterialPropertyRef niMatProp = NULL;
				NiTexturingPropertyRef niTexProp = NULL;
				NiPropertyRef niProp = geom->GetPropertyByType( NiMaterialProperty::TYPE );
				if ( niProp != NULL ) {
					niMatProp = DynamicCast<NiMaterialProperty>( niProp );
				}
				niProp = geom->GetPropertyByType( NiTexturingProperty::TYPE );
				if ( niProp != NULL ) {
					niTexProp = DynamicCast<NiTexturingProperty>( niProp );
				}
							
				//Process Material Property
				if ( niMatProp != NULL ) {
					//Check to see if material has already been found
					if ( materials.find( pair<NiMaterialPropertyRef, NiTexturingPropertyRef>( niMatProp, niTexProp) ) == materials.end() ) {
						//New material/texture combo  - import and add to the list
						matOb = ImportMaterial( niMatProp );
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
					
					//--Look for textures--//
					if ( niTexProp != NULL ) {
						MFnDependencyNode nodeFn;
						NiSourceTextureRef niSrcTex;
						TexDesc tx;

						//Get TexturingProperty Interface
						
						int tx_count = niTexProp->GetTextureCount();

						//Cycle through for each type of texture
						int uv_set = 0;
						for (int i = 0; i < tx_count; ++i) {
							if ( niTexProp->HasTexture( i ) ) {
								tx = niTexProp->GetTexture( i );
								niSrcTex = tx.source;
								uv_set++;

								switch(i) {
									case DARK_MAP:
										//Temporary until/if Dark Textures are supported
										cout << "Warning:  Dark Textures are not yet supported." << endl;
										continue;
									case DETAIL_MAP:
										//Temporary until/if Detail Textures are supported
										cout << "Warning:  Detail Textures are not yet supported." << endl;
										continue;
									case GLOSS_MAP:
										//Temporary until/if Detail Textures are supported
										cout << "Warning:  Gloss Textures are not yet supported." << endl;
										continue;
									case BUMP_MAP:
										//Temporary until/if Bump Map Textures are supported
										cout << "Warning:  Bump Map Textures are not yet supported." << endl;
										continue;
									case DECAL_0_MAP:
										//Temporary until/if Decal Textures are supported
										cout << "Warning:  Decal Textures are not yet supported." << endl;
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
												niProp = geom->GetPropertyByType(NiAlphaProperty::TYPE);
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
										MFnDependencyNode uvcFn;
										uvcFn.create( "uvChooser", "uvChooser" );

										//Connection between the mesh and the uvChooser
										MFnMesh meshFn;
										meshFn.setObject(meshPath);
										dgModifier.connect( meshFn.findPlug("uvSet")[uv_set].child(0), uvcFn.findPlug("uvSets").elementByLogicalIndex(0) );

										//Connections between the uvChooser and the place2dTexture
										dgModifier.connect( uvcFn.findPlug("outUv"), tp2dFn.findPlug("uvCoord") );
										dgModifier.connect( uvcFn.findPlug("outVertexCameraOne"), tp2dFn.findPlug("vertexCameraOne") );
										dgModifier.connect( uvcFn.findPlug("outVertexUvOne"), tp2dFn.findPlug("vertexUvOne") );
										dgModifier.connect( uvcFn.findPlug("outVertexUvTwo"), tp2dFn.findPlug("vertexUvTwo") );
										dgModifier.connect( uvcFn.findPlug("outVertexUvThree"), tp2dFn.findPlug("vertexUvThree") );
									}
								}
							}
						}
					}
					
					dgModifier.doIt();

					//TODO: Restore skinning support in Niflib
					////--Bind Skin if any--//
					//if ( skin_inst.is_null() == false ) {
					//	//Get the NIF skin data interface
					//	blk_ref skin_data = skin_inst["Data"];
					//	ISkinData * data = (ISkinData*)skin_data->QueryInterface( ID_SKIN_DATA );

					//	//Build up the MEL command string
					//	string cmd = "skinCluster -tsb ";
					//	
					//	//Add list of bones to the command
					//	vector<blk_ref> bone_blks = data->GetBones();					

					//	for (unsigned int i = 0; i < bone_blks.size(); ++i) {
					//		cmd.append( objs[ bone_blks[i] ].partialPathName().asChar() );
					//		cmd.append( " " );
					//	}

					//	//Add mesh path to the command
					//	cmd.append( meshPath.partialPathName().asChar() );

					//	//Execute command to create skinCluster bound to specific bones
					//	MStringArray result;
					//	MGlobal::executeCommand( cmd.c_str(), result );
					//	MFnSkinCluster clusterFn;

					//	MSelectionList selList;
					//	selList.add(result[0]);
					//	MObject skinOb;
					//	selList.getDependNode( 0, skinOb );
					//	clusterFn.setObject(skinOb);

					//	//Get a list of all verticies in this mesh
					//	MFnSingleIndexedComponent compFn;
					//	MObject vertices = compFn.create( MFn::kMeshVertComponent );
					//	MItGeometry gIt(meshPath);
					//	MIntArray vertex_indices( gIt.count() );
					//	for ( int j = 0; j < gIt.count(); ++j ) {
					//		vertex_indices[j] = j;
					//	}
					//	compFn.addElements(vertex_indices);

					//	//Set skin weights & bind pose for each bone
					//	for (unsigned int i = 0; i < bone_blks.size(); ++i) {
					//		////Get Bind Pose
					//		//float bind_pose[4][4];
					//		//INode * node = (INode*)bone_blks[i]->QueryInterface(Node);
					//		//node->GetWorldBindPos( bind_pose );
					//		//MFnMatrixData mat;
					//		//mat.set( MMatrix( bind_pose ) );

					//		////Set bone bind pose matrix
					//		//MFnTransform transFn(objs[bone_blks[i]]);
					//		//MPlug bindPose = transFn.findPlug("bindPose");
					//		////if ( bindPose != MObject::kNullObj ) {
					//		//	bindPose.setValue( mat.object() );
					//		////}

					//		//
					//		//MPlug plugBindPre = clusterFn.findPlug("bindPreMatrix") ;
					//		//plugBindPre = plugBindPre.elementByPhysicalIndex( i ) ;
					//		//plugBindPre.setValue( mat.object() );

					//		//MPlug plugBindPose = transFn.findPlug("bindPose");
					//		//plugBindPose.setValue( mat.object() ) ;

					//		//Get weights from NIF
					//		map<int, float> weight_map = data->GetWeights( bone_blks[i] );
					//		
					//		MFloatArray weight_list( gIt.count() );
					//		MIntArray influence_list(1);

					//		//Add this index to the influence list since we're setting one at a time
					//		influence_list[0] = i;
					//		
					//		//Iterate over all verticies in this mesh, setting the weights
					//		for ( int j = 0; j < gIt.count(); ++j ) {
					//			if ( weight_map.find(j) != weight_map.end() ) {
					//				weight_list[j] = weight_map[j];
					//			}
					//			else
					//				weight_list[j] = 0.0f;
					//		}
					//		
					//		clusterFn.setWeights( meshPath, vertices, influence_list, weight_list, true );
					//	}
					//}			
				}
			}
		}



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
		cout << "Error:  " << e.what() << endl;
		return MStatus::kFailure;
	}
	catch( ... ) {
		cout << "Error:  Unknown Exception." << endl;
		return MStatus::kFailure;
	}
	
	//cout << "Finished Read" << endl;

	//Report total number of blocks in memory (hopfully zero)
	//cout << "Blocks in memory:  " << BlocksInMemory() << endl;

	return MStatus::kSuccess;
}

// Recursive function to crawl down tree looking for children and translate those that are understood
void NifTranslator::ImportNodes( NiAVObjectRef niAVObj, map< NiAVObjectRef, MDagPath > & objs, MObject parent )  {
	MObject obj;

	//cout << "Importing " << block << endl
	//	<< "Blocks in memory:  " << BlocksInMemory() << endl;

	////Stop at a non-node
	//if ( node == NULL )
	//	return;

	//This must be a node, so process its basic attributes	
	MFnTransform transFn;
	string name = niAVObj->GetName();
	int flags = niAVObj->GetFlags();
	if ( niAVObj->IsSameType(NiNode::TYPE) && ( (strstr(name.c_str(), "Bip") != NULL ) || ((flags & 8) == 0) ) ) {
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

	//TODO:  Re-implement skinning support in Niflib.  This may not be necessary anyway.
	////If this is a skinned shape, turn off inherit transform and use the World Transform
	////Some NIF files have the shapes parented to the skeleton which
	////messes up the deformation
	//if ( (block->GetBlockType() == "NiTriShape") && (block["Skin Instance"]->asLink().is_null() == false) ) {
	//	transFn.findPlug("inheritsTransform").setValue(false);
	//	transform = node->GetWorldBindPos();
	//}
	//else {
	//	transform = node->GetLocalBindPos();
	//}
	transform = niAVObj->GetLocalTransform();

	//put matrix into a float array
	float trans_arr[4][4];
	transform.AsFloatArr( trans_arr );

	transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
	transFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

	//Set name
	MFnDependencyNode dnFn;
	dnFn.setObject(obj);
	dnFn.setName(MString(name.c_str()));	

	//Add the newly created object to the objects list
	MDagPath path;
	transFn.getPath( path );
	objs[niAVObj] = path;

	//Call this function for any children
	NiNodeRef niNode = DynamicCast<NiNode>(niAVObj);
	if ( niNode != NULL ) {
		vector<NiAVObjectRef> children = niNode->GetChildren();
		for ( uint i = 0; i < children.size(); ++i ) {
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

MObject NifTranslator::ImportMaterial( NiMaterialPropertyRef niMatProp ) {
	//Create the material but don't connect it to parent yet
	MFnPhongShader phongFn;
	MObject obj = phongFn.create();

	//Don't mess with Ambient right now... ugly white
	//TODO: Make this optional?
	//block->["Ambient Color"]->asFloat3( color );
	//phongFn.setAmbientColor( MColor(color[0], color[1], color[2]) );

	Color3 color = niMatProp->GetDiffuseColor();
	phongFn.setColor( MColor(color.r, color.g, color.b) );

	
	//Set Specular color to 0 unless the mesh has a NiSpecularProperty
	phongFn.setSpecularColor( MColor( 0.0f, 0.0f, 0.0f) );

	//TODO: Can't get parent of a property anymore.  Maybe a way to get sibling properties?
	//blk_ref par = block->GetParent();
	//if (par.is_null() == false ) {
	//	blk_ref spec_prop = par["Properties"]->FindLink( "NiSpecularProperty");
	//	if ( spec_prop.is_null() == false && (spec_prop["Flags"] & 1) == true ) {
	//		//This mesh uses specular color - load it
	//		color = block["Specular Color"];
	//		phongFn.setSpecularColor( MColor(color[0], color[1], color[2]) );
	//	}
	//}

	color = niMatProp->GetEmissiveColor();
	phongFn.setIncandescence( MColor(color.r, color.g, color.b) );

	if ( color.r > 0.0 || color.g > 0.0 || color.b > 0.0) {
		phongFn.setGlowIntensity( 0.25 );
	}

	float glossiness = niMatProp->GetGlossiness();
	phongFn.setReflectivity( glossiness );

	float alpha = niMatProp->GetTransparency();
	//Maya's concept of alpha is the reverse of the NIF's concept
	alpha = 1.0f - alpha;
	phongFn.setTransparency( MColor( alpha, alpha, alpha, alpha) );

	string name = niMatProp->GetName();
	phongFn.setName( MString(name.c_str()) );

	return obj;
}

MDagPath NifTranslator::ImportMesh( NiTriBasedGeomRef niGeom, MObject parent ) {
	
	//Get NiTriBAsedGeomData
	NiTriBasedGeomDataRef niGeomData = niGeom->GetData();

	int NumVertices = niGeomData->GetVertexCount();

	vector<Vector3> nif_verts = niGeomData->GetVertices();
	MPointArray maya_verts(NumVertices);

	for (int i = 0; i < NumVertices; ++i) {
		maya_verts[i] = MPoint(nif_verts[i].x, nif_verts[i].y, nif_verts[i].z, 0.0f);
	}

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

	//--Examine properties--//
	vector<MString> uv_set_list;

	//Make invisible if bit flag 1 is set
	if ( (niGeom->GetFlags() & 1) == true ) {
		MPlug vis = meshFn.findPlug( MString("visibility") );
		vis.setValue(false);
	}

	//Create a list of the UV sets used by the texturing property if there is one
	NiPropertyRef niProp = niGeom->GetPropertyByType( NiTexturingProperty::TYPE );
	NiTexturingPropertyRef niTexProp;
	if ( niProp != NULL ) {
		niTexProp = DynamicCast<NiTexturingProperty>(niProp);
	}
	if ( niTexProp != NULL ) {
		int tx_count = niTexProp->GetTextureCount();

		for ( int i = 0; i < tx_count; ++i ) {
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
				}
			}
		}
	}

	

	// Get default (first) UV Set if there is one		
	if ( niGeomData->GetUVSetCount() > 0 ) {
		meshFn.clearUVs();
		vector<TexCoord> uv_set;
		//Arrays for maya
		MFloatArray u_arr(NumVertices), v_arr(NumVertices);

		for (int i = 0; i < niGeomData->GetUVSetCount(); ++i) {
			uv_set = niGeomData->GetUVSet(i);

			for (int j = 0; j < NumVertices; ++j) {
				u_arr[j] = uv_set[j].u;
				v_arr[j] = 1.0f - uv_set[j].v;
			}
			
			//Assign the UVs to the object
			if (i > 0) {
				meshFn.createUVSet( uv_set_list[i] );
			}
	
			meshFn.setUVs( u_arr, v_arr, &uv_set_list[i] );
			meshFn.assignUVs( maya_poly_counts, maya_connects, &uv_set_list[i] );

			////Delete default "map1" UV set
			//meshFn.deleteUVSet( MString("map1") );
		}
	}

	// Don't load the normals now because they glitch up the skinning (sigh)
	//// Load Normals
	//vector<Vector3> nif_normals(NumVertices);
	//nif_normals = data->GetNormals();

	//MVectorArray maya_normals(NumVertices);
	//MIntArray vert_list(NumVertices);
	//for (int i = 0; i < NumVertices; ++i) {
	//	maya_normals[i] = MVector(nif_normals[i].x, nif_normals[i].y, nif_normals[i].z );
	//	vert_list[i] = i;
	//}

	//meshFn.setVertexNormals( maya_normals, vert_list, MSpace::kObject );


	//Delete everything that was used to load verticies
	delete [] poly_counts;

	return meshPath;
}

void ExportNodes( map< string, blk_ref > & objs, blk_ref root );
void ExportFileTextures( map< string, blk_ref > & textures );