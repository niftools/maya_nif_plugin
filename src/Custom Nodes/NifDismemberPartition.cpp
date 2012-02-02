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
	MFnTypedAttribute output_flags_attribute;
	MFnMessageAttribute input_faces_attribute;
	MStatus status;

	input_faces_attribute.setStorable(true);
	output_flags_attribute.setStorable(true);

	inputFaces = input_faces_attribute.create("Input Faces", "inputFaces", &status);
	outputFlags = output_flags_attribute.create("Dismember Flags", "dismemberFlags", MFnData::kStringArray, &status);

	status = MPxNode::addAttribute(inputFaces);
	status = MPxNode::addAttribute(outputFlags);

	return MStatus::kSuccess;
}

MObject NifDismemberPartition::inputFaces;

MObject NifDismemberPartition::outputFlags;

MTypeId NifDismemberPartition::id(0x7ff11);
