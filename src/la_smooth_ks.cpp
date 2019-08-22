//----------------------------------------------------------------------------------
//		�Ǐ��̈�I���E���ω��t�B���^
//----------------------------------------------------------------------------------
#include <windows.h>
#include <stdlib.h>
#include "filter.h"
#include "version.h"

#define RANGE 3
static float *mean_y[RANGE] = {nullptr, nullptr, nullptr};
static float *var_y[RANGE] = {nullptr, nullptr, nullptr};
static struct {
	int range, w;
} allocated = {0, 0};


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

BOOL
func_exit(FILTER *fp)
{
	for ( int i=0; i<allocated.range; i++ ) {
		free(mean_y[i]);
		mean_y[i] = nullptr;
		free(var_y[i]);
		var_y[i] = nullptr;
	}
	allocated.range = 0;
	allocated.w = 0;
	
	return TRUE;
}

//---------------------------------------------------------------------
//		�t�B���^�����֐�
//---------------------------------------------------------------------
#define YCP_EDIT(fpip, x, y) ((fpip)->ycp_edit+(x)+(fpip)->max_w*(y))
#define YCP_TEMP(fpip, x, y) ((fpip)->ycp_temp+(x)+(fpip)->max_w*(y))
#define PMOD(a, b) ( a<0 ? a%b+b : a%b )

static int imin, imax;

static void set_mean_var(FILTER_PROC_INFO *fpip, int ii, int y);
static void set_smoothed(FILTER_PROC_INFO *fpip, int x, int y, int offset);

// �{��
BOOL
func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	if ( fpip->h < (RANGE*2-1) || fpip->w < (RANGE*2-1) ) {
		OutputDebugString("The image width or height is too small for the range.");
		return FALSE;
	}
	if ( allocated.range < RANGE || allocated.w < fpip->w ) {
		func_exit(fp);
		allocated.range = RANGE;
		allocated.w = fpip->w;
		for (int i=0; i<RANGE; i++) {
			mean_y[i] = static_cast<float *>( malloc(sizeof(int)*static_cast<size_t>(fpip->w+RANGE)) );
			if ( mean_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
			var_y[i] = static_cast<float *>( malloc(sizeof(int)*static_cast<size_t>(fpip->w+RANGE)) );
			if ( var_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
		}
	}
	
	imin = -(RANGE/2);
	imax = imin + RANGE;
	int offset = -imin;
	for (int i=imin; i<imax-1; i++) {
		set_mean_var(fpip, PMOD(i+offset, RANGE), i);
	}
	for (int y=0; y<fpip->h; y++) {
		int i=imax-1;
		set_mean_var(fpip, PMOD(i+offset, RANGE), y+i);
		for (int x=0; x<fpip->w; x++) {
			set_smoothed(fpip, x, y, offset);
		}
		offset = PMOD(offset+1, RANGE);
	}

	PIXEL_YC *swap = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = swap;

	return TRUE;
}

// ���ׂĂ� x �ɂ��āCy �𒆐S�Ƃ��� mean_y, var_y �� i �ɃZ�b�g
static void
set_mean_var(FILTER_PROC_INFO *fpip, int ii, int y)
{
	int vskip = 0;
	for (int x=0; x<fpip->w; x++) {
		YCP_TEMP(fpip, x, ii)->y = 0;
		for (int i=imin; i<imax; i++) {
			if ( y+i < 0 || fpip->w <= y+i ) {
				vskip++;
			} else {
				YCP_TEMP(fpip, x, ii)->y = static_cast<short>( YCP_TEMP(fpip, x, ii)->y + YCP_EDIT(fpip, x, y+i)->y );
			}
		}
	}
	for (int x=imin; x<fpip->w+imax; x++) {
		int xx = x-imin;
		mean_y[ii][xx] = 0;
		int hskip = 0;
		for (int j=imin; j<imax; j++) {
			if ( x+j < 0 || fpip->h <= x+j ) {
				hskip++;
			} else {
				mean_y[ii][xx] += YCP_TEMP(fpip, x+j, ii)->y;
			}
		}
		mean_y[ii][xx] /= static_cast<float>( (RANGE-vskip)*(RANGE-hskip) );
	}
}

// x, y �̕�������f�l�� ycp_temp �ɃZ�b�g
static void
set_smoothed(FILTER_PROC_INFO *fpip, int x, int y, int offset)
{
	
}
