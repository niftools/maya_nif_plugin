#include "include/Importers/NifMeshImporter.h"

MDagPath NifMeshImporter::ImportMesh( NiAVObjectRef root, MObject parent ) {
	//out << "ImportMesh() begin" << endl;

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

	//out << "Try out ComplexShape::Merge on" << root << endl;
	ComplexShape cs;

	cs.Merge( root );

	vector<WeightedVertex> nif_verts = cs.GetVertices();
	unsigned NumVertices = unsigned(nif_verts.size());
	//out << "Num Vertices:  " << NumVertices << endl;

	MPointArray maya_verts(NumVertices);

#if _DEBUG
	//we manually extract the vertices from the mesh 
	//all the vertices that have the exact position are ignored 
	//in a way they are merged
	NiTriBasedGeomRef nif_geometry = DynamicCast<NiTriBasedGeom>(root);
	NiGeometryDataRef nif_geometry_data = nif_geometry->GetData();
	vector<Vector3> nif_vertices = nif_geometry_data->GetVertices();
	vector<Vector3> nif_merged_vertices;

	for(int i = 0;i < nif_geometry_data->GetVertexCount(); i++) {
		int duplicate = 0;

		for(int j = 0;j < nif_merged_vertices.size(); j++) {
			if(nif_merged_vertices[j].x == nif_vertices[i].x && nif_merged_vertices[j].y == nif_vertices[i].y && nif_merged_vertices[j].z == nif_vertices[i].z) {
				duplicate = 1;
			}
		}

		if(duplicate == 0) {
			nif_merged_vertices.push_back(nif_vertices[i]);
		}
	}


	//if the manually extracted vertices are ok it executes the else branch
	//else if the number of merged vertices differ from the complexshape vertex count, we use the complexshape vertices
	if(NumVertices != nif_merged_vertices.size()) {
		for (unsigned i = 0; i < NumVertices; ++i) {
			maya_verts[i] = MPoint(nif_verts[i].position.x, nif_verts[i].position.y, nif_verts[i].position.z, 0.0f);
		}
	} else {
		for (unsigned i = 0; i < NumVertices; ++i) {
			maya_verts[i] = MPoint(nif_merged_vertices[i].x, nif_merged_vertices[i].y, nif_merged_vertices[i].z, 0.0f);
		}
	}
#else 
	for (unsigned i = 0; i < NumVertices; ++i) {
		maya_verts[i] = MPoint(nif_verts[i].position.x, nif_verts[i].position.y, nif_verts[i].position.z, 0.0f);
	}
#endif

	//out << "Getting polygons..." << endl;
	//Get Polygons
	int NumPolygons = 0;
	vector<ComplexFace> niRawFaces = cs.GetFaces();

	MIntArray maya_poly_counts;
	MIntArray maya_connects;
	vector<ComplexFace> niFaces;

	//float max_dif = 0;
	//int count_dif = 0;

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

	//#if _DEBUG
	//
	//	cout<<"OBJ dump"<<endl<<endl<<endl<<endl;
	//	for(int i = 0;i < maya_verts.length(); i++) {
	//		cout<<"v "<<maya_verts[i].x<<"  "<<maya_verts[i].y<<" "<<maya_verts[i].z<<endl;
	//	}
	//
	//	int j = 0;
	//
	//	for(int i = 0;i < maya_poly_counts.length(); i++) {
	//		cout<<"f ";
	//		for(int k = j; k < j + maya_poly_counts[i]; k++) {
	//			cout<<(maya_connects[k] + 1)<<" ";
	//		}
	//		cout<<endl;
	//		j += maya_poly_counts[i];
	//	}
	//#endif

	//NumPolygons = triangles.size();
	NumPolygons = niFaces.size();
	//out << "Num Polygons:  " << NumPolygons << endl;

	//MIntArray maya_connects( connects, NumPolygons * 3 );

	//Create Mesh with empty default UV set at first
	MDagPath meshPath;
	MFnMesh meshFn;

	MStatus stat;

	meshFn.create( NumVertices, maya_poly_counts.length(), maya_verts, maya_poly_counts, maya_connects, parent, &stat );

	if ( stat != MS::kSuccess ) {
		//out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to create mesh.");
	}

	meshFn.getPath( meshPath );

	//out << "Importing vertex colors..." << endl;
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
			//out << stat.errorString().asChar() << endl;
			throw runtime_error("Failed to set Colors.");
		}
	}

	//out << "Creating a list of the UV sets..." << endl;
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

	/*out << "UV Set List:  "  << endl;
	for ( unsigned int i = 0; i < uv_set_list.size(); ++i ) {
		out << uv_set_list[i].asChar() << endl;
	}*/

	//out << "Getting default UV set..." << endl;
	// Get default (first) UV Set if there is one		
	if ( niUVSets.size() > 0 ) {
		meshFn.clearUVs();
		vector<TexCoord> uv_set;

		//out << "Loop through " << niUVSets.size() << " UV sets..." << endl;
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

			//out << "Check if a UV set needs to be created..." << endl;
			if ( uv_set_name != "map1" ) {
				meshFn.createUVSet( uv_set_name );
				//out << "Creating UV Set:  " << uv_set_name.asChar() << endl;
			} else {
				//Clear out the current UV set
				//meshFn.clearUVs( &uv_set_name );
			}

			//out << "Set UVs...  u_arr:  " << u_arr.length() << " v_arr:  " << v_arr.length() << " uv_set_name " << uv_set_name.asChar() << endl;
			MStatus stat = meshFn.setUVs( u_arr, v_arr, &uv_set_name );
			if ( stat != MS::kSuccess ) {
				//out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to set UVs.");
			}

			//out << "Create list of which UV to assign to each polygon vertex";
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
				//out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to assign UVs.");
			}
		}
	}

	//See if the user wants us to import the normals
	if ( this->translatorOptions->import_normals ) {
		//out << "Getting Normals..." << endl;
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
				//out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to set Normals.");
			}
		}
	}

	//out << "Importing Materials and textures" << endl;
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
		this->translatorUtils->ConnectShader( propGroups[i], meshPath, sel_lists[i] );
	}

	//out << "Bind skin if any" << endl;
	//--Bind Skin if any--//

	vector<NiNodeRef> bone_nodes = cs.GetSkinInfluences();
	if ( bone_nodes.size() != 0 ) {
		//Build up the MEL command string
		string cmd = "skinCluster -tsb ";

		//out << "Add list of bones to the command" << endl;
		//Add list of bones to the command
		for (unsigned int i = 0; i < bone_nodes.size(); ++i) {
			cmd.append( this->translatorData->importedNodes[ StaticCast<NiAVObject>(bone_nodes[i]) ].partialPathName().asChar() );
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

	//out << "ImportMesh() end" << endl;

	//some kind of weird bug in Maya 2012
	//vertices may have their positions changed when creating a mesh so they have to be repositioned
	//may be in other Maya versions
#if MAYA_API_VERSION == 201200
	MItMeshVertex vert_iter(parent);

	int j = 0;

	while(!vert_iter.isDone()) {
		vert_iter.setPosition(maya_verts[j]);
		vert_iter.next();
		j++;
	}
#endif

	return meshPath;
}

NifMeshImporter::NifMeshImporter() {

}

NifMeshImporter::NifMeshImporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions,translatorData,translatorUtils) {

}

NifMeshImporter::~NifMeshImporter() {

}

std::string NifMeshImporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose);
	out<<"NifTranslatorMeshImporter"<<endl;

	return out.str();
}

const Type& NifMeshImporter::GetType() const {
	return TYPE;
}

const Type NifMeshImporter::TYPE("NifMeshImporter",&NifTranslatorFixtureItem::TYPE);
