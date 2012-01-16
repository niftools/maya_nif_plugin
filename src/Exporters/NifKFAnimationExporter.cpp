#include "include/Exporters/NifKFAnimationExporter.h"

NifKFAnimationExporter::NifKFAnimationExporter() {

}

NifKFAnimationExporter::NifKFAnimationExporter( NifTranslatorOptionsRef translatorOptions, NifTranslatorDataRef translatorData, NifTranslatorUtilsRef translatorUtils ) 
	: NifTranslatorFixtureItem(translatorOptions, translatorData, translatorUtils) {
}


void NifKFAnimationExporter::ExportAnimation( NiControllerSequenceRef controller_sequence, MObject object ) {
	string interpolator_type = "NiTransformInterpolator";

	MFnTransform node(object);
	MPlug plug = node.findPlug("interpolatorType");

	if(!plug.isNull()) {
		interpolator_type = plug.asString().asChar();
	}

	NiInterpolatorRef interpolator;

	MPlugArray animated_plugs;
	MAnimUtil::findAnimatedPlugs(object, animated_plugs);

	MPlug translationX_plug;
	MPlug translationY_plug;
	MPlug translationZ_plug;

	MPlug scaleX_plug;
	MPlug scaleY_plug;
	MPlug scaleZ_plug;

	MPlug rotationX_plug;
	MPlug rotationY_plug;
	MPlug rotationZ_plug;

	for(int i = 0; i < animated_plugs.length(); i++) {

		string name = animated_plugs[i].partialName().asChar();
		string name2 = animated_plugs[i].name().asChar();

		if(animated_plugs[i].partialName() == "tx") {
			translationX_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "ty") {
			translationY_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "tz") {
			translationZ_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "sx") {
			scaleX_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "sy") {
			scaleY_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "sz") {
			scaleZ_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "rx") {
			rotationX_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "ry") {
			rotationY_plug = animated_plugs[i];
			continue;
		}
		if(animated_plugs[i].partialName() == "rz") {
			rotationZ_plug = animated_plugs[i];
			continue;
		}
	}

	MObjectArray animation_array;

	MFnAnimCurve translateX;
	MFnAnimCurve translateY;
	MFnAnimCurve translateZ;
	MFnAnimCurve scaleX;
	MFnAnimCurve scaleY;
	MFnAnimCurve scaleZ;
	MFnAnimCurve rotateX;
	MFnAnimCurve rotateY;
	MFnAnimCurve rotateZ;

	if(!translationX_plug.isNull()) {
		MAnimUtil::findAnimation(translationX_plug, animation_array);
		translateX.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!translationY_plug.isNull()) {
		MAnimUtil::findAnimation(translationY_plug, animation_array);
		translateY.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!translationZ_plug.isNull()) {
		MAnimUtil::findAnimation(translationZ_plug, animation_array);
		translateZ.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!scaleX_plug.isNull()) {
		MAnimUtil::findAnimation(scaleX_plug, animation_array);
		scaleX.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!scaleY_plug.isNull()) {
		MAnimUtil::findAnimation(scaleY_plug, animation_array);
		scaleY.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!scaleZ_plug.isNull()) {
		MAnimUtil::findAnimation(scaleZ_plug, animation_array);
		scaleZ.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!rotationX_plug.isNull()) {
		MAnimUtil::findAnimation(rotationX_plug, animation_array);
		rotateX.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!rotationY_plug.isNull()) {
		MAnimUtil::findAnimation(rotationY_plug, animation_array);
		rotateY.setObject(animation_array[0]);
		animation_array.clear();
	}
	if(!rotationZ_plug.isNull()) {
		MAnimUtil::findAnimation(rotationZ_plug, animation_array);
		rotateZ.setObject(animation_array[0]);
		animation_array.clear();
	}

	MStatus stat;
	MTransformationMatrix rest_position = node.transformation();
	MVector rest_translation = rest_position.getTranslation(MSpace::kPostTransform);
	double rest_scale[3];
	rest_position.getScale(rest_scale, MSpace::kPostTransform);
	double rest_q_x;
	double rest_q_y;
	double rest_q_z;
	double rest_q_w;
	rest_position.getRotationQuaternion(rest_q_x, rest_q_y, rest_q_z, rest_q_w, MSpace::kPostTransform);

	MPlug rest_plug;

	string name = node.name().asChar();

	rest_plug = node.findPlug("translateRest");
	if(!rest_plug.isNull()) {
		rest_plug = node.findPlug("translateRestX");
		rest_translation.x = rest_plug.asDouble();
		rest_plug = node.findPlug("translateRestY");
		rest_translation.y = rest_plug.asDouble();
		rest_plug = node.findPlug("translateRestZ");
		rest_translation.z = rest_plug.asDouble();
	}

	rest_plug = node.findPlug("scaleRest");
	if(!rest_plug.isNull()) {
		rest_plug = node.findPlug("scaleRestX");
		rest_scale[0] = rest_plug.asDouble();
		rest_plug = node.findPlug("scaleRestY");
		rest_scale[1] = rest_plug.asDouble();
		rest_plug = node.findPlug("scaleRestZ");
		rest_scale[2] = rest_plug.asDouble();
	}

	rest_plug = node.findPlug("rotateRest");
	if(!rest_plug.isNull()) {
		rest_plug = node.findPlug("rotateRestX");
		rest_q_x = rest_plug.asDouble();
		rest_plug = node.findPlug("rotateRestY");
		rest_q_y = rest_plug.asDouble();
		rest_plug = node.findPlug("rotateRestZ");
		rest_q_z = rest_plug.asDouble();

		MEulerRotation euler_qq((rest_q_x / 180) * PI, (rest_q_y / 180) * PI, (rest_q_z / 180) * PI);
		MQuaternion qq = euler_qq.asQuaternion();
		Quaternion qqq(qq.w, qq.x, qq.y, qq.z);

		rest_q_w = qq.w;
		rest_q_x = qq.x;
		rest_q_y = qq.y;
		rest_q_z = qq.z;
	}

	if(interpolator_type == "NiTransformInterpolator") {
		NiTransformInterpolatorRef transform_interpolator = DynamicCast<NiTransformInterpolator>(NiTransformInterpolator::Create());
		interpolator = DynamicCast<NiInterpolator>(transform_interpolator);

		transform_interpolator->SetRotation(Quaternion(rest_q_w, rest_q_x, rest_q_y, rest_q_z));
		transform_interpolator->SetScale(pow((float)(rest_scale[0] * rest_scale[1] * rest_scale[2]), (float)(1.0 / 3.0)));
		transform_interpolator->SetTranslation(Vector3(rest_translation.x, rest_translation.y, rest_translation.z));
		
		if(!translateX.object().isNull() || !translateY.object().isNull() || !translateZ.object().isNull()) {
			vector<Key<Vector3>> translationKeys;

			float default_x = node.transformation().translation(MSpace::kPostTransform).x;
			float default_y = node.transformation().translation(MSpace::kPostTransform).y;
			float default_z = node.transformation().translation(MSpace::kPostTransform).z;

			int translate_index_x = 0;
			int translate_index_y = 0;
			int translate_index_z = 0;
			MTime time_min(100000.0, MTime::kSeconds);
			int choice = -1;

			while(translate_index_x <  translateX.numKeys() || translate_index_y < translateY.numKeys() || translate_index_z < translateZ.numKeys()) {
				time_min.setValue(100000.0);

				float x = default_x;
				float y = default_y;
				float z = default_z;

				if(!translateX.object().isNull() && time_min > translateX.time(translate_index_x)) {
					time_min = translateX.time(translate_index_x);
					choice = 0;
				}
				if(!translateY.object().isNull() && time_min > translateY.time(translate_index_y)) {
					time_min = translateY.time(translate_index_y);
					choice = 1;
				}
				if(!translateZ.object().isNull() && time_min > translateZ.time(translate_index_z)) {
					time_min = translateZ.time(translate_index_z);
					choice = 2;
				}

				if(choice == 0) {
					x = translateX.value(translate_index_x);
					translate_index_x++;

					if(!translateY.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - translateY.time(translate_index_y).asUnits(MTime::kSeconds)) < 0.00001) {
							y = translateY.value(translate_index_y);
							translate_index_y++;
						} else {
							y = translateY.evaluate(time_min);
						}
					}

					if(!translateZ.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - translateZ.time(translate_index_z).asUnits(MTime::kSeconds)) < 0.00001) {
							z = translateZ.value(translate_index_z);
							translate_index_z++;
						} else {
							z = translateZ.evaluate(time_min);
						}
					}
				}

				if(choice == 1) {
					y = translateY.value(translate_index_y);
					translate_index_y++;

					if(!translateX.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - translateX.time(translate_index_x).asUnits(MTime::kSeconds)) < 0.00001) {
							x = translateX.value(translate_index_x);
							translate_index_x++;
						} else {
							x = translateX.evaluate(time_min);
						}
					}

					if(!translateZ.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - translateZ.time(translate_index_z).asUnits(MTime::kSeconds)) < 0.00001) {
							z = translateZ.value(translate_index_z);
							translate_index_z++;
						} else {
							z = translateZ.evaluate(time_min);
						}
					}
				}

				if(choice == 2) {
					z = translateZ.value(translate_index_z);
					translate_index_z++;

					if(!translateX.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - translateX.time(translate_index_x).asUnits(MTime::kSeconds)) < 0.00001) {
							x = translateX.value(translate_index_x);
							translate_index_x++;
						} else {
							x = translateX.evaluate(time_min);
						}
					}

					if(!translateY.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - translateY.time(translate_index_y).asUnits(MTime::kSeconds)) < 0.00001) {
							y = translateY.value(translate_index_y);
							translate_index_y++;
						} else {
							y = translateY.evaluate(time_min);
						}
					}
				}

				Key<Vector3> translation_key;
				translation_key.time = time_min.asUnits(MTime::kSeconds);
				translation_key.data.x = x;
				translation_key.data.y = y;
				translation_key.data.z = z;

				translationKeys.push_back(translation_key);
			}

			if(transform_interpolator->GetData() == NULL) {
				NiTransformDataRef transform_data = DynamicCast<NiTransformData>(NiTransformData::Create());
				transform_interpolator->SetData(transform_data);
			}

			transform_interpolator->GetData()->SetTranslateType(KeyType::LINEAR_KEY);
			transform_interpolator->GetData()->SetTranslateKeys(translationKeys);
		}

		if(!scaleX.object().isNull() || !scaleY.object().isNull() || !scaleY.object().isNull()) {
			vector<Key<float>> scaleKeys;

			double ss[3];
			node.transformation().getScale(ss, MSpace::kPostTransform);

			float default_x = ss[0];
			float default_y = ss[1];
			float default_z = ss[2];

			int scale_index_x = 0;
			int scale_index_y = 0;
			int scale_index_z = 0;
			MTime time_min(100000.0, MTime::kSeconds);
			int choice = -1;

			while(scale_index_x <  scaleX.numKeys() || scale_index_y < scaleY.numKeys() || scale_index_z < scaleZ.numKeys()) {
				time_min.setValue(100000.0);

				float x = default_x;
				float y = default_y;
				float z = default_z;

				if(!scaleX.object().isNull() && time_min > scaleX.time(scale_index_x)) {
					time_min = scaleX.time(scale_index_x);
					choice = 0;
				}
				if(!scaleY.object().isNull() && time_min > scaleY.time(scale_index_y)) {
					time_min = scaleY.time(scale_index_y);
					choice = 1;
				}
				if(!scaleZ.object().isNull() && time_min > scaleZ.time(scale_index_z)) {
					time_min = scaleZ.time(scale_index_z);
					choice = 2;
				}

				if(choice == 0) {
					x = scaleX.value(scale_index_x);
					scale_index_x++;

					if(!scaleY.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - scaleY.time(scale_index_y).asUnits(MTime::kSeconds)) < 0.00001) {
							y = scaleY.value(scale_index_y);
							scale_index_y++;
						} else {
							y = scaleY.evaluate(time_min);
						}
					}

					if(!scaleZ.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - scaleZ.time(scale_index_z).asUnits(MTime::kSeconds)) < 0.00001) {
							z = scaleZ.value(scale_index_z);
							scale_index_z++;
						} else {
							z = scaleZ.evaluate(time_min);
						}
					}
				}

				if(choice == 1) {
					y = scaleY.value(scale_index_y);
					scale_index_y++;

					if(!scaleX.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - scaleX.time(scale_index_x).asUnits(MTime::kSeconds)) < 0.00001) {
							x = scaleX.value(scale_index_x);
							scale_index_x++;
						} else {
							x = scaleX.evaluate(time_min);
						}
					}

					if(!scaleZ.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - scaleZ.time(scale_index_z).asUnits(MTime::kSeconds)) < 0.00001) {
							z = scaleZ.value(scale_index_z);
							scale_index_z++;
						} else {
							z = scaleZ.evaluate(time_min);
						}
					}
				}

				if(choice == 2) {
					z = scaleZ.value(scale_index_z);
					scale_index_z++;

					if(!scaleX.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - scaleX.time(scale_index_x).asUnits(MTime::kSeconds)) < 0.00001) {
							x = scaleX.value(scale_index_x);
							scale_index_x++;
						} else {
							x = scaleX.evaluate(time_min);
						}
					}

					if(!scaleY.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - scaleY.time(scale_index_y).asUnits(MTime::kSeconds)) < 0.00001) {
							y = scaleY.value(scale_index_y);
							scale_index_y++;
						} else {
							y = scaleY.evaluate(time_min);
						}
					}
				}

				Key<float> scale_key;
				scale_key.time = time_min.asUnits(MTime::kSeconds);
				scale_key.data = pow(x * y * z, 1.0f / 3.0f);

				scaleKeys.push_back(scale_key);
			}

			if(transform_interpolator->GetData() == NULL) {
				NiTransformDataRef transform_data = DynamicCast<NiTransformData>(NiTransformData::Create());
				transform_interpolator->SetData(transform_data);
			}

			transform_interpolator->GetData()->SetScaleType(KeyType::LINEAR_KEY);
			transform_interpolator->GetData()->SetScaleKeys(scaleKeys);
		}

		if(!rotateX.object().isNull() || !rotateY.object().isNull() || !rotateZ.object().isNull()) {
			vector<Key<Vector3>> rotationKeys;

			double rr[3];

			MTransformationMatrix::RotationOrder ord;

			node.transformation().getRotation(rr, ord, MSpace::kPostTransform);

			float default_x;
			float default_y;
			float default_z;

			switch(ord) {
			case MTransformationMatrix::RotationOrder::kXYZ:
				default_x = rr[0];
				default_y = rr[1];
				default_z = rr[2];
				break;
			case MTransformationMatrix::RotationOrder::kXZY:
				default_x = rr[0];
				default_y = rr[2];
				default_z = rr[1];
				break;
			case MTransformationMatrix::RotationOrder::kYXZ:
				default_x = rr[1];
				default_y = rr[0];
				default_z = rr[2];
				break;
			case MTransformationMatrix::RotationOrder::kYZX:
				default_x = rr[2];
				default_y = rr[0];
				default_z = rr[1];
				break;
			case MTransformationMatrix::RotationOrder::kZXY:
				default_x = rr[1];
				default_y = rr[2];
				default_z = rr[0];
				break;
			case MTransformationMatrix::RotationOrder::kZYX:
				default_x = rr[2];
				default_y = rr[1];
				default_z = rr[0];
				break;
			}

			int choice = -1;
			int rotate_index_x = 0;
			int rotate_index_y = 0;
			int rotate_index_z = 0;

			MTime time_min(100000.0, MTime::kSeconds);

			while(rotate_index_x <  rotateX.numKeys() || rotate_index_y < rotateY.numKeys() || rotate_index_z < rotateZ.numKeys()) {
				time_min.setValue(100000.0);

				float x = default_x;
				float y = default_y;
				float z = default_z;

				if(!rotateX.object().isNull() && time_min > rotateX.time(rotate_index_x)) {
					time_min = rotateX.time(rotate_index_x);
					choice = 0;
				}
				if(!rotateY.object().isNull() && time_min > rotateY.time(rotate_index_y)) {
					time_min = rotateY.time(rotate_index_y);
					choice = 1;
				}
				if(!rotateZ.object().isNull() && time_min > rotateZ.time(rotate_index_z)) {
					time_min = rotateZ.time(rotate_index_z);
					choice = 2;
				}

				if(choice == 0) {
					x = rotateX.value(rotate_index_x);
					rotate_index_x++;

					if(!rotateY.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - rotateY.time(rotate_index_y).asUnits(MTime::kSeconds)) < 0.00001) {
							y = rotateY.value(rotate_index_y);
							rotate_index_y++;
						} else {
							y = rotateY.evaluate(time_min);
						}
					}
					if(!rotateZ.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - rotateZ.time(rotate_index_z).asUnits(MTime::kSeconds)) < 0.00001) {
							z = rotateZ.value(rotate_index_z);
							rotate_index_z++;
						} else {
							z = rotateZ.evaluate(time_min);
						}
					}
				}

				if(choice == 1) {
					y = rotateY.value(rotate_index_y);
					rotate_index_y++;

					if(!rotateX.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - rotateX.time(rotate_index_x).asUnits(MTime::kSeconds)) < 0.00001) {
							x = rotateX.value(rotate_index_x);
							rotate_index_x++;
						} else {
							x = rotateX.evaluate(time_min);
						}
					}
					if(!rotateZ.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - rotateZ.time(rotate_index_z).asUnits(MTime::kSeconds)) < 0.00001) {
							z = rotateZ.value(rotate_index_z);
							rotate_index_z++;
						} else {
							z = rotateZ.evaluate(time_min);
						}
					}
				}

				if(choice == 2) {
					z = rotateZ.value(rotate_index_z);
					rotate_index_z++;

					if(!rotateX.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - rotateX.time(rotate_index_x).asUnits(MTime::kSeconds)) < 0.00001) {
							x = rotateX.value(rotate_index_x);
							rotate_index_x++;
						} else {
							x = rotateX.evaluate(time_min);
						}
					}
					if(!rotateY.object().isNull()) {
						if(abs(time_min.asUnits(MTime::kSeconds) - rotateY.time(rotate_index_y).asUnits(MTime::kSeconds)) < 0.00001) {
							y = rotateY.value(rotate_index_y);
							rotate_index_y++;
						} else {
							y = rotateY.evaluate(time_min);
						}
					}
				}

				Key<Vector3> rotation_key;
				rotation_key.time = time_min.asUnits(MTime::kSeconds);
				rotation_key.data.x = x;
				rotation_key.data.y = y;
				rotation_key.data.z = z;

				rotationKeys.push_back(rotation_key);
			}

			MString rotation_type = "XYZ";
			MPlug rotation_plug = node.findPlug("rotationType");
			string node_name = node.name().asChar(); 

			if(!rotation_plug.isNull()) {
				rotation_type = rotation_plug.asString();
			}

			if(transform_interpolator->GetData() == NULL) {
				NiTransformDataRef transform_data = DynamicCast<NiTransformData>(NiTransformData::Create());
				transform_interpolator->SetData(transform_data);
			}

			NiTransformDataRef transform_data = transform_interpolator->GetData();
			
			if(rotation_type == "XYZ") {
				transform_data->SetRotateType(KeyType::XYZ_ROTATION_KEY);

				transform_data->SetXRotateType(KeyType::LINEAR_KEY);
				transform_data->SetYRotateType(KeyType::LINEAR_KEY);
				transform_data->SetZRotateType(KeyType::LINEAR_KEY);
				
				vector<Key<float>> rotationXKeys;
				vector<Key<float>> rotationYKeys;
				vector<Key<float>> rotationZKeys;

				for(int i = 0; i < rotationKeys.size(); i++) {
					Key<float> rotateX_key;
					Key<float> rotateY_key;
					Key<float> rotateZ_key;

					rotateX_key.time = rotationKeys[i].time;
					rotateY_key.time = rotationKeys[i].time;
					rotateZ_key.time = rotationKeys[i].time;

					rotateX_key.data = rotationKeys[i].data.x;
					rotateY_key.data = rotationKeys[i].data.y;
					rotateZ_key.data = rotationKeys[i].data.z;

					rotationXKeys.push_back(rotateX_key);
					rotationYKeys.push_back(rotateY_key);
					rotationZKeys.push_back(rotateZ_key);
				} 

				transform_data->SetXRotateKeys(rotationXKeys);
				transform_data->SetYRotateKeys(rotationYKeys);
				transform_data->SetZRotateKeys(rotationZKeys);
			} else {
				transform_data->SetRotateType(KeyType::LINEAR_KEY);

				vector<Key<Quaternion>> rotateQuaternionKeys;

				for(int i = 0; i < rotationKeys.size(); i++) {
					Key<Quaternion> rotate_key;

					MEulerRotation m_euler_rotation(rotationKeys[i].data.x, rotationKeys[i].data.y, rotationKeys[i].data.z);
					MQuaternion m_quaternion = m_euler_rotation.asQuaternion();

					rotate_key.data.x = m_quaternion.x;
					rotate_key.data.y = m_quaternion.y;
					rotate_key.data.z = m_quaternion.z;
					rotate_key.data.w = m_quaternion.w;

					rotate_key.time = rotationKeys[i].time;

					rotateQuaternionKeys.push_back(rotate_key);
				}

				transform_data->SetQuatRotateKeys(rotateQuaternionKeys);
			}
		}

	} else  if(interpolator_type == "NiBSplineCompTransformInterpolator") {
		NiBSplineCompTransformInterpolatorRef spline_interpolator = DynamicCast<NiBSplineCompTransformInterpolator>(NiBSplineCompTransformInterpolator::Create());
		interpolator = DynamicCast<NiInterpolator>(spline_interpolator);

		spline_interpolator->SetTranslation(Vector3(rest_translation.x, rest_translation.y, rest_translation.z));
		spline_interpolator->SetScale(pow((float)(rest_scale[0] * rest_scale[1] * rest_scale[2]), (float)(1.0 / 3.0)));
		spline_interpolator->SetRotation(Quaternion(rest_q_w, rest_q_x, rest_q_y, rest_q_z));
		spline_interpolator->SetStartTime(controller_sequence->GetStartTime());
		spline_interpolator->SetStopTime(controller_sequence->GetStopTime());

		NiBSplineDataRef spline_data;
		NiBSplineBasisDataRef spline_basis_data;

		int control_points = this->translatorOptions->numberOfKeysToSample;

		MFnDependencyNode node(object);
		MPlug plug = node.findPlug("controlPoints");

		if(!plug.isNull()) {
			control_points = plug.asInt();
		}

		if(this->translatorData->splinesData.find(control_points) != this->translatorData->splinesData.end()) {
			spline_data = this->translatorData->splinesData.at(control_points);
			spline_basis_data = this->translatorData->splinesBasisData.at(control_points);

		} else {
			spline_data = DynamicCast<NiBSplineData>(NiBSplineData::Create());
			spline_basis_data = DynamicCast<NiBSplineBasisData>(NiBSplineBasisData::Create());
			spline_basis_data->SetNumControlPoints(control_points);

			this->translatorData->splinesData[control_points] = spline_data;
			this->translatorData->splinesBasisData[control_points] = spline_basis_data;
		}

		spline_interpolator->SetSplineData(spline_data);
		spline_interpolator->SetBasisData(spline_basis_data);

		if(!translateX.object().isNull() || !translateY.object().isNull() || !translateZ.object().isNull()) {
			vector<Vector3> translate_keys;

			float current_time = controller_sequence->GetStartTime();
			float time_increment = (controller_sequence->GetStopTime() - controller_sequence->GetStartTime()) / control_points;
			float translation_min = FLT_MAX;
			float translation_max = FLT_MIN;

			for(int i = 0; i < control_points; i++) {
				Vector3 key(rest_translation.x, rest_translation.y, rest_translation.z);
				double value;

				if(!translateX.object().isNull()) {
					translateX.evaluate(MTime(current_time, MTime::kSeconds), value);
					key.x = value;
				}
				if(!translateY.object().isNull()) {
					translateY.evaluate(MTime(current_time, MTime::kSeconds), value);
					key.y = value;
				}
				if(!translateZ.object().isNull()) {
					translateZ.evaluate(MTime(current_time, MTime::kSeconds), value);
					key.z = value;
				}

				if(translation_min > key.x) {
					translation_min = key.x;
				}
				if(translation_min > key.y) {
					translation_min = key.y;
				}
				if(translation_min > key.z) {
					translation_min = key.z;
				}

				if(translation_max < key.x) {
					translation_max = key.x;
				}
				if(translation_max < key.y) {
					translation_max = key.y;
				}
				if(translation_max < key.z) {
					translation_max = key.z;
				}

				translate_keys.push_back(key);
				current_time += time_increment;
			}
			
			double translation_bias = (translation_min + translation_max) / 2;
			double translation_multiplier = translation_max - translation_bias;

			vector<short> short_control_points;

			for(int i = 0; i < translate_keys.size(); i++) {
				short x;
				short y;
				short z;

				x = ((translate_keys[i].x - translation_bias) /	translation_multiplier) * 32767;
				y = ((translate_keys[i].y - translation_bias) / translation_multiplier) * 32767;
				z = ((translate_keys[i].z - translation_bias) / translation_multiplier) * 32767;

				short_control_points.push_back(x);
				short_control_points.push_back(y);
				short_control_points.push_back(z);
			}

			spline_interpolator->SetTranslationOffset(spline_data->GetNumShortControlPoints());
			spline_interpolator->SetTranslateBias(translation_bias);
			spline_interpolator->SetTranslateMultiplier(translation_multiplier);
			spline_data->AppendShortControlPoints(short_control_points);

		} else {
			spline_interpolator->SetTranslationOffset(65535);
			spline_interpolator->SetTranslateBias(FLT_MAX);
			spline_interpolator->SetTranslateMultiplier(FLT_MAX);
		}

		if(!rotateX.object().isNull() || !rotateY.object().isNull() || !rotateZ.object().isNull()) {
			vector<MQuaternion> rotate_keys;

			float current_time = controller_sequence->GetStartTime();
			float time_increment = (controller_sequence->GetStopTime() - controller_sequence->GetStartTime()) / control_points;
			double rotation_min = FLT_MAX;
			double rotation_max = FLT_MIN;


			MQuaternion rest_qq(rest_q_x, rest_q_y, rest_q_z, rest_q_w);
			MEulerRotation rot = rest_qq.asEulerRotation();
			float rest_r_x = rot.x;
			float rest_r_y = rot.y;
			float rest_r_z = rot.z;

			for(int i = 0; i < control_points; i++) {
				MEulerRotation euler_key(rest_r_x, rest_r_y, rest_r_z);
				double value;

				if(!rotateX.object().isNull()) {
					rotateX.evaluate(MTime(current_time, MTime::kSeconds), value);
					euler_key.x = value;
				}
				if(!rotateY.object().isNull()) {
					rotateY.evaluate(MTime(current_time, MTime::kSeconds), value);
					euler_key.y = value;
				}
				if(!rotateZ.object().isNull()) {
					rotateZ.evaluate(MTime(current_time, MTime::kSeconds), value);
					euler_key.z = value;
				}

				MQuaternion key = euler_key.asQuaternion();

				if(rotation_min > key.w) {
					rotation_min = key.w;
				}
				if(rotation_min > key.x) {
					rotation_min = key.x;
				}
				if(rotation_min > key.y) {
					rotation_min = key.y;
				}
				if(rotation_min > key.z) {
					rotation_min = key.z;
				}

				if(rotation_max < key.w) {
					rotation_max = key.w;
				}
				if(rotation_max < key.x) {
					rotation_max = key.x;
				}
				if(rotation_max < key.y) {
					rotation_max = key.y;
				}
				if(rotation_max < key.z) {
					rotation_max = key.z;
				}

				rotate_keys.push_back(key);
				current_time += time_increment;
			}

			vector<short> short_control_points;

			double rotation_bias;
			double rotation_multiplier;

			rotation_bias = (rotation_min + rotation_max) / 2;
			rotation_multiplier = rotation_max - rotation_bias;

			for(int i = 0; i < rotate_keys.size(); i++) {
				short w;
				short x;
				short y;
				short z;

				w = ((rotate_keys[i].w - rotation_bias) / rotation_multiplier) * 32767.0;
				x = ((rotate_keys[i].x - rotation_bias) / rotation_multiplier) * 32767.0;
				y = ((rotate_keys[i].y - rotation_bias) / rotation_multiplier) * 32767.0;
				z = ((rotate_keys[i].z - rotation_bias) / rotation_multiplier) * 32767.0;
				
				short_control_points.push_back(w);
				short_control_points.push_back(x);
				short_control_points.push_back(y);
				short_control_points.push_back(z);
			}

			spline_interpolator->SetRotationOffset(spline_data->GetNumShortControlPoints());
			spline_interpolator->SetRotationBias(rotation_bias);
			spline_interpolator->SetRotationMultiplier(rotation_multiplier);
			spline_data->AppendShortControlPoints(short_control_points);
		} else {
			spline_interpolator->SetRotationOffset(65535);
			spline_interpolator->SetRotationBias(FLT_MAX);
			spline_interpolator->SetRotationMultiplier(FLT_MAX);
		}
		
		if(!scaleX.object().isNull() || !scaleY.object().isNull() || !scaleZ.object().isNull()) {
			vector<float> scale_keys;

			float current_time = controller_sequence->GetStartTime();
			float time_increment = (controller_sequence->GetStopTime() - controller_sequence->GetStartTime()) / control_points;
			float scale_min = FLT_MAX;
			float scale_max = FLT_MIN;

			for(int i = 0; i < control_points; i++) {
				Vector3 key(rest_scale[0], rest_scale[1], rest_scale[2]);
				double value;

				if(!scaleX.object().isNull()) {
					scaleX.evaluate(MTime(current_time, MTime::kSeconds), value);
					key.x = value;
				}
				if(!scaleY.object().isNull()) {
					scaleY.evaluate(MTime(current_time, MTime::kSeconds), value);
					key.y = value;
				}
				if(!scaleZ.object().isNull()) {
					scaleZ.evaluate(MTime(current_time, MTime::kSeconds), value);
					key.z = value;
				}

				float float_key = pow(key.x * key.y * key.z, 1.0f/3.0f);

				if(scale_min > float_key) {
					scale_min = float_key;
				}
				if(scale_max < float_key) {
					scale_min = float_key;
				}

				scale_keys.push_back(float_key);
				current_time += time_increment;
			}

			double scale_bias = (scale_min + scale_max) / 2;
			double scale_multiplier = scale_max - scale_bias;

			vector<short> short_control_points;

			for(int i = 0; i < scale_keys.size(); i++) {
				short x;

				x = ((scale_keys[i] - scale_bias) /	scale_multiplier) * 32767;

				short_control_points.push_back(x);
			}

			spline_interpolator->SetScaleOffset(spline_data->GetNumShortControlPoints());
			spline_interpolator->SetScaleBias(scale_bias);
			spline_interpolator->SetScaleMultiplier(scale_multiplier);
			spline_data->AppendShortControlPoints(short_control_points);
		} else {
			spline_interpolator->SetScaleOffset(65535);
			spline_interpolator->SetScaleBias(FLT_MAX);
			spline_interpolator->SetScaleMultiplier(FLT_MAX);
		}

	} else if(interpolator_type == "NiPoint3Interpolator") {

	}

	if(interpolator != NULL) {
		Niflib::byte priority = 30;
		MPlug plug_priority = node.findPlug("animationPriority");

		if(!plug_priority.isNull()) {
			priority = plug_priority.asInt();
		}

		NiTransformControllerRef transform_controller = DynamicCast<NiTransformController>(NiTransformController::Create());
		NiObjectNETRef target = DynamicCast<NiObjectNET>(NiObjectNET::Create());

		target->SetName(this->translatorUtils->MakeNifName(node.name()));
		transform_controller->SetInterpolator(interpolator);
		target->AddController(transform_controller);

		controller_sequence->AddInterpolator(DynamicCast<NiSingleInterpController>(transform_controller), priority);
	}
}

float NifKFAnimationExporter::GetAnimationStartTime() {
	float start_time = FLT_MAX;
	MPlugArray animated_plugs;
	MObjectArray animation_curves;

	MItDag iterator(MItDag::kDepthFirst);

	for(; !iterator.isDone(); iterator.next()) {
		MFnDagNode node(iterator.currentItem());
		if(node.isIntermediateObject()) {
			continue;
		}

		if ( 
			node.name().substring(0, 13) == "UniversalManip" ||
			node.name() == "groundPlane_transform" ||
			node.name() == "ViewCompass" ||
			node.name() == "Manipulator1" ||
			node.name() == "persp" ||
			node.name() == "top" ||
			node.name() == "front" ||
			node.name() == "side"
			) {
				continue;
		}

		if(!iterator.currentItem().hasFn(MFn::Type::kTransform)) {
			continue;
		}

		if(!(this->translatorOptions->exportType == "allanimation") && (!this->translatorUtils->isExportedJoint(node.name()) && !this->translatorUtils->isExportedJoint(node.name()))) {
			continue;
		}

		if(MAnimUtil::isAnimated(iterator.currentItem())) {
			animated_plugs.clear();
			MAnimUtil::findAnimatedPlugs(iterator.currentItem(), animated_plugs);
		}

		for(int i = 0; i < animated_plugs.length(); i++) {
			MString partial_name = animated_plugs[i].partialName();
			if(partial_name == "rx" || partial_name == "ry" || partial_name == "rz" ||
				partial_name == "tx" || partial_name == "ty" || partial_name == "tz" ||
				partial_name == "sx" || partial_name == "sy" || partial_name == "sz") {
					MAnimUtil::findAnimation(animated_plugs[i], animation_curves);
			}
		}
	}

	MFnAnimCurve animation_curve;
	for(int i = 0; i < animation_curves.length(); i++) {
		animation_curve.setObject(animation_curves[i]);
		
		if(animation_curve.numKeys() > 0 && animation_curve.time(0).asUnits(MTime::kSeconds) < start_time) {
			start_time = animation_curve.time(0).asUnits(MTime::kSeconds);
		}
	}

	return start_time;
}


float NifKFAnimationExporter::GetAnimationEndTime() {
	float end_time = FLT_MIN;

	MPlugArray animated_plugs;
	MObjectArray animation_curves;

	MItDag iterator(MItDag::kDepthFirst);

	for(; !iterator.isDone(); iterator.next()) {
		MFnDagNode node(iterator.currentItem());
		if(node.isIntermediateObject()) {
			continue;
		}

		if ( 
			node.name().substring(0, 13) == "UniversalManip" ||
			node.name() == "groundPlane_transform" ||
			node.name() == "ViewCompass" ||
			node.name() == "Manipulator1" ||
			node.name() == "persp" ||
			node.name() == "top" ||
			node.name() == "front" ||
			node.name() == "side"
			) {
				continue;
		}

		if(!iterator.currentItem().hasFn(MFn::Type::kTransform)) {
			continue;
		}

		if(!(this->translatorOptions->exportType == "allanimation") && (!this->translatorUtils->isExportedJoint(node.name()) && !this->translatorUtils->isExportedJoint(node.name()))) {
			continue;
		}

		if(MAnimUtil::isAnimated(iterator.currentItem())) {
			animated_plugs.clear();
			MAnimUtil::findAnimatedPlugs(iterator.currentItem(), animated_plugs);
		}

		for(int i = 0; i < animated_plugs.length(); i++) {
			MString partial_name = animated_plugs[i].partialName();
			if(partial_name == "rx" || partial_name == "ry" || partial_name == "rz" ||
				partial_name == "tx" || partial_name == "ty" || partial_name == "tz" ||
				partial_name == "sx" || partial_name == "sy" || partial_name == "sz") {
					MAnimUtil::findAnimation(animated_plugs[i], animation_curves);
			}
		}
	}

	MFnAnimCurve animation_curve;
	for(int i = 0; i < animation_curves.length(); i++) {
		animation_curve.setObject(animation_curves[i]);

		if(animation_curve.numKeys() > 0 && animation_curve.time(animation_curve.numKeys() - 1).asUnits(MTime::kSeconds) > end_time) {
			end_time = animation_curve.time(animation_curve.numKeys() - 1).asUnits(MTime::kSeconds);
		}
	}

	return end_time;
}


string NifKFAnimationExporter::asString( bool verbose /*= false */ ) const {
	stringstream out;

	out<<NifTranslatorFixtureItem::asString(verbose)<<endl;
	out<<"NifKFAnimationExporter"<<endl;

	return out.str();
}

const Type& NifKFAnimationExporter::getType() const {
	return TYPE;
}

const Type NifKFAnimationExporter::TYPE("NifKFAnimationExporter", &NifTranslatorFixtureItem::TYPE);
