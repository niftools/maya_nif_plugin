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
	MStatus status;

	target_faces_attribute.setStorable(true);
	target_faces_attribute.setConnectable(true);
	target_faces_attribute.setWritable(true);
	body_parts_flags_attribute.setStorable(true);
	parts_flags_attribute.setStorable(true);

	targetFaces = target_faces_attribute.create("targetFaces", "tF", &status);
	partsFlags = parts_flags_attribute.create("partsFlags", "pF", MFnData::kStringArray, &status);
	bodyPartsFlags = body_parts_flags_attribute.create("bodyPartsFlags", "bPF", MFnData::kStringArray, &status);

	status = MPxNode::addAttribute(targetFaces);
	status = MPxNode::addAttribute(partsFlags);
	status = MPxNode::addAttribute(bodyPartsFlags);

	return MStatus::kSuccess;
}

MObject NifDismemberPartition::targetFaces;

MObject NifDismemberPartition::partsFlags;

MObject NifDismemberPartition::bodyPartsFlags;

MTypeId NifDismemberPartition::id(0x7ff11);
