//************************************************************************** 
//* Export.cpp	- Ascii File Exporter
//* 
//* By Christer Janson
//* Kinetix Development
//*
//* January 20, 1997 CCJ Initial coding
//*
//* This module contains the main export functions.
//*
//* Copyright (c) 1997, All Rights Reserved. 
//***************************************************************************

#include "asciiexp.h"
#include "ISkin.h"
#include "modstack.h"
#include <map>
#include <DirectXMath.h>
using namespace DirectX;

#define MAX_BLEND 4
/****************************************************************************

  Global output [Scene info]

****************************************************************************/

// Dump some global animation information.
void AsciiExp::ExportGlobalInfo()
{
	// Set locale to 'C' to ensure that a dot is used as the decimal separator when writing a value to the output file
	//MaxLocaleHandler lh;

	Interval range = ip->GetAnimRange();

	struct tm* newtime;
	time_t aclock;

	time(&aclock);
	newtime = localtime(&aclock);

	TSTR today = _tasctime(newtime);  // The date string has a \n appended.
	today.remove(today.length() - 1); // Remove the \n

	// Start with a file identifier and format version
	_ftprintf(pStream, _T("%s\t%d\n"), ID_FILEID, VERSION);

	Interface14* iface = GetCOREInterface14();
	UINT codepage = iface->DefaultTextSaveCodePage(true);
	TSTR versionName = GetString(IDS_VERSIONSTRING);
	const char* versionName_locale = versionName.ToCP(codepage);

	// Text token describing the above as a comment.
	_ftprintf(pStream, _T("%s \"%hs  %1.2f - %s\"\n"), ID_COMMENT, versionName_locale, VERSION / 100.0f, today.data());

	_ftprintf(pStream, _T("%s {\n"), ID_SCENE);

	TSTR curtFileName = FixupName(ip->GetCurFileName());
	const char* curtFileName_locale = curtFileName.ToCP(codepage);
	_ftprintf(pStream, _T("\t%s \"%hs\"\n"), ID_FILENAME, curtFileName_locale);
	_ftprintf(pStream, _T("\t%s %d\n"), ID_FIRSTFRAME, range.Start() / GetTicksPerFrame());
	_ftprintf(pStream, _T("\t%s %d\n"), ID_LASTFRAME, range.End() / GetTicksPerFrame());
	_ftprintf(pStream, _T("\t%s %d\n"), ID_FRAMESPEED, GetFrameRate());
	_ftprintf(pStream, _T("\t%s %d\n"), ID_TICKSPERFRAME, GetTicksPerFrame());



	_ftprintf(pStream, _T("}\n"));
}

/****************************************************************************

  GeomObject output

****************************************************************************/

void AsciiExp::ExportGeomObject(INode* node, int indentLevel)
{
	ObjectState os = node->EvalWorldState(GetStaticFrame());
	if (!os.obj)
		return;

	// Targets are actually geomobjects, but we will export them
	// from the camera and light objects, so we skip them here.
	if (os.obj->ClassID() == Class_ID(TARGET_CLASS_ID, 0))
		return;


	TSTR indent = GetIndent(indentLevel);

	ExportNodeHeader(node, ID_GEOMETRY, indentLevel);

	ExportNodeTM(node, indentLevel);

	//오직 geometry만 GetIncludeMesh true
	if (GetIncludeMesh()) {
		ExportMesh(node, GetStaticFrame(), indentLevel);
	}

	if (GetIncludeAnim()) {
		//ExportAnimKeys(node, indentLevel);
	}

	if (GetIncludeMeshAnim()) {
		//ExportAnimMesh(node, indentLevel); //이건 node transform이아닌 vertex자체를 움직이는거라 아마 얼굴표정 천 같은거에 사용될듯
	}

	_ftprintf(pStream, _T("%s}\n"), indent.data());
}

/****************************************************************************

  Shape output

****************************************************************************/

void AsciiExp::ExportShapeObject(INode* node, int indentLevel)
{
	ExportNodeHeader(node, ID_SHAPE, indentLevel);
	ExportNodeTM(node, indentLevel);
	TimeValue t = GetStaticFrame();
	Matrix3 tm = node->GetObjTMAfterWSM(t);

	TSTR indent = GetIndent(indentLevel);

	ObjectState os = node->EvalWorldState(t);
	if (!os.obj || os.obj->SuperClassID() != SHAPE_CLASS_ID) {
		_ftprintf(pStream, _T("%s}\n"), indent.data());
		return;
	}

	ShapeObject* shape = (ShapeObject*)os.obj;
	PolyShape pShape;
	int numLines;

	// We will output ahspes as a collection of polylines.
	// Each polyline contains collection of line segments.
	shape->MakePolyShape(t, pShape);
	numLines = pShape.numLines;

	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_SHAPE_LINECOUNT, numLines);

	for (int poly = 0; poly < numLines; poly++) {
		_ftprintf(pStream, _T("%s\t%s %d {\n"), indent.data(), ID_SHAPE_LINE, poly);
		DumpPoly(&pShape.lines[poly], tm, indentLevel);
		_ftprintf(pStream, _T("%s\t}\n"), indent.data());
	}

	if (GetIncludeAnim()) {
		ExportAnimKeys(node, indentLevel);
	}

	_ftprintf(pStream, _T("%s}\n"), indent.data());
}

void AsciiExp::DumpPoly(PolyLine* line, Matrix3 tm, int indentLevel)
{
	int numVerts = line->numPts;

	TSTR indent = GetIndent(indentLevel);

	if (line->IsClosed()) {
		_ftprintf(pStream, _T("%s\t\t%s\n"), indent.data(), ID_SHAPE_CLOSED);
	}

	_ftprintf(pStream, _T("%s\t\t%s %d\n"), indent.data(), ID_SHAPE_VERTEXCOUNT, numVerts);

	// We differ between true and interpolated knots
	for (int i = 0; i < numVerts; i++) {
		PolyPt* pt = &line->pts[i];
		if (pt->flags & POLYPT_KNOT) {
			_ftprintf(pStream, _T("%s\t\t%s\t%d\t%s\n"), indent.data(), ID_SHAPE_VERTEX_KNOT, i,
				Format(tm * pt->p).data());
		}
		else {
			_ftprintf(pStream, _T("%s\t\t%s\t%d\t%s\n"), indent.data(), ID_SHAPE_VERTEX_INTERP,
				i, Format(tm * pt->p).data());
		}

	}
}

/****************************************************************************

  Light output

****************************************************************************/

void AsciiExp::ExportLightObject(INode* node, int indentLevel)
{
	TimeValue t = GetStaticFrame();
	TSTR indent = GetIndent(indentLevel);

	ExportNodeHeader(node, ID_LIGHT, indentLevel);

	ObjectState os = node->EvalWorldState(t);
	if (!os.obj) {
		_ftprintf(pStream, _T("%s}\n"), indent.data());
		return;
	}

	GenLight* light = (GenLight*)os.obj;
	struct LightState ls;
	Interval valid = FOREVER;
	Interval animRange = ip->GetAnimRange();

	light->EvalLightState(t, valid, &ls);

	// This is part os the lightState, but it doesn't
	// make sense to output as an animated setting so
	// we dump it outside of ExportLightSettings()

	_ftprintf(pStream, _T("%s\t%s "), indent.data(), ID_LIGHT_TYPE);
	switch (ls.type) {
	case OMNI_LIGHT:  _ftprintf(pStream, _T("%s\n"), ID_LIGHT_TYPE_OMNI); break;
	case TSPOT_LIGHT: _ftprintf(pStream, _T("%s\n"), ID_LIGHT_TYPE_TARG);  break;
	case DIR_LIGHT:   _ftprintf(pStream, _T("%s\n"), ID_LIGHT_TYPE_DIR); break;
	case FSPOT_LIGHT: _ftprintf(pStream, _T("%s\n"), ID_LIGHT_TYPE_FREE); break;
	}

	ExportNodeTM(node, indentLevel);
	// If we have a target object, export Node TM for the target too.
	INode* target = node->GetTarget();
	if (target) {
		ExportNodeTM(target, indentLevel);
	}

	int shadowMethod = light->GetShadowMethod();
	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_LIGHT_SHADOWS,
		shadowMethod == LIGHTSHADOW_NONE ? ID_LIGHT_SHAD_OFF :
		shadowMethod == LIGHTSHADOW_MAPPED ? ID_LIGHT_SHAD_MAP :
		ID_LIGHT_SHAD_RAY);


	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_LIGHT_USELIGHT, Format(light->GetUseLight()).data());

	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_LIGHT_SPOTSHAPE,
		light->GetSpotShape() == RECT_LIGHT ? ID_LIGHT_SHAPE_RECT : ID_LIGHT_SHAPE_CIRC);

	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_LIGHT_USEGLOBAL, Format(light->GetUseGlobal()).data());
	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_LIGHT_ABSMAPBIAS, Format(light->GetAbsMapBias()).data());
	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_LIGHT_OVERSHOOT, Format(light->GetOvershoot()).data());

	ExclList* el = light->GetExclList();  // DS 8/31/00 . switched to NodeIDTab from NameTab
	if (el->Count()) {
		_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_LIGHT_EXCLUSIONLIST);
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_NUMEXCLUDED, Format(el->Count()).data());
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_EXCLINCLUDE, Format(el->TestFlag(NT_INCLUDE)).data());
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_EXCL_AFFECT_ILLUM, Format(el->TestFlag(NT_AFFECT_ILLUM)).data());
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_EXCL_AFFECT_SHAD, Format(el->TestFlag(NT_AFFECT_SHADOWCAST)).data());
		Interface14* iface = GetCOREInterface14();
		UINT codepage = iface->DefaultTextSaveCodePage(true);
		for (int nameid = 0; nameid < el->Count(); nameid++) {
			INode* n = (*el)[nameid];	// DS 8/31/00
			if (n)
			{
				TSTR name = n->GetName();
				const char* name_locale = name.ToCP(codepage);
				_ftprintf(pStream, _T("%s\t\t%s \"%hs\"\n"), indent.data(), ID_LIGHT_EXCLUDED, name_locale);
			}
		}
		_ftprintf(pStream, _T("%s\t}\n"), indent.data());
	}

	// Export light settings for frame 0
	ExportLightSettings(&ls, light, t, indentLevel);

	// Export animated light settings
	if (!valid.InInterval(animRange) && GetIncludeCamLightAnim()) {
		_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_LIGHT_ANIMATION);

		TimeValue t = animRange.Start();

		while (1) {
			valid = FOREVER; // Extend the validity interval so the camera can shrink it.
			light->EvalLightState(t, valid, &ls);

			t = valid.Start() < animRange.Start() ? animRange.Start() : valid.Start();

			// Export the light settings at this frame
			ExportLightSettings(&ls, light, t, indentLevel + 1);

			if (valid.End() >= animRange.End()) {
				break;
			}
			else {
				t = (valid.End() / GetTicksPerFrame() + GetMeshFrameStep()) * GetTicksPerFrame();
			}
		}

		_ftprintf(pStream, _T("%s\t}\n"), indent.data());
	}

	// Export animation keys for the light node
	if (GetIncludeAnim()) {
		ExportAnimKeys(node, indentLevel);

		// If we have a target, export animation keys for the target too.
		if (target) {
			ExportAnimKeys(target, indentLevel);
		}
	}

	_ftprintf(pStream, _T("%s}\n"), indent.data());
}

void AsciiExp::ExportLightSettings(LightState* ls, GenLight* light, TimeValue t, int indentLevel)
{
	TSTR indent = GetIndent(indentLevel);

	_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_LIGHT_SETTINGS);

	// Frame #
	_ftprintf(pStream, _T("%s\t\t%s %d\n"), indent.data(), ID_TIMEVALUE, t);

	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_COLOR, Format(ls->color).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_INTENS, Format(ls->intens).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_ASPECT, Format(ls->aspect).data());

	if (ls->type != OMNI_LIGHT) {
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_HOTSPOT, Format(ls->hotsize).data());
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_FALLOFF, Format(ls->fallsize).data());
	}
	if (ls->type != DIR_LIGHT && ls->useAtten) {
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_ATTNSTART, Format(ls->attenStart).data());
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_ATTNEND, Format(ls->attenEnd).data());
	}

	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_TDIST, Format(light->GetTDist(t)).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_MAPBIAS, Format(light->GetMapBias(t)).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_MAPRANGE, Format(light->GetMapRange(t)).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_MAPSIZE, Format(light->GetMapSize(t)).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_LIGHT_RAYBIAS, Format(light->GetRayBias(t)).data());

	_ftprintf(pStream, _T("%s\t}\n"), indent.data());
}


/****************************************************************************

  Camera output

****************************************************************************/

void AsciiExp::ExportCameraObject(INode* node, int indentLevel)
{
	TSTR indent = GetIndent(indentLevel);

	ExportNodeHeader(node, ID_CAMERA, indentLevel);

	INode* target = node->GetTarget();
	if (target) {
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_CAMERA_TYPE, ID_CAMERATYPE_TARGET);
	}
	else {
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_CAMERA_TYPE, ID_CAMERATYPE_FREE);
	}


	ExportNodeTM(node, indentLevel);
	// If we have a target object, export animation keys for the target too.
	if (target) {
		ExportNodeTM(target, indentLevel);
	}

	CameraState cs;
	TimeValue t = GetStaticFrame();
	Interval valid = FOREVER;
	// Get animation range
	Interval animRange = ip->GetAnimRange();

	ObjectState os = node->EvalWorldState(t);
	CameraObject* cam = (CameraObject*)os.obj;

	cam->EvalCameraState(t, valid, &cs);

	ExportCameraSettings(&cs, cam, t, indentLevel);

	// Export animated camera settings
	if (!valid.InInterval(animRange) && GetIncludeCamLightAnim()) {

		_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_CAMERA_ANIMATION);

		TimeValue t = animRange.Start();

		while (1) {
			valid = FOREVER; // Extend the validity interval so the camera can shrink it.
			cam->EvalCameraState(t, valid, &cs);

			t = valid.Start() < animRange.Start() ? animRange.Start() : valid.Start();

			// Export the camera settings at this frame
			ExportCameraSettings(&cs, cam, t, indentLevel + 1);

			if (valid.End() >= animRange.End()) {
				break;
			}
			else {
				t = (valid.End() / GetTicksPerFrame() + GetMeshFrameStep()) * GetTicksPerFrame();
			}
		}

		_ftprintf(pStream, _T("%s\t}\n"), indent.data());
	}

	// Export animation keys for the light node
	if (GetIncludeAnim()) {
		ExportAnimKeys(node, indentLevel);

		// If we have a target, export animation keys for the target too.
		if (target) {
			ExportAnimKeys(target, indentLevel);
		}
	}

	_ftprintf(pStream, _T("%s}\n"), indent.data());
}

void AsciiExp::ExportCameraSettings(CameraState* cs, CameraObject* cam, TimeValue t, int indentLevel)
{
	TSTR indent = GetIndent(indentLevel);

	_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_CAMERA_SETTINGS);

	// Frame #
	_ftprintf(pStream, _T("%s\t\t%s %d\n"), indent.data(), ID_TIMEVALUE, t);

	if (cs->manualClip) {
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_CAMERA_HITHER, Format(cs->hither).data());
		_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_CAMERA_YON, Format(cs->yon).data());
	}

	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_CAMERA_NEAR, Format(cs->nearRange).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_CAMERA_FAR, Format(cs->farRange).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_CAMERA_FOV, Format(cs->fov).data());
	_ftprintf(pStream, _T("%s\t\t%s %s\n"), indent.data(), ID_CAMERA_TDIST, Format(cam->GetTDist(t)).data());

	_ftprintf(pStream, _T("%s\t}\n"), indent.data());
}


/****************************************************************************

  Helper object output

****************************************************************************/

void AsciiExp::ExportHelperObject(INode* node, int indentLevel)
{
	TSTR indent = GetIndent(indentLevel);
	ExportNodeHeader(node, ID_HELPER, indentLevel);

	// We don't really know what kind of helper this is, but by exporting
	// the Classname of the helper object, the importer has a chance to
	// identify it.
	Object* helperObj = node->EvalWorldState(0).obj;
	if (helperObj) {
		TSTR className;
		helperObj->GetClassName(className, true);
		Interface14* iface = GetCOREInterface14();
		UINT codepage = iface->DefaultTextSaveCodePage(true);
		const char* className_locale = className.ToCP(codepage);
		//밑에 일단 빼봄
		//_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_HELPER_CLASS, className_locale);
	}

	ExportNodeTM(node, indentLevel);

	if (helperObj) {
		TimeValue	t = GetStaticFrame();
		Matrix3		oTM = node->GetObjectTM(t);
		Box3		bbox;

		helperObj->GetDeformBBox(t, bbox, &oTM);

		//_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_BOUNDINGBOX_MIN, Format(bbox.pmin).data());
		//_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_BOUNDINGBOX_MAX, Format(bbox.pmax).data());
	}

	if (GetIncludeAnim()) {
		//ExportAnimKeys(node, indentLevel);
	}

	_ftprintf(pStream, _T("%s}\n"), indent.data());
}


/****************************************************************************

  Node Header

****************************************************************************/

// The Node Header consists of node type (geometry, helper, camera etc.)
// node name and parent node
void AsciiExp::ExportNodeHeader(INode* node, const TCHAR* type, int indentLevel)
{
	TSTR indent = GetIndent(indentLevel);
	Interface14* iface = GetCOREInterface14();
	UINT codepage = iface->DefaultTextSaveCodePage(true);

	// Output node header and object type 
	TSTR typeStr = type;
	const char* type_locale = typeStr.ToCP(codepage);
	_ftprintf(pStream, _T("%s%hs {\n"), indent.data(), type_locale);

	// Node name
	TSTR nodeStr = FixupName(node->GetName());
	const char* name_locale = nodeStr.ToCP(codepage);
	_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_NODE_NAME, name_locale);

	//  If the node is linked, export parent node name
	INode* parent = node->GetParentNode();
	if (parent && !parent->IsRootNode()) {
		TSTR parentName = FixupName(parent->GetName());
		const char* parentName_locale = parentName.ToCP(codepage);
		_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_NODE_PARENT, parentName_locale);
	}
}


/****************************************************************************

  Node Transformation

****************************************************************************/

void AsciiExp::ExportNodeTM(INode* node, int indentLevel)
{
	Matrix3 pivot = node->GetNodeTM(GetStaticFrame());
	TSTR indent = GetIndent(indentLevel);

	// Node name
	// We export the node name together with the nodeTM, because some objects
	// (like a camera or a spotlight) has an additional node (the target).
	// In that case the nodeTM and the targetTM is exported after eachother
	// and the nodeName is how you can tell them apart.
	Interface14* iface = GetCOREInterface14();
	UINT codepage = iface->DefaultTextSaveCodePage(true);

	//LocalTM을 얻는다
	//LocalTM이란 (자신의 월드행렬) * (부모월드행렬의 역행렬)
	Matrix3 LocalTM, OffsetTM;
	LocalTM = node->GetNodeTM(GetStaticFrame()) * Inverse(node->GetParentTM(GetStaticFrame()));
	OffsetTM = Inverse(node->GetNodeTM(GetStaticFrame())); //이거의 역행렬이 offsetMatrix가 될것, ap로분해한 이동값만 빼주는 것도 시도해보자.

	//LocalTM을 분해(decomp_affine함수이용)해서 이동, 회전, 스케일 값등을 얻는다
	AffineParts ap;
	decomp_affine(LocalTM, &ap);
	// Node의 로컬행렬, 월드행렬을 출력한다
	// 3ds max의 오른손 좌표계를 directX의 왼손 좌표계로 변환한다
	_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), _T("*NODE_TRANSFORMATION_MATRIX"));
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*LOCAL_TM0"), Format(LocalTM.GetRow(0)).data());
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*LOCAL_TM1"), Format(LocalTM.GetRow(2)).data());
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*LOCAL_TM2"), Format(LocalTM.GetRow(1)).data());
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*LOCAL_TM3"), Format(LocalTM.GetRow(3)).data());

	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*Offset_TM0"), Format(OffsetTM.GetRow(0)).data());
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*Offset_TM1"), Format(OffsetTM.GetRow(2)).data());
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*Offset_TM2"), Format(OffsetTM.GetRow(1)).data());
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*Offset_TM3"), Format(OffsetTM.GetRow(3)).data());
	_ftprintf(pStream, _T("%s\t}\n"), indent.data());

	//Node의 로컬행렬을 분해한 값들을 출력한다
	//아직 Format(Point3)만 y,z 바꾸긴함{
	_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), _T("*NODE_LOCAL_TRANSFORMATION_MATRIX_DECOMPOSITION"));
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*TM_TRANSLATION_COMPONENTS"), Format(ap.t).data()); //이동값(벡터)
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*TM_ESSENTIAL_ROTATION"), Format(ap.q).data()); //회전값(쿼터니언)
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*TM_STRETCH_ROTATION"), Format(ap.u).data()); //스케일회전값(쿼터니언)
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*TM_STRETCH_FACTORS"), Format(ap.k).data()); //스케일값(벡터)
	_ftprintf(pStream, _T("%s\t\t%s\t%s\n"), indent.data(), _T("*TM_STRETCH_FLIP_SIGN"), Format(ap.f).data()); //스케일플립값(float)
	_ftprintf(pStream, _T("%s\t}\n"), indent.data());



	//DumpMatrix3(&pivot, indentLevel + 2);
}

/***************************

WriteBoneList

*************************/

void AsciiExp::WriteBoneList()
{
	_ftprintf(pStream, _T("%s %d\t{\n"), _T("BONE_LIST"), static_cast<int>(m_BoneMap.size()));

	for (auto& iter : m_BoneMap)
	{
		_ftprintf(pStream, _T("\t\"%s\"\t%d\n"), iter.first.c_str(), iter.second);
	}
	_ftprintf(pStream, _T("}\n"));
}

/****************************************************************************

  Animation output

****************************************************************************/

// If the object is animated, then we will output the entire mesh definition
// for every specified frame. This can result in, dare I say, rather large files.
//
// Many target systems (including MAX itself!) cannot always read back this
// information. If the objects maintains the same number of verices it can be
// imported as a morph target, but if the number of vertices are animated it
// could not be read back in withou special tricks.
// Since the target system for this exporter is unknown, it is up to the
// user (or developer) to make sure that the data conforms with the target system.

void AsciiExp::ExportAnimMesh(INode* node, int indentLevel)
{
	ObjectState os = node->EvalWorldState(GetStaticFrame());
	if (!os.obj)
		return;

	TSTR indent = GetIndent(indentLevel);

	// Get animation range
	Interval animRange = ip->GetAnimRange();
	// Get validity of the object
	Interval objRange = os.obj->ObjectValidity(GetStaticFrame());

	// If the animation range is not fully included in the validity
	// interval of the object, then we're animated.
	if (!objRange.InInterval(animRange)) {

		_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_MESH_ANIMATION);

		TimeValue t = animRange.Start();

		while (1) {
			// This may seem strange, but the object in the pipeline
			// might not be valid anymore.
			os = node->EvalWorldState(t);
			objRange = os.obj->ObjectValidity(t);
			t = objRange.Start() < animRange.Start() ? animRange.Start() : objRange.Start();

			// Export the mesh definition at this frame
			ExportMesh(node, t, indentLevel + 1);

			if (objRange.End() >= animRange.End()) {
				break;
			}
			else {
				t = (objRange.End() / GetTicksPerFrame() + GetMeshFrameStep()) * GetTicksPerFrame();
			}
		}
		_ftprintf(pStream, _T("%s\t}\n"), indent.data());
	}
}


/****************************************************************************

  Mesh output

****************************************************************************/

Modifier* AsciiExp::FindSkinModifier(INode* node)
{
	Object* obj = node->GetObjectRef();
	if (!obj) return nullptr;

	// Obj가 DerivedObject인 경우 Modifier Stack 탐색
	while (obj->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		IDerivedObject* dobj = static_cast<IDerivedObject*>(obj);
		int numMods = dobj->NumModifiers();

		for (int i = 0; i < numMods; ++i)
		{
			Modifier* mod = dobj->GetModifier(i);
			if (mod && mod->ClassID() == SKIN_CLASSID)
				return mod; // Skin Modifier 발견
		}
		obj = dobj->GetObjRef();
	}

	// Modifier 없으면 nullptr 반환
	return nullptr;
}


void AsciiExp::ExportSkinName(INode* pNode, Modifier* pMod, int NumVert) //vertex하나에 대하여
{
	ISkin* pSkin = (ISkin*)pMod->GetInterface(I_SKIN);
	if (!pSkin) return;
	ISkinContextData* pSkinData = (ISkinContextData*)pSkin->GetContextInterface(pNode);
	int nNumNode = pSkinData->GetNumAssignedBones(NumVert);

	std::multimap<float, int> mWeightList;
	mWeightList.clear();
	for (int x = 0; x < nNumNode; x++)
	{
		float fWeight = pSkinData->GetBoneWeight(NumVert, x);
		mWeightList.insert(std::make_pair(fWeight, x));
	}
	if (nNumNode > MAX_BLEND)
		nNumNode = MAX_BLEND;
	float normalizedWeight;
	float fFactor = 0;
	_ftprintf(pStream, _T("\t\t\t%d\t"), nNumNode);

	std::multimap<float, int>::reverse_iterator _iterRS = mWeightList.rbegin();
	std::multimap<float, int>::reverse_iterator _iterRE = mWeightList.rend();
	for (int i = 0; _iterRS != _iterRE; ++_iterRS, i++)
	{
		normalizedWeight = _iterRS->first;
		if (i == (nNumNode - 1)) //마지막 반복자라면
			normalizedWeight = 1 - fFactor;
		else
			fFactor += normalizedWeight;
		INode* pBone = pSkin->GetBone(pSkinData->GetAssignedBone(NumVert, _iterRS->second));
		if (pBone)
		{
			int nBoneID = -1;
			if (m_BoneMap.find(pBone->GetName()) == m_BoneMap.end())
			{
				m_BoneMap[pBone->GetName()] = m_BoneCounter;
				nBoneID = m_BoneCounter;
				m_BoneCounter++;
			}
			else
			{
				nBoneID = m_BoneMap[pBone->GetName()];
			}
			if (nBoneID == -1) DebugPrint(_M("can't find bone : %s"), pBone->GetName());
			_ftprintf(pStream, _T("%0.4f\t%d\t"), normalizedWeight, nBoneID);
		}
		if (i == (nNumNode - 1)) break;
	}
	_ftprintf(pStream, _T("\n"));
}



void AsciiExp::ComputeTangentVector(INode* node, Mesh* mesh, bool bNegScale, TimeValue t)
{
	if (!node || !mesh) return;
	if (!mesh->getNumTVerts()) return;
	Matrix3 Aftertm = node->GetObjTMAfterWSM(t);
	Matrix3 LocalTM, WorldTM, InvWorldTM;
	LocalTM = node->GetNodeTM(GetStaticFrame()) * Inverse(node->GetParentTM(GetStaticFrame()));
	WorldTM = node->GetNodeTM(GetStaticFrame());
	InvWorldTM = Inverse(WorldTM);
	Modifier* pSkinMod = FindSkinModifier(node);
	int vx[3];

	if (bNegScale)
	{
		vx[0] = 2;
		vx[1] = 1;
		vx[2] = 0;
	}
	else
	{
		vx[0] = 0;
		vx[1] = 1;
		vx[2] = 2;
	}
	m_mTangent.clear();
	m_mBinormal.clear();
	std::unordered_map<UINT, XMFLOAT3> mTan1;
	std::unordered_map<UINT, XMFLOAT3> mTan2;
	std::unordered_map<UINT, XMFLOAT3> mNormal;
	for (int i = 0; i < mesh->getNumFaces(); i++)
	{
		UINT uiIndex1 = mesh->faces[i].v[vx[2]];
		UINT uiIndex2 = mesh->faces[i].v[vx[1]];
		UINT uiIndex3 = mesh->faces[i].v[vx[0]];

		Point3 v1 = Aftertm * InvWorldTM * mesh->verts[uiIndex1];
		Point3 v2 = Aftertm * InvWorldTM * mesh->verts[uiIndex2];
		Point3 v3 = Aftertm * InvWorldTM * mesh->verts[uiIndex3];
		UVVert w1 = mesh->tVerts[mesh->tvFace[i].t[vx[2]]];
		UVVert w2 = mesh->tVerts[mesh->tvFace[i].t[vx[1]]];
		UVVert w3 = mesh->tVerts[mesh->tvFace[i].t[vx[0]]];
		w1.y = 1.0f - w1.y;
		w2.y = 1.0f - w2.y;
		w3.y = 1.0f - w3.y;

		float x1 = v2.x - v1.x;
		float x2 = v3.x - v1.x;
		float y1 = v2.z - v1.z;
		float y2 = v3.z - v1.z;
		float z1 = v2.y - v1.y;
		float z2 = v3.y - v1.y;

		float s1 = w2.x - w1.x;
		float s2 = w3.x - w1.x;
		float t1 = w2.y - w1.y;
		float t2 = w3.y - w1.y;
		float fTemp = (s1 * t2 - s2 * t1);
		if (fTemp == 0.0f) continue;
		float r = 1.0F / (s1 * t2 - s2 * t1);
		XMFLOAT3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
		XMFLOAT3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);


		mTan1[i * 3 + 0] = sdir;
		mTan1[i * 3 + 1] = sdir;
		mTan1[i * 3 + 2] = sdir;

		mTan2[i * 3 + 0] = tdir;
		mTan2[i * 3 + 1] = tdir;
		mTan2[i * 3 + 2] = tdir;

		for (int j = 2; j >= 0; j--)
		{
			int vert = mesh->faces[i].getVert(vx[j]);

			Point3 vn;
			if (pSkinMod)
			{
				//non-uniform scale변환을 위해 이렇게 함
				//InvWorldTM자체가 역행렬이라 전치행렬 * n만 한 구조
				vn.x = (Aftertm * InvWorldTM).GetColumn3(0).x * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).x +
					(Aftertm * InvWorldTM).GetColumn3(0).y * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).y +
					(Aftertm * InvWorldTM).GetColumn3(0).z * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).z;

				vn.y = (Aftertm * InvWorldTM).GetColumn3(1).x * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).x +
					(Aftertm * InvWorldTM).GetColumn3(1).y * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).y +
					(Aftertm * InvWorldTM).GetColumn3(1).z * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).z;

				vn.z = (Aftertm * InvWorldTM).GetColumn3(2).x * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).x +
					(Aftertm * InvWorldTM).GetColumn3(2).y * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).y +
					(Aftertm * InvWorldTM).GetColumn3(2).z * (GetVertexNormal(mesh, i, mesh->getRVertPtr(vert))).z;
			}
			else
			{
				vn = GetVertexNormal(mesh, i, mesh->getRVertPtr(vert));
			}
			XMFLOAT3 vtrNormal(vn.x, vn.z, vn.y);
			XMVECTOR v = XMLoadFloat3(&vtrNormal);
			v = XMVector3Normalize(v);
			XMStoreFloat3(&vtrNormal, v);

			if (j == 2)
				mNormal[i * 3 + 0] = vtrNormal;
			if (j == 1)
				mNormal[i * 3 + 1] = vtrNormal;
			if (j == 0)
				mNormal[i * 3 + 2] = vtrNormal;
		}
	}
	for (int i = 0; i < (mesh->getNumFaces() * 3); i++)
	{
		XMFLOAT3 vtrNormal, vtrTan1, vtrTan2;
		auto _find = mNormal.find(i);
		if (_find == mNormal.end()) continue;
		vtrNormal = _find->second;

		_find = mTan1.find(i);
		if (_find == mTan1.end()) continue;
		vtrTan1 = _find->second;

		_find = mTan2.find(i);
		if (_find == mTan2.end()) continue;
		vtrTan2 = _find->second;

		XMVECTOR vNormal = XMLoadFloat3(&vtrNormal);
		XMVECTOR vTan1 = XMLoadFloat3(&vtrTan1);
		XMVECTOR vTan2 = XMLoadFloat3(&vtrTan2);

		// vtrTangent = vtrTan1 - vtrNormal * dot(vtrNormal, vtrTan1)
		XMFLOAT3 vtrTangent;
		XMVECTOR vTangent = XMVectorSubtract(vTan1, XMVectorMultiply(vNormal, XMVectorReplicate(XMVectorGetX(XMVector3Dot(vNormal, vTan1)))));
		vTangent = XMVector3Normalize(vTangent);
		XMStoreFloat3(&vtrTangent, vTangent);

		// Binormal (Bitangent) 계산
		XMFLOAT3 vtrBinormal;
		XMVECTOR vBinormal = XMVector3Cross(vNormal, vTangent);
		vBinormal = XMVector3Normalize(vBinormal);

		// Flip 여부 계산
		float fFlip = XMVectorGetX(XMVector3Dot(vBinormal, vTan2)) < 0.0f ? -1.0f : 1.0f;
		vBinormal = XMVectorScale(vBinormal, fFlip);
		XMStoreFloat3(&vtrBinormal, vBinormal);

		// 유효성 검사 후 map에 저장
		if (XMVectorGetX(XMVector3Length(vTangent)) >= 0.1f)
			m_mTangent[i] = vtrTangent;
		if (XMVectorGetX(XMVector3Length(vBinormal)) >= 0.1f)
			m_mBinormal[i] = vtrBinormal;
	}
}

void AsciiExp::ExportMesh(INode* node, TimeValue t, int indentLevel)
{
	int i, j, index;
	Point3 v;
	Mtl* nodeMtl = node->GetMtl();
	Matrix3 Aftertm = node->GetObjTMAfterWSM(t); //일단 worldTM이랑 같음. before도 테스트해보니 after하고 똑같음
	Matrix3 LocalTM, WorldTM, InvWorldTM;
	LocalTM = node->GetNodeTM(GetStaticFrame()) * Inverse(node->GetParentTM(GetStaticFrame()));
	WorldTM = node->GetNodeTM(GetStaticFrame());
	InvWorldTM = Inverse(WorldTM); //Aftertm * InvWorldTM는 offset관련은 아닌 것 같고, 그냥 안전장치인듯
	BOOL negScale = TMNegParity(Aftertm); //웬만하면 다 0이라고 보면됨
	TSTR indent;
	indent = GetIndent(indentLevel + 1);

	ObjectState os = node->EvalWorldState(t);
	if (!os.obj || os.obj->SuperClassID() != GEOMOBJECT_CLASS_ID) {
		return; // Safety net. This shouldn't happen.
	}

	Modifier* pSkinMod = FindSkinModifier(node); //node에 skin이 있다는 건 뼈도 있다는 뜻

	int vx[3];
	// Order of the vertices. Get 'em counter clockwise if the objects is
	// negatively scaled.
	if (negScale) {
		vx[0] = 2;
		vx[1] = 1;
		vx[2] = 0;
	}
	else {
		vx[0] = 0;
		vx[1] = 1;
		vx[2] = 2;
	}

	BOOL needDel;
	TriObject* tri = GetTriObjectFromNode(node, t, needDel);
	if (!tri) {
		return;
	}

	Mesh* mesh = &tri->GetMesh();

	mesh->buildNormals();

	ComputeTangentVector(node, mesh, negScale, t);

	_ftprintf(pStream, _T("%s%s {\n"), indent.data(), ID_MESH);
	mesh->buildBoundingBox();
	Matrix3 combinedTM = Aftertm * InvWorldTM;
	Box3 BoundingBox = mesh->getBoundingBox(&combinedTM);
	_ftprintf(pStream, _T("%s\t%s\t%f\t%f\t%f\n"), indent.data(), _T("*MAX_VERT"), BoundingBox.pmax.x, BoundingBox.pmax.z, BoundingBox.pmax.y);
	_ftprintf(pStream, _T("%s\t%s\t%f\t%f\t%f\n"), indent.data(), _T("*MIN_VERT"), BoundingBox.pmin.x, BoundingBox.pmin.z, BoundingBox.pmin.y);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_MESH_NUMFACES, mesh->getNumFaces());
	_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_MESH_FACE_LIST);

	for (i = 0; i < mesh->getNumFaces(); i++)
	{
		_ftprintf(pStream, _T("\t\t\t%s\t%d\n"), _T("*FN"), i);
		if (node->GetMtl())
		{
			int SubMtlsID = mesh->faces[i].getMatID();
			Mtl* curMtl = node->GetMtl();
			//current MtlNum
			_ftprintf(pStream, _T("\t\t\t%d\t"), mtlList.GetMtlID(curMtl)); //geometry하나당 mtl id한개, submtl잇다면 추가

			//sub MtlNum
			if (curMtl->NumSubMtls() > 0)
			{
				int SubMtlsID = mesh->faces[i].getMatID();
				_ftprintf(pStream, _T("%d\n"), SubMtlsID);
			}
			else
			{
				_ftprintf(pStream, _T("%d\n"), 0);
			}
		}
		else
		{
			_ftprintf(pStream, _T("\t\t\t-1\n"));
		}
		for (j = 2; j >= 0; j--)
		{
			int nVertexNum = mesh->faces[i].v[vx[j]]; //index
			Point3 vn;
			ZeroMemory(&vn, sizeof(Point3));
			UVVert tv;
			ZeroMemory(&tv, sizeof(UVVert));
			v = VectorTransform(Aftertm * InvWorldTM, mesh->verts[nVertexNum]);
			int NumTvert = mesh->getNumTVerts(); //uv좌표 개수
			if (NumTvert)
			{
				tv = mesh->tVerts[mesh->tvFace[i].t[vx[j]]];
				tv.y = 1.0f - tv.y;
			}

			if (pSkinMod)
			{
				//non-uniform scale변환을 위해 이렇게 함
				vn.x = (Aftertm * InvWorldTM).GetColumn3(0).x * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).x +
					(Aftertm * InvWorldTM).GetColumn3(0).y * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).y +
					(Aftertm * InvWorldTM).GetColumn3(0).z * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).z;

				vn.y = (Aftertm * InvWorldTM).GetColumn3(1).x * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).x +
					(Aftertm * InvWorldTM).GetColumn3(1).y * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).y +
					(Aftertm * InvWorldTM).GetColumn3(1).z * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).z;

				vn.z = (Aftertm * InvWorldTM).GetColumn3(2).x * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).x +
					(Aftertm * InvWorldTM).GetColumn3(2).y * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).y +
					(Aftertm * InvWorldTM).GetColumn3(2).z * (GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum))).z;
			}
			else
			{
				vn = GetVertexNormal(mesh, i, mesh->getRVertPtr(nVertexNum));
			}
			XMFLOAT3 vtrTangent(0, 0, 0);
			XMFLOAT3 vtrBinormal(0, 0, 0);
			auto _find = m_mTangent.find(i * 3 + (2 - j));
			if (_find != m_mTangent.end()) vtrTangent = _find->second;
			_find = m_mBinormal.find(i * 3 + (2 - j));
			if (_find != m_mBinormal.end()) vtrBinormal = _find->second;

			_ftprintf(pStream, _T("\t\t\t%s\t"), Format(v).data());
			_ftprintf(pStream, _T("%.4f\t%.4f\t"), tv.x, tv.y);
			_ftprintf(pStream, _T("%s\t"), Format(vn).data());
			_ftprintf(pStream, _T("%.4f\t%.4f\t%.4f\n"), vtrTangent.x, vtrTangent.y, vtrTangent.z);
			//_ftprintf(pStream, _T("%f\t%f\t%f\n"), vtrBinormal.x, vtrBinormal.y, vtrBinormal.z);

			if (pSkinMod)
				ExportSkinName(node, pSkinMod, nVertexNum);
			else
				_ftprintf(pStream, _T("\t\t\t%d\n"), 1);

		}


	}
	_ftprintf(pStream, _T("\t\t}\n"));
	_ftprintf(pStream, _T("%s}\n"), indent.data());

	if (needDel) {
		delete tri;
	}
}

Point3 AsciiExp::GetVertexNormal(Mesh* mesh, int faceNo, RVertex* rv)
{
	Face* f = &mesh->faces[faceNo];
	DWORD smGroup = f->smGroup;
	int numNormals = 0;
	Point3 vertexNormal;

	// Is normal specified
	// SPCIFIED is not currently used, but may be used in future versions.
	if (rv->rFlags & SPECIFIED_NORMAL) {
		vertexNormal = rv->rn.getNormal();
	}
	// If normal is not specified it's only available if the face belongs
	// to a smoothing group
	else if ((numNormals = rv->rFlags & NORCT_MASK) != 0 && smGroup) {
		// If there is only one vertex is found in the rn member.
		if (numNormals == 1) {
			vertexNormal = rv->rn.getNormal();
		}
		else {
			// If two or more vertices are there you need to step through them
			// and find the vertex with the same smoothing group as the current face.
			// You will find multiple normals in the ern member.
			for (int i = 0; i < numNormals; i++) {
				if (rv->ern[i].getSmGroup() & smGroup) {
					vertexNormal = rv->ern[i].getNormal();
				}
			}
		}
	}
	else {
		// Get the normal from the Face if no smoothing groups are there
		vertexNormal = mesh->getFaceNormal(faceNo);
	}

	return vertexNormal;
}

/****************************************************************************

  Inverse Kinematics (IK) Joint information

****************************************************************************/

void AsciiExp::ExportIKJoints(INode* node, int indentLevel)
{
	Control* cont;
	TSTR indent = GetIndent(indentLevel);

	if (node->TestAFlag(A_INODE_IK_TERMINATOR))
		_ftprintf(pStream, _T("%s\t%s\n"), indent.data(), ID_IKTERMINATOR);

	if (node->TestAFlag(A_INODE_IK_POS_PINNED))
		_ftprintf(pStream, _T("%s\t%s\n"), indent.data(), ID_IKPOS_PINNED);

	if (node->TestAFlag(A_INODE_IK_ROT_PINNED))
		_ftprintf(pStream, _T("%s\t%s\n"), indent.data(), ID_IKROT_PINNED);

	// Position joint
	cont = node->GetTMController()->GetPositionController();
	if (cont) {
		JointParams* joint = (JointParams*)cont->GetProperty(PROPID_JOINTPARAMS);
		if (joint && !joint->IsDefault()) {
			// Has IK Joints!!!
			_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_IKJOINT);
			DumpJointParams(joint, indentLevel + 1);
			_ftprintf(pStream, _T("%s\t}\n"), indent.data());
		}
	}

	// Rotational joint
	cont = node->GetTMController()->GetRotationController();
	if (cont) {
		JointParams* joint = (JointParams*)cont->GetProperty(PROPID_JOINTPARAMS);
		if (joint && !joint->IsDefault()) {
			// Has IK Joints!!!
			_ftprintf(pStream, _T("%s\t%s {\n"), indent.data(), ID_IKJOINT);
			DumpJointParams(joint, indentLevel + 1);
			_ftprintf(pStream, _T("%s\t}\n"), indent.data());
		}
	}
}

void AsciiExp::DumpJointParams(JointParams* joint, int indentLevel)
{
	TSTR indent = GetIndent(indentLevel);
	float scale = joint->scale;

	_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_IKTYPE, joint->Type() == JNT_POS ? ID_IKTYPEPOS : ID_IKTYPEROT);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKDOF, joint->dofs);

	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKXACTIVE, joint->flags & JNT_XACTIVE ? 1 : 0);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKYACTIVE, joint->flags & JNT_YACTIVE ? 1 : 0);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKZACTIVE, joint->flags & JNT_ZACTIVE ? 1 : 0);

	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKXLIMITED, joint->flags & JNT_XLIMITED ? 1 : 0);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKYLIMITED, joint->flags & JNT_YLIMITED ? 1 : 0);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKZLIMITED, joint->flags & JNT_ZLIMITED ? 1 : 0);

	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKXEASE, joint->flags & JNT_XEASE ? 1 : 0);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKYEASE, joint->flags & JNT_YEASE ? 1 : 0);
	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKZEASE, joint->flags & JNT_ZEASE ? 1 : 0);

	_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_IKLIMITEXACT, joint->flags & JNT_LIMITEXACT ? 1 : 0);

	for (int i = 0; i < joint->dofs; i++) {
		_ftprintf(pStream, _T("%s\t%s %d %s %s %s\n"), indent.data(), ID_IKJOINTINFO, i, Format(joint->min[i]).data(), Format(joint->max[i]).data(), Format(joint->damping[i]).data());
	}

}

/****************************************************************************

  Material and Texture Export

****************************************************************************/

int AsciiExp::SearchMtl(Mtl* mtl)
{
	int mtlCount = 0;
	for (int i = 0; i < mtl->NumSubTexmaps(); i++) {
		Texmap* subTex = mtl->GetSubTexmap(i);
		if (subTex) {
			// If it is a standard material we can see if the map is enabled.
			if (mtl->ClassID() == Class_ID(DMTL_CLASS_ID, 0)) {
				if (!((StdMat*)mtl)->MapEnabled(i))
					continue;

			}
			mtlCount++; //subTex한번 발견되면 증가시키고 나온다
			break;
		}
	}

	if (mtl->NumSubMtls() > 0) {

		for (int i = 0; i < mtl->NumSubMtls(); i++) {
			Mtl* subMtl = mtl->GetSubMtl(i);
			if (subMtl) {
				mtlCount += SearchMtl(subMtl);
			}
		}
	}

	return mtlCount;
}

bool AsciiExp::searchMesh(INode* node, bool exportSelected, int& meshNum, int* indexNum)
{
	if (exportSelected && node->Selected() == FALSE)
		return TREE_CONTINUE;

	if (!exportSelected || node->Selected()) {

		ObjectState os = node->EvalWorldState(0);

		if (os.obj && os.obj->SuperClassID() == GEOMOBJECT_CLASS_ID) {
			if (GetIncludeObjGeom())
			{
				BOOL needDel;
				TriObject* tri = GetTriObjectFromNode(node, GetStaticFrame(), needDel);
				if (!tri) {
					return FALSE;
				}

				Mesh* mesh = &tri->GetMesh();

				indexNum[meshNum] = mesh->getNumFaces() * 3;
				if (needDel) {
					delete tri;
				}

				meshNum++;
			}
		}

	}

	for (int c = 0; c < node->NumberOfChildren(); c++) {
		if (!searchMesh(node->GetChildNode(c), exportSelected, meshNum,indexNum))
			return FALSE;
	}


	return true;
}

void AsciiExp::Export_Mtl_Mesh_Index_Count(bool exportSelected)
{
	if (!GetIncludeMtl()) {
		return;
	}

	//material 수 구하기
	int numMtls = mtlList.Count(); //sub말고 mtl자체의 수 

	int totalMtlCount = 0; //sub포함 사용되는 mtl수. exported될 것

	for (int i = 0; i < numMtls; i++)
	{
		totalMtlCount += SearchMtl(mtlList.GetMtl(i));
	}


	//mesh,index 수 구하기
	int meshCount = 0;
	int indexCount[50] = { 0, }; //최대메쉬 50개
	if (!exportSelected)
	{
		int numChildren = ip->GetRootNode()->NumberOfChildren();
		for (int idx = 0; idx < numChildren; idx++)
		{
			if (ip->GetCancel())
				break;
			searchMesh(ip->GetRootNode()->GetChildNode(idx), exportSelected, meshCount,indexCount);
		}
	}
	else
	{
		int numSelCount = ip->GetSelNodeCount();
		for (int idx = 0; idx < numSelCount; idx++)
		{
			if (ip->GetCancel())
				break;
			searchMesh(ip->GetSelNode(idx), exportSelected, meshCount, indexCount);
		}
	}

	_ftprintf(pStream, _T("%s %d\n"), _T("usedMaterialCnt"), totalMtlCount);
	_ftprintf(pStream, _T("%s %d\n"), _T("meshCnt"), meshCount);
	for (int i = 0; i < meshCount; i++)
	{
		_ftprintf(pStream, _T("%s %d\n"), _T("indexCnt"), indexCount[i]);
	}
}

void AsciiExp::ExportMaterialList()
{
	if (!GetIncludeMtl()) {
		return;
	}

	_ftprintf(pStream, _T("%s {\n"), ID_MATERIAL_LIST);

	int numMtls = mtlList.Count();
	_ftprintf(pStream, _T("\t%s %d\n"), ID_MATERIAL_COUNT, numMtls);

	DebugPrint(_M("material Num = %d"), numMtls);

	for (int i = 0; i < numMtls; i++) {
		DumpMaterial(mtlList.GetMtl(i), i, -1, 0);
	}

	_ftprintf(pStream, _T("}\n"));
}

void AsciiExp::ExportMaterial(INode* node, int indentLevel)
{
	Mtl* mtl = node->GetMtl();

	TSTR indent = GetIndent(indentLevel);

	// If the node does not have a material, export the wireframe color
	if (mtl) {
		int mtlID = mtlList.GetMtlID(mtl);
		if (mtlID >= 0) {
			_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_MATERIAL_REF, mtlID);
		}
	}
	else {
		DWORD c = node->GetWireColor();
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_WIRECOLOR,
			Format(Color(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f)).data());
	}
}

void AsciiExp::DumpMaterial(Mtl* mtl, int mtlID, int subNo, int indentLevel)
{
	int i;
	TimeValue t = GetStaticFrame();

	if (!mtl) return;

	TSTR indent = GetIndent(indentLevel + 1);

	TSTR className;
	mtl->GetClassName(className, true);


	if (subNo == -1) {
		// Top level material
		_ftprintf(pStream, _T("%s%s %d {\n"), indent.data(), _T("*MATERIAL ID"), mtlID);
	}
	else {
		_ftprintf(pStream, _T("%s%s %d {\n"), indent.data(), ID_SUBMATERIAL, subNo);
	}
	Interface14* iface = GetCOREInterface14();
	UINT codepage = iface->DefaultTextSaveCodePage(true);
	TSTR mtlName = FixupName(mtl->GetName());
	const char* mtlName_locale = mtlName.ToCP(codepage);
	_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_MATNAME, mtlName_locale);

	// We know the Standard material, so we can get some extra info
	if (mtl->ClassID() == Class_ID(DMTL_CLASS_ID, 0)) {
		StdMat* std = (StdMat*)mtl;

		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_AMBIENT, Format(std->GetAmbient(t)).data());
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_DIFFUSE, Format(std->GetDiffuse(t)).data());
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_SPECULAR, Format(std->GetSpecular(t)).data());
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_TRANSPARENCY, Format(std->GetXParency(t)).data());
		if (std->GetTwoSided()) {
			_ftprintf(pStream, _T("%s\t%s\n"), indent.data(), ID_TWOSIDED);
		}
	}
	else {
		// Note about material colors:
		// This is only the color used by the interactive renderer in MAX.
		// To get the color used by the scanline renderer, we need to evaluate
		// the material using the mtl->Shade() method.
		// Since the materials are procedural there is no real "diffuse" color for a MAX material
		// but we can at least take the interactive color.

		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_AMBIENT, Format(mtl->GetAmbient()).data());
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_DIFFUSE, Format(mtl->GetDiffuse()).data());
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_SPECULAR, Format(mtl->GetSpecular()).data());
		_ftprintf(pStream, _T("%s\t%s %s\n"), indent.data(), ID_TRANSPARENCY, Format(mtl->GetXParency()).data());
	}
	DebugPrint(_M("subTexmapNum : %d"), mtl->NumSubTexmaps());
	for (i = 0; i < mtl->NumSubTexmaps(); i++) {
		Texmap* subTex = mtl->GetSubTexmap(i);
		float amt = 1.0f;
		if (subTex) {
			// If it is a standard material we can see if the map is enabled.
			if (mtl->ClassID() == Class_ID(DMTL_CLASS_ID, 0)) {
				if (!((StdMat*)mtl)->MapEnabled(i))
					continue;
				amt = ((StdMat*)mtl)->GetTexmapAmt(i, 0);

			}
			DumpTexture(subTex, mtl->ClassID(), i, amt, indentLevel + 1);
		}
	}

	if (mtl->NumSubMtls() > 0) {
		//_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_NUMSUBMTLS, mtl->NumSubMtls());

		for (i = 0; i < mtl->NumSubMtls(); i++) {
			Mtl* subMtl = mtl->GetSubMtl(i);
			if (subMtl) {
				DumpMaterial(subMtl, 0, i, indentLevel + 1);
			}
		}
	}
	_ftprintf(pStream, _T("%s}\n"), indent.data());
}


// For a standard material, this will give us the meaning of a map
// givien its submap id.
TCHAR* AsciiExp::GetMapID(Class_ID cid, int subNo)
{
	static TCHAR buf[50];


	if (cid == Class_ID(0, 0)) {
		_tcscpy(buf, ID_ENVMAP);
	}
	else if (cid == Class_ID(DMTL_CLASS_ID, 0)) {
		switch (subNo) {
		case ID_AM: _tcscpy(buf, ID_MAP_AMBIENT); break;
		case ID_DI: _tcscpy(buf, ID_MAP_DIFFUSE); break;
		case ID_SP: _tcscpy(buf, ID_MAP_SPECULAR); break;
		case ID_BU: _tcscpy(buf, ID_MAP_BUMP); break;
		}
	}
	else {
		_tcscpy(buf, ID_MAP_GENERIC);
	}


	return buf;
}

void AsciiExp::DumpTexture(Texmap* tex, Class_ID cid, int subNo, float amt, int indentLevel)
{
	if (!tex) return;

	TSTR indent = GetIndent(indentLevel + 1);

	TSTR className;
	tex->GetClassName(className, true);

	_ftprintf(pStream, _T("%s%s {\n"), indent.data(), _T("*TEXTURE"));

	Interface14* iface = GetCOREInterface14();
	UINT codepage = iface->DefaultTextSaveCodePage(true);
	TSTR texName = FixupName(tex->GetName());
	const char* texName_locale = texName.ToCP(codepage);
	_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_TEXNAME, texName_locale);
	//TSTR classNameStr = FixupName(className);
	//const char* className_locale = classNameStr.ToCP(codepage);
	//_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_TEXCLASS, className_locale);

	// If we include the subtexture ID, a parser could be smart enough to get
	// the class name of the parent texture/material and make it mean something.
	//_ftprintf(pStream, _T("%s\t%s %d\n"), indent.data(), ID_TEXSUBNO, subNo);

	// Is this a bitmap texture?
	// We know some extra bits 'n pieces about the bitmap texture
	if (tex->ClassID() == Class_ID(BMTEX_CLASS_ID, 0x00)) {
		TSTR mapName = ((BitmapTex*)tex)->GetMapName();
		TSTR mapNameStr = FixupName(mapName);
		const char* mapName_locale = mapNameStr.ToCP(codepage);
		_ftprintf(pStream, _T("%s\t%s \"%hs\"\n"), indent.data(), ID_BITMAP, mapName_locale);
	}

	DebugPrint(_M("texTexmapNum : %d"), tex->NumSubTexmaps());
	for (int i = 0; i < tex->NumSubTexmaps(); i++) {
		DumpTexture(tex->GetSubTexmap(i), tex->ClassID(), i, 1.0f, indentLevel + 1);
	}

	_ftprintf(pStream, _T("%s}\n"), indent.data());
}

void AsciiExp::DumpUVGen(StdUVGen* uvGen, int indentLevel)
{
	int mapType = uvGen->GetCoordMapping(0);
	TimeValue t = GetStaticFrame();
	TSTR indent = GetIndent(indentLevel + 1);

	_ftprintf(pStream, _T("%s%s "), indent.data(), ID_MAPTYPE);

	switch (mapType) {
	case UVMAP_EXPLICIT: _ftprintf(pStream, _T("%s\n"), ID_MAPTYPE_EXP); break;
	case UVMAP_SPHERE_ENV: _ftprintf(pStream, _T("%s\n"), ID_MAPTYPE_SPH); break;
	case UVMAP_CYL_ENV:  _ftprintf(pStream, _T("%s\n"), ID_MAPTYPE_CYL); break;
	case UVMAP_SHRINK_ENV: _ftprintf(pStream, _T("%s\n"), ID_MAPTYPE_SHR); break;
	case UVMAP_SCREEN_ENV: _ftprintf(pStream, _T("%s\n"), ID_MAPTYPE_SCR); break;
	}

	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_U_OFFSET, Format(uvGen->GetUOffs(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_V_OFFSET, Format(uvGen->GetVOffs(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_U_TILING, Format(uvGen->GetUScl(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_V_TILING, Format(uvGen->GetVScl(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_ANGLE, Format(uvGen->GetAng(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_BLUR, Format(uvGen->GetBlur(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_BLUR_OFFSET, Format(uvGen->GetBlurOffs(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_NOISE_AMT, Format(uvGen->GetNoiseAmt(t)).data());
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_NOISE_SIZE, Format(uvGen->GetNoiseSize(t)).data());
	_ftprintf(pStream, _T("%s%s %d\n"), indent.data(), ID_NOISE_LEVEL, uvGen->GetNoiseLev(t));
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_NOISE_PHASE, Format(uvGen->GetNoisePhs(t)).data());
}

/****************************************************************************

  Face Mapped Material functions

****************************************************************************/

BOOL AsciiExp::CheckForAndExportFaceMap(Mtl* mtl, Mesh* mesh, int indentLevel)
{
	if (!mtl || !mesh) {
		return FALSE;
	}

	ULONG matreq = mtl->Requirements(-1);

	// Are we using face mapping?
	if (!(matreq & MTLREQ_FACEMAP)) {
		return FALSE;
	}

	TSTR indent = GetIndent(indentLevel + 1);

	// OK, we have a FaceMap situation here...

	_ftprintf(pStream, _T("%s%s {\n"), indent.data(), ID_MESH_FACEMAPLIST);
	for (int i = 0; i < mesh->getNumFaces(); i++) {
		Point3 tv[3];
		Face* f = &mesh->faces[i];
		make_face_uv(f, tv);
		_ftprintf(pStream, _T("%s\t%s %d {\n"), indent.data(), ID_MESH_FACEMAP, i);
		_ftprintf(pStream, _T("%s\t\t%s\t%d\t%d\t%d\n"), indent.data(), ID_MESH_FACEVERT, (int)tv[0].x, (int)tv[0].y, (int)tv[0].z);
		_ftprintf(pStream, _T("%s\t\t%s\t%d\t%d\t%d\n"), indent.data(), ID_MESH_FACEVERT, (int)tv[1].x, (int)tv[1].y, (int)tv[1].z);
		_ftprintf(pStream, _T("%s\t\t%s\t%d\t%d\t%d\n"), indent.data(), ID_MESH_FACEVERT, (int)tv[2].x, (int)tv[2].y, (int)tv[2].z);
		_ftprintf(pStream, _T("%s\t}\n"), indent.data());
	}
	_ftprintf(pStream, _T("%s}\n"), indent.data());

	return TRUE;
}


/****************************************************************************

  Misc Utility functions

****************************************************************************/

// Return an indentation string
TSTR AsciiExp::GetIndent(int indentLevel)
{
	TSTR indentString = _T("");
	for (int i = 0; i < indentLevel; i++) {
		indentString += _T("\t");
	}

	return indentString;
}

// Determine is the node has negative scaling.
// This is used for mirrored objects for example. They have a negative scale factor
// so when calculating the normal we should take the vertices counter clockwise.
// If we don't compensate for this the objects will be 'inverted'.
BOOL AsciiExp::TMNegParity(Matrix3& m)
{
	return (DotProd(CrossProd(m.GetRow(0), m.GetRow(1)), m.GetRow(2)) < 0.0) ? 1 : 0;
}

// Return a pointer to a TriObject given an INode or return NULL
// if the node cannot be converted to a TriObject
TriObject* AsciiExp::GetTriObjectFromNode(INode* node, TimeValue t, int& deleteIt)
{
	deleteIt = FALSE;
	Object* obj = node->EvalWorldState(t).obj;
	if (obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0))) {
		TriObject* tri = (TriObject*)obj->ConvertToType(t,
			Class_ID(TRIOBJ_CLASS_ID, 0));
		// Note that the TriObject should only be deleted
		// if the pointer to it is not equal to the object
		// pointer that called ConvertToType()
		if (obj != tri) deleteIt = TRUE;
		return tri;
	}
	else {
		return NULL;
	}
}

// Print out a transformation matrix in different ways.
// Apart from exporting the full matrix we also decompose
// the matrix and export the components.
void AsciiExp::DumpMatrix3(Matrix3* m, int indentLevel)
{
	Point3 row;
	TSTR indent = GetIndent(indentLevel);

	// Dump the whole Matrix
	row = m->GetRow(0);
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_TM_ROW0, Format(row).data());
	row = m->GetRow(1);
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_TM_ROW1, Format(row).data());
	row = m->GetRow(2);
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_TM_ROW2, Format(row).data());
	row = m->GetRow(3);
	_ftprintf(pStream, _T("%s%s %s\n"), indent.data(), ID_TM_ROW3, Format(row).data());
}

// From the SDK
// How to calculate UV's for face mapped materials.
static Point3 basic_tva[3] = {
	Point3(0.0,0.0,0.0),Point3(1.0,0.0,0.0),Point3(1.0,1.0,0.0)
};
static Point3 basic_tvb[3] = {
	Point3(1.0,1.0,0.0),Point3(0.0,1.0,0.0),Point3(0.0,0.0,0.0)
};
static int nextpt[3] = { 1,2,0 };
static int prevpt[3] = { 2,0,1 };

void AsciiExp::make_face_uv(Face* f, Point3* tv)
{
	int na, nhid, i;
	Point3* basetv;
	/* make the invisible edge be 2->0 */
	nhid = 2;
	if (!(f->flags & EDGE_A))  nhid = 0;
	else if (!(f->flags & EDGE_B)) nhid = 1;
	else if (!(f->flags & EDGE_C)) nhid = 2;
	na = 2 - nhid;
	basetv = (f->v[prevpt[nhid]] < f->v[nhid]) ? basic_tva : basic_tvb;
	for (i = 0; i < 3; i++) {
		tv[i] = basetv[na];
		na = nextpt[na];
	}
}


/****************************************************************************

  String manipulation functions

****************************************************************************/

#define CTL_CHARS  31
#define SINGLE_QUOTE 39

// Replace some characters we don't care for.
TCHAR* AsciiExp::FixupName(const TCHAR* name)
{
	static TCHAR buffer[256];
	TCHAR* cPtr;

	_tcsncpy_s(buffer, _countof(buffer), name, _TRUNCATE);
	cPtr = buffer;

	while (*cPtr) {
		if (*cPtr == _T('"'))
			*cPtr = SINGLE_QUOTE;
		else if (*cPtr <= CTL_CHARS)
			*cPtr = _T('_');
		cPtr++;
	}

	return buffer;
}

// International settings in Windows could cause a number to be written
// with a "," instead of a ".".
// To compensate for this we need to convert all , to . in order to make the
// format consistent.
void AsciiExp::CommaScan(TCHAR* buf)
{
	for (; *buf; buf++) if (*buf == _T(',')) *buf = _T('.');
}

TSTR AsciiExp::Format(int value)
{
	TCHAR buf[50];

	_stprintf(buf, _T("%d"), value);
	return buf;
}


TSTR AsciiExp::Format(float value)
{
	TCHAR buf[40];

	_stprintf(buf, szFmtStr, value);
	CommaScan(buf);
	return TSTR(buf);
}

TSTR AsciiExp::Format(Point3 value)
{
	TCHAR buf[120];
	TCHAR fmt[120];

	_stprintf(fmt, _T("%s\t%s\t%s"), szFmtStr, szFmtStr, szFmtStr);
	_stprintf(buf, fmt, value.x, value.z, value.y);

	CommaScan(buf);
	return buf;
}

TSTR AsciiExp::Format(Color value)
{
	TCHAR buf[120];
	TCHAR fmt[120];

	_stprintf(fmt, _T("%s\t%s\t%s"), szFmtStr, szFmtStr, szFmtStr);
	_stprintf(buf, fmt, value.r, value.g, value.b);

	CommaScan(buf);
	return buf;
}

TSTR AsciiExp::Format(AngAxis value)
{
	TCHAR buf[160];
	TCHAR fmt[160];

	_stprintf(fmt, _T("%s\t%s\t%s\t%s"), szFmtStr, szFmtStr, szFmtStr, szFmtStr);
	_stprintf(buf, fmt, value.axis.x, value.axis.y, value.axis.z, value.angle);

	CommaScan(buf);
	return buf;
}


TSTR AsciiExp::Format(Quat value)
{
	// A Quat is converted to an AngAxis before output.

	Point3 axis;
	float angle;
	AngAxisFromQ(value, &angle, axis);

	return Format(AngAxis(axis, angle));
}

TSTR AsciiExp::Format(ScaleValue value)
{
	TCHAR buf[280];

	_stprintf(buf, _T("%s %s"), Format(value.s).data(), Format(value.q).data());
	CommaScan(buf);
	return buf;
}
