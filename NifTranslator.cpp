#include "NifTranslator.h"

//#define _DEBUG
ofstream out;

//--Function Definitions--//

//--NifTranslator::identifyFile--//

// This routine must use passed data to determine if the file is of a type supported by this translator

// Code adapted from animImportExport example that comes with Maya 6.5

MPxFileTranslator::MFileKind NifTranslator::identifyFile (const MFileObject& fileName, const char* buffer, short size) const {
	MString fName = fileName.name();
	if(fName.toUpperCase() != "NIF" && fName.toUpperCase() != "KF")
		return kNotMyFileType;

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

	status = plugin.registerNode("nifDismemberPartition", NifDismemberPartition::id, NifDismemberPartition::creator, NifDismemberPartition::initialize, MPxNode::kDependNode);
	if (!status) {
		status.perror("registerNifDismemberPartition");
		return status;
	}

	//Execute the command to create the NifTools Menu
	MGlobal::executeCommand("nifTranslatorMenuCreate");


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

	status = plugin.deregisterNode(NifDismemberPartition::id);
	if (!status) {
		status.perror("deregisterFileTranslator");
		return status;
	}

	//Execute the command to delete the NifTools Menu
	MGlobal::executeCommand("nifTranslatorMenuRemove");
	if (!status) {
		status.perror("deregisterNifDismemberPartition");
		return status;
	}

	return status;
}

//--NifTranslator::reader--//

//This routine is called by Maya when it is necessary to load a file of a type supported by this translator.
//Responsible for reading the contents of the given file, and creating Maya objects via API or MEL calls to reflect the data in the file.
MStatus NifTranslator::reader	 (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	NifTranslatorDataRef translator_data(new NifTranslatorData());
	NifTranslatorOptionsRef translator_options(new NifTranslatorOptions());
	NifTranslatorUtilsRef translator_utils(new NifTranslatorUtils(translator_data,translator_options));

	translator_options->ParseOptionsString(optionsString);
	MStringArray fileNameParts;
	file.name().toLowerCase().split('.', fileNameParts);
	MString fileExtension = fileNameParts[(int)(fileNameParts.length()) - 1];

	if(fileExtension == "nif") {
		NifInfo file_info = ReadHeaderInfo(file.name().asChar());

		if(file_info.version = VER_20_2_0_7 && file_info.userVersion == 12) {
			NifSkyrimImportingFixtureRef skyrim_importer_fixture = new NifSkyrimImportingFixture(translator_options, translator_data, translator_utils);

			skyrim_importer_fixture->ReadNodes(file);
		} else {
			NifDefaultImportingFixture importer_fixture(translator_data,translator_options,translator_utils);

			importer_fixture.ReadNodes(file);
		}

		return MStatus::kSuccess;
	}

	if(fileExtension == "kf") {
		NifKFAnimationImporterRef kf_animation_importer(new NifKFAnimationImporter(translator_options, translator_data, translator_utils));

		NifKFImportingFixtureRef kf_importing_fixture(new NifKFImportingFixture(translator_options, translator_data, translator_utils));

		return kf_importing_fixture->ReadNodes(file);
	}
	
	return MStatus::kFailure;
}

//--NifTranslator::writer--//

//This routine is called by Maya when it is necessary to save a file of a type supported by this translator.
//Responsible for traversing all objects in the current Maya scene, and writing a representation to the given
//file in the supported format.
MStatus NifTranslator::writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) {
	NifTranslatorOptionsRef translator_options(new NifTranslatorOptions());
	NifTranslatorDataRef translator_data(new NifTranslatorData());
	NifTranslatorUtilsRef translator_utils(new NifTranslatorUtils(translator_data, translator_options));

	translator_options->ParseOptionsString(optionsString);
	MStringArray fileNameParts;
	file.name().toLowerCase().split('.', fileNameParts);
	MString file_extension = fileNameParts[(int)(fileNameParts.length()) - 1];

	string export_type = "geometry";
	if(translator_options->exportType.length() > 1) {
		export_type = translator_options->exportType;
	}

	if(export_type == "geometry") {
		NifDefaultExportingFixtureRef exportingFixture(new NifDefaultExportingFixture(translator_data, translator_options, translator_utils));

		return exportingFixture->WriteNodes(file);
	}

	if(export_type == "animation") {
		NifKFExportingFixtureRef exporting_fixture(new NifKFExportingFixture(translator_options, translator_data, translator_utils));

		return exporting_fixture->WriteNodes(file);
	}
	
	return MStatus::kFailure;
}

//MMatrix MatrixN2M( const Matrix44 & n ) {
//	//Copy Niflib matrix to Maya matrix
//
//	float myMat[4][4] = { 
//		n[0][0], n[0][1], n[0][2], n[0][3],
//		n[1][0], n[1][1], n[1][2], n[1][3],
//		n[2][0], n[2][1], n[2][2], n[2][3],
//		n[3][0], n[3][1], n[3][2], n[3][3]
//	};
//
//	return MMatrix(myMat);
//}
//
//Matrix44 MatrixM2N( const MMatrix & n ) {
//	//Copy Maya matrix to Niflib matrix
//	return Matrix44( 
//		(float)n[0][0], (float)n[0][1], (float)n[0][2], (float)n[0][3],
//		(float)n[1][0], (float)n[1][1], (float)n[1][2], (float)n[1][3],
//		(float)n[2][0], (float)n[2][1], (float)n[2][2], (float)n[2][3],
//		(float)n[3][0], (float)n[3][1], (float)n[3][2], (float)n[3][3]
//	);
//}
