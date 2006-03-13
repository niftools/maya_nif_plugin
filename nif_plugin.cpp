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

	if ((nameLength > 4) && !strcasecmp(name+nameLength-4, ".nif")) {
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
	MFnPlugin plugin( obj, "NIFLA", PLUGIN_VERSION, "Any");

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
		blk_ref root= ReadNifTree( file.fullName().asChar() );

		//cout << "Importing Nodes..." << endl;
		//Import Nodes, starting at each child of the root
		map< blk_ref, MDagPath > objs;
		attr_ref children_attr = root["Children"];
		if ( children_attr.is_null() == false ) {
			//Root is a Parent Node
			list<blk_ref> root_children = children_attr->asLinkList();
			list<blk_ref>::iterator node_it;
			for (node_it = root_children.begin(); node_it != root_children.end(); ++node_it ) {
				ImportNodes( *node_it, objs );
			}
		} else {
			//Root is a lone block
			ImportNodes( root, objs );
		}
		

		//Report total number of blocks in memory
		//cout << "Blocks in memory:  " << BlocksInMemory() << endl;
		
		//--Import Data--//
		//cout << "Importing Data..." << endl;
		//maps to hold information about what has already been imported
		map< pair<blk_ref, blk_ref>, MObject > materials;
		map< blk_ref, MObject > textures;

		//Iterate through all nodes that were imported
		map< blk_ref, MDagPath >::iterator it;
		for ( it = objs.begin(); it != objs.end(); ++it ) {
			
			//Create some more friendly names for the block and its type
			blk_ref block = it->first;
			string blk_type = block->GetBlockType();

			//Import the data based on the type of node - NiTriShape only so far
			if ( blk_type == "NiTriShape" || blk_type == "NiTriStrips" ) {
				////If this is a skinned shape, turn off inherit transform
				////Some NIF files have the shapes parented to the skeleton which
				////messes up the deformation
				blk_ref skin_inst = block["Skin Instance"];
				//if ( skin_inst.is_null() == false ) {
				//	MFnDagNode nodeFn( it->second.node() );
				//	nodeFn.findPlug("inheritsTransform").setValue(false);
				//}

				//Import Mesh			
				MDagPath meshPath = ImportMesh( block["Data"], it->second.node() );

				//--Look for Materials--//
				MObject grpOb;
				MObject matOb;
				MObject txOb;

				blk_ref mat_block = block["Properties"]->FindLink( "NiMaterialProperty" );
				blk_ref tx_prop_block = block["Properties"]->FindLink( "NiTexturingProperty" );

				if ( mat_block.is_null() == false) {
					//Check to see if material has already been found
					if ( materials.find( pair<blk_ref, blk_ref>( mat_block, tx_prop_block) ) == materials.end() ) {
						//New material/texture combo  - import and add to the list
						matOb = ImportMaterial( mat_block );
						materials[ pair<blk_ref, blk_ref>( mat_block, tx_prop_block ) ] = matOb;
					} else {
						// Use the existing material
						matOb = materials[ pair<blk_ref, blk_ref>( mat_block, tx_prop_block ) ];
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
					if (tx_prop_block.is_null() == false ) {
						MFnDependencyNode nodeFn;
						blk_ref tx_block;
						TexDesc tx;

						//Get TexturingProperty Interface
						ITexturingProperty * tx_prop = QueryTexturingProperty( tx_prop_block );
						int tx_count = tx_prop->GetTextureCount();

						//Cycle through for each type of texture
						int uv_set = 0;
						for (int i = 0; i < tx_count; ++i) {
							tx = tx_prop->GetTexture( i );
							tx_block = tx.source;

							switch(i) {
								case DARK_MAP:
									//Temporary until/if Dark Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Dark Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case DETAIL_MAP:
									//Temporary until/if Detail Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Detail Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case GLOSS_MAP:
									//Temporary until/if Detail Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Gloss Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case BUMP_MAP:
									//Temporary until/if Bump Map Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Bump Map Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case DECAL_0_MAP:
									//Temporary until/if Decal Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Decal Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
							}

							if ( tx_block.is_null() == false) {
								//Check if texture has already been used
								if ( textures.find( tx_block ) == textures.end() ) {
									//New texture - import it and add it to the list
									txOb = ImportTexture( tx_block );
									textures[tx_block] = txOb;
								} else {
									//Already found, use existing one
									txOb = textures[tx_block];
								}
							
								nodeFn.setObject(txOb);
								MPlug tx_outColor = nodeFn.findPlug( MString("outColor") );
								switch(i) {
									case BASE_MAP:
										//Base Texture
										dgModifier.connect( tx_outColor, phongFn.findPlug("color") );
										//Check if Alpha needs to be used
										{
											blk_ref alpha = block["Properties"]->FindLink( "NiAlphaProperty");
											if ( alpha.is_null() == false && (alpha["Flags"] & 1) == true ) {
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
							++uv_set;
						}
					}
					
					dgModifier.doIt();

					//--Bind Skin if any--//
					if ( skin_inst.is_null() == false ) {
						//Get the NIF skin data interface
						blk_ref skin_data = skin_inst["Data"];
						ISkinData * data = (ISkinData*)skin_data->QueryInterface( ID_SKIN_DATA );

						//Build up the MEL command string
						string cmd = "skinCluster -tsb ";
						
						//Add list of bones to the command
						vector<blk_ref> bone_blks = data->GetBones();					

						for (unsigned int i = 0; i < bone_blks.size(); ++i) {
							cmd.append( objs[ bone_blks[i] ].partialPathName().asChar() );
							cmd.append( " " );
						}

						//Add mesh path to the command
						cmd.append( meshPath.partialPathName().asChar() );

						//Execute command to create skinCluster bound to specific bones
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

						//Set skin weights & bind pose for each bone
						for (unsigned int i = 0; i < bone_blks.size(); ++i) {
							////Get Bind Pose
							//float bind_pose[4][4];
							//INode * node = (INode*)bone_blks[i]->QueryInterface(Node);
							//node->GetWorldBindPos( bind_pose );
							//MFnMatrixData mat;
							//mat.set( MMatrix( bind_pose ) );

							////Set bone bind pose matrix
							//MFnTransform transFn(objs[bone_blks[i]]);
							//MPlug bindPose = transFn.findPlug("bindPose");
							////if ( bindPose != MObject::kNullObj ) {
							//	bindPose.setValue( mat.object() );
							////}

							//
							//MPlug plugBindPre = clusterFn.findPlug("bindPreMatrix") ;
							//plugBindPre = plugBindPre.elementByPhysicalIndex( i ) ;
							//plugBindPre.setValue( mat.object() );

							//MPlug plugBindPose = transFn.findPlug("bindPose");
							//plugBindPose.setValue( mat.object() ) ;

							//Get weights from NIF
							map<int, float> weight_map = data->GetWeights( bone_blks[i] );
							
							MFloatArray weight_list( gIt.count() );
							MIntArray influence_list(1);

							//Add this index to the influence list since we're setting one at a time
							influence_list[0] = i;
							
							//Iterate over all verticies in this mesh, setting the weights
							for ( int j = 0; j < gIt.count(); ++j ) {
								if ( weight_map.find(j) != weight_map.end() ) {
									weight_list[j] = weight_map[j];
								}
								else
									weight_list[j] = 0.0f;
							}
							
							clusterFn.setWeights( meshPath, vertices, influence_list, weight_list, true );
						}
					}			
				}
			}
		}



		//--Reposition Nodes--//
		for ( it = objs.begin(); it != objs.end(); ++it ) {
			MFnTransform transFn( it->second );
			Matrix44 transform;
			INode * node = (INode*)it->first->QueryInterface(ID_NODE);
			transform = node->GetLocalTransform();
			float trans_arr[4][4];
			transform.AsFloatArr( trans_arr );

			transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
			
		}

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
void NifTranslator::ImportNodes( blk_ref block, map< blk_ref, MDagPath > & objs, MObject parent )  {
	MObject obj;
	INode * node = QueryNode(block);

	//cout << "Importing " << block << endl
	//	<< "Blocks in memory:  " << BlocksInMemory() << endl;

	//Stop at a non-node
	if ( node == NULL )
		return;

	//This must be a node, so process its basic attributes	
	MFnTransform transFn;
	string name = block["Name"];
	int flags = block["Flags"];
	if ( block->GetBlockType() == "NiNode" && ( (strstr(name.c_str(), "Bip") != NULL ) || ((flags & 8) == 0) ) ) {
		// This is a bone, create an IK joint parented to the given parent
		MFnIkJoint IKjointFn;
		obj = IKjointFn.create( parent );

		//Set Transofrm Fn to work on the new IK joint
		transFn.setObject( obj );
	}
	else {
		//Not a bone, create a transform node parented to the given parent
		obj = transFn.create( parent );
	}

	//--Set the Transform Matrix--//
	Matrix44 transform;

	//If this is a skinned shape, turn off inherit transform and use the World Transform
	//Some NIF files have the shapes parented to the skeleton which
	//messes up the deformation
	if ( (block->GetBlockType() == "NiTriShape") && (block["Skin Instance"]->asLink().is_null() == false) ) {
		transFn.findPlug("inheritsTransform").setValue(false);
		transform = node->GetWorldBindPos();
	}
	else {
		transform = node->GetLocalBindPos();
	}

	//put matrix into a float array
	float trans_arr[4][4];
	transform.AsFloatArr( trans_arr );

	transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
	transFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

	// If the node has a name, set it
	attr_ref name_attr = block["Name"];
	if ( name_attr.is_null() == false ) {
		MFnDependencyNode dnFn;
		dnFn.setObject(obj);
		string name = name_attr;
		dnFn.setName(MString(name.c_str()));	
	}

	//Add the newly created object to the objects list
	MDagPath path;
	transFn.getPath( path );
	objs[block] = path;

	//Call this function for all children
	attr_ref child_attr = block["Children"];
	if ( child_attr.is_null() == false) {
		list<blk_ref> links = child_attr;
		list<blk_ref>::iterator it;
		for (it = links.begin(); it != links.end(); ++it) {
			if ( it->is_null() == false && (*it)->GetParent() == block ) {
				ImportNodes( *it, objs, obj );
			}
		}
	}
}

MObject NifTranslator::ImportTexture( blk_ref block ) {
	MObject obj;

	//block = block->GetLink("Base Texture");
	
	//If the texture is stored externally, use it
	TexSource ts = block["Texture Source"];

	if ( ts.useExternal == true ) {
		//An external texture is used, create a texture node
		MFnDependencyNode nodeFn;
		//obj = nodeFn.create( MFn::kFileTexture, MString( ts.fileName.c_str() ) );
		obj = nodeFn.create( MString("file"), MString( ts.fileName.c_str() ) );

		string file_path = texture_path + ts.fileName;

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

MObject NifTranslator::ImportMaterial( blk_ref block ) {
	//Create the material but don't connect it to parent yet
	MFnPhongShader phongFn;
	MObject obj = phongFn.create();

	//Don't mess with Ambient right now... ugly white
	//block->["Ambient Color"]->asFloat3( color );
	//phongFn.setAmbientColor( MColor(color[0], color[1], color[2]) );

	Float3 color = block["Diffuse Color"];
	phongFn.setColor( MColor(color[0], color[1], color[2]) );

	//Set Specular color to 0 unless the mesh has a NiSpecularProperty
	phongFn.setSpecularColor( MColor( 0.0, 0.0, 0.0) );

	blk_ref par = block->GetParent();
	if (par.is_null() == false ) {
		blk_ref spec_prop = par["Properties"]->FindLink( "NiSpecularProperty");
		if ( spec_prop.is_null() == false && (spec_prop["Flags"] & 1) == true ) {
			//This mesh uses specular color - load it
			color = block["Specular Color"];
			phongFn.setSpecularColor( MColor(color[0], color[1], color[2]) );
		}
	}

	color = block["Emissive Color"];
	phongFn.setIncandescence( MColor(color[0], color[1], color[2]) );

	if ( color[0] > 0.0 || color[1] > 0.0 || color[2] > 0.0) {
		phongFn.setGlowIntensity( 0.25 );
	}

	float glossiness = block["Glossiness"];
	phongFn.setReflectivity( glossiness );

	float alpha = block["Alpha"];
	phongFn.setTranslucenceCoeff( alpha );

	string name = block["Name"];
	phongFn.setName( MString(name.c_str()) );

	return obj;
}

MDagPath NifTranslator::ImportMesh( blk_ref block, MObject parent ) {
	//Get ShapeData and TriShapeData interface
	IShapeData * data = QueryShapeData(block);
	ITriShapeData * tri_data = QueryTriShapeData(block);
	ITriStripsData * strip_data = QueryTriStripsData(block);

	if ( data == NULL ) { throw runtime_error("IShapeData interface not returend"); }
	if ( tri_data == NULL && strip_data == NULL ) { throw runtime_error("Mesh did not return a Triangle or Strip interface."); }

	int NumVertices = data->GetVertexCount();

	vector<Vector3> nif_verts = data->GetVertices();
	MPointArray maya_verts(NumVertices);

	for (int i = 0; i < NumVertices; ++i) {
		maya_verts[i] = MPoint(nif_verts[i].x, nif_verts[i].y, nif_verts[i].z, 0.0f);
	}

	//Get Polygons
	int NumPolygons = 0;
	vector<Triangle> triangles;
	if ( tri_data != NULL ) {
		//We're dealing with Triangle Data
		triangles = tri_data->GetTriangles();
		
	} else {
		//We're dealing with Strip Data
		triangles = strip_data->GetTriangles();
	}

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
	vector<Color4> nif_colors = data->GetColors();
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

	//Get Parent to examine properties
	blk_ref par = block->GetParent();
	vector<MString> uv_set_list;
	
	if ( par.is_null() == false  ) {
		//Make invisible if bit flag 1 is set
		if ( (par["Flags"] & 1) == true ) {
			MPlug vis = meshFn.findPlug( MString("visibility") );
			vis.setValue(false);
		}

		//Create a list of the UV sets used by the texturing property if there is one
		blk_ref tx_prop_blk = par["Properties"]->FindLink( "NiTexturingProperty");
		if ( tx_prop_blk.is_null() == false ) {
			ITexturingProperty * tx_prop = QueryTexturingProperty( tx_prop_blk );
			int tx_count = tx_prop->GetTextureCount();

			for ( int i = 0; i < tx_count; ++i ) {
				if ( tx_prop->GetTexture(i).isUsed == true ) {
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
	}

	

	// Get default (first) UV Set if there is one		
	if ( data->GetUVSetCount() > 0 ) {
		meshFn.clearUVs();
		vector<TexCoord> uv_set;
		//Arrays for maya
		MFloatArray u_arr(NumVertices), v_arr(NumVertices);

		for (int i = 0; i < data->GetUVSetCount(); ++i) {
			uv_set = data->GetUVSet(i);

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

//--NifTranslator::writer--//

//This routine is called by Maya when it is necessary to save a file of a type supported by this translator.
//Responsible for traversing all objects in the current Maya scene, and writing a representation to the given
//file in the supported format.
MStatus NifTranslator::writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	
	try {
		//Create new root node
		blk_ref root = CreateBlock("NiNode");
		root["Name"] = "Scene Root";
		root["Scale"]->Set(1.0f);

		//A map to hold associations between DAG paths and NIF blocks
		map< string, blk_ref > nodes;

		//Export nodes and get back a map of DAG path to Nif block
		ExportNodes( nodes, root );

		//A map to hold associations between names and NIF blocks
		map< string, blk_ref > textures;

		//Export file textures and get back a map of DAG path to Nif block
		ExportFileTextures( textures );

		//--Write finished NIF file--//

		WriteNifTree(file.fullName().asChar(), root);
	}
	catch( exception & e ) {
		cout << "Error:  " << e.what() << endl;
		return MStatus::kFailure;
	}
	catch( ... ) {
		cout << "Error:  Unknown Exception." << endl;
		return MStatus::kFailure;
	}
	
	return MS::kSuccess;
}

void ExportMesh( blk_ref tri_shape, MObject mesh ) {
	blk_ref tsd_blk = CreateBlock("NiTriShapeData");
	tri_shape["Data"] = tsd_blk;
	IShapeData * shape_data = QueryShapeData( tsd_blk );
	ITriShapeData * tri_data = QueryTriShapeData( tsd_blk );

	MFnMesh fn(mesh);

	// this will hold the returned vertex positions
	MPointArray vts;

	// use the function set to get the points
	fn.getPoints(vts);

	vector<Vector3> nif_vts( vts.length() );

	// only want non-history items
	for( int i=0; i != vts.length(); ++i ) {
		nif_vts[i].x = float(vts[i].x);
		nif_vts[i].y = float(vts[i].y);
		nif_vts[i].z = float(vts[i].z);
	}

	

	// this will hold the returned vertex positions
	MFloatVectorArray nmls;

	// use the function set to get the points
	fn.getNormals(nmls);

	vector<Vector3> nif_nmls( nif_vts.size() );
	//Set them to zero for now so we can detect when a vertex needs to be split
	for( int i=0; i != vts.length(); ++i ) {
		nif_nmls[i].Set( 0.0f, 0.0f, 0.0f );
	}

	vector<Triangle> nif_tris;

	// this will hold references to the shaders used on the meshes
	MObjectArray Shaders;

	// this is used to hold indices to the materials returned in the object array
	MIntArray    FaceIndices;

	// get the shaders used by the i'th mesh instance
	// Assume this is not instanced for now
	// TODO support instanceing properly
	fn.getConnectedShaders(0,Shaders,FaceIndices);

	if ( Shaders.length() <= 1 ) {
		// TODO export material if there is one
		//Shaders[0] holds material for all faces if Shaders.length() == 1
		
		// attach an iterator to the mesh
		MItMeshPolygon itPoly(mesh);

		//int face = 0;

		// Create a list of faces with vertex IDs, and duplicate normals so they have the same ID
		while(!itPoly.isDone()) {

			//cout << "Face " << face << ":" << endl;
			//++face;

            if ( itPoly.polygonVertexCount() != 3 ) {
				throw runtime_error( "This exporter only supports triangulated meshes.  Please triangulate all meshes before exporting with Polygons->Triangulate" );
			}

			Triangle new_face;

			// print all vertex, normal and uv indices
			for( int i=0; i < 3; ++i ) {

				//cout << "   Vertex " << i << ":" << endl;

                int v = itPoly.vertexIndex(i);
				int n = itPoly.normalIndex(i);

				//Make a new vertex if the normal has already been set
				if ( nif_nmls[v].x != 0.0f || nif_nmls[v].y != 0.0f || nif_nmls[v].z != 0.0f ) {
					//cout << "      Normal has already been set.  Cloning vertex." << endl;

					//Make sure new normal isn't basically the same as the old one before we make a new vertex for it
					if ( abs( nif_nmls[v].x - float(nmls[n].x)) > 0.00001f && abs( nif_nmls[v].y - float(nmls[n].y)) > 0.00001f && abs( nif_nmls[v].z - float(nmls[n].z)) > 0.00001f ) {
					
						//Clone this vertex so it can have its own normal
						nif_vts.push_back( nif_vts[v] );

						//Set the new vertex index
						v = nif_vts.size() - 1;

						//Add a new normal to the end of the nif_nmls list
						nif_nmls.resize( nif_vts.size() );

						//cout << "      New vertex count:  " << nif_vts.size() << endl;
					}
				}
				
				//Put normal values into the same index as the new or existing vertex
				nif_nmls[v].Set( float(nmls[n].x), float(nmls[n].y), float(nmls[n].z) );


				//TODO: UV Coordinates
				//if(bUvs) {
				//  	

				//	// have to get the uv index seperately
				//	int uv_index;

				//	// ouput each uv index
				//	for(int k=0;k<sets.length();++k) {
				//	  	

				//		itPoly.getUVIndex(i,uv_index,&sets[k]);

				//		cout << " " << uv_index;
				//	}
				//}

				//Add the vertex coordinate to the new face
				new_face[i] = unsigned short(v);
			}

			//Push the face into the face list
			nif_tris.push_back(new_face);

			// move to next face
			itPoly.next();

		}
		
		//Set data in NIf block
		shape_data->SetVertexCount( nif_vts.size() );
		shape_data->SetVertices( nif_vts );
		shape_data->SetNormals( nif_nmls );
		tri_data->SetTriangles( nif_tris );
	} else {
		throw runtime_error( "For now you must use only one material per mesh." );
		// if more than one material is used, write out the face indices the materials
		// are applied to.

			//TODO Break up mesh into sub-meshes in this case
		//	cout << "\t\tmaterials " << Shaders.length() << endl;

		//	// i'm going to sort the face indicies into groups based on
		//	// the applied material - might as well... ;)
		//	vector< vector< int > > FacesByMatID;

		//	// set to same size as num of shaders
		//	FacesByMatID.resize(Shaders.length());

		//	// put face index into correct array
		//	for(int j=0;j < FaceIndices.length();++j)
		//	{
		//		FacesByMatID[ FaceIndices[j] ].push_back(j);
		//	}

		//	// now write each material and the face indices that use them
		//	for(int j=0;j < Shaders.length();++j)
		//	{
		//		cout << "\t\t\t"
		//			<< GetShaderName( Shaders[j] ).asChar()
		//			<< "\n\t\t\t"
		//			<< FacesByMatID[j].size()
		//			<< "\n\t\t\t\t";

		//		vector< int >::iterator it = FacesByMatID[j].begin();
		//		for( ; it != FacesByMatID[j].end(); ++it )
		//		{
		//			cout << *it << " ";
		//		}
		//		cout << endl;
		//	}
		//}
		//break;
	}
}

//--Itterate through all of the Maya DAG nodes, adding them to the tree--//
void ExportNodes( map< string, blk_ref > & objs, blk_ref root ) {

	//Create iterator to go through all DAG nodes depth first
	MItDag it(MItDag::kDepthFirst);
	while(!it.isDone()) {

		// attach a function set for a dag node to the
		// object. Rather than access data directly,
		// we access it via the function set.
		MFnDagNode nodeFn(it.item());

		// only want non-history items
		if( !nodeFn.isIntermediateObject() ) {

			//Check if this is a transform node
			if ( it.item().hasFn(MFn::kTransform) ) {
				//This is a transform node, check if it is an IK joint or a shape

				blk_ref block;

				bool tri_shape = false;
				MObject matching_child;

				//Check to see what kind of node we should create
				for( int i=0; i!=nodeFn.childCount(); ++i ) {
					// get a handle to the child
					if ( nodeFn.child(i).hasFn(MFn::kMesh) ) {
						tri_shape = true;
						matching_child = nodeFn.child(i);
						break;
					}

				}
	
				if ( tri_shape == true ) {
					//NiTriShape
					block = CreateBlock("NiTriShape");
					ExportMesh( block, matching_child );
				} else {
					//NiNode
					block = CreateBlock("NiNode");
				}

				//Fix name
				string name = string( nodeFn.name().asChar() );
				replace(name.begin(), name.end(), '_', ' ');
				block["Name"] = name;
				MMatrix my_trans= nodeFn.transformationMatrix();

				//--Extract Scale from first 3 rows--//
				double scale[3];
				for (int r = 0; r < 3; ++r) {
					//Get scale for this row
					scale[r] = sqrt(my_trans[r][0] * my_trans[r][0] + my_trans[r][1] * my_trans[r][1] + my_trans[r][2] * my_trans[r][2] + my_trans[r][3] * my_trans[r][3]);
				
					//Normalize the row by dividing each factor by scale
					my_trans[r][0] /= scale[r];
					my_trans[r][1] /= scale[r];
					my_trans[r][2] /= scale[r];
					my_trans[r][3] /= scale[r];
				}

				//Get Rotatoin
				Matrix33 nif_rotate;
				for ( int r = 0; r < 3; ++r ) {
					for ( int c = 0; c < 3; ++c ) {
						nif_rotate[r][c] = float(my_trans[r][c]);
					}
				}
				//Get Translation
				Float3 nif_trans;
				nif_trans[0] = float(my_trans[3][0]);
				nif_trans[1] = float(my_trans[3][1]);
				nif_trans[2] = float(my_trans[3][2]);
				//nif_trans.Set( float(my_trans[3][0]), float(my_trans[3][1]), float(my_trans[3][2]) );
				
				//Store Transform Values in block
				block["Scale"] = float(scale[0] + scale[1] + scale[2]) / 3.0f;
				block["Rotation"] = nif_rotate;
				block["Translation"] = nif_trans;

				//Associate block with node DagPath
				string path = nodeFn.fullPathName().asChar();
				objs[path] = block;
			}
		}

		// move to next node
		it.next();
	}

						////Is this a joint and therefore has bind pose info?
					//if ( it.item().hasFn(MFn::kJoint) ) {
					//}
				/*}*/
		//}

		//// get the name of the node
		//MString name = fn.name();

		//// write the node type found
		//cout << "node: " << name.asChar() << endl;

		//// write the info about the children
		//cout <<"num_kids " << fn.childCount() << endl;

		//for(int i=0;i<fn.childCount();++i) {
//			// get the MObject for the i'th child
		//MObject child = fn.child(i);

		//// attach a function set to it
		//MFnDagNode fnChild(child);

		//// write the child name
		//cout << "\t" << fnChild.name().asChar();
		//cout << endl;

	//Loop through again, this time connecting the parents to the children
	it.reset();
	while(!it.isDone()) {
		MFnDagNode nodeFn(it.item());

		//Get path to this block
		string blk_path = nodeFn.fullPathName().asChar();

		for( unsigned int i = 0; i < nodeFn.parentCount(); ++i ) {
  			// get the MObject for the i'th parent
			MObject parent = nodeFn.parent(i);

			// attach a function set to it
			MFnDagNode parentFn(parent);

			string par_path = parentFn.fullPathName().asChar();

			//Check if parent exists in map we've built
			if ( objs.find( par_path ) != objs.end() ) {
				//Object found
				if ( objs[par_path]->GetBlockType() != "NiTriShape" ) {
					objs[par_path]["Children"]->AddLink( objs[blk_path] );
				}
			} else {
				//See if block was created at all
				if ( objs.find( blk_path ) != objs.end() ) {
					//Block was created, parent to scene root
					root["Children"]->AddLink( objs[blk_path] );
				}
			}
		}

		// move to next node
		it.next();
	}
}

//--Iterate through all file textures, creating NiSourceTexture blocks for each one--//
void ExportFileTextures( map< string, blk_ref > & textures ) {
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
		blk_ref ni_tex = CreateBlock("NiSourceTexture");
		TexSource tx_src;
		tx_src.useExternal = true;
		MString fname;
		ftn.getValue(fname);
		tx_src.fileName = fname.asChar();
		ni_tex["Texture Source"] = tx_src;
		ni_tex["Pixel Layout"] = 5; //default
		ni_tex["Use Mipmaps"] = 2; //default
		ni_tex["Alpha Format"] = 3; //default
		ni_tex["Unknown Byte"] = 1; //default
		
		//Associate block with fileTexture DagPath
		string path = fn.name().asChar();
		textures[path] = ni_tex;

		//TEMP: Write the block so we can see it worked
		cout << ni_tex->asString();

		// get next fileTexture
		it.next();
	} 
}