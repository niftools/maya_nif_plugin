#include "include/Custom Nodes/NifDismemberPartition.h"


NifDismemberPartition::NifDismemberPartition() {

}

NifDismemberPartition::~NifDismemberPartition() {

}

MStatus NifDismemberPartition::compute( const MPlug& plug, MDataBlock& dataBlock ) {
	return MStatus::kSuccess;
}

void* NifDismemberPartition::creator() {
	return new NifDismemberPartition();
}

MStatus NifDismemberPartition::initialize() {
	MFnTypedAttribute body_parts_flags_attribute;
	MFnTypedAttribute parts_flags_attribute;
	MFnMessageAttribute target_faces_attribute;
	MFnMessageAttribute target_shape_attribute;

	MStatus status;

	target_faces_attribute.setStorable(true);
	target_faces_attribute.setConnectable(true);
	target_faces_attribute.setWritable(true);
	target_shape_attribute.setWritable(true);
	target_shape_attribute.setConnectable(true);
	target_shape_attribute.setStorable(true);

	body_parts_flags_attribute.setStorable(true);
	parts_flags_attribute.setStorable(true);

	targetFaces = target_faces_attribute.create("targetFaces", "tF", &status);
	targetShape = target_shape_attribute.create("targetShape", "tS", &status);
	partsFlags = parts_flags_attribute.create("partsFlags", "pF", MFnData::kStringArray, &status);
	bodyPartsFlags = body_parts_flags_attribute.create("bodyPartsFlags", "bPF", MFnData::kStringArray, &status);

	status = MPxNode::addAttribute(targetFaces);
	status = MPxNode::addAttribute(targetShape);
	status = MPxNode::addAttribute(partsFlags);
	status = MPxNode::addAttribute(bodyPartsFlags);

	return MStatus::kSuccess;
}

MStringArray NifDismemberPartition::bodyPartTypeToStringArray( BSDismemberBodyPartType body_parts ) {
	MStringArray ret;

	if(body_parts == 0) {
		ret.append("BP_TORSO");
		return ret;
	}

	bool is_done = false;
	while(is_done == false) {
		is_done = true;

		if((BP_SECTIONCAP_HEAD & body_parts) == BP_SECTIONCAP_HEAD) {
			/*!< Section Cap | Head */
			ret.append("BP_SECTIONCAP_HEAD");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD);
			is_done = false;
		} else if((BP_SECTIONCAP_HEAD2 & body_parts) == BP_SECTIONCAP_HEAD2) {
			ret.append("BP_SECTIONCAP_HEAD2");
			/*!< Section Cap | Head 2 */
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD2);
			is_done = false;
		} else if((BP_SECTIONCAP_LEFTARM & body_parts) == BP_SECTIONCAP_LEFTARM) {
			/*!< Section Cap | Left Arm */
			ret.append("BP_SECTIONCAP_LEFTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM);
			is_done = false;
		} else if((BP_SECTIONCAP_LEFTARM2 & body_parts) == BP_SECTIONCAP_LEFTARM2) {
			/*!< Section Cap | Left Arm 2 */
			ret.append("BP_SECTIONCAP_LEFTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM2);
			is_done = false;
		} else if((BP_SECTIONCAP_RIGHTARM & body_parts) == BP_SECTIONCAP_RIGHTARM) {
			/*!< Section Cap | Right Arm */
			ret.append("BP_SECTIONCAP_RIGHTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM);
			is_done = false;
		} else if((BP_SECTIONCAP_RIGHTARM2 & body_parts) == BP_SECTIONCAP_RIGHTARM2) {
			/*!< Section Cap | Right Arm 2 */
			ret.append("BP_SECTIONCAP_RIGHTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM2);
			is_done = false;
		} else if((BP_SECTIONCAP_LEFTLEG & body_parts) == BP_SECTIONCAP_LEFTLEG) {
			/*!< Section Cap | Left Leg */ 
			ret.append("BP_SECTIONCAP_LEFTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG);
			is_done = false;
		} else if((BP_SECTIONCAP_LEFTLEG2 & body_parts) == BP_SECTIONCAP_LEFTLEG2) {
			/*!< Section Cap | Left Leg 2 */
			ret.append("BP_SECTIONCAP_LEFTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG2);
			is_done = false;
		} else if((BP_SECTIONCAP_LEFTLEG3 & body_parts) == BP_SECTIONCAP_LEFTLEG3) {
			/*!< Section Cap | Left Leg 3 */
			ret.append("BP_SECTIONCAP_LEFTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG3);
			is_done = false;
		} else if((BP_SECTIONCAP_RIGHTLEG & body_parts) == BP_SECTIONCAP_RIGHTLEG) {
			/*!< Section Cap | Right Leg */
			ret.append("BP_SECTIONCAP_RIGHTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG);
			is_done = false;
		} else if((BP_SECTIONCAP_RIGHTLEG2 & body_parts) == BP_SECTIONCAP_RIGHTLEG2) {
			/*!< Section Cap | Right Leg 2 */
			ret.append("BP_SECTIONCAP_RIGHTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG2);
			is_done = false;
		} else if((BP_SECTIONCAP_RIGHTLEG3 & body_parts) == BP_SECTIONCAP_RIGHTLEG3) {
			/*!< Section Cap | Right Leg 3 */
			ret.append("BP_SECTIONCAP_RIGHTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG3);
			is_done = false;
		} else if((BP_SECTIONCAP_BRAIN & body_parts) == BP_SECTIONCAP_BRAIN) {
			/*!< Section Cap | Brain */
			ret.append("BP_SECTIONCAP_BRAIN");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_BRAIN);
			is_done = false;
		} else if((BP_TORSOCAP_HEAD & body_parts) == BP_TORSOCAP_HEAD) {
			/*!< Torso Cap | Head */
			ret.append("BP_TORSOCAP_HEAD");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD);
			is_done = false;
		} else if((BP_TORSOCAP_HEAD2 & body_parts) == BP_TORSOCAP_HEAD2) {
			/*!< Torso Cap | Head 2 */
			ret.append("BP_TORSOCAP_HEAD2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD2);
			is_done = false;
		} else if((BP_TORSOCAP_LEFTARM & body_parts) == BP_TORSOCAP_LEFTARM) {
			/*!< Torso Cap | Left Arm */
			ret.append("BP_TORSOCAP_LEFTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM);
			is_done = false;
		} else if((BP_TORSOCAP_LEFTARM2 & body_parts) == BP_TORSOCAP_LEFTARM2) {
			/*!< Torso Cap | Left Arm 2 */
			ret.append("BP_TORSOCAP_LEFTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM2);
			is_done = false;
		} else if((BP_TORSOCAP_RIGHTARM & body_parts) == BP_TORSOCAP_RIGHTARM) {
			/*!< Torso Cap | Right Arm */
			ret.append("BP_TORSOCAP_RIGHTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM);
			is_done = false;
		} else if((BP_TORSOCAP_RIGHTARM2 & body_parts) == BP_TORSOCAP_RIGHTARM2) {
			/*!< Torso Cap | Right Arm 2 */
			ret.append("BP_TORSOCAP_RIGHTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM2);
			is_done = false;
		} else if((BP_TORSOCAP_LEFTLEG & body_parts) == BP_TORSOCAP_LEFTLEG) {
			/*!< Torso Cap | Left Leg */
			ret.append("BP_TORSOCAP_LEFTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG);
			is_done = false;
		} else if((BP_TORSOCAP_LEFTLEG2 & body_parts) == BP_TORSOCAP_LEFTLEG2) {
			/*!< Torso Cap | Left Leg 2 */
			ret.append("BP_TORSOCAP_LEFTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG2);
			is_done = false;
		} else if((BP_TORSOCAP_LEFTLEG3 & body_parts) == BP_TORSOCAP_LEFTLEG3) {
			/*!< Torso Cap | Left Leg 3 */
			ret.append("BP_TORSOCAP_LEFTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG3);
			is_done = false;
		} else if((BP_TORSOCAP_RIGHTLEG & body_parts) == BP_TORSOCAP_RIGHTLEG) {
			/*!< Torso Cap | Right Leg */
			ret.append("BP_TORSOCAP_RIGHTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG);
			is_done = false;
		} else if((BP_TORSOCAP_RIGHTLEG2 & body_parts) == BP_TORSOCAP_RIGHTLEG2) {
			/*!< Torso Cap | Right Leg 2 */
			ret.append("BP_TORSOCAP_RIGHTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG2);
			is_done = false;
		} else if((BP_TORSOCAP_RIGHTLEG3 & body_parts) == BP_TORSOCAP_RIGHTLEG3) {
			/*!< Torso Cap | Right Leg 3 */
			ret.append("BP_TORSOCAP_RIGHTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG3);
			is_done = false;
		} else if((BP_TORSOCAP_BRAIN & body_parts) == BP_TORSOCAP_BRAIN) {
			/*!< Torso Cap | Brain */
			ret.append("BP_TORSOCAP_BRAIN");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_BRAIN);
			is_done = false;
		} else if((BP_TORSOSECTION_HEAD & body_parts) == BP_TORSOSECTION_HEAD) {
			/*!< Torso Section | Head */
			ret.append("BP_TORSOSECTION_HEAD");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD);
			is_done = false;
		} else if((BP_TORSOSECTION_HEAD2 & body_parts) == BP_TORSOSECTION_HEAD2) {
			/*!< Torso Section | Head 2 */
			ret.append("BP_TORSOSECTION_HEAD2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD2);
			is_done = false;
		} else if((BP_TORSOSECTION_LEFTARM & body_parts) == BP_TORSOSECTION_LEFTARM) {
			/*!< Torso Section | Left Arm */
			ret.append("BP_TORSOSECTION_LEFTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM);
			is_done = false;
		} else if((BP_TORSOSECTION_LEFTARM2 & body_parts) == BP_TORSOSECTION_LEFTARM2) {
			/*!< Torso Section | Left Arm 2 */
			ret.append("BP_TORSOSECTION_LEFTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM2);
			is_done = false;
		} else if((BP_TORSOSECTION_RIGHTARM & body_parts) == BP_TORSOSECTION_RIGHTARM) {
			/*!< Torso Section | Right Arm */
			ret.append("BP_TORSOSECTION_RIGHTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM);
			is_done = false;
		} else if((BP_TORSOSECTION_RIGHTARM2 & body_parts) == BP_TORSOSECTION_RIGHTARM2) {
			/*!< Torso Section | Right Arm 2 */
			ret.append("BP_TORSOSECTION_RIGHTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM2);
			is_done = false;
		} else if((BP_TORSOSECTION_LEFTLEG & body_parts) == BP_TORSOSECTION_LEFTLEG) {
			/*!< Torso Section | Left Leg */
			ret.append("BP_TORSOSECTION_LEFTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG);
		} else if((BP_TORSOSECTION_LEFTLEG2 & body_parts) == BP_TORSOSECTION_LEFTLEG2) {
			/*!< Torso Section | Left Leg 2 */
			ret.append("BP_TORSOSECTION_LEFTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG2);
			is_done = false;
		} else if((BP_TORSOSECTION_LEFTLEG3 & body_parts) == BP_TORSOSECTION_LEFTLEG3) {
			/*!< Torso Section | Left Leg 3 */
			ret.append("BP_TORSOSECTION_LEFTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG2);
			is_done = false;
		} else if((BP_TORSOSECTION_RIGHTLEG & body_parts) == BP_TORSOSECTION_RIGHTLEG) {
			/*!< Torso Section | Right Leg */
			ret.append("BP_TORSOSECTION_RIGHTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG);
			is_done = false;
		} else if((BP_TORSOSECTION_RIGHTLEG2 & body_parts) == BP_TORSOSECTION_RIGHTLEG2) {
			/*!< Torso Section | Right Leg 2 */
			ret.append("BP_TORSOSECTION_RIGHTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG2);
			is_done = false;
		} else if((BP_TORSOSECTION_RIGHTLEG3 & body_parts) == BP_TORSOSECTION_RIGHTLEG3) {
			/*!< Torso Section | Right Leg 3 */
			ret.append("BP_TORSOSECTION_RIGHTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG3);
			is_done = false;
		} else if((BP_TORSOSECTION_BRAIN & body_parts) == BP_TORSOSECTION_BRAIN) {
			/*!< Torso Section | Brain */
			ret.append("BP_TORSOSECTION_BRAIN");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_BRAIN);
			is_done = false;
		} else if((BP_HEAD & body_parts) == BP_HEAD) {
			/*!< Head */
			ret.append("BP_HEAD");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD);
			is_done = false;
		} else if((BP_HEAD2 & body_parts) == BP_HEAD2) {
			/*!< Head 2 */
			ret.append("BP_HEAD2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_HEAD2);
			is_done = false;
		} else if((BP_LEFTARM & body_parts) == BP_LEFTARM) {
			/*!< Left Arm */
			ret.append("BP_LEFTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM);
			is_done = false;
		} else if((BP_LEFTARM2 & body_parts) == BP_LEFTARM2) {
			/*!< Left Arm 2 */
			ret.append("BP_LEFTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTARM2);
			is_done = false;
		} else if((BP_RIGHTARM & body_parts) == BP_RIGHTARM) {
			/*!< Right Arm */
			ret.append("BP_RIGHTARM");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM);
			is_done = false;
		} else if((BP_RIGHTARM2 & body_parts) == BP_RIGHTARM2) {
			/*!< Right Arm 2 */ 
			ret.append("BP_RIGHTARM2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTARM2);
			is_done = false;
		} else if((BP_LEFTLEG & body_parts) == BP_LEFTLEG) {
			/*!< Left Leg */
			ret.append("BP_LEFTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG);
			is_done = false;
		} else if((BP_LEFTLEG2 & body_parts) == BP_LEFTLEG2) {
			/*!< Left Leg 2 */
			ret.append("BP_LEFTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG2);
			is_done = false;
		} else if((BP_LEFTLEG3 & body_parts) == BP_LEFTLEG3) {
			/*!< Left Leg 3 */
			ret.append("BP_LEFTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_LEFTLEG3);
			is_done = false;
		} else if((BP_RIGHTLEG & body_parts) == BP_RIGHTLEG) {
			/*!< Right Leg */
			ret.append("BP_RIGHTLEG");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG);
			is_done = false;
		} else if((BP_RIGHTLEG2 & body_parts) == BP_RIGHTLEG2) {
			/*!< Right Leg 2 */
			ret.append("BP_RIGHTLEG2");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG2);
			is_done = false;
		} else if((BP_RIGHTLEG3 & body_parts) == BP_RIGHTLEG3) {
			/*!< Right Leg 3 */
			ret.append("BP_RIGHTLEG3");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_RIGHTLEG3);
			is_done = false;
		} else if((BP_BRAIN & body_parts) == BP_BRAIN) {
			/*!< Brain */
			ret.append("BP_BRAIN");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~BP_BRAIN);
			is_done = false;
		} else if((32 & body_parts) == 32) {
			ret.append("UNKNOWN_FLAG_32");
			body_parts = (BSDismemberBodyPartType)(body_parts & ~32);
			is_done = false;
		}
	}

	return ret;
}

MStringArray NifDismemberPartition::partToStringArray( BSPartFlag parts ) {
	MStringArray ret;

	if((PF_EDITOR_VISIBLE & parts) == PF_EDITOR_VISIBLE) {
		/*!< Visible in Editor */
		ret.append("PF_EDITOR_VISIBLE");
	}
	if((PF_START_NET_BONESET & parts) == PF_START_NET_BONESET) {
		/*!< Start a new shared boneset.  It is expected this BoneSet and the following sets in the Skin Partition will have the same bones. */
		ret.append("PF_START_NET_BONESET");
	}

	return ret;
}

MObject NifDismemberPartition::targetFaces;

MObject NifDismemberPartition::targetShape;

MObject NifDismemberPartition::partsFlags;

MObject NifDismemberPartition::bodyPartsFlags;

MTypeId NifDismemberPartition::id(0x7ff11);
