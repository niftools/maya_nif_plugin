#include "include/Importers/NifDefaultImportingFixture.h"

MStatus NifDefaultImportingFixture::ReadNodes( const MFileObject& file )
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
			if ( this->translatorOptions->import_bind_pose ) {
				SendNifTreeToBindPos( root_node );
			}

			//Check if the user wants us to try to combine new skins with
			//an existing skeleton
			if ( this->translatorOptions->importCombineSkeletons ) {
				//Enumerate existing nodes by name
				this->translatorData->existingNodes.clear();
				MItDag dagIt( MItDag::kDepthFirst);

				for ( ; !dagIt.isDone(); dagIt.next() ) {
					MFnTransform transFn( dagIt.item() );
					//out << "Adding " << transFn.name().asChar() << " to list of existing nodes" << endl;
					MDagPath nodePath;
					dagIt.getPath( nodePath );
					this->translatorData->existingNodes[ transFn.name().asChar() ] = nodePath;
				}

				//Adjust NiNodes in the original file that match names
				//in the maya scene to have the same transforms before
				//importing the new mesh over the top of the old one
				NiAVObjectRef rootAV = DynamicCast<NiAVObject>(root);
				if ( rootAV != NULL ) {
					this->translatorUtils->AdjustSkeleton( rootAV );
				}
			}

			//Check if the root node has a non-identity transform
			if ( root_node->GetLocalTransform() == Matrix44::IDENTITY ) {
				//Root has no transform, so treat it as the scene root
				vector<NiAVObjectRef> root_children = root_node->GetChildren();

				bool reserved = MProgressWindow::reserve();

				if(reserved == true) {
					MProgressWindow::setProgressMin(0);
					MProgressWindow::setProgressMax(root_children.size() - 1);
					MProgressWindow::setTitle("Importing nodes");
					MProgressWindow::startProgress();
					MProgressWindow::setInterruptable(false);

					for ( unsigned int i = 0; i < root_children.size(); ++i ) {
						this->nodeImporter->ImportNodes( root_children[i], this->translatorData->importedNodes );
						MProgressWindow::advanceProgress(1);
					}

					MProgressWindow::endProgress();
				} else {
					for ( unsigned int i = 0; i < root_children.size(); ++i ) {
						this->nodeImporter->ImportNodes( root_children[i], this->translatorData->importedNodes );
					}
				}
			} else {
				//Root has a transform, so it's probably part of the scene
				this->nodeImporter->ImportNodes( StaticCast<NiAVObject>(root_node), this->translatorData->importedNodes );
			}
		} else {
			NiAVObjectRef rootAVObj = DynamicCast<NiAVObject>(root);
			if ( rootAVObj != NULL ) {
				//Root is importable, but has no children
				this->nodeImporter->ImportNodes( rootAVObj, this->translatorData->importedNodes );
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
			this->materialImporter->ImportMaterialsAndTextures( rootAVObj );
		}


		//--Import Meshes--//
		//out << "Importing Meshes..." << endl;

		//Iterate through all meshes that were imported.
		//This had to be deffered because all bones must exist
		//when attaching skin

		bool reserved;

		reserved = MProgressWindow::reserve();

		if(reserved == true) {
			MProgressWindow::setProgressMin(0);
			MProgressWindow::setProgressMax(this->translatorData->importedMeshes.size() - 1);
			MProgressWindow::setTitle("Importing meshes");
			MProgressWindow::startProgress();

			for ( unsigned i = 0; i < this->translatorData->importedMeshes.size(); ++i ) {
				//out << "Importing mesh..." << endl;
				//Import Mesh
				MDagPath meshPath = this->meshImporter->ImportMesh( this->translatorData->importedMeshes[i].first, this->translatorData->importedMeshes[i].second);

				MProgressWindow::advanceProgress(1);
			}

			MProgressWindow::endProgress();
		} else {
			for ( unsigned i = 0; i < this->translatorData->importedMeshes.size(); ++i ) {
				//out << "Importing mesh..." << endl;
				//Import Mesh
				MDagPath meshPath = this->meshImporter->ImportMesh( this->translatorData->importedMeshes[i].first, this->translatorData->importedMeshes[i].second);
			}
		}

		
		//out << "Done importing meshes." << endl;

		//--Import Animation--//
		//out << "Importing Animation keyframes..." << endl;

		//Iterate through all imported nodes, looking for any with animation keys

		//for ( map<NiAVObjectRef,MDagPath>::iterator it = this->translatorData->importedNodes.begin(); it != this->translatorData->importedNodes.end(); ++it ) {
		//	//Check to see if this node has any animation controllers
		//	if ( it->first->IsAnimated() ) {
		//		this->animationImporter->ImportControllers( it->first, it->second );
		//	}
		//}

		//out << "Deselecting anything that was selected by MEL commands" << endl;
		MGlobal::clearSelectionList();

		//Clear temporary data
		//out << "Clearing temporary data" << endl;
		this->translatorData->Reset();
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

NifDefaultImportingFixture::NifDefaultImportingFixture() {

}

NifDefaultImportingFixture::NifDefaultImportingFixture( NifTranslatorDataRef translatorData, NifTranslatorOptionsRef translatorOptions, NifTranslatorUtilsRef translatorUtils ) {
	this->translatorData = translatorData;
	this->translatorOptions = translatorOptions;
	this->translatorUtils = translatorUtils;
	this->nodeImporter = new NifNodeImporter(translatorOptions, translatorData, translatorUtils);
	this->meshImporter = new NifMeshImporter(translatorOptions, translatorData, translatorUtils);
	this->materialImporter = new NifMaterialImporter(translatorOptions, translatorData, translatorUtils);
	this->animationImporter = new NifAnimationImporter(translatorOptions, translatorData, translatorUtils);
}

string NifDefaultImportingFixture::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifImportingFixture::asString(verbose)<<endl;
	out<<"NifDefaultImporterFixture"<<endl;
	out<<"NifTranslatorData"<<endl;
	out<<this->translatorData->asString(verbose)<<endl;
	out<<"NifTranslatorOptions"<<endl;
	out<<this->translatorOptions->asString(verbose)<<endl;
	out<<"NifTranslatorUtils"<<endl;
	out<<this->translatorUtils->asString(verbose)<<endl;
	out<<"NifNodeImporter"<<endl;
	out<<this->nodeImporter->asString(verbose)<<endl;
	out<<"NifMeshImporter"<<endl;
	out<<this->meshImporter->asString(verbose)<<endl;
	out<<"NifMaterialImporter"<<endl;
	out<<this->materialImporter->asString(verbose)<<endl;
	out<<"NifAnimationImporter"<<endl;
	out<<this->animationImporter->asString(verbose)<<endl;

	return out.str();
}

const Type& NifDefaultImportingFixture::GetType() const {
	return TYPE;
}

const Type NifDefaultImportingFixture::TYPE("NifDefaultImporterFixture",&NifImportingFixture::TYPE);
