#include "Importers\NifKFAnimationImporter.h"

NifKFAnimationImporter::NifKFAnimationImporter() {

}

NifKFAnimationImporter::NifKFAnimationImporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {

}


void NifKFAnimationImporter::ImportAnimation( NiInterpolatorRef interpolator,MString& targetObject ) {
	MObject object = this->GetObjectByName(targetObject);

	if(object.isNull()) {
		return;
	}

	vector<Key<Vector3>> translationKeys;
	vector<Key<float>> scaleKeys;
	vector<Key<Quaternion>> rotationQuaternionKeys;

	vector<Key<float>> rotationXKeys;
	vector<Key<float>> rotationYKeys;
	vector<Key<float>> rotationZKeys;
	
	MFnTransform transformNode(object);
	MPlug plug = transformNode.findPlug("interpolatorType");
	MString node_name = transformNode.name();
	MString mel_command = "addAttr -dataType \"string\" -shortName \"interpolatorType\" ";

	if(plug.isNull()) {	
		MGlobal::executeCommand(mel_command + node_name);
	}

	if(interpolator->GetType().IsSameType(NiTransformInterpolator::TYPE)) {
		NiTransformInterpolatorRef transformInterpolator = DynamicCast<NiTransformInterpolator>(interpolator);

		NiTransformDataRef transformData = transformInterpolator->GetData();

		if(transformData != NULL) {
			if(transformData->GetRotateType() == KeyType::XYZ_ROTATION_KEY) {
				rotationXKeys = transformData->GetXRotateKeys();
				rotationYKeys = transformData->GetYRotateKeys();
				rotationZKeys = transformData->GetZRotateKeys();
			} else {
				rotationQuaternionKeys = transformData->GetQuatRotateKeys();
			}

			translationKeys = transformData->GetTranslateKeys();
			scaleKeys = transformData->GetScaleKeys();
		}

		mel_command = "setAttr -type \"string\" ";
		MGlobal::executeCommand(mel_command + node_name + "\.interpolatorType \"NiTransformInterpolator\"");
	}

	if(interpolator->GetType().IsSameType(NiBSplineCompTransformInterpolator::TYPE)) {
		NiBSplineCompTransformInterpolatorRef splineInterpolator = DynamicCast<NiBSplineCompTransformInterpolator>(interpolator);
		NiBSplineBasisDataRef spline_interpolator_bdata = splineInterpolator->GetBasisData();
		
		if(spline_interpolator_bdata != NULL) {
			if(splineInterpolator->GetTranslateBias() != FLT_MAX && splineInterpolator->GetTranslateMultiplier() != FLT_MAX) {
				translationKeys = splineInterpolator->SampleTranslateKeys(spline_interpolator_bdata->GetNumControlPoints(), this->translatorOptions->spline_animation_degree);
			}
			if(splineInterpolator->GetRotationBias() != FLT_MAX && splineInterpolator->GetRotationMultiplier() != FLT_MAX) {
				rotationQuaternionKeys = splineInterpolator->SampleQuatRotateKeys(spline_interpolator_bdata->GetNumControlPoints(), this->translatorOptions->spline_animation_degree);	
			}
			if(splineInterpolator->GetScaleBias() != FLT_MAX && splineInterpolator->GetScaleMultiplier() != FLT_MAX) {
				scaleKeys = splineInterpolator->SampleScaleKeys(spline_interpolator_bdata->GetNumControlPoints(), this->translatorOptions->spline_animation_degree);
			}

			mel_command = "setAttr -type \"string\" ";
			MGlobal::executeCommand(mel_command + node_name + "\.interpolatorType \"NiBSplineCompTransformInterpolator\"");
		}
	}

	if(interpolator->GetType().IsSameType(NiPoint3Interpolator::TYPE)) {
		NiPoint3InterpolatorRef point3Interpolator = DynamicCast<NiPoint3Interpolator>(interpolator);
		NiPosDataRef posData = point3Interpolator->GetData();

		translationKeys = posData->GetKeys();

		mel_command = "setAttr -type \"string\" ";
		MGlobal::executeCommand(mel_command + node_name + "\.interpolatorType \"NiPoint3Interpolator\"");
	}

	if(translationKeys.size() > 0) {
		MFnAnimCurve translationX;
		MFnAnimCurve translationY;
		MFnAnimCurve translationZ;

		translationX.create(transformNode.findPlug("translateX"));
		translationY.create(transformNode.findPlug("translateY"));
		translationZ.create(transformNode.findPlug("translateZ"));

		for(int i = 0; i < translationKeys.size(); i++) {
			float positionX = translationKeys.at(i).data.x;
			float positionY = translationKeys.at(i).data.y;
			float positionZ = translationKeys.at(i).data.z;

			MTime time(translationKeys.at(i).time,MTime::kSeconds);

			translationX.addKey(time, positionX);
			translationY.addKey(time, positionY);
			translationZ.addKey(time, positionZ);
		}

	} else {

	}
	
	if(scaleKeys.size() > 0) {
		MFnAnimCurve scaleX;
		MFnAnimCurve scaleY;
		MFnAnimCurve scaleZ;

		scaleX.create(transformNode.findPlug("scaleX"));
		scaleY.create(transformNode.findPlug("scaleY"));
		scaleZ.create(transformNode.findPlug("scaleZ"));

		for(int i = 0; i < scaleKeys.size(); i++) {
			float scale = scaleKeys.at(i).data;

			MTime time(scaleKeys.at(i).time, MTime::kSeconds);

			scaleX.addKey(time, scale);
			scaleY.addKey(time, scale);
			scaleZ.addKey(time, scale);
		}

	} else {

	}

	if(rotationQuaternionKeys.size() > 0 ) {
		MFnAnimCurve rotationX;
		MFnAnimCurve rotationY;
		MFnAnimCurve rotationZ;

		rotationX.create(transformNode.findPlug("rotateX"));
		rotationY.create(transformNode.findPlug("rotateY"));
		rotationZ.create(transformNode.findPlug("rotateZ"));

		MEulerRotation mPrevRot;

		for(int i = 0; i < rotationQuaternionKeys.size(); i++) {
			Quaternion qt = rotationQuaternionKeys.at(i).data;

			MQuaternion mQuat( qt.x, qt.y, qt.z, qt.w );
			MEulerRotation mRot = mQuat.asEulerRotation();
			MEulerRotation mAlt;
			mAlt[0] = PI + mRot[0];
			mAlt[1] = PI - mRot[1];
			mAlt[2] = PI + mRot[2];

			for ( size_t j = 0; j < 3; ++j ) {
				double prev_diff = abs(mRot[j] - mPrevRot[j]);
				//Try adding and subtracting multiples of 2 pi radians to get
				//closer to the previous angle
				while (true) {
					double new_angle = mRot[j] - (PI * 2);
					double diff = abs( new_angle - mPrevRot[j] );
					if ( diff < prev_diff ) {
						mRot[j] = new_angle;
						prev_diff = diff;
					} else {
						break;
					}
				}
				while (true) {
					double new_angle = mRot[j] + (PI * 2);
					double diff = abs( new_angle - mPrevRot[j] );
					if ( diff < prev_diff ) {
						mRot[j] = new_angle;
						prev_diff = diff;
					} else {
						break;
					}
				}
			}

			for ( size_t j = 0; j < 3; ++j ) {
				double prev_diff = abs(mAlt[j] - mPrevRot[j]);
				//Try adding and subtracting multiples of 2 pi radians to get
				//closer to the previous angle
				while (true) {
					double new_angle = mAlt[j] - (PI * 2);
					double diff = abs( new_angle - mPrevRot[j] );
					if ( diff < prev_diff ) {
						mAlt[j] = new_angle;
						prev_diff = diff;
					} else {
						break;
					}
				}
				while (true) {
					double new_angle = mAlt[j] + (PI * 2);
					double diff = abs( new_angle - mPrevRot[j] );
					if ( diff < prev_diff ) {
						mAlt[j] = new_angle;
						prev_diff = diff;
					} else {
						break;
					}
				}
			}


			//Try taking advantage of the fact that:
			//Rotate(x,y,z) = Rotate(pi + x, pi - y, pi +z)
			double rot_diff = ( (abs(mRot[0] - mPrevRot[0]) + abs(mRot[1] - mPrevRot[1]) + abs(mRot[2] - mPrevRot[2]) ) / 3.0 );
			double alt_diff = ( (abs(mAlt[0] - mPrevRot[0]) + abs(mAlt[1] - mPrevRot[1]) + abs(mAlt[2] - mPrevRot[2]) ) / 3.0 );

			if ( alt_diff < rot_diff ) {
				mRot = mAlt;
			}

			mPrevRot = mRot;

			float rotateX = mRot[0];
			float rotateY = mRot[1];
			float rotateZ = mRot[2];

			MTime time(rotationQuaternionKeys.at(i).time, MTime::kSeconds);

			rotationX.addKey(time, rotateX);
			rotationY.addKey(time, rotateY);
			rotationZ.addKey(time, rotateZ);
		}
	} else {
		
	}

	if(rotationXKeys.size() > 0) {
		MFnAnimCurve rotationX;

		rotationX.create(transformNode.findPlug("rotateX"));

		for(int i = 0;i < rotationXKeys.size(); i++) {
			MTime time(rotationXKeys[i].time, MTime::kSeconds);

			rotationX.addKey(time, rotationXKeys[i].data);
		}
	}

	if(rotationYKeys.size() > 0) {
		MFnAnimCurve rotationY;

		rotationY.create(transformNode.findPlug("rotateY"));

		for(int i = 0;i < rotationYKeys.size(); i++) {
			MTime time(rotationYKeys[i].time, MTime::kSeconds);

			rotationY.addKey(time, rotationYKeys[i].data);
		}
	}

	if(rotationZKeys.size() > 0) {
		MFnAnimCurve rotationZ;

		rotationZ.create(transformNode.findPlug("rotateZ"));

		for(int i = 0;i < rotationZKeys.size(); i++) {
			MTime time(rotationZKeys[i].time, MTime::kSeconds);

			rotationZ.addKey(time, rotationZKeys[i].data);
		}
	}
	
}


MObject NifKFAnimationImporter::GetObjectByName( MString& name ) {
	MItDag iterator(MItDag::kDepthFirst);
	MObject object;

	for(;!iterator.isDone();iterator.next()) {
		MFnDagNode node(iterator.item());

		string nodeName = node.name().asChar();
		string targetName = name.asChar();

		if(node.name() == name && iterator.item().hasFn(MFn::kTransform)) {
			object = iterator.item();
			break;
		}
	}

	return object;
}

string NifKFAnimationImporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifKFAnimationImporter"<<endl;

	return out.str();
}

const Type& NifKFAnimationImporter::GetType() const {
	return TYPE;
}

const Type NifKFAnimationImporter::TYPE("NifKFAnimationImporter",&NifTranslatorFixtureItem::TYPE);