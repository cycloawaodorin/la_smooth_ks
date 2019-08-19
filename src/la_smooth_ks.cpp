//----------------------------------------------------------------------------------
//		局所領域選択・平均化フィルタ
//----------------------------------------------------------------------------------
#include <windows.h>
#include "filter.h"
#include "version.h"

//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define CHECK_N 2
static TCHAR check_name0[] = "注目点中心2次モーメントで領域を選択";
static TCHAR check_name1[] = "色差のみ平均化";
static TCHAR *check_name[CHECK_N] = {check_name0, check_name1};
static int check_default[CHECK_N]  = {0, 0};
#define FILTER_NAME "局所領域選択・平均化フィルタ"
static TCHAR filter_name[] = FILTER_NAME;
static TCHAR filter_information[] = FILTER_NAME " " VERSION " by KAZOON";

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0, // 設定ウィンドウサイズ - 設定しない
	filter_name,
	0, // トラックバーの数
	NULL, NULL, NULL, NULL, // その他トラックバー関連
	CHECK_N,
	check_name,
	check_default,
	func_proc,
	NULL, // func_init
	func_exit,
	NULL, NULL, NULL, NULL, NULL, 0,
	filter_information,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	0, 0 // reserve
};

EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall
GetFilterTable(void)
{
	return &filter;
}

//---------------------------------------------------------------------
//		フィルタ処理関数
//---------------------------------------------------------------------
BOOL
func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	return TRUE;
}

BOOL
func_exit(FILTER *fp)
{
	// OutputDebugString("func_exit called\n");
	return TRUE;
}
