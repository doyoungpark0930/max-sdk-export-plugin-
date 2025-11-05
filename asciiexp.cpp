//************************************************************************** 
//* Asciiexp.cpp	- Ascii File Exporter
//* 
//* By Christer Janson
//* Kinetix Development
//*
//* January 20, 1997 CCJ Initial coding
//*
//* This module contains the DLL startup functions
//*
//* Copyright (c) 1997, All Rights Reserved. 
//***************************************************************************

#include "asciiexp.h"

#include "3dsmaxport.h"

HINSTANCE hInstance;

static BOOL showPrompts;
static BOOL exportSelected;

using namespace MaxSDK::Util;

// Class ID. These must be unique and randomly generated!!
// If you use this as a sample project, this is the first thing
// you should change!
#define ASCIIEXP_CLASS_ID	Class_ID(0x721976f5, 0x37cd5bab)

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		MaxSDK::Util::UseLanguagePackLocale();
		hInstance = hinstDLL;
		DisableThreadLibraryCalls(hInstance);
	}

	return (TRUE);
}


__declspec(dllexport) const TCHAR* LibDescription()
{
	return GetString(IDS_LIBDESCRIPTION);
}

/// MUST CHANGE THIS NUMBER WHEN ADD NEW CLASS 
__declspec(dllexport) int LibNumberClasses()
{
	return 1;
}


__declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
	switch (i) {
	case 0: return GetAsciiExpDesc();
	default: return 0;
	}
}

__declspec(dllexport) ULONG LibVersion()
{
	return VERSION_3DSMAX;
}

// Let the plug-in register itself for deferred loading
__declspec(dllexport) ULONG CanAutoDefer()
{
	return 1;
}

class AsciiExpClassDesc :public ClassDesc {
public:
	int				IsPublic() { return 1; }
	void* Create(BOOL loading = FALSE) { return new AsciiExp; }
	const TCHAR* ClassName() { return GetString(IDS_ASCIIEXP); }
	const TCHAR* NonLocalizedClassName() { return _T("AsciiExp"); }
	SClass_ID		SuperClassID() { return SCENE_EXPORT_CLASS_ID; }
	Class_ID		ClassID() { return ASCIIEXP_CLASS_ID; }
	const TCHAR* Category() { return GetString(IDS_CATEGORY); }
};

static AsciiExpClassDesc AsciiExpDesc;

ClassDesc* GetAsciiExpDesc()
{
	return &AsciiExpDesc;
}

TCHAR* GetString(int id)
{
	static TCHAR buf[256];

	if (hInstance)
		return LoadString(hInstance, id, buf, _countof(buf)) ? buf : NULL;

	return NULL;
}

AsciiExp::AsciiExp()
{
	// These are the default values that will be active when 
	// the exporter is ran the first time.
	// After the first session these options are sticky.
	bIncludeMesh = TRUE;
	bIncludeAnim = TRUE;
	bIncludeMtl = TRUE;
	bIncludeMeshAnim = FALSE;
	bIncludeCamLightAnim = FALSE;
	bIncludeIKJoints = FALSE;
	bIncludeNormals = FALSE;
	bIncludeTextureCoords = FALSE;
	bIncludeVertexColors = FALSE;
	bIncludeObjGeom = TRUE;
	bIncludeObjShape = TRUE;
	bIncludeObjCamera = TRUE;
	bIncludeObjLight = TRUE;
	bIncludeObjHelper = TRUE;
	bAlwaysSample = FALSE;
	nKeyFrameStep = 5;
	nMeshFrameStep = 5;
	nPrecision = 4;
	nStaticFrame = 0;
	pStream = NULL;
}

AsciiExp::~AsciiExp()
{
}

int AsciiExp::ExtCount()
{
	return 1;
}

const TCHAR* AsciiExp::Ext(int n)
{
	switch (n) {
	case 0:
		// This cause a static string buffer overwrite
		// return GetString(IDS_EXTENSION1);
		//return _T("ASE");
		return _T("dy"); //새로운 확장자이름
	}
	return _T("");
}

const TCHAR* AsciiExp::LongDesc()
{
	return GetString(IDS_LONGDESC);
}

const TCHAR* AsciiExp::ShortDesc()
{
	return GetString(IDS_SHORTDESC);
}

const TCHAR* AsciiExp::AuthorName()
{
	return _T("Christer Janson");
}

const TCHAR* AsciiExp::CopyrightMessage()
{
	return GetString(IDS_COPYRIGHT);
}

const TCHAR* AsciiExp::OtherMessage1()
{
	return _T("");
}

const TCHAR* AsciiExp::OtherMessage2()
{
	return _T("");
}

unsigned int AsciiExp::Version()
{
	return 100;
}

static INT_PTR CALLBACK AboutBoxDlgProc(HWND hWnd, UINT msg,
	WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		CenterWindow(hWnd, GetParent(hWnd));
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EndDialog(hWnd, 1);
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void AsciiExp::ShowAbout(HWND hWnd)
{
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, AboutBoxDlgProc, 0);
}


// Dialog proc
static INT_PTR CALLBACK ExportDlgProc(HWND hWnd, UINT msg,
	WPARAM wParam, LPARAM lParam)
{
	Interval animRange;
	ISpinnerControl* spin;

	AsciiExp* exp = DLGetWindowLongPtr<AsciiExp*>(hWnd);
	switch (msg) {
	case WM_INITDIALOG:
		exp = (AsciiExp*)lParam;
		DLSetWindowLongPtr(hWnd, lParam);
		CenterWindow(hWnd, GetParent(hWnd));
		CheckDlgButton(hWnd, IDC_MESHDATA, exp->GetIncludeMesh());
		CheckDlgButton(hWnd, IDC_ANIMKEYS, exp->GetIncludeAnim());
		CheckDlgButton(hWnd, IDC_MATERIAL, exp->GetIncludeMtl());
		CheckDlgButton(hWnd, IDC_MESHANIM, exp->GetIncludeMeshAnim());
		CheckDlgButton(hWnd, IDC_CAMLIGHTANIM, exp->GetIncludeCamLightAnim());
		CheckDlgButton(hWnd, IDC_IKJOINTS, exp->GetIncludeIKJoints());
		CheckDlgButton(hWnd, IDC_NORMALS, exp->GetIncludeNormals());
		CheckDlgButton(hWnd, IDC_TEXCOORDS, exp->GetIncludeTextureCoords());
		CheckDlgButton(hWnd, IDC_VERTEXCOLORS, exp->GetIncludeVertexColors());
		CheckDlgButton(hWnd, IDC_OBJ_GEOM, exp->GetIncludeObjGeom());
		CheckDlgButton(hWnd, IDC_OBJ_SHAPE, exp->GetIncludeObjShape());
		CheckDlgButton(hWnd, IDC_OBJ_CAMERA, exp->GetIncludeObjCamera());
		CheckDlgButton(hWnd, IDC_OBJ_LIGHT, exp->GetIncludeObjLight());
		CheckDlgButton(hWnd, IDC_OBJ_HELPER, exp->GetIncludeObjHelper());

		CheckRadioButton(hWnd, IDC_RADIO_USEKEYS, IDC_RADIO_SAMPLE,
			exp->GetAlwaysSample() ? IDC_RADIO_SAMPLE : IDC_RADIO_USEKEYS);

		// Setup the spinner controls for the controller key sample rate 
		spin = GetISpinner(GetDlgItem(hWnd, IDC_CONT_STEP_SPIN));
		spin->LinkToEdit(GetDlgItem(hWnd, IDC_CONT_STEP), EDITTYPE_INT);
		spin->SetLimits(1, 100, TRUE);
		spin->SetScale(1.0f);
		spin->SetValue(exp->GetKeyFrameStep(), FALSE);
		ReleaseISpinner(spin);

		// Setup the spinner controls for the mesh definition sample rate 
		spin = GetISpinner(GetDlgItem(hWnd, IDC_MESH_STEP_SPIN));
		spin->LinkToEdit(GetDlgItem(hWnd, IDC_MESH_STEP), EDITTYPE_INT);
		spin->SetLimits(1, 100, TRUE);
		spin->SetScale(1.0f);
		spin->SetValue(exp->GetMeshFrameStep(), FALSE);
		ReleaseISpinner(spin);

		// Setup the spinner controls for the floating point precision 
		spin = GetISpinner(GetDlgItem(hWnd, IDC_PREC_SPIN));
		spin->LinkToEdit(GetDlgItem(hWnd, IDC_PREC), EDITTYPE_INT);
		spin->SetLimits(1, 10, TRUE);
		spin->SetScale(1.0f);
		spin->SetValue(exp->GetPrecision(), FALSE);
		ReleaseISpinner(spin);

		// Setup the spinner control for the static frame#
		// We take the frame 0 as the default value
		animRange = exp->GetInterface()->GetAnimRange();
		spin = GetISpinner(GetDlgItem(hWnd, IDC_STATIC_FRAME_SPIN));
		spin->LinkToEdit(GetDlgItem(hWnd, IDC_STATIC_FRAME), EDITTYPE_INT);
		spin->SetLimits(animRange.Start() / GetTicksPerFrame(), animRange.End() / GetTicksPerFrame(), TRUE);
		spin->SetScale(1.0f);
		spin->SetValue(0, FALSE);
		ReleaseISpinner(spin);

		// Enable / disable mesh options
		EnableWindow(GetDlgItem(hWnd, IDC_NORMALS), exp->GetIncludeMesh());
		EnableWindow(GetDlgItem(hWnd, IDC_TEXCOORDS), exp->GetIncludeMesh());
		EnableWindow(GetDlgItem(hWnd, IDC_VERTEXCOLORS), exp->GetIncludeMesh());
		break;

	case CC_SPINNER_CHANGE:
		spin = (ISpinnerControl*)lParam;
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_MESHDATA:
			// Enable / disable mesh options
			EnableWindow(GetDlgItem(hWnd, IDC_NORMALS), IsDlgButtonChecked(hWnd,
				IDC_MESHDATA));
			EnableWindow(GetDlgItem(hWnd, IDC_TEXCOORDS), IsDlgButtonChecked(hWnd,
				IDC_MESHDATA));
			EnableWindow(GetDlgItem(hWnd, IDC_VERTEXCOLORS), IsDlgButtonChecked(hWnd,
				IDC_MESHDATA));
			break;
		case IDOK:
			exp->SetIncludeMesh(IsDlgButtonChecked(hWnd, IDC_MESHDATA));
			exp->SetIncludeAnim(IsDlgButtonChecked(hWnd, IDC_ANIMKEYS));
			exp->SetIncludeMtl(IsDlgButtonChecked(hWnd, IDC_MATERIAL));
			exp->SetIncludeMeshAnim(IsDlgButtonChecked(hWnd, IDC_MESHANIM));
			exp->SetIncludeCamLightAnim(IsDlgButtonChecked(hWnd, IDC_CAMLIGHTANIM));
			exp->SetIncludeIKJoints(IsDlgButtonChecked(hWnd, IDC_IKJOINTS));
			exp->SetIncludeNormals(IsDlgButtonChecked(hWnd, IDC_NORMALS));
			exp->SetIncludeTextureCoords(IsDlgButtonChecked(hWnd, IDC_TEXCOORDS));
			exp->SetIncludeVertexColors(IsDlgButtonChecked(hWnd, IDC_VERTEXCOLORS));
			exp->SetIncludeObjGeom(IsDlgButtonChecked(hWnd, IDC_OBJ_GEOM));
			exp->SetIncludeObjShape(IsDlgButtonChecked(hWnd, IDC_OBJ_SHAPE));
			exp->SetIncludeObjCamera(IsDlgButtonChecked(hWnd, IDC_OBJ_CAMERA));
			exp->SetIncludeObjLight(IsDlgButtonChecked(hWnd, IDC_OBJ_LIGHT));
			exp->SetIncludeObjHelper(IsDlgButtonChecked(hWnd, IDC_OBJ_HELPER));
			exp->SetAlwaysSample(IsDlgButtonChecked(hWnd, IDC_RADIO_SAMPLE));

			spin = GetISpinner(GetDlgItem(hWnd, IDC_CONT_STEP_SPIN));
			exp->SetKeyFrameStep(spin->GetIVal());
			ReleaseISpinner(spin);

			spin = GetISpinner(GetDlgItem(hWnd, IDC_MESH_STEP_SPIN));
			exp->SetMeshFrameStep(spin->GetIVal());
			ReleaseISpinner(spin);

			spin = GetISpinner(GetDlgItem(hWnd, IDC_PREC_SPIN));
			exp->SetPrecision(spin->GetIVal());
			ReleaseISpinner(spin);

			spin = GetISpinner(GetDlgItem(hWnd, IDC_STATIC_FRAME_SPIN));
			exp->SetStaticFrame(spin->GetIVal() * GetTicksPerFrame());
			ReleaseISpinner(spin);

			EndDialog(hWnd, 1);
			break;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

namespace {
	FILE* OpenWriteFileWithBom(const TCHAR* filename, UINT codepage)
	{
		unsigned int encoding = TextFile::Writer::WRITE_BOM | codepage;

		// for a360 support - allows binary diff syncing
		MaxSDK::Util::Path storageNamePath(filename);
		storageNamePath.SaveBaseFile();

		switch (encoding & MSDE_CP_MASK)
		{
		case CP_UTF8:
			return  _tfopen(filename, _T("wt, ccs=UTF-8"));
			break;

		case MSDE_CP_UTF16:
			if (encoding & TextFile::Writer::FLIPPED)
			{
				return _tfopen(filename, _T("wt, ccs=UTF-16BE"));
			}
			else
			{
				return _tfopen(filename, _T("wt, ccs=UNICODE"));
			}
			break;
		default:
			return _tfopen(filename, _T("wt"));
		}
	}
}

// Start the exporter!
// This is the real entrypoint to the exporter. After the user has selected
// the filename (and he's prompted for overwrite etc.) this method is called.
int AsciiExp::DoExport(const TCHAR* name, ExpInterface* ei, Interface* i, BOOL suppressPrompts, DWORD options)
{
	// Set a global prompt display switch
	showPrompts = suppressPrompts ? FALSE : TRUE;
	exportSelected = (options & SCENE_EXPORT_SELECTED) ? TRUE : FALSE;
	DebugPrint(_M("DoExport has been run, selected : %s"), exportSelected ? _M("true") : _M("false"));
	// Grab the interface pointer.
	ip = i;

	_stprintf(szFmtStr, _T("%%4.%df"), nPrecision);

	// Get the options the user selected the last time
	ReadConfig();

	if (showPrompts) {
		// Prompt the user with our dialogbox, and get all the options.
		if (!DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_ASCIIEXPORT_DLG),
			ip->GetMAXHWnd(), ExportDlgProc, (LPARAM)this)) {
			return IMPEXP_CANCEL;
		}
	}

	_stprintf(szFmtStr, _T("%%4.%df"), nPrecision);

	// Open the stream
	Interface14* iface = GetCOREInterface14();
	UINT codepage = iface->DefaultTextSaveCodePage(true);
	pStream = OpenWriteFileWithBom(name, codepage);
	if (!pStream)
		return IMPEXP_FAIL;

	// Startup the progress bar.
	ip->ProgressStart(GetString(IDS_PROGRESS_MSG), TRUE, nullptr, nullptr);

	// Get a total node count by traversing the scene
	// We don't really need to do this, but it doesn't take long, and
	// it is nice to have an accurate progress bar.
	nTotalNodeCount = 0;
	nCurNode = 0;
	PreProcess(ip->GetRootNode(), nTotalNodeCount);

	// First we write out a file header with global information. 
	ExportGlobalInfo();

	//mtl, index개수 따져서 출력하는 함수먼저. vertex는 할 필요없음
	Export_Material_Index_Count();

	// Export list of material definitions
	ExportMaterialList();

	// Call our node enumerator.
	// The nodeEnum function will recurse into itself and 
	// export each object found in the scene.

	if (!exportSelected)
	{
		int numChildren = ip->GetRootNode()->NumberOfChildren();
		for (int idx = 0; idx < numChildren; idx++)
		{
			if (ip->GetCancel())
				break;
			nodeEnum(ip->GetRootNode()->GetChildNode(idx), 0);
		}
	}
	else
	{
		int numSelCount = ip->GetSelNodeCount();
		for (int idx = 0; idx < numSelCount; idx++)
		{
			if (ip->GetCancel())
				break;
			//SelectedNodeEnum(ip->GetSelNode(idx), 0);
			nodeEnum(ip->GetSelNode(idx), 0);
		}
	}
	WriteBoneList();
	//원래이거
	/*
	 int numChildren = ip->GetRootNode()->NumberOfChildren();
	for (int idx = 0; idx<numChildren; idx++) {
		if (ip->GetCancel())
			break;
		nodeEnum(ip->GetRootNode()->GetChildNode(idx), 0);
	}*/

	// We're done. Finish the progress bar.
	ip->ProgressEnd();

	// Close the stream
	fclose(pStream);
	pStream = NULL;

	// Write the current options to be used next time around.
	WriteConfig();

	return IMPEXP_SUCCESS;
}

BOOL AsciiExp::SupportsOptions(int ext, DWORD options) {
	assert(ext == 0);	// We only support one extension
	return(options == SCENE_EXPORT_SELECTED) ? TRUE : FALSE;
}

// This method is the main object exporter.
// It is called once of every node in the scene. The objects are
// exported as they are encoutered.

// Before recursing into the children of a node, we will export it.
// The benefit of this is that a nodes parent is always before the
// children in the resulting file. This is desired since a child's
// transformation matrix is optionally relative to the parent.

BOOL AsciiExp::nodeEnum(INode* node, int indentLevel)
{
	if (exportSelected && node->Selected() == FALSE)
		return TREE_CONTINUE;

	nCurNode++;
	ip->ProgressUpdate((int)((float)nCurNode / nTotalNodeCount * 100.0f));

	// Stop recursing if the user pressed Cancel 
	if (ip->GetCancel())
		return FALSE;

	TSTR indent = GetIndent(indentLevel);

	// Only export if exporting everything or it's selected
	if (!exportSelected || node->Selected()) {

		// The ObjectState is a 'thing' that flows down the pipeline containing
		// all information about the object. By calling EvalWorldState() we tell
		// max to eveluate the object at end of the pipeline.
		ObjectState os = node->EvalWorldState(0);

		// The obj member of ObjectState is the actual object we will export.
		if (os.obj) {

			// We look at the super class ID to determine the type of the object.
			switch (os.obj->SuperClassID()) {
			case GEOMOBJECT_CLASS_ID:
				if (GetIncludeObjGeom()) ExportGeomObject(node, indentLevel);
				break;
			case CAMERA_CLASS_ID:
				if (GetIncludeObjCamera()) ExportCameraObject(node, indentLevel);
				break;
			case LIGHT_CLASS_ID:
				break;
			case SHAPE_CLASS_ID:
				break;
			case HELPER_CLASS_ID:
				if (GetIncludeObjHelper()) ExportHelperObject(node, indentLevel);
				break;
			}
		}
	}

	// For each child of this node, we recurse into ourselves 
	// until no more children are found.
	for (int c = 0; c < node->NumberOfChildren(); c++) {
		if (!nodeEnum(node->GetChildNode(c), indentLevel))
			return FALSE;
	}

	return TRUE;
}


void AsciiExp::PreProcess(INode* node, int& nodeCount)
{
	nodeCount++;

	// Add the nodes material to out material list
	// Null entries are ignored when added...
	mtlList.AddMtl(node->GetMtl());

	// For each child of this node, we recurse into ourselves 
	// and increment the counter until no more children are found.
	for (int c = 0; c < node->NumberOfChildren(); c++) {
		PreProcess(node->GetChildNode(c), nodeCount);
	}
}

/****************************************************************************

 Configuration.
 To make all options "sticky" across sessions, the options are read and
 written to a configuration file every time the exporter is executed.

 ****************************************************************************/

TSTR AsciiExp::GetCfgFilename()
{
	TSTR filename;

	filename += ip->GetDir(APP_PLUGCFG_DIR);
	filename += _T("\\");
	filename += CFGFILENAME;

	return filename;
}

// NOTE: Update anytime the CFG file changes
#define CFG_VERSION 0x03

BOOL AsciiExp::ReadConfig()
{
	TSTR filename = GetCfgFilename();
	FILE* cfgStream;

	cfgStream = _tfopen(filename, _T("rb"));
	if (!cfgStream)
		return FALSE;

	// First item is a file version
	int fileVersion = _getw(cfgStream);

	if (fileVersion > CFG_VERSION) {
		// Unknown version
		fclose(cfgStream);
		return FALSE;
	}

	SetIncludeMesh(fgetc(cfgStream));
	SetIncludeAnim(fgetc(cfgStream));
	SetIncludeMtl(fgetc(cfgStream));
	SetIncludeMeshAnim(fgetc(cfgStream));
	SetIncludeCamLightAnim(fgetc(cfgStream));
	SetIncludeIKJoints(fgetc(cfgStream));
	SetIncludeNormals(fgetc(cfgStream));
	SetIncludeTextureCoords(fgetc(cfgStream));
	SetIncludeObjGeom(fgetc(cfgStream));
	SetIncludeObjShape(fgetc(cfgStream));
	SetIncludeObjCamera(fgetc(cfgStream));
	SetIncludeObjLight(fgetc(cfgStream));
	SetIncludeObjHelper(fgetc(cfgStream));
	SetAlwaysSample(fgetc(cfgStream));
	SetMeshFrameStep(_getw(cfgStream));
	SetKeyFrameStep(_getw(cfgStream));

	// Added for version 0x02
	if (fileVersion > 0x01) {
		SetIncludeVertexColors(fgetc(cfgStream));
	}

	// Added for version 0x03
	if (fileVersion > 0x02) {
		SetPrecision(_getw(cfgStream));
	}

	fclose(cfgStream);

	return TRUE;
}

void AsciiExp::WriteConfig()
{
	TSTR filename = GetCfgFilename();
	FILE* cfgStream;

	cfgStream = _tfopen(filename, _T("wb"));
	if (!cfgStream)
		return;

	// Write CFG version
	_putw(CFG_VERSION, cfgStream);

	fputc(GetIncludeMesh(), cfgStream);
	fputc(GetIncludeAnim(), cfgStream);
	fputc(GetIncludeMtl(), cfgStream);
	fputc(GetIncludeMeshAnim(), cfgStream);
	fputc(GetIncludeCamLightAnim(), cfgStream);
	fputc(GetIncludeIKJoints(), cfgStream);
	fputc(GetIncludeNormals(), cfgStream);
	fputc(GetIncludeTextureCoords(), cfgStream);
	fputc(GetIncludeObjGeom(), cfgStream);
	fputc(GetIncludeObjShape(), cfgStream);
	fputc(GetIncludeObjCamera(), cfgStream);
	fputc(GetIncludeObjLight(), cfgStream);
	fputc(GetIncludeObjHelper(), cfgStream);
	fputc(GetAlwaysSample(), cfgStream);
	_putw(GetMeshFrameStep(), cfgStream);
	_putw(GetKeyFrameStep(), cfgStream);
	fputc(GetIncludeVertexColors(), cfgStream);
	_putw(GetPrecision(), cfgStream);

	fclose(cfgStream);
}


BOOL MtlKeeper::AddMtl(Mtl* mtl)
{
	if (!mtl) {
		return FALSE;
	}

	int numMtls = mtlTab.Count();
	for (int i = 0; i < numMtls; i++) {
		if (mtlTab[i] == mtl) {
			return FALSE;
		}
	}
	mtlTab.Append(1, &mtl, 25);

	return TRUE;
}

int MtlKeeper::GetMtlID(Mtl* mtl)
{
	int numMtls = mtlTab.Count();
	for (int i = 0; i < numMtls; i++) {
		if (mtlTab[i] == mtl) {
			return i;
		}
	}
	return -1;
}

int MtlKeeper::Count()
{
	return mtlTab.Count();
}

Mtl* MtlKeeper::GetMtl(int id)
{
	return mtlTab[id];
}


std::vector<Mtl*> AsciiExp::SortMtrl()
{
	int i, j;
	std::vector<Mtl*> ret;
	int numMtls = mtlList.Count();
	//일단 모든 메터리얼을 벡터에 넣는다
	for (i = 0; i < numMtls; i++)
	{
		Mtl* pMtrl = mtlList.GetMtl(i);
		if (!pMtrl) continue;

		if (mtlList.GetMtl(i)->NumSubMtls() > 0)
		{
			for (j = 0; j < mtlList.GetMtl(i)->NumSubMtls(); j++)
			{
				ret.push_back(mtlList.GetMtl(i)->GetSubMtl(j));
			}
		}
		else
		{
			ret.push_back(mtlList.GetMtl(i));
		}
	}

	//그런 후 이미지 이름이 같은 건 다 뺀다
	return DuplicationNameDelete(ret);

}

//duplication name delete function
std::vector<Mtl*> AsciiExp::DuplicationNameDelete(std::vector<Mtl*>& vMtl)
{
	std::vector<Mtl*> ret;
	std::vector<Mtl*>::iterator _iter = vMtl.begin();
	for (; _iter != vMtl.end(); ++_iter)
	{
		if (!(*_iter))continue;
		if (!(*_iter)->GetName()) continue;
		bool IsDuplication = false;
		std::vector<Mtl*>::iterator _iterCmp = ret.begin();
		for (; _iterCmp != ret.end(); ++_iterCmp)
		{
			if (!(*_iterCmp))continue;
			if (!(*_iterCmp)->GetName()) continue;

			if (_wcsicmp((*_iter)->GetName(), (*_iterCmp)->GetName()) == 0)
			{
				IsDuplication = true;
				break;
			}
		}
		if (!IsDuplication)
		{
			ret.push_back((*_iter));
		}
	}
	return ret;
}