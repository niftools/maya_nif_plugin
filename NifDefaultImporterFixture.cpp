#include "NifDefaultImporterFixture.h"

MStatus NifDefaultImporterFixture::ReadNodes( const MFileObject& file )
{
	try {
		//out << "Reading NIF File..." << endl;
		//Read NIF file
		NiObjectRef root = ReadNifTree( file.fullName().asChar() );

		//out << "Importing Nodes..." << endl;
		//Import Nodes, starting at each child of the root
		NiNodeRef root_node = DynamicCast<NiNode>(root);
		if ( root_node != NULL ) {
			//Root is a NiNode and may have children

			//Check if the user wants us to try to find the bind pose
			if ( this->translatorOptions.import_bind_pose ) {
				SendNifTreeToBindPos( root_node );
			}

			//Check if the user wants us to try to combine new skins with
			//an existing skeleton
			if ( this->translatorOptions.import_comb_skel ) {
				//Enumerate existing nodes by name
				this->translatorData.existingNodes.clear();
				MItDag dagIt( MItDag::kDepthFirst);

				for ( ; !dagIt.isDone(); dagIt.next() ) {
					MFnTransform transFn( dagIt.item() );
					//out << "Adding " << transFn.name().asChar() << " to list of existing nodes" << endl;
					MDagPath nodePath;
					dagIt.getPath( nodePath );
					this->translatorData.existingNodes[ transFn.name().asChar() ] = nodePath;
				}

				//Adjust NiNodes in the original file that match names
				//in the maya scene to have the same transforms before
				//importing the new mesh over the top of the old one
				NiAVObjectRef rootAV = DynamicCast<NiAVObject>(root);
				if ( rootAV != NULL ) {
					this->translatorUtils.AdjustSkeleton( rootAV );
				}
			}

			//Check if the root node has a non-identity transform
			if ( root_node->GetLocalTransform() == Matrix44::IDENTITY ) {
				//Root has no transform, so treat it as the scene root
				vector<NiAVObjectRef> root_children = root_node->GetChildren();

				for ( unsigned int i = 0; i < root_children.size(); ++i ) {
					this->nodeImporter.ImportNodes( root_children[i], this->translatorData.importedNodes );
				}
			} else {
				//Root has a transform, so it's probably part of the scene
				this->nodeImporter.ImportNodes( StaticCast<NiAVObject>(root_node), this->translatorData.importedNodes );
			}
		} else {
			NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
			if ( rootAVObj != NULL ) {
				//Root is importable, but has no children
				this->nodeImporter.ImportNodes( rootAVObj, this->translatorData.importedNodes );
			} else {
				//Root cannot be imported
				MGlobal::displayError( "The root of this NIF file is not derived from the NiAVObject class.  It cannot be imported." );
				return MStatus::kFailure;
			}
		}

		//--Import Materials--//
		//out << "Importing Materials..." << endl;

		NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
		if ( rootAVObj != NULL ) {
			//Root is importable
			this->materialImporter.ImportMaterialsAndTextures( rootAVObj );
		}


		//--Import Meshes--//
		//out << "Importing Meshes..." << endl;

		//Iterate through all meshes that were imported.
		//This had to be deffered because all bones must exist
		//when attaching skin
		for ( unsigned i = 0; i < this->translatorData.importedMeshes.size(); ++i ) {
			//out << "Importing mesh..." << endl;
			//Import Mesh
			MDagPath meshPath = this->meshImporter.ImportMesh( this->translatorData.importedMeshes[i].first, this->translatorData.importedMeshes[i].second);
		}
		//out << "Done importing meshes." << endl;

		//--Import Animation--//
		//out << "Importing Animation keyframes..." << endl;

		//Iterate through all imported nodes, looking for any with animation keys

		for ( map<NiAVObjectRef,MDagPath>::iterator it = this->translatorData.importedNodes.begin(); it != this->translatorData.importedNodes.end(); ++it ) {
			//Check to see if this node has any animation controllers
			if ( it->first->IsAnimated() ) {
				this->animationImporter.ImportControllers( it->first, it->second );
			}
		}

		//out << "Deselecting anything that was selected by MEL commands" << endl;
		MGlobal::clearSelectionList();

		//Clear temporary data
		//out << "Clearing temporary data" << endl;
		this->translatorData.Reset();
	}
	catch( exception & e ) {
		MGlobal::displayError( e.what() );
		return MStatus::kFailure;
	}
	catch( ... ) {
		MGlobal::displayError( "Error:  Unknown Exception." );
		return MStatus::kFailure;
	}
}

NifDefaultImporterFixture::NifDefaultImporterFixture()
: translatorData(NifTranslatorData()), translatorOptions(NifTranslatorOptions()), translatorUtils(NifTranslatorUtils()), nodeImporter(NifNodeImporter()), meshImporter(NifMeshImporter()), materialImporter(NifMaterialImporter()), animationImporter(NifAnimationImporter()) {

}

NifDefaultImporterFixture::NifDefaultImporterFixture( NifTranslatorData& translatorData, NifTranslatorOptions& translatorOptions, NifTranslatorUtils& translatorUtils, NifNodeImporter& nodeImporter, NifMeshImporter& meshImporter, NifMaterialImporter& materialImporter,NifAnimationImporter& animationImporter )
: translatorData(translatorData), translatorOptions(translatorOptions), translatorUtils(translatorUtils), nodeImporter(nodeImporter), meshImporter(meshImporter), materialImporter(materialImporter), animationImporter(animationImporter) {

}
