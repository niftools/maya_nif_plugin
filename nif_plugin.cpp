// Maya NIF File Translator Plugin
// Based on information from NIFLA

/* Copyright (c) 2005, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

   * Neither the name of the NIF File Format Library and Tools
     project nor the names of its contributors may be used to endorse
     or promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE. */

//--Includes--//
#include <maya/MDagPath.h>
#include <maya/MEulerRotation.h>
#include <maya/MFloatArray.h>
#include <maya/MFnBase.h>
#include <maya/MFnComponent.h>
#include <maya/MFnData.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPhongShader.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnSet.h>
#include <maya/MFnTransform.h>
#include <maya/MFStream.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h> 
#include <maya/MItDag.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItGeometry.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPointArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MVector.h>

#include <string.h> 
#include <vector>
#include <cmath>

#include "niflib.h"

//--Constants--//
const char PLUGIN_VERSION [] = "pre-alpha";	
const char TRANSLATOR_NAME [] = "NetImmerse Format";

//--Globals--//
string texture_path; // Path to textures gotten from option string

//--Function Definitions--//



//--NifTranslator Class--//

//This class implements the file translation functionality of the plug-in
class NifTranslator : public MPxFileTranslator {
public:
	//Constructor
	NifTranslator () {};

	//Destructor
	virtual ~NifTranslator () {};

	//Factory function for Maya to use to create a NifTranslator
	static void* creator() { return new NifTranslator(); }

	//This routine is called by Maya when it is necessary to load a file of a type supported by this translator.
	//Responsible for reading the contents of the given file, and creating Maya objects via API or MEL calls to reflect the data in the file.
	MStatus reader (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode);
	
	//This routine is called by Maya when it is necessary to save a file of a type supported by this translator.
	//Responsible for traversing all objects in the current Maya scene, and writing a representation to the given file in the supported format.
	MStatus writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode);
	
	//Returns true if the class has a read method and false otherwise
	bool haveReadMethod () const { return true; }

	//Returns true if the class has a reference method and false otherwise
	bool haveReferenceMethod () const { return false; }

	//Returns true if the class has a write method and false otherwise
	bool haveWriteMethod () const { return true; }

	//Returns true if the class can deal with namespaces and false otherwise
	bool haveNamespaceSupport () const { return false; }

	//Returns a string containing the default extension of the translator, excluding the period at the beginning
	MString defaultExtension () const { return MString("nif"); }

	//Returns true if the class can open and import files
	//Returns false if the class can only import files
	bool canBeOpened() const { return true; }

	// This routine must use passed data to determine if the file is of a type supported by this translator
	MFileKind identifyFile (const MFileObject& fileName, const char* buffer, short size) const;

private:
	void ImportNodes( blk_ref block, map< blk_ref, MDagPath > & objs, MObject parent = MObject::kNullObj );
	MDagPath ImportMesh( blk_ref block, MObject parent = MObject::kNullObj );
	MObject ImportMaterial( blk_ref block );
	MObject ImportTexture( blk_ref block );

};

//--NifTranslator::reader--//

//This routine is called by Maya when it is necessary to load a file of a type supported by this translator.
//Responsible for reading the contents of the given file, and creating Maya objects via API or MEL calls to reflect the data in the file.
MStatus NifTranslator::reader (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	cout << "Begining Read..." << endl;
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

		cout << "Reading NIF File..." << endl;
		//Read NIF file
		blk_ref root= ReadNifTree( file.fullName().asChar() );

		cout << "Importing Nodes..." << endl;
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
		cout << "Blocks in memory:  " << BlocksInMemory() << endl;
		
		//--Import Data--//
		cout << "Importing Data..." << endl;
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
			if ( blk_type == "NiTriShape" ) {
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
						Texture tx;

						//Cycle through for each type of texture
						int uv_set = 0;
						for (int i = 0; i < 7; ++i) {
							switch(i) {
								case 0:
									tx_block = tx_prop_block["Base Texture"];
									tx = tx_prop_block["Base Texture"];
									break;
								case 1:
									tx_block = tx_prop_block["Dark Texture"];
									tx = tx_prop_block["Dark Texture"];

									//Temporary until/if Dark Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Dark Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case 2:
									tx_block = tx_prop_block["Detail Texture"];
									tx = tx_prop_block["Detail Texture"];

									//Temporary until/if Detail Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Detail Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case 3:
									tx_block = tx_prop_block["Gloss Texture"];
									tx = tx_prop_block["Gloss Texture"];

									//Temporary until/if Detail Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Gloss Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case 4:
									tx_block = tx_prop_block["Glow Texture"];
									tx = tx_prop_block["Glow Texture"];
									break;
								case 5:
									tx_block = tx_prop_block["Bump Map Texture"];
									tx = tx_prop_block["Bump Map Texture"];

									//Temporary until/if Bump Map Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Bump Map Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								case 6:
									tx_block = tx_prop_block["Decal 0 Texture"];
									tx = tx_prop_block["Decal 0 Texture"];

									//Temporary until/if Decal Textures are supported
									if ( tx.isUsed == true ) {
										cout << "Warning:  Decal Textures are not yet supported." << endl;
										uv_set++;
									}
									continue;
								default:
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
									case 0:
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
									case 4:
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
							//node->GetBindPosition( bind_pose );
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
	
	cout << "Finished Read" << endl;

	//Report total number of blocks in memory (hopfully zero)
	cout << "Blocks in memory:  " << BlocksInMemory() << endl;

	return MStatus::kSuccess;
}

// Recursive function to crawl down tree looking for children and translate those that are understood
void NifTranslator::ImportNodes( blk_ref block, map< blk_ref, MDagPath > & objs, MObject parent )  {
	MObject obj;
	INode * node = QueryNode(block);

	//Stop at a non-node
	if ( node == NULL )
		return;

	//This must be a node, so process its basic attributes	
	MFnTransform transFn;
	string name = block->GetName();
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
		//node->GetWorldTransform( transform );
		transform = node->GetBindPosition();

	}
	else {
		//node->GetLocalTransform( transform );
		//Get parent node if any

		transform = node->GetLocalBindPos();
	}

	//put matrix into a float array
	float trans_arr[4][4];
	transform.AsFloatArr( trans_arr );

	transFn.set( MTransformationMatrix(MMatrix(trans_arr)) );
	transFn.setRotationOrder( MTransformationMatrix::kXYZ, false );

	// If the node has a name, set it
	if ( block->Namable() == true ) {
		MFnDependencyNode dnFn;
		dnFn.setObject(obj);
		string name = block->GetName();
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
	TextureSource ts = block["Texture Source"];

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

	string name = block->GetName();
	phongFn.setName( MString(name.c_str()) );

	return obj;
}

MDagPath NifTranslator::ImportMesh( blk_ref block, MObject parent ) {
	//Get ShapeData and TriShapeData interface
	IShapeData * data = QueryShapeData(block);
	ITriShapeData * tri_data = QueryTriShapeData(block);

	if ( data == NULL ) { throw runtime_error("IShapeData interface not returend"); }
	if ( tri_data == NULL ) { throw runtime_error("ITriShapeData interface not returend"); }

	int NumVertices = data->GetVertexCount();
	int NumPolygons = tri_data->GetTriangleCount();

	vector<Vector3> nif_verts = data->GetVertices();
	MPointArray maya_verts(NumVertices);

	for (int i = 0; i < NumVertices; ++i) {
		maya_verts[i] = MPoint(nif_verts[i].x, nif_verts[i].y, nif_verts[i].z, 0.0f);
	}

//Theoretical Example
//int NumVertices = data->GetVertexCount();
//int NumPolygons = data->GetTriangleCount();
//
//MPoint * maya_verts = new MPoint[NumVertices];
//MPointArray maya_verts(NumVertices);
//
//attr_ref verts = tri_data_block["Verticies"];
//for (i = 0; i < NumVertices; ++i) {
//      attr_ref elem = verts->GetElement(i);
//      MPointArray[i] = MPoint( elem->GetChild("x"), elem->GetChild("y"), elem->GetChild("z"), 0.0f ) );
//}

	//Nifs only have triangles, so all polys have 3 verticies
	int * poly_counts = new int[NumPolygons];
	for (int i = 0; i < NumPolygons; ++i) {
		poly_counts[i] = 3;
	}
	MIntArray maya_poly_counts( poly_counts, NumPolygons );

	//There are 3 verticies to list per triangle
	vector<int> connects( NumPolygons * 3 );// = new int[NumPolygons * 3];

	vector<Triangle> triangles = tri_data->GetTriangles();

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
	vector<Color> nif_colors = data->GetColors();
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
		blk_ref tx_prop = par["Properties"]->FindLink( "NiTexturingProperty");
		if ( tx_prop.is_null() == false ) {
			if ( tx_prop["Base Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("map1") );
			}
			if ( tx_prop["Dark Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("dark") );
			}
			if ( tx_prop["Detail Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("detail") );
			}
			if ( tx_prop["Gloss Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("gloss") );
			}
			if ( tx_prop["Glow Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("glow") );
			}
			if ( tx_prop["Bump Map Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("bump") );
			}
			if ( tx_prop["Decal 0 Texture"]->asTexture().isUsed == true ) {
				uv_set_list.push_back( MString("decal0") );
			}
		}
	}

	

	// Get default (first) UV Set if there is one		
	if ( data->GetUVSetCount() > 0 ) {
		meshFn.clearUVs();
		vector<UVCoord> uv_set;
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
	//vector<Vector3D> nif_normals(NumVertices);
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

//--NifTranslator::writer--//

//This routine is called by Maya when it is necessary to save a file of a type supported by this translator.
//Responsible for traversing all objects in the current Maya scene, and writing a representation to the given
//file in the supported format.
MStatus NifTranslator::writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	
	try {
	
		//Create new root node
		blk_ref root = CreateBlock("NiNode");
		root->SetName("Scene Root");
		root["Scale"]->Set(1.0f);


		cout << root->asString() << endl;

		//A map to hold associations between DAG paths and NIF blocks
		map< string, blk_ref > objs;

		//Itterate through all of the Maya DAG nodes, adding them to the tree
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
					if ( it.item().hasFn(MFn::kShape) ) {
						//NiTriShape
						block = CreateBlock("NiTriShape");
					} else {
						//NiNode
						block = CreateBlock("NiNode");
					}

					//Fix name
					string name = string( nodeFn.name().asChar() );
					replace(name.begin(), name.end(), '_', ' ');
					block->SetName( name );
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
					objs[par_path]["Children"]->AddLink( objs[blk_path] );
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

		//Write finished NIF file
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

//--NifTranslator::identifyFile--//

// This routine must use passed data to determine if the file is of a type supported by this translator

// Code adapted from animImportExport example that comes with Maya 6.5

MPxFileTranslator::MFileKind NifTranslator::identifyFile (const MFileObject& fileName, const char* buffer, short size) const {
	cout << "Checking File Type..." << endl;
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
	cout << "Initializing Plugin..." << endl;
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

	cout << "Done Initializing." << endl;
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