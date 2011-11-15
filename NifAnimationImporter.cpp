#include "NifAnimationImporter.h"

void NifAnimationImporter::ImportControllers( NiAVObjectRef niAVObj, MDagPath & path )
{
	list<NiTimeControllerRef> controllers = niAVObj->GetControllers();

	//Iterate over the controllers, reacting properly to each type
	for ( list<NiTimeControllerRef>::iterator it = controllers.begin(); it != controllers.end(); ++it ) {
		if ( (*it)->IsDerivedType( NiKeyframeController::TYPE ) ) {
			//--NiKeyframeController--//
			NiKeyframeControllerRef niKeyCont = DynamicCast<NiKeyframeController>(*it);

			NiKeyframeDataRef niKeyData = niKeyCont->GetData();

			MFnTransform transFn( path.node() );

			MFnAnimCurve traXFn;
			MFnAnimCurve traYFn;
			MFnAnimCurve traZFn;

			MObject traXcurve = traXFn.create( transFn.findPlug("translateX") );
			MObject traYcurve = traYFn.create( transFn.findPlug("translateY") );
			MObject traZcurve = traZFn.create( transFn.findPlug("translateZ") );

			MTimeArray traTimes;
			MDoubleArray traXValues;
			MDoubleArray traYValues;
			MDoubleArray traZValues;

			vector<Key<Vector3> > tra_keys = niKeyData->GetTranslateKeys();

			for ( size_t i = 0; i < tra_keys.size(); ++i) {
				traTimes.append( MTime( tra_keys[i].time, MTime::kSeconds ) );
				traXValues.append( tra_keys[i].data.x );
				traYValues.append( tra_keys[i].data.y );
				traZValues.append( tra_keys[i].data.z );

				//traXFn.addKeyframe( tra_keys[i].time * 24.0, tra_keys[i].data.x );
				//traYFn.addKeyframe( tra_keys[i].time * 24.0, tra_keys[i].data.y );
				//traZFn.addKeyframe( tra_keys[i].time * 24.0, tra_keys[i].data.z );
			}

			traXFn.addKeys( &traTimes, &traXValues );
			traYFn.addKeys( &traTimes, &traYValues );
			traZFn.addKeys( &traTimes, &traZValues );

			KeyType kt = niKeyData->GetRotateType();

			if ( kt != XYZ_ROTATION_KEY ) {
				vector<Key<Quaternion> > rot_keys = niKeyData->GetQuatRotateKeys();

				MFnAnimCurve rotXFn;
				MFnAnimCurve rotYFn;
				MFnAnimCurve rotZFn;

				MObject rotXcurve = rotXFn.create( transFn.findPlug("rotateX") );
				//rotXFn.findPlug("rotationInterpolation").setValue(3);
				MObject rotYcurve = rotYFn.create( transFn.findPlug("rotateY") );
				//rotYFn.findPlug("rotationInterpolation").setValue(3);
				MObject rotZcurve = rotZFn.create( transFn.findPlug("rotateZ") );
				//rotZFn.findPlug("rotationInterpolation").setValue(3);

				MTimeArray rotTimes;
				MDoubleArray rotXValues;
				MDoubleArray rotYValues;
				MDoubleArray rotZValues;

				MEulerRotation mPrevRot;
				for( size_t i = 0; i < rot_keys.size(); ++i ) {
					Quaternion niQuat = rot_keys[i].data;

					MQuaternion mQuat( niQuat.x, niQuat.y, niQuat.z, niQuat.w );
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

					rotTimes.append( MTime(rot_keys[i].time, MTime::kSeconds) );
					rotXValues.append( mRot[0] );
					rotYValues.append( mRot[1] );
					rotZValues.append( mRot[2] );
				}


				rotXFn.addKeys( &rotTimes, &rotXValues );
				rotYFn.addKeys( &rotTimes, &rotYValues );
				rotZFn.addKeys( &rotTimes, &rotZValues );
			}
		}
	}
}

NifAnimationImporter::~NifAnimationImporter() {

}


NifAnimationImporter::NifAnimationImporter( NifTranslatorOptions& translatorOptions, NifTranslatorData& translatorData, NifTranslatorUtils& utils ) 
	: NifTranslatorFixtureItem(translatorOptions,translatorData,translatorUtils) {

}

NifAnimationImporter::NifAnimationImporter() {

}

