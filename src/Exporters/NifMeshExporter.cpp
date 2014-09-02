#include "Exporters/NifMeshExporter.h"

NifMeshExporter::NifMeshExporter() {

}

NifMeshExporter::NifMeshExporter( NifNodeExporterRef nodeExporter, NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {
		this->nodeExporter = nodeExporter;
}


void NifMeshExporter::EnumerateSkinClusters()
{
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

				this->translatorData->meshClusters[ meshFn.fullPathName().asChar() ].push_back( clusterObj );

				//out << "Cluster:  "  << clusterFn.name().asChar() << "  Mesh:  " << meshFn.fullPathName().asChar() <<endl;
			}
		}
	}
}


void NifMeshExporter::ExportMesh( MObject dagNode )
{
	//out << "NifTranslator::ExportMesh {";
	ComplexShape cs;
	MStatus stat;
	MObject mesh;

	//Find Mesh child of given transform object
	MFnDagNode nodeFn(dagNode);

	cs.SetName( this->translatorUtils->MakeNifName(nodeFn.name()) );


	for( int i = 0; i != nodeFn.childCount(); ++i ) {
		// get a handle to the child
		if ( nodeFn.child(i).hasFn(MFn::kMesh) ) {
			MFnMesh tempFn( nodeFn.child(i) );
			//No history items
			if ( !tempFn.isIntermediateObject() ) {
				//out << "Found a mesh child." << endl;
				mesh = nodeFn.child(i);
				break;
			}
		}
	}

	MFnMesh visibleMeshFn(mesh, &stat);
	if ( stat != MS::kSuccess ) {
		//out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to create visibleMeshFn.");
	}

	//out << visibleMeshFn.name().asChar() << ") {" << endl;
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

	//out << "Use the function set to get the points" << endl;
	// use the function set to get the points
	stat = meshFn.getPoints(vts);
	if ( stat != MS::kSuccess ) {
		//out << stat.errorString().asChar() << endl;
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

	//out << "Use the function set to get the colors" << endl;
	MColorArray myColors;
	meshFn.getFaceVertexColors( myColors );

	//out << "Prepare NIF color vector" << endl;
	vector<Color4> niColors( myColors.length() );
	for( unsigned int i = 0; i < myColors.length(); ++i ) {
		niColors[i] = Color4( myColors[i].r, myColors[i].g, myColors[i].b, myColors[i].a );
	}
	cs.SetColors( niColors );


	// this will hold the returned vertex positions
	MFloatVectorArray nmls;

	//out << "Use the function set to get the normals" << endl;
	// use the function set to get the normals
	stat = meshFn.getNormals( nmls, MSpace::kTransform );
	if ( stat != MS::kSuccess ) {
		//out << stat.errorString().asChar() << endl;
		throw runtime_error("Failed to get normals");
	}

	//out << "Prepare NIF normal vector" << endl;
	vector<Vector3> nif_nmls( nmls.length() );
	for( int i=0; i != nmls.length(); ++i ) {
		nif_nmls[i].x = float(nmls[i].x);
		nif_nmls[i].y = float(nmls[i].y);
		nif_nmls[i].z = float(nmls[i].z);
	}
	cs.SetNormals( nif_nmls );

	//out << "Use the function set to get the UV set names" << endl;
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

	//out << "Get the connected shaders" << endl;
	// get the shaders used by the i'th mesh instance
	// Assume this is not instanced for now
	// TODO support instancing properly
	stat = visibleMeshFn.getConnectedShaders(0,Shaders,FaceIndices);

	if ( stat != MS::kSuccess ) {
		//out << stat.errorString().asChar() << endl;
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

		//out << "Found attached shader:  ";
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

		//out << fnDep.name().asChar() << endl;
		vector<NiPropertyRef> niProps = this->translatorData->shaders[ fnDep.name().asChar() ];

		propGroups.push_back( niProps );
	}
	cs.SetPropGroups( propGroups );

	//out << "Export vertex and normal data" << endl;
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
					//out << stat.errorString().asChar() << endl;
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
	if ( this->translatorData->meshClusters.find( visibleMeshFn.fullPathName().asChar() ) != this->translatorData->meshClusters.end() ) {
		const vector<MObject> & clusters = this->translatorData->meshClusters[ visibleMeshFn.fullPathName().asChar() ];
		//for ( vector<MObject>::const_iterator cluster = clusters.begin(); cluster != clusters.end(); ++cluster ) {
		//TODO:  Support shapes with more than one skin cluster affecting them
		if ( clusters.size() > 1 ) {
			throw runtime_error("Objects with multiple skin clusters affecting them are not currently supported.  Try deleting the history and re-binding them.");
		}

		vector<MObject>::const_iterator cluster = clusters.begin();
		if ( cluster->isNull() != true ) {	
			MFnSkinCluster clusterFn(*cluster);


			//out << "Processing skin..." << endl;
			//Get path to visible mesh
			MDagPath meshPath;
			visibleMeshFn.getPath( meshPath );

			//out << "Getting a list of all verticies in this mesh" << endl;
			//Get a list of all vertices in this mesh
			MFnSingleIndexedComponent compFn;
			MObject vertices = compFn.create( MFn::kMeshVertComponent );
			MItGeometry gIt(meshPath);
			MIntArray vertex_indices( gIt.count() );
			for ( int vert_index = 0; vert_index < gIt.count(); ++vert_index ) {
				vertex_indices[vert_index] = vert_index;
			}
			compFn.addElements(vertex_indices);

			//out << "Getting Influences" << endl;
			//Get influences
			MDagPathArray myBones;
			clusterFn.influenceObjects( myBones, &stat );

			//out << "Creating a list of NiNodeRefs of influences." << endl;
			//Create list of NiNodeRefs of influences
			vector<NiNodeRef> niBones( myBones.length() );
			for ( unsigned int bone_index = 0; bone_index < niBones.size(); ++bone_index ) {
				if ( this->translatorData->nodes.find( myBones[bone_index].fullPathName().asChar() ) == this->translatorData->nodes.end() ) {
					//There is a problem; one of the joints was not exported.  Abort.
					throw runtime_error("One of the joints necessary to export a bound skin was not exported.");
				}
				niBones[bone_index] = this->translatorData->nodes[ myBones[bone_index].fullPathName().asChar() ];
			}

			//out << "Getting weights from Maya" << endl;
			//Get weights from Maya
			MDoubleArray myWeights;
			unsigned int bone_count = myBones.length();
			stat = clusterFn.getWeights( meshPath, vertices, myWeights, bone_count );
			if ( stat != MS::kSuccess ) {
				//out << stat.errorString().asChar() << endl;
				throw runtime_error("Failed to get vertex weights.");
			}

			//out << "Setting skin influence list in ComplexShape" << endl;
			//Set skin information in ComplexShape
			cs.SetSkinInfluences( niBones );

			//out << "Adding weights to ComplexShape vertices" << endl;
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

	//out << "Setting vertex info" << endl;
	//Set vertex info now that any skins have been processed
	cs.SetVertices( nif_vts );

	//ComplexShape is now complete, so split it

	//Get parent
	NiNodeRef parNode = this->translatorUtils->GetDAGParent( dagNode );
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
	this->nodeExporter->ExportAV(tempAV, dagNode );

	bool tangent_space = (this->translatorOptions->exportTangentSpace == "obliviontangentspace");
	//out << "Split ComplexShape from " << meshFn.name().asChar() << endl;
	NiAVObjectRef avObj = cs.Split( parNode, tempAV->GetLocalTransform() * transform, this->translatorOptions->exportBonesPerSkinPartition, 
		this->translatorOptions->exportAsTriStrips, tangent_space, this->translatorOptions->exportMinimumVertexWeight );

	//out << "Get the NiAVObject portion of the root of the split" <<endl;
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
			if( this->translatorOptions->exportMorrowindRename ) {
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


	//out << "}" << endl;
}

void NifMeshExporter::ApplyAllSkinOffsets( NiAVObjectRef & root )
{
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

string NifMeshExporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifMeshExporter"<<endl;

	return out.str();
}

const Type& NifMeshExporter::GetType() const {
	return TYPE;
}

const Type NifMeshExporter::TYPE("NifMeshExporter",&NifTranslatorRefObject::TYPE);
