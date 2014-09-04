#include "Exporters/NifNodeExporter.h"

NifNodeExporter::NifNodeExporter() {

}

NifNodeExporter::NifNodeExporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {

}

void NifNodeExporter::ExportDAGNodes()
{
	//out << "NifTranslator::ExportDAGNodes {" << endl
		//<< "Creating DAG iterator..." << endl;
	//Create iterator to go through all DAG nodes depth first
	MItDag it(MItDag::kDepthFirst);

	//out << "Looping through all DAG nodes..." << endl;
	for( ; !it.isDone(); it.next() ) {
		//out << "Attaching function set for DAG node to the object." << endl;

		// attach a function set for a dag node to the
		// object. Rather than access data directly,
		// we access it via the function set.
		MFnDagNode nodeFn(it.item());

		/*MPlugArray animPlugs;
		MObjectArray animCurves;

		bool hasAnim = MAnimUtil::findAnimatedPlugs(it.item(),animPlugs);

		if(hasAnim == true) {
		MAnimUtil::findAnimatedPlugs(it.item(),animPlugs);

		for(int  x = 0;x < animPlugs.length(); x++) {
		MPlug pl = animPlugs[x];
		MString nn = pl.name();
		const char * ccc = nn.asChar();

		animCurves.clear();
		MAnimUtil::findAnimation(pl,animCurves);
		int count = animCurves.length();
		for(int y = 0;y < animCurves.length(); y++) {
		MFnAnimCurve fnCurve(animCurves[y]);
		for(int z = 0;z < fnCurve.numKeys();z++) {
		double cc = fnCurve.value(z);
		double cc2 = cc;
		}
		}
		}
		}*/



		const char* name = nodeFn.name().asChar();


		//out << "Object name is:  " << nodeFn.name().asChar() << endl;

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
			//out << "Object is not a history item" << endl;

			//Check if this is a transform node
			if ( it.item().hasFn(MFn::kTransform) ) {
				//This is a transform node, check if it is an IK joint or a shape
				//out << "Object is a transform node." << endl;

				NiAVObjectRef avObj;

				bool tri_shape = false;
				bool bounding_box = false;
				bool intermediate = false;
				MObject matching_child;

				MString nn = nodeFn.name();

				//Check to see what kind of node we should create
				for( int i = 0; i != nodeFn.childCount(); ++i ) {

					//out << "API Type:  " << nodeFn.child(i).apiTypeStr() << endl;
					// get a handle to the child
					if ( nodeFn.child(i).hasFn(MFn::kMesh)  && this->translatorUtils->isExportedShape(nodeFn.name())) {
						MFnMesh meshFn( nodeFn.child(i) );
						//history items don't count
						if ( !meshFn.isIntermediateObject() ) {
							//out << "Object is a mesh." << endl;
							tri_shape = true;
							matching_child = nodeFn.child(i);
							break;
						} else {
							//This has an intermediate mesh under it.  Don't export it at all.
							intermediate = true;
							break;
						}

					} else if ( nodeFn.child(i).hasFn( MFn::kBox ) ) {
						//out << "Found Bounding Box" << endl;

						//Set bounding box info
						BoundingBox bb;

						Matrix44 niMat= this->translatorUtils->MatrixM2N( nodeFn.transformationMatrix() );

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
							if ( this->translatorData->nodes.find( parFn.fullPathName().asChar() ) != this->translatorData->nodes.end() ) {
								NiNodeRef parNode = this->translatorData->nodes[parFn.fullPathName().asChar()];
								parNode->SetBoundingBox(bb);
							}
						}

						bounding_box = true;
					}
				}

				if ( !intermediate ) {

					if ( tri_shape == true ) {
						//out << "Adding Mesh to list to be exported later..." << endl;
						this->translatorData->meshes.push_back( it.item() );
						//NiTriShape
					} else if ( bounding_box == true ) {
						//Do nothing
					} else if (it.item().hasFn(MFn::kJoint) && this->translatorUtils->isExportedJoint(nodeFn.name())) {
						//out << "Creating a NiNode..." << endl;
						//NiNode
						NiNodeRef niNode = new NiNode;
						ExportAV( StaticCast<NiAVObject>(niNode), it.item() );

						//out << "Associating NiNode with node DagPath..." << endl;
						//Associate NIF object with node DagPath
						string path = nodeFn.fullPathName().asChar();
						this->translatorData->nodes[path] = niNode;

						//Parent should have already been created since we used a
						//depth first iterator in ExportDAGNodes
						NiNodeRef parNode = this->translatorUtils->GetDAGParent( it.item() );
						parNode->AddChild( StaticCast<NiAVObject>(niNode) );
					}
				}
			}
		}
	}
	//out << "Loop complete" << endl;


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


	//out << "}" << endl;
}

void NifNodeExporter::ExportAV( NiAVObjectRef avObj, MObject dagNode )
{
	// attach a function set for a dag node to the object.
	MFnDagNode nodeFn(dagNode);

	//out << "Fixing name from " << nodeFn.name().asChar() << " to ";

	//Fix name
	string name = this->translatorUtils->MakeNifName( nodeFn.name() );
	avObj->SetName( name );
	//out << name << endl;

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

	//Set default collision propagation type of "Use triangles"
	avObj->SetFlags(10);

	//Set visibility
	MPlug vis = nodeFn.findPlug( MString("visibility") );
	bool value;
	vis.getValue(value);
	//out << "Visibility of " << nodeFn.name().asChar() << " is " << value << endl;
	if ( value == false ) {
		avObj->SetVisibility(false);
	}


	//out << "Copying Maya matrix to Niflib matrix" << endl;
	//Copy Maya matrix to Niflib matrix
	Matrix44 ni_trans( 
		(float)my_trans[0][0], (float)my_trans[0][1], (float)my_trans[0][2], (float)my_trans[0][3],
		(float)my_trans[1][0], (float)my_trans[1][1], (float)my_trans[1][2], (float)my_trans[1][3],
		(float)my_trans[2][0], (float)my_trans[2][1], (float)my_trans[2][2], (float)my_trans[2][3],
		(float)my_trans[3][0], (float)my_trans[3][1], (float)my_trans[3][2], (float)my_trans[3][3]
	);

	//out << "Storing local transform values..." << endl;
	//Store Transform Values
	avObj->SetLocalTransform( ni_trans );
}

string NifNodeExporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifNodeExporter"<<endl;

	return out.str();
}

const Type& NifNodeExporter::GetType() const {
	return TYPE;
}

const Type NifNodeExporter::TYPE("NifNodeExporter",&NifTranslatorFixtureItem::TYPE);
