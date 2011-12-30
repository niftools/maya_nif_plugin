#include "include/Common/NifTranslatorOptions.h"


NifTranslatorOptions::NifTranslatorOptions()
{
	this->export_version = VER_4_0_0_2;
	this->export_user_version = 0;
	this->import_bind_pose = false;
	this->import_normals = false;
	this->import_no_ambient = false;
	this->export_white_ambient = false;
	this->import_comb_skel = false;
	this->joint_match = "";
	this->use_name_mangling = false;
	this->export_tri_strips = false;
	this->export_part_bones = 0;
	this->export_tan_space = false;
	this->export_mor_rename = false;
	this->tex_path_mode = PATH_MODE_AUTO;

	this->spline_animation_npoints = 4;
	this->spline_animation_degree = 4;
}


void NifTranslatorOptions::Reset()
{
	this->export_version = VER_4_0_0_2;
	this->export_user_version = 0;
	this->import_bind_pose = false;
	this->import_normals = false;
	this->import_no_ambient = false;
	this->export_white_ambient = false;
	this->import_comb_skel = false;
	this->joint_match = "";
	this->use_name_mangling = false;
	this->export_tri_strips = false;
	this->export_part_bones = 0;
	this->export_tan_space = false;
	this->export_mor_rename = false;
	this->tex_path_mode = PATH_MODE_AUTO;

	this->spline_animation_npoints = 4;
	this->spline_animation_degree = 4;
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
			this->texture_path = tokens[1].asChar();

			//Replace back slash with forward slash
			for ( unsigned i = 0; i < this->texture_path.size(); ++i ) {
				if ( this->texture_path[i] == '\\' ) {
					this->texture_path[i] = '/';
				}
			}

			//out << "Texture Path:  " << texture_path << endl;

		}
		if ( tokens[0] == "exportVersion" ) {
			this->export_version = ParseVersionString( tokens[1].asChar() );

			if ( export_version == VER_INVALID ) {
				MGlobal::displayWarning( "Invalid export version specified.  Using default of 4.0.0.2." );
				this->export_version = VER_4_0_0_2;
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
				this->import_normals = true;
			} else {
				this->import_normals = false;
			}
			//out << "Import Normals:  " << import_normals << endl;
		}
		if ( tokens[0] == "importNoAmbient" ) {
			if ( tokens[1] == "1" ) {
				this->import_no_ambient = true;
			} else {
				this->import_no_ambient = false;
			}
			//out << "Import No Ambient:  " << import_no_ambient << endl;
		}
		if ( tokens[0] == "exportWhiteAmbient" ) {
			if ( tokens[1] == "1" ) {
				this->export_white_ambient = true;
			} else {
				this->export_white_ambient = false;
			}
			//out << "Export White Ambient:  " << export_white_ambient << endl;
		}
		if ( tokens[0] == "exportTriStrips" ) {
			if ( tokens[1] == "1" ) {
				this->export_tri_strips = true;
			} else {
				this->export_tri_strips = false;
			}
			//out << "Export Triangle Strips:  " << export_tri_strips << endl;
		}
		if ( tokens[0] == "exportTanSpace" ) {
			if ( tokens[1] == "1" ) {
				this->export_tan_space = true;
			} else {
				this->export_tan_space = false;
			}
			//out << "Export Tangent Space:  " << export_tan_space << endl;
		}
		if ( tokens[0] == "exportMorRename" ) {
			if ( tokens[1] == "1" ) {
				this->export_mor_rename = true;
			} else {
				this->export_mor_rename = false;
			}
			//out << "Export Morrowind Rename:  " << export_mor_rename << endl;
		}
		if ( tokens[0] == "importSkelComb" ) {
			if ( tokens[1] == "1" ) {
				this->import_comb_skel = true;
			} else {
				this->import_comb_skel = false;
			}
			//out << "Combine with Existing Skeleton:  " << import_comb_skel << endl;
		}
		if ( tokens[0] == "exportPartBones" ) {
			this->export_part_bones = atoi( tokens[1].asChar() );
			//out << "Max Bones per Skin Partition:  " << export_part_bones << endl;
		}
		if ( tokens[0] == "exportUserVersion" ) {
			this->export_user_version = atoi( tokens[1].asChar() );
			//out << "User Version:  " << export_user_version << endl;
		}
		if ( tokens[0] == "importJointMatch" ) {
			this->joint_match = tokens[1].asChar();
			//out << "Import Joint Match:  " << joint_match << endl;
		}
		if ( tokens[0] == "useNameMangling" ) {
			if ( tokens[1] == "1" ) {
				this->use_name_mangling = true;
			} else {
				this->use_name_mangling = false;
			}
			//out << "Use Name Mangling:  " << use_name_mangling << endl;
		}
		if ( tokens[0] == "exportPathMode" ) {
			//out << " Export Texture Path Mode:  ";
			if ( tokens[1] == "Name" ) {
				this->tex_path_mode = PATH_MODE_NAME;
				//out << "Name" << endl;
			} else if ( tokens[1] == "Full" ) {
				this->tex_path_mode = PATH_MODE_FULL;
				//out << "Full" << endl;
			} else if ( tokens[1] == "Prefix" ) {
				this->tex_path_mode = PATH_MODE_PREFIX;
				//out << "Prefix" << endl;
			} else {
				//Auto is default
				this->tex_path_mode = PATH_MODE_AUTO;
				//out << "Auto" << endl;
			}
		}

		if ( tokens[0] == "exportPathPrefix" ) {
			this->tex_path_prefix = tokens[1].asChar();
			//out << "Export Texture Path Prefix:  " << tex_path_prefix << endl;
		}

		if(tokens[0] == "exportedShapes") {
			if(tokens.length() > 1) {
				MStringArray exportedShapesTokens; 
				tokens[1].split(',',exportedShapesTokens);
				this->export_shapes.clear();
				for(int k = 0;k < exportedShapesTokens.length();k++) {
					this->export_shapes.push_back(exportedShapesTokens[k].asChar());
				}
			}

		}

		if(tokens[0] == "exportedJoints") {
			if(tokens.length() > 1) {
				MStringArray exportedJointsTokens;
				tokens[1].split(',',exportedJointsTokens);
				this->export_joints.clear();
				for(int k = 0;k < exportedJointsTokens.length();k++) {
					this->export_joints.push_back(exportedJointsTokens[k].asChar());
				}
			}

		}
	}
}

std::string NifTranslatorOptions::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorRefObject::asString(verbose);

	out<<"Export user version:  "<<this->export_user_version<<endl;
	out<<"Export version:   "<<this->export_version<<endl;
	out<<"Import bind pose:   "<<this->import_bind_pose<<endl;
	out<<"Import normals:   "<<this->import_normals<<endl;
	out<<"Import no ambient:   "<<this->import_no_ambient<<endl;
	out<<"Export white ambient:   "<<this->export_white_ambient<<endl;
	out<<"Import combined skeleton:   "<<this->import_comb_skel<<endl;
	out<<"Joint match:   "<<this->joint_match<<endl;
	out<<"Use name mangling:   "<<this->use_name_mangling<<endl;
	out<<"Export tri strips:   "<<this->export_tri_strips<<endl;
	out<<"Export partition bones:   "<<this->export_part_bones<<endl;
	out<<"Export tangent space:   "<<this->export_tan_space<<endl;
	out<<"Export morrowind rename:   "<<this->export_mor_rename<<endl;
	out<<"Texture path mode:   "<<this->tex_path_mode<<endl;
	out<<"Texture path:   "<<this->texture_path<<endl;
	out<<"Texture prefix:   "<<this->tex_path_prefix<<endl;

	if(verbose == true) {
		out<<"Exported shapes:   "<<endl;
		for(int i = 0;i < this->export_shapes.size(); i++) {
			out<<this->export_shapes[i]<<endl;
		}

		out<<"Exported joints:   "<<endl;
		for(int i = 0;i < this->export_joints.size(); i++) {
			out<<this->export_shapes[i]<<endl;
		}
	}

	return out.str();
}

const Type& NifTranslatorOptions::GetType() const {
	return TYPE;
}

const Type NifTranslatorOptions::TYPE("NifTranslatorOptions", &NifTranslatorRefObject::TYPE);


