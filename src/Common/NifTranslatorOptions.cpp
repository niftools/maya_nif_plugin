#include "include/Common/NifTranslatorOptions.h"


NifTranslatorOptions::NifTranslatorOptions()
{
	this->exportVersion = VER_4_0_0_2;
	this->exportUserVersion = 0;
	this->import_bind_pose = false;
	this->importNormals = false;
	this->importNoAmbient = false;
	this->exportWhiteAmbient = false;
	this->importCombineSkeletons = false;
	this->jointMatch = "";
	this->useNameMangling = false;
	this->exportAsTriStrips = false;
	this->exportBonesPerSkinPartition = 0;
	this->exportTangentSpace = false;
	this->exportMorrowindRename = false;
	this->texturePathMode = PATH_MODE_AUTO;
}


void NifTranslatorOptions::Reset()
{
	this->exportVersion = VER_4_0_0_2;
	this->exportUserVersion = 0;
	this->import_bind_pose = false;
	this->importNormals = false;
	this->importNoAmbient = false;
	this->exportWhiteAmbient = false;
	this->importCombineSkeletons = false;
	this->jointMatch = "";
	this->useNameMangling = false;
	this->exportAsTriStrips = false;
	this->exportBonesPerSkinPartition = 0;
	this->exportTangentSpace = false;
	this->exportMorrowindRename = false;
	this->texturePathMode = PATH_MODE_AUTO;
}

NifTranslatorOptions::~NifTranslatorOptions() {

}

void NifTranslatorOptions::ParseOptionsString( const MString & optionsString )
{
	//Parse Options String
	MStringArray options;
	//out << "optionsString:" << endl;
	//out << optionsString.asChar() << endl;
	optionsString.split( ';', options );
	for (unsigned int i = 0; i < options.length(); ++i) {
		MStringArray tokens;
		options[i].split( '=', tokens );
		//out << "tokens[0]:  " << tokens[0].asChar() << endl;
		//out << "tokens[1]:  " << tokens[1].asChar() << endl;
		if ( tokens[0] == "texturePath" ) {
			this->texturePath = tokens[1].asChar();

			//Replace back slash with forward slash
			for ( unsigned i = 0; i < this->texturePath.size(); ++i ) {
				if ( this->texturePath[i] == '\\' ) {
					this->texturePath[i] = '/';
				}
			}

			//out << "Texture Path:  " << texture_path << endl;

		}
		if ( tokens[0] == "exportVersion" ) {
			this->exportVersion = ParseVersionString( tokens[1].asChar() );

			if ( exportVersion == VER_INVALID ) {
				MGlobal::displayWarning( "Invalid export version specified.  Using default of 4.0.0.2." );
				this->exportVersion = VER_4_0_0_2;
			}
			//out << "Export Version:  0x" << hex << export_version << dec << endl;
		}
		if ( tokens[0] == "importBindPose" ) {
			if ( tokens[1] == "1" ) {
				this->import_bind_pose = true;
			} else {
				this->import_bind_pose = false;
			}
			//out << "Import Bind Pose:  " << import_bind_pose << endl;
		}
		if ( tokens[0] == "importNormals" ) {
			if ( tokens[1] == "1" ) {
				this->importNormals = true;
			} else {
				this->importNormals = false;
			}
			//out << "Import Normals:  " << import_normals << endl;
		}
		if ( tokens[0] == "importNoAmbient" ) {
			if ( tokens[1] == "1" ) {
				this->importNoAmbient = true;
			} else {
				this->importNoAmbient = false;
			}
			//out << "Import No Ambient:  " << import_no_ambient << endl;
		}
		if ( tokens[0] == "exportWhiteAmbient" ) {
			if ( tokens[1] == "1" ) {
				this->exportWhiteAmbient = true;
			} else {
				this->exportWhiteAmbient = false;
			}
			//out << "Export White Ambient:  " << export_white_ambient << endl;
		}
		if ( tokens[0] == "exportTriStrips" ) {
			if ( tokens[1] == "1" ) {
				this->exportAsTriStrips = true;
			} else {
				this->exportAsTriStrips = false;
			}
			//out << "Export Triangle Strips:  " << export_tri_strips << endl;
		}
		if ( tokens[0] == "exportTanSpace" ) {
			if ( tokens[1] == "1" ) {
				this->exportTangentSpace = true;
			} else {
				this->exportTangentSpace = false;
			}
			//out << "Export Tangent Space:  " << export_tan_space << endl;
		}
		if ( tokens[0] == "exportMorRename" ) {
			if ( tokens[1] == "1" ) {
				this->exportMorrowindRename = true;
			} else {
				this->exportMorrowindRename = false;
			}
			//out << "Export Morrowind Rename:  " << export_mor_rename << endl;
		}
		if ( tokens[0] == "importSkelComb" ) {
			if ( tokens[1] == "1" ) {
				this->importCombineSkeletons = true;
			} else {
				this->importCombineSkeletons = false;
			}
			//out << "Combine with Existing Skeleton:  " << import_comb_skel << endl;
		}
		if ( tokens[0] == "exportPartBones" ) {
			this->exportBonesPerSkinPartition = atoi( tokens[1].asChar() );
			//out << "Max Bones per Skin Partition:  " << export_part_bones << endl;
		}
		if ( tokens[0] == "exportUserVersion" ) {
			this->exportUserVersion = atoi( tokens[1].asChar() );
			//out << "User Version:  " << export_user_version << endl;
		}
		if( tokens[0] == "exportUserVersion2") {
			this->exportUserVersion2 = atoi( tokens[1].asChar());
			//out << "User Version2:  " << export_user_version2 << endl;
		}
		if ( tokens[0] == "importJointMatch" ) {
			this->jointMatch = tokens[1].asChar();
			//out << "Import Joint Match:  " << joint_match << endl;
		}
		if ( tokens[0] == "useNameMangling" ) {
			if ( tokens[1] == "1" ) {
				this->useNameMangling = true;
			} else {
				this->useNameMangling = false;
			}
			//out << "Use Name Mangling:  " << use_name_mangling << endl;
		}
		if ( tokens[0] == "exportPathMode" ) {
			//out << " Export Texture Path Mode:  ";
			if ( tokens[1] == "Name" ) {
				this->texturePathMode = PATH_MODE_NAME;
				//out << "Name" << endl;
			} else if ( tokens[1] == "Full" ) {
				this->texturePathMode = PATH_MODE_FULL;
				//out << "Full" << endl;
			} else if ( tokens[1] == "Prefix" ) {
				this->texturePathMode = PATH_MODE_PREFIX;
				//out << "Prefix" << endl;
			} else {
				//Auto is default
				this->texturePathMode = PATH_MODE_AUTO;
				//out << "Auto" << endl;
			}
		}

		if ( tokens[0] == "exportPathPrefix" ) {
			this->texturePathPrefix = tokens[1].asChar();
			//out << "Export Texture Path Prefix:  " << tex_path_prefix << endl;
		}

		if(tokens[0] == "exportType") {
			this->exportType = tokens[1].asChar();
		}

		if(tokens[0] == "animationName") {
			if(tokens.length() > 1) {
				this->animationName = tokens[1].asChar();
			}
		}

		if(tokens[0] == "animationTarget") {
			if(tokens.length() > 1) {
				this->animationTarget = tokens[1].asChar();
			}
		}

		if(tokens[0] == "numberOfKeysToSample") {
			if(tokens.length() > 1) {
				this->numberOfKeysToSample = atoi(tokens[1].asChar());
			}
		}

		if(tokens[0] == "exportedShapes") {
			if(tokens.length() > 1) {
				MStringArray exportedShapesTokens; 
				tokens[1].split(',',exportedShapesTokens);
				this->exportedShapes.clear();
				for(int k = 0;k < exportedShapesTokens.length();k++) {
					this->exportedShapes.push_back(exportedShapesTokens[k].asChar());
				}
			}
		}
		
		if(tokens[0] == "exportedJoints") {
			if(tokens.length() > 1) {
				MStringArray exportedJointsTokens;
				tokens[1].split(',',exportedJointsTokens);
				this->exportedJoints.clear();
				for(int k = 0;k < exportedJointsTokens.length();k++) {
					this->exportedJoints.push_back(exportedJointsTokens[k].asChar());
				}
			}
		}
	}
}

std::string NifTranslatorOptions::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorRefObject::asString(verbose);

	out<<"Export user version:  "<<this->exportUserVersion<<endl;
	out<<"Export version:   "<<this->exportVersion<<endl;
	out<<"Import bind pose:   "<<this->import_bind_pose<<endl;
	out<<"Import normals:   "<<this->importNormals<<endl;
	out<<"Import no ambient:   "<<this->importNoAmbient<<endl;
	out<<"Export white ambient:   "<<this->exportWhiteAmbient<<endl;
	out<<"Import combined skeleton:   "<<this->importCombineSkeletons<<endl;
	out<<"Joint match:   "<<this->jointMatch<<endl;
	out<<"Use name mangling:   "<<this->useNameMangling<<endl;
	out<<"Export tri strips:   "<<this->exportAsTriStrips<<endl;
	out<<"Export partition bones:   "<<this->exportBonesPerSkinPartition<<endl;
	out<<"Export tangent space:   "<<this->exportTangentSpace<<endl;
	out<<"Export morrowind rename:   "<<this->exportMorrowindRename<<endl;
	out<<"Texture path mode:   "<<this->texturePathMode<<endl;
	out<<"Texture path:   "<<this->texturePath<<endl;
	out<<"Texture prefix:   "<<this->texturePathPrefix<<endl;

	if(verbose == true) {
		out<<"Exported shapes:   "<<endl;
		for(int i = 0;i < this->exportedShapes.size(); i++) {
			out<<this->exportedShapes[i]<<endl;
		}

		out<<"Exported joints:   "<<endl;
		for(int i = 0;i < this->exportedJoints.size(); i++) {
			out<<this->exportedShapes[i]<<endl;
		}
	}

	return out.str();
}

const Type& NifTranslatorOptions::GetType() const {
	return TYPE;
}

const Type NifTranslatorOptions::TYPE("NifTranslatorOptions", &NifTranslatorRefObject::TYPE);


