//----------------------------------------------------------------------------------
//		�Ǐ��̈�I���E���ω��t�B���^
//----------------------------------------------------------------------------------
#include <windows.h>
#include "filter.h"
#include "version.h"

//---------------------------------------------------------------------
//		�t�B���^�\���̒�`
//---------------------------------------------------------------------
#define CHECK_N 2
static TCHAR check_name0[] = "���ړ_���S2�����[�����g�ŗ̈��I��";
static TCHAR check_name1[] = "�F���̂ݕ��ω�";
static TCHAR *check_name[CHECK_N] = {check_name0, check_name1};
static int check_default[CHECK_N]  = {0, 0};
#define FILTER_NAME "�Ǐ��̈�I���E���ω��t�B���^"
static TCHAR filter_name[] = FILTER_NAME;
static TCHAR filter_information[] = FILTER_NAME " " VERSION " by KAZOON";

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0, // �ݒ�E�B���h�E�T�C�Y - �ݒ肵�Ȃ�
	filter_name,
	0, // �g���b�N�o�[�̐�
	NULL, NULL, NULL, NULL, // ���̑��g���b�N�o�[�֘A
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
//		�t�B���^�����֐�
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
